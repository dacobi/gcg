extends RigidBody3D

# Control inputs (set by Lua script via godotSetProperty)
var accel_input: float = 0.0
var brake_input: float = 0.0
var steer_input: float = 0.0
var handbrake_input: float = 0.0
var air_time: float = 0.0

var mount_FL = Vector3(-1.0, -0.025, -1.7)
var mount_FR = Vector3(1.0, -0.025, -1.7)
var mount_RL = Vector3(-1.1, -0.025, 1.75)
var mount_RR = Vector3(1.1, -0.025, 1.75)
var radius_front = 0.5
var radius_rear = 0.5

# Tuning coefficients set by Lua script
var engine_force_value = 8000.0
var brake_force_value = 200.0
var max_steer = 0.8
var wheel_friction_slip = 10.5
var suspension_travel = 0.25
var suspension_stiffness = 120.0
var suspension_max_force = 12000.0
var damping_compression = 6.0
var damping_relaxation = 8.0
var downforce_multiplier = 120.0
var car_mass = 1200.0
var center_of_mass_y = -0.2
var center_of_mass_z = -0.1

var esp_max_yaw_damping = 1.5
var drift_assist_damping = 25.0
var aero_drag_coeff = 0.30
var steer_speed_limit_max_speed = 80.0
var steer_speed_limit_min_mult = 0.7

# Properties accessed by RaycastWheel
var motor_input := 0.0
var hand_break := false
var in_standing_burnout := false
var is_slipping := false
var total_wheels := 4
var acceleration := 600.0
var max_speed := 58.33
var slider_max_speed_kmh := 210.0
var nitro_input := 0.0
var nitro_seconds := 0.0
var max_nitro_seconds := 100.0
var nitro_active := false
var nitro_flames: Array = []
var in_countdown: bool = false
var over_extend = 0.05
var z_traction = 0.05
var default_radius_front = 0.5
var default_radius_rear = 0.5
var use_shapecast = 1.0
var drivetrain_mode = 0.0
var tire_turn_speed = 6.0
var slip_FL: float = 0.0
var slip_FR: float = 0.0
var slip_RL: float = 0.0
var slip_RR: float = 0.0

var engine_rpm: float = 1000.0

var fmod_event = null
var fmod_banks = []
var fmod_banks_loaded = false
var engine_audio: AudioStreamPlayer = null
var show_collision_debug = 0.0

var shift_timer: float = 0.0
var current_gear_sim: int = -2
var brake_timer: float = 0.0
var manual_transmission: bool = false
var manual_gear_input: int = 1
var gear_max_speeds: Array = [11.1, 22.2, 36.1, 50.0, 63.8, 100.0]
var gear_torque_ratios: Array = [2.2, 1.6, 1.25, 1.0, 0.85, 0.7]

var skid_marks: Array[GPUParticles3D] = []
var smoke_particles: Array[GPUParticles3D] = []

# Resources
var accel_curve: Curve
var grip_curve: Curve

var wheels: Array = []
var start_transform: Transform3D

