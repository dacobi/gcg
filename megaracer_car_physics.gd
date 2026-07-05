extends CharacterBody3D

# Control inputs (set by Lua script via godotSetProperty)
var accel_input: float = 0.0
var brake_input: float = 0.0
var steer_input: float = 0.0

# Physics parameters
var max_speed = 96.0
var acceleration = 66.0
var braking = 105.0
var friction = 1.5
var drag = 0.03
var gravity = 25.0
var steer_speed = 1.8

# Reset parameters
var fall_reset_delay = 2.0

# Current state
var speed = 0.0
var fall_time = 0.0
var start_transform: Transform3D
var last_grounded_pos: Vector3
var start_saved = false
var wheels_grounded = true
var path_follow: PathFollow3D

# Grouped body nodes list and base heights
var body_nodes = []
var body_base_y = {}

# Suspension Mount offsets (relative to car origin)
var mount_FL = Vector3(-1.15, -0.1, -1.1)
var mount_FR = Vector3(1.15, -0.1, -1.1)
var mount_RL = Vector3(-1.3, -0.1, 0.9)
var mount_RR = Vector3(1.3, -0.1, 0.9)
var rest_length = 0.5
var max_travel = 0.25

# Wheel radiuses
var radius_front = 0.4
var radius_rear = 0.5

# 2-DoF Body Visuals tilt states (Y bounce is now physical!)
var body_roll = 0.0
var body_roll_vel = 0.0
var body_pitch = 0.0
var body_pitch_vel = 0.0

# Tuning coefficients
var hover_stiffness = 300.0
var hover_damping = 25.0
var body_roll_stiffness = 180.0
var body_roll_damping = 14.0
var body_pitch_stiffness = 180.0
var body_pitch_damping = 14.0

var body_roll_steer_multiplier = 0.2
var body_pitch_accel_multiplier = 0.08
var body_pitch_brake_multiplier = 0.12
var max_steer_visual_angle = 0.39 # ~22.5 degrees

