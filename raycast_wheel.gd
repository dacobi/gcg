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

	if shapecast:
		shapecast.target_position.x = -(rest_dist + final_over_extend) - offset_shapecast
		shapecast.add_exception(get_parent())
		shapecast.position.y = offset_shapecast

func apply_wheel_physics(car: RigidBody3D) -> void:
	var final_over_extend = over_extend
	if car.get("over_extend") != null:
		final_over_extend = car.get("over_extend")

	target_position.y = -(rest_dist + wheel_radius + final_over_extend)
	if shapecast:
		shapecast.target_position.x = -(rest_dist + final_over_extend) - offset_shapecast
		shapecast.enabled = (car.get("use_shapecast") > 0.5)

	# 1. Rotate wheel visuals locally (local Y is cylinder axis in this model)
	if visual_wheel and is_instance_valid(visual_wheel):
		var forward_dir = -global_basis.z
		var speed = forward_dir.dot(car.linear_velocity)
		visual_wheel.rotate_object_local(Vector3(0, 1, 0), speed * get_physics_process_delta_time() / wheel_radius)

	# 2. Collision detection using RayCast3D or ShapeCast3D
	var is_colliding_now = false
	var contact = Vector3.ZERO
	var normal = Vector3.UP

	if shapecast and shapecast.enabled:
		if shapecast.is_colliding():
			is_colliding_now = true
			contact = shapecast.get_collision_point(0)
			normal = shapecast.get_collision_normal(0)
	else:
		if is_colliding():
			is_colliding_now = true
			contact = get_collision_point()
			normal = get_collision_normal()

	if not is_colliding_now:
		# Extended suspension state - position visual pivot at full 3D coordinate
		if visual_pivot and is_instance_valid(visual_pivot):
			visual_pivot.position = Vector3(position.x, position.y - (rest_dist + final_over_extend), position.z)
			visual_pivot.rotation.y = rotation.y
		return

	# 3. Calculate suspension travel and Hooke's Law + damping force
	# Calculate suspension length along the local suspension axis (local Y) to avoid diagonal projection bugs with ShapeCast3D
	var vertical_dist = -global_basis.y.dot(contact - global_position)
	var spring_len = maxf(0.0, vertical_dist - wheel_radius)
	var offset = rest_dist - spring_len

	# Position visual pivot at full 3D coordinate to match physics mount points exactly
	if visual_pivot and is_instance_valid(visual_pivot):
		visual_pivot.position = Vector3(position.x, position.y - spring_len, position.z)
		visual_pivot.rotation.y = rotation.y

	# Force application origin is the wheel center
	var wheel_center = global_position - global_basis.y * spring_len
	var force_pos = wheel_center - car.global_position

	var spring_force = spring_strength * offset
	var tire_vel = car.linear_velocity + car.angular_velocity.cross(wheel_center - car.to_global(car.center_of_mass))
	
	# Compression vs expansion damping
	var local_y_vel = global_basis.y.dot(tire_vel)
	var damping_coeff = spring_damping / 15.0
	if car.get("damping_compression") != null and car.get("damping_relaxation") != null:
		damping_coeff = car.get("damping_compression")
		if local_y_vel > 0.0:
			damping_coeff = car.get("damping_relaxation")
			
	var spring_damp_f = (damping_coeff * 15.0) * local_y_vel
	var suspension_force = clampf(spring_force - spring_damp_f, -max_spring_force, max_spring_force)

	var y_force = suspension_force * normal

	# 4. Motor Acceleration force
	if is_motor and car.get("motor_input"):
		var forward_dir = -global_basis.z
		var speed_val = forward_dir.dot(car.linear_velocity)
		var speed_ratio = speed_val / car.get("max_speed")
		var ac = 1.0
		if car.get("accel_curve") and car.get("accel_curve") is Curve:
			ac = car.get("accel_curve").sample_baked(speed_ratio)
		var accel_force = forward_dir * car.get("acceleration") * car.get("motor_input") * ac
		car.apply_force(accel_force, force_pos)

	# 5. Tire lateral X traction (Steering response)
	var steering_x_vel = global_basis.x.dot(tire_vel)
	var forward_dir = -global_basis.z

	grip_factor = absf(steering_x_vel / maxf(0.001, tire_vel.length()))
	if absf(forward_dir.dot(car.linear_velocity)) < 0.2:
		grip_factor = 0.0

	var x_traction = 0.5
	if grip_curve and grip_curve is Curve:
		x_traction = grip_curve.sample_baked(grip_factor)

	if not car.get("hand_break") and grip_factor < 0.2:
		car.set("is_slipping", false)

	if car.get("hand_break"):
		x_traction = 0.01
	elif car.get("is_slipping"):
		x_traction = 0.1
		
	# Scale lateral grip dynamically using the wheel_friction_slip property (reference: 10.5)
	if car.get("wheel_friction_slip") != null:
		x_traction = x_traction * (car.get("wheel_friction_slip") / 10.5)

	var gravity_val = -car.get_gravity().y
	var x_force = -global_basis.x * steering_x_vel * x_traction * ((car.mass * gravity_val) / car.get("total_wheels"))

	# 6. Tire longitudinal Z traction (braking, drag, rolling resistance)
	var f_speed = forward_dir.dot(tire_vel)
	var final_z_traction = z_traction
	if car.get("z_traction") != null:
		final_z_traction = car.get("z_traction")
		
	var z_friction = final_z_traction
	if absf(f_speed) < 0.01:
		z_friction = 2.0
		
	if is_braking:
		if car.get("brake_force_value") != null:
			z_friction = car.get("brake_force_value") * 0.002
		else:
			z_friction = z_brake_traction
			
	var z_force = global_basis.z * f_speed * z_friction * ((car.mass * gravity_val) / car.get("total_wheels"))

	# 7. Counter sliding at low speeds
	if absf(f_speed) < 0.1:
		var susp = global_basis.y * suspension_force
		z_force.z -= susp.z * car.global_basis.y.dot(Vector3.UP)
		x_force.x -= susp.x * car.global_basis.y.dot(Vector3.UP)

	# Apply all three forces back to parent car Body
	car.apply_force(y_force, force_pos)
	car.apply_force(x_force, force_pos)
	car.apply_force(z_force, force_pos)

	# Apply counter forces on colliding rigid bodies (if any)
	if shapecast:
		for idx in shapecast.get_collision_count():
			var collider = shapecast.get_collider(idx)
			if collider is RigidBody3D:
				collider.apply_force(-(x_force + y_force + z_force), force_pos)