func _ready():
	# Configure Main Hull layer and mask
	collision_layer = 2 # Car layer
	collision_mask = 5  # Hits World (1) and Props (4)

	# Resize collision box to act as the main hull, shrunk and lifted slightly
	var body_col = get_node_or_null("BodyCol")
	if body_col and body_col.shape is BoxShape3D:
		body_col.shape = body_col.shape.duplicate()
		body_col.shape.size = Vector3(2.8, 0.4, 3.0)
		body_col.position.y += 0.2
		
		# Add 4 perfectly smooth skid spheres at the bottom corners to prevent snagging on sharp ramps
		for z_pos in [-1.5, 1.5]:
			for x_pos in [-1.4, 1.4]:
				var sphere_col = CollisionShape3D.new()
				var sphere = SphereShape3D.new()
				sphere.radius = 0.3
				sphere_col.shape = sphere
				sphere_col.position = Vector3(x_pos, 0.1, z_pos)
				
				# Generate debug visual for the sphere so it works with the toggle
				var mi = MeshInstance3D.new()
				mi.name = "CollisionDebugVisual"
				var sph = SphereMesh.new()
				sph.radius = 0.3
				sph.height = 0.6
				mi.mesh = sph
				var mat = StandardMaterial3D.new()
				mat.albedo_color = Color(1, 0, 0, 0.4)
				mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
				mi.material_override = mat
				mi.visible = false
				sphere_col.add_child(mi)
				
				add_child(sphere_col)

	default_radius_front = radius_front
	default_radius_rear = radius_rear
	start_transform = global_transform
	
	# Programmatic construction of curves from the tutorial values
	accel_curve = Curve.new()
	accel_curve.add_point(Vector2(0, 1.0)) # Massive torque off the line!
	accel_curve.add_point(Vector2(0.3, 0.9))
	accel_curve.add_point(Vector2(0.6, 0.8))
	accel_curve.add_point(Vector2(1, 0.1))

	grip_curve = Curve.new()
	grip_curve.add_point(Vector2(0, 1.0))
	grip_curve.add_point(Vector2(0.25, 0.8))
	grip_curve.add_point(Vector2(0.9, 0.0))

	# Enable	# Contact monitor for sparks and FMOD collisions
	contact_monitor = true
	max_contacts_reported = 4
	body_entered.connect(_on_prop_collided)
	
	# Create visual debug meshes for CollisionShape3D children
	for child in get_children():
		if child is CollisionShape3D:
			var mi = MeshInstance3D.new()
			mi.name = "CollisionDebugVisual"
			var shape = child.shape
			if shape is BoxShape3D:
				var box = BoxMesh.new()
				box.size = shape.size
				mi.mesh = box
			elif shape is SphereShape3D:
				var sph = SphereMesh.new()
				sph.radius = shape.radius
				sph.height = shape.radius * 2.0
				mi.mesh = sph
			elif shape is CylinderShape3D:
				var cyl = CylinderMesh.new()
				cyl.top_radius = shape.radius
				cyl.bottom_radius = shape.radius
				cyl.height = shape.height
				mi.mesh = cyl
			elif shape is CapsuleShape3D:
				var cap = CapsuleMesh.new()
				cap.radius = shape.radius
				cap.height = shape.height
				mi.mesh = cap
				
			var mat = StandardMaterial3D.new()
			mat.albedo_color = Color(1, 0, 0, 0.4)
			mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
			mi.material_override = mat
			mi.visible = false
			child.add_child(mi)
			
	# --- PROP-ONLY ANGLED BUMPER ---
	# We use a RigidBody3D attached via a joint so it properly shares the physics space and collides at high speeds!
	var bumper = RigidBody3D.new()
	bumper.name = "AngledBumper"
	bumper.mass = 1.0
	bumper.gravity_scale = 0.0
	bumper.collision_layer = 8 # Bumper layer
	bumper.collision_mask = 4  # ONLY collides with Props (Cones/Barriers)
	bumper.contact_monitor = true
	bumper.max_contacts_reported = 2
	bumper.body_entered.connect(_on_prop_collided)
	
	var bumper_col = CollisionShape3D.new()
	var bumper_shape = BoxShape3D.new()
	bumper_shape.size = Vector3(3.0, 1.0, 2.0)
	bumper_col.shape = bumper_shape
	# Slant it forwards like a plow! (Negative X rotation tilts it down)
	bumper_col.rotation_degrees.x = -45.0
	# Position it at the very front of the car
	bumper_col.position = Vector3(0.0, -0.2, -2.5)
	
	var bumper_mi = MeshInstance3D.new()
	bumper_mi.name = "CollisionDebugVisual"
	var bumper_mesh = BoxMesh.new()
	bumper_mesh.size = bumper_shape.size
	bumper_mi.mesh = bumper_mesh
	var bumper_mat = StandardMaterial3D.new()
	bumper_mat.albedo_color = Color(0, 0, 1, 0.4) # BLUE
	bumper_mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	bumper_mi.material_override = bumper_mat
	bumper_mi.visible = false
	
	bumper_col.add_child(bumper_mi)
	bumper.add_child(bumper_col)
	add_child(bumper)
	
	var joint = Generic6DOFJoint3D.new()
	joint.name = "BumperJoint"
	add_child(joint)
	# Position the joint exactly where the bumper is for maximum stability
	joint.position = bumper_col.position
	joint.node_a = joint.get_path_to(self)
	joint.node_b = joint.get_path_to(bumper)
	# A new Generic6DOFJoint3D has all axes locked by default, making it perfectly rigid!
	
	# --- PROP-ONLY REAR BUMPER ---
	var rear_bumper = RigidBody3D.new()
	rear_bumper.name = "RearBumper"
	rear_bumper.mass = 1.0
	rear_bumper.gravity_scale = 0.0
	rear_bumper.collision_layer = 8 # Bumper layer
	rear_bumper.collision_mask = 4  # ONLY collides with Props (Cones/Barriers)
	rear_bumper.contact_monitor = true
	rear_bumper.max_contacts_reported = 2
	rear_bumper.body_entered.connect(_on_prop_collided)
	
	var rear_bumper_col = CollisionShape3D.new()
	var rear_bumper_shape = BoxShape3D.new()
	rear_bumper_shape.size = Vector3(3.0, 1.0, 1.0) # Flat wall
	rear_bumper_col.shape = rear_bumper_shape
	rear_bumper_col.position = Vector3(0.0, -0.2, 3.0) # Placed further back at the tail lights
	
	var rear_bumper_mi = MeshInstance3D.new()
	rear_bumper_mi.name = "CollisionDebugVisual"
	var rear_bumper_mesh = BoxMesh.new()
	rear_bumper_mesh.size = rear_bumper_shape.size
	rear_bumper_mi.mesh = rear_bumper_mesh
	var rear_bumper_mat = StandardMaterial3D.new()
	rear_bumper_mat.albedo_color = Color(0, 0, 1, 0.4) # BLUE
	rear_bumper_mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	rear_bumper_mi.material_override = rear_bumper_mat
	rear_bumper_mi.visible = false
	
	rear_bumper_col.add_child(rear_bumper_mi)
	rear_bumper.add_child(rear_bumper_col)
	add_child(rear_bumper)
	
	var rear_joint = Generic6DOFJoint3D.new()
	rear_joint.name = "RearBumperJoint"
	add_child(rear_joint)
	rear_joint.position = rear_bumper_col.position
	rear_joint.node_a = rear_joint.get_path_to(self)
	rear_joint.node_b = rear_joint.get_path_to(rear_bumper)
			
	# Dynamically build wheels at startup
	var use_shapecast = true
	var w_fl = create_wheel("FL", mount_FL, radius_front, true, false, use_shapecast)
	var w_fr = create_wheel("FR", mount_FR, radius_front, true, false, use_shapecast)
	var w_rl = create_wheel("RL", mount_RL, radius_rear, false, true, use_shapecast)
	var w_rr = create_wheel("RR", mount_RR, radius_rear, false, true, use_shapecast)
	wheels = [w_fl, w_fr, w_rl, w_rr]
	
	# Create dynamic skid mark particle systems
	var skid_mat = StandardMaterial3D.new()
	skid_mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	skid_mat.vertex_color_use_as_albedo = true
	skid_mat.albedo_color = Color(0.02, 0.02, 0.02, 0.85)
	var skid_mesh = QuadMesh.new()
	skid_mesh.material = skid_mat
	skid_mesh.size = Vector2(0.45, 0.5)
	skid_mesh.orientation = PlaneMesh.FACE_Y
	var skid_proc = ParticleProcessMaterial.new()
	skid_proc.gravity = Vector3.ZERO
	
	for i in range(4):
		var p = GPUParticles3D.new()
		p.emitting = false
		p.amount = 800
		p.lifetime = 4.0
		p.fixed_fps = 0
		p.process_material = skid_proc
		p.draw_pass_1 = skid_mesh
		p.local_coords = false # Particles stay on road when car moves
		add_child(p)
		skid_marks.append(p)
		
	# Create dynamic burnout/drift smoke particle systems with gray procedural texture
	var noise_grad = Gradient.new()
	noise_grad.set_color(0, Color(0.45, 0.45, 0.50, 0.0))    # Transparent dark gray valleys
	noise_grad.set_color(1, Color(0.70, 0.70, 0.73, 0.10)) # 90% transparent light gray peaks
	
	var noise = FastNoiseLite.new()
	noise.noise_type = FastNoiseLite.TYPE_SIMPLEX_SMOOTH
	noise.frequency = 0.06
	noise.fractal_octaves = 2
	
	var noise_tex = NoiseTexture2D.new()
	noise_tex.noise = noise
	noise_tex.color_ramp = noise_grad
	noise_tex.width = 64
	noise_tex.height = 64
	
	var smoke_mat = StandardMaterial3D.new()
	smoke_mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	smoke_mat.vertex_color_use_as_albedo = true
	smoke_mat.albedo_color = Color(1.0, 1.0, 1.0, 1.0) # Base color modulated by noise texture
	smoke_mat.albedo_texture = noise_tex
	smoke_mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	smoke_mat.billboard_mode = BaseMaterial3D.BILLBOARD_PARTICLES
	var smoke_mesh = QuadMesh.new()
	smoke_mesh.material = smoke_mat
	smoke_mesh.size = Vector2(0.25, 0.25) # Much smaller particles!
	var smoke_proc = ParticleProcessMaterial.new()
	smoke_proc.gravity = Vector3(0.0, 2.0, 0.0) # Smoke rises gently upward
	smoke_proc.direction = Vector3(0.0, 1.0, 0.0)
	smoke_proc.spread = 40.0
	smoke_proc.initial_velocity_min = 0.4
	smoke_proc.initial_velocity_max = 1.2
	smoke_proc.scale_min = 0.4
	smoke_proc.scale_max = 1.0
	
	for i in range(4):
		var sp = GPUParticles3D.new()
		sp.emitting = false
		sp.amount = 500
		sp.lifetime = 1.2
		sp.fixed_fps = 0
		sp.process_material = smoke_proc
		sp.draw_pass_1 = smoke_mesh
		sp.local_coords = false # Smoke clouds trail behind in world space!
		add_child(sp)
		smoke_particles.append(sp)
	
	# --- FMOD INIT ---
	# --- FMOD INIT ---
	if ClassDB.class_exists("FmodServer"):
		fmod_banks.append(FmodServer.load_bank("res://Audio/Master.strings.bank", 0))
		fmod_banks.append(FmodServer.load_bank("res://Audio/Master.bank", 0))
		fmod_banks.append(FmodServer.load_bank("res://Audio/Vehicles.bank", 0))
		fmod_banks.append(FmodServer.load_bank("res://Audio/SFX.bank", 0))
		
	# --- HUD INIT ---
	var canvas = CanvasLayer.new()
	add_child(canvas)
	var hud = Control.new()
	var hud_script = load("res://hud.gd")
	if hud_script:
		hud.set_script(hud_script)
		hud.set("car", self)
		hud.set_anchors_preset(Control.PRESET_FULL_RECT)
		canvas.add_child(hud)
		
	var cp_area = Area3D.new()
	cp_area.name = "CheckpointSphere"
	cp_area.collision_layer = 16
	cp_area.collision_mask = 16
	var cp_col = CollisionShape3D.new()
	var cp_shape = SphereShape3D.new()
	cp_shape.radius = 2.0 # Sphere in center of car
	cp_col.shape = cp_shape
	cp_area.add_child(cp_col)
	add_child(cp_area)
	
	_setup_nitro_flames()