func _physics_process(delta):
	# Save starting position on the first physics frame
	if not start_saved:
		start_transform = global_transform
		last_grounded_pos = global_position
		
		# Initialize PathFollow3D helper node
		var path = get_parent().get_node_or_null("Path3D")
		if path:
			path_follow = PathFollow3D.new()
			path_follow.rotation_mode = PathFollow3D.ROTATION_ORIENTED
			path.add_child(path_follow)
			
		# Cache individual body nodes and their initial Y positions
		body_nodes.clear()
		body_base_y.clear()
		for node_name in ["Body", "Canopy", "GlassCoverL", "GlassCoverR", "HeadlightBulbL", "HeadlightBulbR"]:
			var node = get_node_or_null(node_name)
			if node:
				body_nodes.append(node)
				body_base_y[node] = node.position.y
				
		start_saved = true
		
	var space_state = get_world_3d().direct_space_state
	if not space_state: return
	
	# Get ground distances for all 4 wheels
	var d_FL = get_wheel_dist(mount_FL, space_state)
	var d_FR = get_wheel_dist(mount_FR, space_state)
	var d_RL = get_wheel_dist(mount_RL, space_state)
	var d_RR = get_wheel_dist(mount_RR, space_state)
	
	var max_d = rest_length + max_travel
	wheels_grounded = (d_FL < max_d) or (d_FR < max_d) or (d_RL < max_d) or (d_RR < max_d)
	
	# Always apply world gravity, whether grounded or falling
	velocity += Vector3.DOWN * gravity * delta
	
	if wheels_grounded:
		fall_time = 0.0
		last_grounded_pos = global_position
		
		# Physical Hover Suspension
		# Project velocity into local space to get vertical speed
		var local_vel = global_transform.basis.inverse() * velocity
		var avg_d = (d_FL + d_FR + d_RL + d_RR) / 4.0
		var avg_compression = rest_length - avg_d
		
		var hover_force = avg_compression * hover_stiffness - local_vel.y * hover_damping
		local_vel.y += hover_force * delta
		
		# Prevent extreme bouncing limits
		if avg_compression > max_travel:
			local_vel.y = max(local_vel.y, 0.0)
			
		velocity = global_transform.basis * local_vel
	else:
		fall_time += delta
		if fall_time >= fall_reset_delay:
			reset_to_track()
			return
			
	# Get local forward direction (global -Z direction)
	var forward_dir = -global_transform.basis.z.normalized()
	
	# Project velocity onto forward direction (3D when grounded, horizontal when airborne)
	if wheels_grounded:
		speed = velocity.dot(forward_dir)
	else:
		var horizontal_vel = velocity
		horizontal_vel.y = 0.0
		var horizontal_forward = forward_dir
		horizontal_forward.y = 0.0
		if horizontal_forward.length_squared() > 0.001:
			horizontal_forward = horizontal_forward.normalized()
			speed = horizontal_vel.dot(horizontal_forward)
		else:
			speed = 0.0
			
	# Handle acceleration
	if accel_input > 0.0 and wheels_grounded:
		speed += acceleration * accel_input * delta
	# Handle braking/reverse
	elif brake_input > 0.0 and wheels_grounded:
		speed -= braking * brake_input * delta
	else:
		# Apply friction and drag (no friction in mid-air)
		var current_friction = friction if wheels_grounded else 0.0
		speed -= (current_friction * sign(speed) + drag * speed * speed) * delta
		if abs(speed) < 0.1:
			speed = 0.0
			
	# Clamp speed
	speed = clamp(speed, -max_speed * 0.4, max_speed)
	
	# Handle steering (with understeer at high speeds)
	if abs(speed) > 0.1 and wheels_grounded:
		var steer_direction = -steer_input
		var abs_speed = abs(speed)
		
		# Ramps up quickly from 0 to 20 speed
		var speed_factor = clamp(abs_speed / 20.0, 0.0, 1.0)
		
		# Slowly drops off from 40 to max_speed (loses 60% of steering lock at top speed)
		var understeer_factor = 1.0 - clamp((abs_speed - 40.0) / (max_speed - 40.0), 0.0, 0.6)
		
		var turn_rate = steer_speed * steer_direction * speed_factor * understeer_factor * sign(speed)
		rotate_object_local(Vector3.UP, turn_rate * delta)
		
	# Apply forward speed to velocity while maintaining vertical physics
	forward_dir = -global_transform.basis.z.normalized()
	var vertical_vel = velocity - velocity.project(forward_dir)
	velocity = forward_dir * speed + vertical_vel
	
	# Apply lateral friction (grip) to stop sideways sliding
	if wheels_grounded:
		var local_vel = global_transform.basis.inverse() * velocity
		# Extremely high lateral grip (kills sideways velocity instantly)
		local_vel.x = lerp(local_vel.x, 0.0, min(1.0, 60.0 * delta))
		velocity = global_transform.basis * local_vel
	
	# Use move_and_slide with central SphereShape3D as safety net against tunneling
	move_and_slide()
	
	# Align the car's up vector with the exact track normal from PathFollow3D
	if path_follow:
		var curve = path_follow.get_parent().curve
		if curve:
			var closest_offset = curve.get_closest_offset(global_position)
			path_follow.progress = closest_offset
			
			var track_transform = path_follow.global_transform
			var banked_up = track_transform.basis.y
			
			var local_forward = -global_transform.basis.z
			var new_right = local_forward.cross(banked_up).normalized()
			var new_forward = banked_up.cross(new_right).normalized()
			
			var target_b = Basis(new_right, banked_up, -new_forward)
			global_transform.basis = global_transform.basis.slerp(target_b, 25.0 * delta).orthonormalized()
	else:
		var target_up = Vector3.UP
		var local_forward = -global_transform.basis.z
		local_forward.y = 0.0
		local_forward = local_forward.normalized()
		
		var new_right = local_forward.cross(target_up).normalized()
		var target_b = Basis(new_right, target_up, -local_forward)
		global_transform.basis = global_transform.basis.slerp(target_b, 2.0 * delta).orthonormalized()
		
	# Update visual wheel nodes and 2-DoF body forces
	update_visuals(delta, d_FL, d_FR, d_RL, d_RR)

