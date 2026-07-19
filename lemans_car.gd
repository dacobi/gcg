extends RigidBody3D

# Control inputs (set by Lua script via godotSetProperty)
var accel_input: float = 0.0
var brake_input: float = 0.0
var steer_input: float = 0.0
var handbrake_input: float = 0.0

var mount_FL = Vector3(-1.3, 0.025, -1.7)
var mount_FR = Vector3(1.3, 0.025, -1.7)
var mount_RL = Vector3(-1.3, 0.025, 1.75)
var mount_RR = Vector3(1.3, 0.025, 1.75)
var radius_front = 0.5
var radius_rear = 0.5

# Tuning coefficients set by Lua script
var engine_force_value = 8000.0
var brake_force_value = 200.0
var max_steer = 0.4
var wheel_friction_slip = 10.5
var suspension_travel = 0.25
var suspension_stiffness = 120.0
var suspension_max_force = 12000.0
var damping_compression = 6.0
var damping_relaxation = 8.0
var downforce_multiplier = 120.0
var car_mass = 1200.0
var center_of_mass_y = -0.2
var center_of_mass_z = 0.0

# Properties accessed by RaycastWheel
var motor_input := 0.0
var hand_break := false
var is_slipping := false
var total_wheels := 4
var acceleration := 600.0
var max_speed := 96.0
var over_extend = 0.05
var z_traction = 0.05
var default_radius_front = 0.5
var default_radius_rear = 0.5
var use_shapecast = 1.0
var drivetrain_mode = 0.0
var tire_turn_speed = 10.0
var slip_FL: float = 0.0
var slip_FR: float = 0.0
var slip_RL: float = 0.0
var slip_RR: float = 0.0

var engine_rpm: float = 1000.0

var fmod_event = null
var fmod_event_2 = null
var fmod_banks = []
var engine_audio: AudioStreamPlayer = null
var show_collision_debug = 0.0

var shift_timer: float = 0.0
var current_gear_sim: int = 0

# Resources
var accel_curve: Curve
var grip_curve: Curve

var wheels: Array = []
var start_transform: Transform3D