func create_wheel(w_name: String, pos: Vector3, radius: float, is_front: bool, is_drive: bool, use_shapecast: bool) -> RayCast3D:
	var w = RayCast3D.new()
	w.name = "Wheel" + w_name
	w.position = pos
	w.set_script(load("res://raycast_wheel.gd"))
	
	w.wheel_radius = radius
	w.rest_dist = suspension_travel
	w.spring_strength = suspension_stiffness * 50.0
	w.spring_damping = damping_compression * 15.0
	w.max_spring_force = suspension_max_force
	w.over_extend = 0.05
	w.enabled = true
	
	w.is_motor = is_drive
	w.is_steer = is_front
	w.grip_curve = grip_curve
	w.z_brake_traction = 0.5
	w.show_debug = false
	
	# Dynamically bind visual node references
	var pivot_name := ""
	var wheel_node_name := ""
	if w_name == "FL":
		pivot_name = "FrontLeftSteerPivot"
		wheel_node_name = "FrontLeftWheel"
	elif w_name == "FR":
		pivot_name = "FrontRightSteerPivot"
		wheel_node_name = "FrontRightWheel"
	elif w_name == "RL":
		pivot_name = "RearLeftSteerPivot"
		wheel_node_name = "RearLeftWheel"
	elif w_name == "RR":
		pivot_name = "RearRightSteerPivot"
		wheel_node_name = "RearRightWheel"
		
	w.visual_pivot = get_node_or_null(pivot_name)
	if w.visual_pivot:
		w.visual_wheel = w.visual_pivot.get_node_or_null(wheel_node_name)
		
	if use_shapecast:
		var sc = ShapeCast3D.new()
		sc.name = "ShapeCast3D"
		sc.process_physics_priority = -1
		
		var shape = CylinderShape3D.new()
		shape.radius = radius
		shape.height = 0.3
		sc.shape = shape
		
		# Rotate 90 degrees around Z axis so local X points down (casts downwards)
		sc.transform = Transform3D(Basis(Vector3(0, 1, 0), Vector3(-1, 0, 0), Vector3(0, 0, 1)), Vector3.ZERO)
		sc.add_exception(self)
		w.add_child(sc)
		w.shapecast = sc
		
	add_child(w)
	return w