func update_visuals(delta, d_FL, d_FR, d_RL, d_RR):
	var td_FL = clamp(d_FL, rest_length - max_travel, rest_length + max_travel)
	var td_FR = clamp(d_FR, rest_length - max_travel, rest_length + max_travel)
	var td_RL = clamp(d_RL, rest_length - max_travel, rest_length + max_travel)
	var td_RR = clamp(d_RR, rest_length - max_travel, rest_length + max_travel)
	
	var c_FL = rest_length - td_FL
	var c_FR = rest_length - td_FR
	var c_RL = rest_length - td_RL
	var c_RR = rest_length - td_RR
	
	# 1. Update visual wheel nodes position (suspension travel offset)
	var pivot_FL = get_node_or_null("FrontLeftSteerPivot")
	var pivot_FR = get_node_or_null("FrontRightSteerPivot")
	var wheel_RL = get_node_or_null("RearLeftWheel")
	var wheel_RR = get_node_or_null("RearRightWheel")
	
	if pivot_FL:
		pivot_FL.position.y = mount_FL.y - td_FL + radius_front
		pivot_FL.rotation.y = -steer_input * max_steer_visual_angle
	if pivot_FR:
		pivot_FR.position.y = mount_FR.y - td_FR + radius_front
		pivot_FR.rotation.y = -steer_input * max_steer_visual_angle
	if wheel_RL:
		wheel_RL.position.y = mount_RL.y - td_RL + radius_rear
	if wheel_RR:
		wheel_RR.position.y = mount_RR.y - td_RR + radius_rear
		
	# Pivot Axles to follow wheels
	var up = global_transform.basis.y
	var anchor_FL = get_node_or_null("AxleAnchorFL")
	if anchor_FL and pivot_FL: anchor_FL.look_at(pivot_FL.global_position, up)
	var anchor_FR = get_node_or_null("AxleAnchorFR")
	if anchor_FR and pivot_FR: anchor_FR.look_at(pivot_FR.global_position, up)
	var anchor_RL = get_node_or_null("AxleAnchorRL")
	if anchor_RL and wheel_RL: anchor_RL.look_at(wheel_RL.global_position, up)
	var anchor_RR = get_node_or_null("AxleAnchorRR")
	if anchor_RR and wheel_RR: anchor_RR.look_at(wheel_RR.global_position, up)
		
	# Spin wheels
	var wheel_FL = get_node_or_null("FrontLeftSteerPivot/FrontLeftWheel")
	var wheel_FR = get_node_or_null("FrontRightSteerPivot/FrontRightWheel")
	if wheel_FL: wheel_FL.rotate_object_local(Vector3(0, 1, 0), speed * delta * 2.0)
	if wheel_FR: wheel_FR.rotate_object_local(Vector3(0, 1, 0), speed * delta * 2.0)
	if wheel_RL: wheel_RL.rotate_object_local(Vector3(0, 1, 0), speed * delta * 2.0)
	if wheel_RR: wheel_RR.rotate_object_local(Vector3(0, 1, 0), speed * delta * 2.0)
		
	# 2. Simulate 2-DoF Body Forces (roll and pitch only, bounce is physical now)
	var target_roll_steer = -steer_input * clamp(abs(speed) / max_speed, 0.0, 1.0) * body_roll_steer_multiplier
	var target_roll_susp = ((c_FL + c_RL) - (c_FR + c_RR)) * 0.15
	var target_roll = target_roll_steer + target_roll_susp
	var force_roll = (target_roll - body_roll) * body_roll_stiffness - body_roll_vel * body_roll_damping
	body_roll_vel += force_roll * delta
	body_roll += body_roll_vel * delta
	
	var target_pitch_accel = 0.0
	if accel_input > 0.0:
		target_pitch_accel = accel_input * body_pitch_accel_multiplier
	elif brake_input > 0.0:
		target_pitch_accel = -brake_input * body_pitch_brake_multiplier
	var target_pitch_susp = ((c_RL + c_RR) - (c_FL + c_FR)) * 0.15
	var target_pitch = target_pitch_accel + target_pitch_susp
	var force_pitch = (target_pitch - body_pitch) * body_pitch_stiffness - body_pitch_vel * body_pitch_damping
	body_pitch_vel += force_pitch * delta
	body_pitch += body_pitch_vel * delta
	
	for node in body_nodes:
		if is_instance_valid(node):
			# Base Y position remains locked, only apply rotation tilt!
			node.position.y = body_base_y[node]
			node.rotation.z = body_roll
			node.rotation.x = body_pitch

func get_wheel_dist(local_offset: Vector3, space_state) -> float:
	var global_origin = global_transform * local_offset
	var ray_start = global_origin
	var ray_end = global_origin - global_transform.basis.y * (rest_length + 0.4)
	
	var query = PhysicsRayQueryParameters3D.create(ray_start, ray_end)
	query.exclude = [get_rid()]
	var result = space_state.intersect_ray(query)
	
	if result:
		return (ray_start - result.position).length()
	return rest_length + max_travel

func reset_to_track():
	if path_follow:
		var curve = path_follow.get_parent().curve
		if curve:
			var closest_offset = curve.get_closest_offset(last_grounded_pos)
			path_follow.progress = closest_offset
			
			global_transform = path_follow.global_transform
			global_position += global_transform.basis.y * 1.5 # Drop from slightly above
	else:
		global_transform = start_transform
		
	velocity = Vector3.ZERO
	speed = 0.0
	fall_time = 0.0
	body_roll = 0.0
	body_roll_vel = 0.0
	body_pitch = 0.0
	body_pitch_vel = 0.0