func _ready():
	# Resize collision box to extend just beyond the wheels
	var body_col = get_node_or_null("BodyCol")
	if body_col and body_col.shape is BoxShape3D:
		body_col.shape = body_col.shape.duplicate()
		body_col.shape.size = Vector3(3.1, 0.5, 3.4)

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

	# Enable contacts reporting
	contact_monitor = true
	max_contacts_reported = 4
	
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
			
	# Dynamically build wheels at startup
	var use_shapecast = true
	var w_fl = create_wheel("FL", mount_FL, radius_front, true, false, use_shapecast)
	var w_fr = create_wheel("FR", mount_FR, radius_front, true, false, use_shapecast)
	var w_rl = create_wheel("RL", mount_RL, radius_rear, false, true, use_shapecast)
	var w_rr = create_wheel("RR", mount_RR, radius_rear, false, true, use_shapecast)
	wheels = [w_fl, w_fr, w_rl, w_rr]
	
	# --- FMOD INIT ---
	# --- FMOD INIT ---
	if ClassDB.class_exists("FmodServer"):
		fmod_banks.append(FmodServer.load_bank("res://Audio/Master.strings.bank", 0))
		fmod_banks.append(FmodServer.load_bank("res://Audio/Master.bank", 0))
		fmod_banks.append(FmodServer.load_bank("res://Audio/Vehicles.bank", 0))
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
	
	var forward_speed = -global_transform.basis.z.dot(linear_velocity)
	
	# Scale engine acceleration force
	acceleration = engine_force_value * 0.15
	
	# Set inputs
	hand_break = handbrake_input > 0.5
	
	# Handle the SDL initial unpressed trigger quirk (both report 0.5)
	var final_accel = accel_input
	var final_brake = brake_input
	if abs(accel_input - 0.5) < 0.02 and abs(brake_input - 0.5) < 0.02:
		final_accel = 0.0
		final_brake = 0.0

	if final_accel > final_brake:
		motor_input = final_accel
		for w in wheels:
			w.is_braking = false
	elif final_brake > final_accel:
		if forward_speed > 1.0:
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
			
	if hand_break:
		wheels[2].is_braking = true
		wheels[3].is_braking = true
		
	# Apply drivetrain mode (AWD, RWD, FWD)
	for i in range(4):
		if drivetrain_mode < 0.5:
			wheels[i].is_motor = true
		elif drivetrain_mode < 1.5:
			wheels[i].is_motor = (i >= 2)
		else:
			wheels[i].is_motor = (i < 2)

	# Apply steering angle
	var target_angle = -steer_input * max_steer
	wheels[0].rotation.y = lerp(wheels[0].rotation.y, target_angle, tire_turn_speed * delta)
	wheels[1].rotation.y = lerp(wheels[1].rotation.y, target_angle, tire_turn_speed * delta)
	
	# Run wheel physics
	var grounded = false
	for w in wheels:
		w.rest_dist = suspension_travel
		w.spring_strength = suspension_stiffness * 250.0
		w.spring_damping = damping_compression * 200.0
		w.max_spring_force = suspension_max_force
		w.z_brake_traction = brake_force_value * 0.002
		
		w.apply_wheel_physics(self)
		if w.is_colliding():
			grounded = true
			
	# Update slip telemetry variables
	slip_FL = wheels[0].grip_factor
	slip_FR = wheels[1].grip_factor
	slip_RL = wheels[2].grip_factor
	slip_RR = wheels[3].grip_factor
			
	# Align center of mass when airborne for stability
	if not grounded:
		center_of_mass.y = center_of_mass_y - 0.5
		
	# Downforce
	var current_speed = linear_velocity.length()
	apply_central_force(-global_transform.basis.y * (current_speed * downforce_multiplier))

	# Update collision debug visual visibility
	var show_debug = show_collision_debug > 0.5
	for child in get_children():
		if child is CollisionShape3D:
			var mi = child.get_node_or_null("CollisionDebugVisual")
			if mi:
				mi.visible = show_debug
				
	# --- FAKE GEAR RPM LOGIC FOR FMOD ---
	# We use a fixed speed boundary to ensure the car shifts reliably without requiring insane terminal velocity
	var speed = linear_velocity.length()
	var gears = 6
	var speed_per_gear = 12.0 # 43 km/h per gear, hits 6th at 259 km/h
	
	if shift_timer <= 0.0:
		var target_gear = clamp(int(speed / speed_per_gear), 0, gears - 1)
		if target_gear > current_gear_sim:
			# Initiate an upshift! Disconnect the clutch for 250ms
			shift_timer = 0.25
			current_gear_sim = target_gear
		elif target_gear < current_gear_sim:
			# Hysteresis: only downshift if we drop at least 2 m/s below the gear threshold
			var speed_in_gear = speed - (current_gear_sim * speed_per_gear)
			if speed_in_gear < -2.0:
				current_gear_sim = target_gear
		
	var target_rpm = 1000.0
	if shift_timer > 0.0:
		shift_timer -= delta
		# Clutch is in: RPM drops freely, NO engine power to wheels (creates physical stutter!)
		target_rpm = 4000.0
		motor_input = 0.0
		engine_rpm = lerp(engine_rpm, target_rpm, 12.0 * delta)
	else:
		# Clutch is out: RPM bound to wheel speed in the current gear
		var speed_in_gear = speed - (current_gear_sim * speed_per_gear)
		var gear_speed = clamp(speed_in_gear / speed_per_gear, 0.0, 1.0)
		
		if current_gear_sim == 0:
			target_rpm = lerp(1000.0, 9000.0, gear_speed)
		else:
			target_rpm = lerp(5000.0, 9000.0, gear_speed)
		
		# Add rev spikes based on throttle input when accelerating
		if motor_input > 0.1:
			target_rpm += 1500.0 * motor_input
			
		# Clamp to realistic limits
		target_rpm = clamp(target_rpm, 1000.0, 10000.0)
		
		# Smooth approach to target
		engine_rpm = lerp(engine_rpm, target_rpm, 20.0 * delta)
	
	# --- FMOD ENGINE UPDATE ---
	if ClassDB.class_exists("FmodServer"):
		FmodServer.update()
		if fmod_event == null and fmod_event_2 == null:
			# FMOD bank loading takes a few frames to populate the event descriptions
			var events = FmodServer.get_all_event_descriptions()
			if events.size() > 0:
				for e in events:
					if str(e.get_guid()) == "{0c8363b4-23af-4f9c-af4b-0951bfd37d84}":
						fmod_event = FmodServer.create_event_instance_from_description(e)
						if fmod_event: fmod_event.start()
					if str(e.get_guid()) == "{7aa5e8f1-8ec2-42c6-b465-1241a603a055}":
						fmod_event_2 = FmodServer.create_event_instance_from_description(e)
						if fmod_event_2: fmod_event_2.start()
		
		# We must set a listener position, otherwise we might not hear the 3D event
		if FmodServer.has_method("set_listener_transform3d"):
			FmodServer.set_listener_transform3d(0, global_transform)
			
		if fmod_event:
			fmod_event.set_parameter_by_name("RPM", engine_rpm)
			fmod_event.set_parameter_by_name("Load", clamp(motor_input, 0.0, 1.0))
			if fmod_event.has_method("set_3d_attributes"):
				fmod_event.set_3d_attributes(global_transform)
				
		if fmod_event_2:
			fmod_event_2.set_parameter_by_name("RPM", engine_rpm)
			fmod_event_2.set_parameter_by_name("Load", clamp(motor_input, 0.0, 1.0))
			if fmod_event_2.has_method("set_3d_attributes"):
				fmod_event_2.set_3d_attributes(global_transform)
