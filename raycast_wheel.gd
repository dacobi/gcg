extends RayCast3D
class_name RaycastWheel

@export var shapecast : ShapeCast3D
@export var offset_shapecast : float = 0.3

@export_group("Wheel properties")
@export var spring_strength := 5000.0
@export var spring_damping := 150.0
@export var max_spring_force : float = INF
@export var rest_dist := 0.25
@export var over_extend := 0.05
@export var wheel_radius := 0.5
@export var z_traction := 0.05
@export var z_brake_traction := 0.25

@export_category("Motor")
@export var is_motor := false
@export var is_steer := false
@export var grip_curve : Curve

# Visual references set dynamically at runtime
var visual_pivot: Node3D = null
var visual_wheel: Node3D = null

var engine_force := 0.0
var grip_factor  := 0.0
var is_braking   := false

func _ready() -> void:
	var final_over_extend = over_extend
	var car = get_parent()
	if car and car.get("over_extend") != null:
		final_over_extend = car.get("over_extend")
	
	target_position.y = -(rest_dist + wheel_radius + final_over_extend)
	add_exception(get_parent())

	if shapecast:
		shapecast.target_position.x = -(rest_dist + final_over_extend) - offset_shapecast
		shapecast.add_exception(get_parent())
		shapecast.position.y = offset_shapecast

func apply_wheel_physics(car: RigidBody3D) -> void:
	target_position.y = -(rest_dist + wheel_radius + over_extend)
	force_raycast_update()
	
	## Rotates wheel visuals
	var forward_dir   := -global_basis.z
	var vel           := forward_dir.dot(car.linear_velocity)
	if visual_wheel and is_instance_valid(visual_wheel):
		visual_wheel.rotate_object_local(Vector3(0, 1, 0), vel * get_physics_process_delta_time() / wheel_radius)

	if not is_colliding(): 
		if visual_pivot and is_instance_valid(visual_pivot):
			visual_pivot.position = Vector3(position.x, position.y - (rest_dist + over_extend), position.z)
		return

	var contact       := get_collision_point()
	var spring_len    := maxf(0.0, global_position.distance_to(contact) - wheel_radius)
	var offset        := rest_dist - spring_len

	if visual_pivot and is_instance_valid(visual_pivot):
		var target_y = position.y - spring_len
		visual_pivot.position = Vector3(position.x, move_toward(visual_pivot.position.y, target_y, 5 * get_physics_process_delta_time()), position.z)
		visual_pivot.rotation.y = rotation.y

	contact = global_position - global_basis.y * spring_len # Contact is now the wheel origin point
	var force_pos     := contact - car.global_position

	## Spring forces
	var spring_force  := spring_strength * offset
	var tire_vel      := car.linear_velocity + car.angular_velocity.cross(contact - car.global_position)
	var spring_damp_f := spring_damping * global_basis.y.dot(tire_vel)

	var y_force       := (spring_force - spring_damp_f) * get_collision_normal()

	## Acceleration
	if is_motor and car.get("motor_input"):
		var speed_ratio = vel / float(car.get("max_speed"))
		var ac := 1.0
		
		# Reverse speed limiter (max 50 km/h = 13.88 m/s)
		if vel < -13.88 and float(car.get("motor_input")) < 0.0:
			ac = 0.0
		elif car.get("accel_curve"):
			ac = car.get("accel_curve").sample_baked(speed_ratio)
			
		var accel_force = forward_dir * float(car.get("acceleration")) * float(car.get("motor_input")) * ac
		car.apply_force(accel_force, force_pos)

	## Tire X traction (Steering)
	var steering_x_vel := global_basis.x.dot(tire_vel)

	grip_factor        = absf(steering_x_vel/maxf(0.001, tire_vel.length()))
	var x_traction     := 1.0
	if grip_curve:
		x_traction = grip_curve.sample_baked(grip_factor)

	if not car.get("hand_break") and grip_factor < 0.2:
		car.set("is_slipping", false)
		
	# Handbrake only locks the REAR wheels, allowing the front wheels to steer the drift!
	if car.get("hand_break") and not is_steer:
		x_traction = 0.01
	elif car.get("is_slipping"):
		x_traction = 0.1
		
	# Prevent traction from dropping completely to 0 on ice, keeping some slide control
	x_traction = maxf(x_traction, 0.3)
	
	if grip_factor > 0.4:
		if is_steer:
			# Front wheels: give massive grip bonus during a slide to pull the nose out
			x_traction = maxf(x_traction, 0.8)
		else:
			# Rear wheels: guarantee enough grip to catch the slide when you release the e-brake
			x_traction = maxf(x_traction, 0.6)

	var gravity        = -car.get_gravity().y
	var weight_on_wheel = (car.mass * gravity)/float(car.get("total_wheels"))
	
	# Convert viscous lateral drag (parachute effect) into constant Coulomb friction
	# We cap the lateral slip multiplier to 3.0 so you can slide sideways at 200km/h without stopping instantly
	var lateral_slip = minf(absf(steering_x_vel), 3.0)
	var x_force = -global_basis.x * signf(steering_x_vel) * lateral_slip * x_traction * weight_on_wheel * 2.0
	
	# Moderate Dynamic Understeer: Front wheels lose some efficiency as speed increases
	# This keeps the car stable at high speeds without making it impossible to drift
	var speed = car.linear_velocity.length()
	if is_steer:
		var understeer_factor = clampf(1.0 - (speed / 100.0), 0.6, 1.0)
		x_force *= understeer_factor
	
	# Clamp lateral force to prevent 360 spins at 250 km/h
	var max_grip = weight_on_wheel * float(car.get("wheel_friction_slip"))
	
	# Rear wheels get a slight absolute hard cap bonus to prevent snapping
	if not is_steer:
		max_grip *= 1.15
		
	if x_force.length() > max_grip:
		x_force = x_force.normalized() * max_grip

	## Tire Z traction (Longitudinal)
	var f_vel          := forward_dir.dot(tire_vel)
	var z_friction     := z_traction
	if is_braking:
		z_friction = z_brake_traction
	var z_force        = global_basis.z * f_vel * z_friction * ((car.mass * gravity)/float(car.get("total_wheels")))
	
	# Clamp longitudinal force (braking) to prevent instant stops at 250 km/h
	# 5x baseline (7.5G max decel)
	var max_z_grip = weight_on_wheel * 7.5
	if z_force.length() > max_z_grip:
		z_force = z_force.normalized() * max_z_grip

	car.apply_force(y_force, force_pos)
	car.apply_force(x_force, force_pos)
	car.apply_force(z_force, force_pos)