func _physics_process(delta: float) -> void:
	# Update mass and COM from properties set by Lua script
	mass = car_mass
	center_of_mass_mode = RigidBody3D.CENTER_OF_MASS_MODE_CUSTOM
	center_of_mass = Vector3(0, center_of_mass_y, center_of_mass_z)
	
	var horizontal_vel = linear_velocity
	horizontal_vel.y = 0.0
	
	var forward_speed = -global_transform.basis.z.dot(linear_velocity)
	
	# --- ESP & DRIFT ASSIST LOGIC ---
	
	# Dynamic ESP / Yaw Stabilizer
	# A clean, realistic Electronic Stability Program. It applies a smooth counter-torque proportional to 
	# your rotation speed to prevent the car from spinning like a top.
	# You can adjust 'esp_max_yaw_damping' in the editor to control how strictly it fights your drifts.
	var abs_speed = linear_velocity.length()
	var yaw_damping = clampf(abs_speed / 20.0, 0.0, esp_max_yaw_damping)
	
	# Calculate spin (yaw) purely relative to the car's physical roof-to-floor axis!
	var local_angular_vel = global_transform.basis.inverse() * angular_velocity
	var local_yaw_rate = local_angular_vel.y
	
	# Apply a counter-torque strictly along the car's local Up vector
	var counter_torque = -global_transform.basis.y * (local_yaw_rate * mass * yaw_damping)
	apply_torque(counter_torque)
	
	# Handle nitrous system
	if nitro_input > 0.5 and nitro_seconds > 0.01:
		nitro_active = true
		nitro_seconds = maxf(0.0, nitro_seconds - delta)
		for f in nitro_flames:
			if not f.emitting:
				f.emitting = true
	else:
		if nitro_seconds <= 0.01:
			nitro_seconds = 0.0
		nitro_active = false
		for f in nitro_flames:
			if f.emitting:
				f.emitting = false

	# Scale engine acceleration force
	if in_countdown:
		acceleration = 0.0
		max_speed = slider_max_speed_kmh / 3.6
		hand_break = true
	else:
		acceleration = engine_force_value * 0.15
		if nitro_active:
			acceleration *= 2.0
			max_speed = 300.0 / 3.6
		else:
			max_speed = slider_max_speed_kmh / 3.6
		hand_break = handbrake_input > 0.5
	
	# Handle the SDL initial unpressed trigger quirk (both report 0.5)
	var final_accel = accel_input
	var final_brake = brake_input
	if abs(accel_input - 0.5) < 0.02 and abs(brake_input - 0.5) < 0.02:
		final_accel = 0.0
		final_brake = 0.0

	in_standing_burnout = false
	var is_forward_gear = (manual_gear_input >= 1) if manual_transmission else true
	if is_forward_gear and final_accel > 0.05 and final_brake > 0.05 and forward_speed < (25.0 / 3.6):
		in_standing_burnout = true
		var base_motor = final_accel
		if manual_transmission:
			var gear_idx = clamp(manual_gear_input - 1, 0, gear_max_speeds.size() - 1)
			var gear_max = gear_max_speeds[gear_idx]
			if forward_speed > gear_max:
				base_motor = 0.0
			else:
				var torque_mult = gear_torque_ratios[gear_idx]
				if gear_idx >= 1:
					var sim_rpm = 1000.0 + 8000.0 * clampf(maxf(forward_speed, 0.0) / gear_max, 0.0, 1.0)
					var bog_factor = clampf((sim_rpm - 1000.0) / 3000.0, 0.08, 1.0)
					torque_mult *= bog_factor
				base_motor = final_accel * torque_mult
		var steer_creep = clampf((absf(steer_input) - 0.1) / 0.8, 0.0, 1.0)
		var thrust_mult = lerpf(0.0, 0.45, steer_creep)
		motor_input = base_motor * thrust_mult
		wheels[0].is_braking = true
		wheels[1].is_braking = true
		wheels[2].is_braking = false
		wheels[3].is_braking = false
	elif manual_transmission:
		if manual_gear_input == 0:
			# Neutral: Throttle revs engine freely without drive torque. Brake slows car until stopped, then reverse.
			if final_brake > 0.05:
				if forward_speed > 1.0 and final_brake > final_accel:
					motor_input = 0.0
					for w in wheels:
						w.is_braking = true
				else:
					motor_input = -final_brake
					for w in wheels:
						w.is_braking = false
			else:
				motor_input = 0.0
				for w in wheels:
					w.is_braking = false
		else:
			# Forward Gears 1..6: Throttle drives forward. Brake ONLY brakes (NEVER goes into reverse).
			if final_brake > 0.05 and (final_brake >= final_accel or forward_speed >= (25.0 / 3.6)):
				motor_input = 0.0
				for w in wheels:
					w.is_braking = true
			elif final_accel > 0.05:
				var gear_idx = clamp(manual_gear_input - 1, 0, gear_max_speeds.size() - 1)
				var gear_max = gear_max_speeds[gear_idx]
				if forward_speed > gear_max:
					motor_input = 0.0 # Rev limit cut
				else:
					var torque_mult = gear_torque_ratios[gear_idx]
					if gear_idx >= 1:
						var sim_rpm = 1000.0 + 8000.0 * clampf(maxf(forward_speed, 0.0) / gear_max, 0.0, 1.0)
						var bog_factor = clampf((sim_rpm - 1000.0) / 3000.0, 0.08, 1.0)
						torque_mult *= bog_factor
					motor_input = final_accel * torque_mult
				for w in wheels:
					w.is_braking = false
			else:
				motor_input = 0.0
				for w in wheels:
					w.is_braking = false
	else:
		if final_brake > 0.05 and (final_brake >= final_accel or (final_accel > 0.05 and forward_speed >= (25.0 / 3.6))):
			if forward_speed > 1.0:
				motor_input = 0.0
				for w in wheels:
					w.is_braking = true
			else:
				motor_input = -final_brake
				for w in wheels:
					w.is_braking = false
		elif final_accel > 0.05:
			motor_input = final_accel
			for w in wheels:
				w.is_braking = false
		else:
			motor_input = 0.0
			for w in wheels:
				w.is_braking = false
			
	if hand_break:
		wheels[2].is_braking = true
		wheels[3].is_braking = true

	# Run wheel physics exactly like tutorial
	var grounded = false
	for i in range(4):
		var w = wheels[i]
		
		w.rest_dist = suspension_travel
		w.spring_strength = suspension_stiffness * 250.0
		w.spring_damping = damping_compression * 200.0
		
		var r = radius_front if i < 2 else radius_rear
		w.wheel_radius = r
		if w.visual_wheel and is_instance_valid(w.visual_wheel):
			var scale_f = r / 0.5
			w.visual_wheel.scale = Vector3(scale_f, scale_f, scale_f)
			
		w.z_brake_traction = brake_force_value * 0.002
		w.is_motor = (i >= 2) # Strict RWD realistic physics
		w.is_steer = (i < 2)
		
		w.apply_wheel_physics(self)
		
		# Update steering
		if w.is_steer:
			var current_speed = linear_velocity.length()
			
			# RACING MODE: Cap normal steering to ~25 degrees (0.45 rad) max, then apply speed limit
			var normal_max_steer = minf(max_steer, 0.45)
			var speed_steer_limit = clampf(1.0 - (current_speed / steer_speed_limit_max_speed), steer_speed_limit_min_mult, 1.0)
			var racing_steer = normal_max_steer * speed_steer_limit
			
			# DRIFT MODE: Unlock the steering up to the full max_steer limit when sliding!
			var velocity_dir = horizontal_vel.normalized()
			var forward_dir = -global_transform.basis.z
			var slip_angle = 0.0
			if horizontal_vel.length_squared() > 1.0:
				slip_angle = acos(clampf(forward_dir.dot(velocity_dir), -1.0, 1.0))
			
			var drift_steer = clampf(slip_angle * 1.2, 0.0, max_steer)
			
			var dynamic_max_steer = maxf(racing_steer, drift_steer)
			
			var target_steer = -float(steer_input) * dynamic_max_steer
			w.rotation.y = move_toward(w.rotation.y, target_steer, tire_turn_speed * delta)
		
		if w.is_colliding():
			grounded = true
			
			# Skid marks
			if skid_marks.size() > i and skid_marks[i] != null:
				skid_marks[i].global_position = w.get_collision_point() + Vector3.UP * 0.05
				skid_marks[i].look_at(skid_marks[i].global_position + global_transform.basis.z)
				
				var is_skidding = false
				if hand_break or w.grip_factor > 0.4:
					is_skidding = true
				if w.is_braking and forward_speed > 10.0:
					is_skidding = true
				if in_standing_burnout and i >= 2:
					is_skidding = true
				skid_marks[i].emitting = is_skidding
				if smoke_particles.size() > i and smoke_particles[i] != null:
					smoke_particles[i].global_position = w.get_collision_point() + Vector3.UP * 0.2
					smoke_particles[i].emitting = is_skidding
		else:
			if skid_marks.size() > i and skid_marks[i] != null:
				skid_marks[i].emitting = false
			if smoke_particles.size() > i and smoke_particles[i] != null:
				smoke_particles[i].emitting = false
			
	# Update slip telemetry variables
	slip_FL = wheels[0].grip_factor
	slip_FR = wheels[1].grip_factor
	slip_RL = wheels[2].grip_factor
	slip_RR = wheels[3].grip_factor

	# --- AIR STABILIZATION ---
	if not grounded:
		air_time += delta
		if air_time > 0.2:
			# Heavily damp all rotational momentum (forward pitching from the jump lip)
			# This stops the spinning and locks the car into its natural launch trajectory
			var damp_torque = -angular_velocity * 15000.0
			apply_torque(damp_torque)
	else:
		air_time = 0.0

	if grounded:
		center_of_mass_mode = RigidBody3D.CENTER_OF_MASS_MODE_CUSTOM
		center_of_mass = Vector3(0, center_of_mass_y, center_of_mass_z)
	else:
		center_of_mass_mode = RigidBody3D.CENTER_OF_MASS_MODE_CUSTOM
		center_of_mass = Vector3.DOWN*0.5
		
	# Downforce and Aerodynamic Drag
	var current_speed_for_aero = linear_velocity.length()
	apply_central_force(-global_transform.basis.y * (current_speed_for_aero * downforce_multiplier))
	
	if current_speed_for_aero > 0.1:
		var drag_force = -linear_velocity.normalized() * (current_speed_for_aero * current_speed_for_aero * aero_drag_coeff)
		apply_central_force(drag_force)

	# Update collision debug visual visibility
	var show_debug = show_collision_debug > 0.5
	for child in get_children():
		if child is CollisionShape3D:
			var mi = child.get_node_or_null("CollisionDebugVisual")
			if mi:
				mi.visible = show_debug
				
	# Also update the bumper's debug visual
	var bumper = get_node_or_null("AngledBumper")
	if bumper:
		for child in bumper.get_children():
			if child is CollisionShape3D:
				var mi = child.get_node_or_null("CollisionDebugVisual")
				if mi:
					mi.visible = show_debug
					
	var rear_bumper = get_node_or_null("RearBumper")
	if rear_bumper:
		for child in rear_bumper.get_children():
			if child is CollisionShape3D:
				var mi = child.get_node_or_null("CollisionDebugVisual")
				if mi:
					mi.visible = show_debug
					
	# --- FAKE GEAR RPM LOGIC FOR FMOD ---
	# Calculate horizontal speed to prevent downshifting while drifting sideways!
	# This ignores the Y axis (falling) but respects lateral slides.
	horizontal_vel = linear_velocity
	horizontal_vel.y = 0.0
	var speed = horizontal_vel.length()
	var gears = 6
	
	# Dynamic gear lengths (in m/s). 11.1 m/s = 40 km/h, 22.2 = 80 km/h, etc.
	var gear_max_speeds = [11.1, 22.2, 36.1, 50.0, 63.8, 100.0]
	
	if (brake_input > 0.1 and forward_speed > 1.0) or hand_break:
		brake_timer += delta
	else:
		brake_timer = 0.0
		
	var target_gear = 0
	if manual_transmission:
		if manual_gear_input == 0:
			if motor_input < -0.05:
				target_gear = -1 # Reverse
			else:
				target_gear = -2 # Neutral
		else:
			target_gear = clamp(manual_gear_input - 1, 0, gears - 1)
	else:
		if speed < 0.5 and abs(motor_input) < 0.05:
			target_gear = -2
		elif motor_input < -0.05 or (forward_speed < -0.5 and motor_input < 0.05):
			target_gear = -1
		else:
			for i in range(gears):
				if speed < gear_max_speeds[i]:
					target_gear = i
					break
				if i == gears - 1:
					target_gear = i
			
	if shift_timer <= 0.0:
		if target_gear > current_gear_sim:
			# Initiate an upshift! Disconnect the clutch for 250ms (faster, punchy shifts)
			shift_timer = 0.25
			current_gear_sim = target_gear
		elif target_gear < current_gear_sim:
			if brake_timer > 0.5 and not manual_transmission:
				# Drop down gears silently when braking hard in auto mode
				current_gear_sim = target_gear
			else:
				if current_gear_sim <= 0 or manual_transmission:
					current_gear_sim = target_gear
				else:
					var prev_max = gear_max_speeds[current_gear_sim - 1]
					if speed < prev_max - 2.0:
						current_gear_sim = target_gear
		
	var target_rpm = 1000.0
	if in_countdown:
		# Auto-clutch disengaged during countdown: rev freely up to 9000 RPM!
		target_rpm = 1000.0 + accel_input * 8000.0
		engine_rpm = lerp(engine_rpm, target_rpm, 15.0 * delta)
	elif in_standing_burnout:
		# Standing burnout: rev freely up to 9000 RPM while tires smoke!
		target_rpm = 1000.0 + accel_input * 8000.0
		engine_rpm = lerp(engine_rpm, target_rpm, 15.0 * delta)
	elif manual_transmission and current_gear_sim == -2 and accel_input > 0.05:
		# Rev freely in Neutral!
		target_rpm = 1000.0 + accel_input * 8000.0
		engine_rpm = lerp(engine_rpm, target_rpm, 15.0 * delta)
	elif brake_timer > 0.5 and not manual_transmission:
		# Clutch out and drop to idle immediately while braking in auto mode
		target_rpm = 1000.0
		engine_rpm = lerp(engine_rpm, target_rpm, 10.0 * delta)
	elif shift_timer > 0.0:
		shift_timer -= delta
		# Clutch is in: purely visual and audio gear shift effect (no momentum loss)
		target_rpm = 5000.0
		engine_rpm = lerp(engine_rpm, target_rpm, 6.0 * delta)
	elif current_gear_sim >= 0 and motor_input <= 0.05 and not manual_transmission:
		# Coasting without throttle: clutch disengaged, RPM drops smoothly to idle (~1000 RPM).
		target_rpm = 1000.0
		engine_rpm = lerp(engine_rpm, target_rpm, 5.0 * delta)
	else:
		# Clutch is engaged: RPM bound to wheel speed in the current gear
		var gear_speed = 0.0
		
		if current_gear_sim == -1:
			# Reverse gear logic (max 50 km/h = 13.88 m/s, max 5000 RPM)
			gear_speed = clamp(speed / 13.88, 0.0, 1.0)
			target_rpm = lerp(1000.0, 5000.0, gear_speed)
		elif current_gear_sim == -2:
			# Neutral gear
			target_rpm = 1000.0
		else:
			if manual_transmission:
				gear_speed = clampf(speed / gear_max_speeds[current_gear_sim], 0.0, 1.0)
				target_rpm = lerp(1000.0, 9000.0, gear_speed)
			else:
				var prev_max = 0.0 if current_gear_sim <= 0 else gear_max_speeds[current_gear_sim - 1]
				var current_max = gear_max_speeds[current_gear_sim] if current_gear_sim >= 0 else gear_max_speeds[0]
				gear_speed = clamp((speed - prev_max) / (current_max - prev_max), 0.0, 1.0)
				
				if current_gear_sim <= 0:
					target_rpm = lerp(1000.0, 9000.0, gear_speed)
				else:
					target_rpm = lerp(5000.0, 9000.0, gear_speed)
		
		# Clamp to realistic limits
		var max_rpm = 5000.0 if current_gear_sim == -1 else 10000.0
		target_rpm = clamp(target_rpm, 1000.0, max_rpm)
		
		# Responsive approach to target (crisp re-engagement when throttle is pressed)
		engine_rpm = lerp(engine_rpm, target_rpm, 15.0 * delta)
	
	# --- FMOD ENGINE UPDATE ---
	if ClassDB.class_exists("FmodServer"):
		FmodServer.update()
		if fmod_event == null and not fmod_banks_loaded:
			# FMOD bank loading takes a few frames to populate the event descriptions
			var events = FmodServer.get_all_event_descriptions()
			if events.size() > 0:
				fmod_banks_loaded = true
				for e in events:
					if str(e.get_guid()) == "{0c8363b4-23af-4f9c-af4b-0951bfd37d84}":
						fmod_event = FmodServer.create_event_instance_from_description(e)
						if fmod_event: fmod_event.start()
		
		# We must set a listener position, otherwise we might not hear the 3D event
		if FmodServer.has_method("set_listener_transform3d"):
			FmodServer.set_listener_transform3d(0, global_transform)
			
		if fmod_event:
			fmod_event.set_parameter_by_name("RPM", engine_rpm)
			fmod_event.set_parameter_by_name("Load", clamp(motor_input, 0.0, 1.0))
			if fmod_event.has_method("set_3d_attributes"):
				fmod_event.set_3d_attributes(global_transform)

