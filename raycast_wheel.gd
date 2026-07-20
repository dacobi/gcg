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
		if car.get("accel_curve"):
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
	if car.get("hand_break"):
		x_traction = 0.01
	elif car.get("is_slipping"):
		x_traction = 0.1

	var gravity        = -car.get_gravity().y
	var weight_on_wheel = (car.mass * gravity)/float(car.get("total_wheels"))
	
	# Massive baseline traction multiplier
	var x_force        = -global_basis.x * steering_x_vel * x_traction * weight_on_wheel * 3.0
	
	# Aggressive Dynamic Understeer: Front wheels lose efficiency as speed increases
	# This guarantees the front tires wash out before the rear tires can spin out
	var speed = car.linear_velocity.length()
	if is_steer:
		var understeer_factor = clampf(1.0 - (speed / 60.0), 0.3, 1.0)
		x_force *= understeer_factor
	
	# Clamp lateral force to prevent 360 spins at 250 km/h
	var max_grip = weight_on_wheel * float(car.get("wheel_friction_slip"))
	
	# Rear wheels get an absolute hard cap bonus to prevent snapping
	if not is_steer:
		max_grip *= 1.5
		
	if x_force.length() > max_grip:
		x_force = x_force.normalized() * max_grip

	## Tire Z traction (Longitudinal)
	var f_vel          := forward_dir.dot(tire_vel)
	var z_friction     := z_traction
	if is_braking:
		z_friction = z_brake_traction
	var z_force        = global_basis.z * f_vel * z_friction * ((car.mass * gravity)/float(car.get("total_wheels")))
	
	# Clamp longitudinal force (braking) to prevent instant stops at 250 km/h
	# 1.5 represents a very strong real-world sports car tire friction coefficient
	var max_z_grip = ((car.mass * gravity)/float(car.get("total_wheels"))) * 1.5
	if z_force.length() > max_z_grip:
		z_force = z_force.normalized() * max_z_grip

	car.apply_force(y_force, force_pos)
	car.apply_force(x_force, force_pos)
	car.apply_force(z_force, force_pos)