func _on_prop_collided(body: Node):
	if body.has_method("on_nitro_collected"):
		body.on_nitro_collected(self)
	elif body is PhysicsBody3D and body.collision_layer == 4:
		if ClassDB.class_exists("FmodServer"):
			if FmodServer.has_method("create_event_instance"):
				var ev = FmodServer.create_event_instance("event:/Interactables/Wooden Collision")
				if ev:
					if ev.has_method("set_parameter_by_name"):
						var collision_speed = linear_velocity.length() * 2.0
						ev.set_parameter_by_name("speed", collision_speed)
					if ev.has_method("set_3d_attributes"):
						ev.set_3d_attributes(self.global_transform)
					if ev.has_method("start"):
						ev.start()
					if ev.has_method("release"):
						ev.release()

func add_nitro(seconds: float) -> void:
	nitro_seconds = minf(max_nitro_seconds, nitro_seconds + seconds)

func _setup_nitro_flames() -> void:
	var positions = [
		Vector3(-0.626, -0.12, 3.45),
		Vector3(-0.506, -0.12, 3.45),
		Vector3(0.506, -0.12, 3.45),
		Vector3(0.626, -0.12, 3.45)
	]
	for pos in positions:
		var parts = CPUParticles3D.new()
		parts.amount = 200
		parts.lifetime = 0.08
		parts.speed_scale = 1.0
		parts.explosiveness = 0.05
		parts.position = pos
		parts.local_coords = true # Emit in local car space so flames are 100% solid, coherent, and gap-free at any speed!
		
		parts.direction = Vector3(0, 0, 1) # Shoot backward along +Z
		parts.spread = 0.5 # Laser-focused blowtorch jet (was 5.0)
		parts.gravity = Vector3(0, 1.0, 0) # Slight upward heat rise
		parts.initial_velocity_min = 15.0
		parts.initial_velocity_max = 25.0
		
		# Scale taper from 6cm down to 1.2cm needle point
		var curve = Curve.new()
		curve.add_point(Vector2(0.0, 1.0))
		curve.add_point(Vector2(1.0, 0.2))
		parts.scale_amount_curve = curve
		parts.scale_amount_min = 1.0
		parts.scale_amount_max = 1.0
		
		# Color gradient: Short Blue Root (0-15%) -> Darker Burnt Orange (40%) -> Crimson Red (75%)
		var grad = Gradient.new()
		grad.add_point(0.0, Color(0.0, 0.5, 1.0, 1.0))   # Hot electric blue root
		grad.add_point(0.15, Color(0.0, 0.7, 1.0, 1.0))  # Short blue nozzle core (0-15%)
		grad.add_point(0.4, Color(0.85, 0.3, 0.0, 0.95)) # Darker burnt orange
		grad.add_point(0.75, Color(0.6, 0.05, 0.0, 0.8)) # Deep darker crimson red
		grad.add_point(1.0, Color(0.2, 0.0, 0.0, 0.0))   # Fade out
		parts.color_ramp = grad
		
		var mesh = QuadMesh.new()
		mesh.size = Vector2(0.06, 0.06) # 6cm diameter exactly matching tailpipe
		var mat = StandardMaterial3D.new()
		mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		mat.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
		mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
		mat.vertex_color_use_as_albedo = true
		mat.billboard_mode = BaseMaterial3D.BILLBOARD_PARTICLES
		mesh.material = mat
		parts.mesh = mesh
		
		parts.emitting = false
		add_child(parts)
		nitro_flames.append(parts)
