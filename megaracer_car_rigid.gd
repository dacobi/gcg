extends VehicleBody3D

# Control inputs (set by Lua script via godotSetProperty)
var accel_input: float = 0.0
var brake_input: float = 0.0
var steer_input: float = 0.0
var handbrake_input: float = 0.0

var mount_FL = Vector3(-1.15, -0.1, -1.1)
var mount_FR = Vector3(1.15, -0.1, -1.1)
var mount_RL = Vector3(-1.3, -0.1, 0.9)
var mount_RR = Vector3(1.3, -0.1, 0.9)
var radius_front = 0.4
var radius_rear = 0.5

var wheels = {}

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

var start_transform: Transform3D

func _ready():
	start_transform = global_transform
	
	# Create visual debug meshes for all CollisionShape3D children
	for child in get_children():
		if child is CollisionShape3D:
			var mi = MeshInstance3D.new()
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
			child.add_child(mi)
	
	create_wheel("FL", mount_FL, radius_front, true, false)
	create_wheel("FR", mount_FR, radius_front, true, false)
	create_wheel("RL", mount_RL, radius_rear, false, true)
	create_wheel("RR", mount_RR, radius_rear, false, true)

func create_wheel(w_name, pos, radius, is_front, is_drive):
	var w = VehicleWheel3D.new()
	w.name = "Wheel" + w_name
	w.position = pos
	
	w.wheel_radius = radius
	w.wheel_rest_length = 0.15
	
	w.use_as_steering = is_front
	w.use_as_traction = is_drive
	
	add_child(w)
	wheels[w_name] = w
	
func _physics_process(delta):
	for w_name in wheels:
		var w = wheels[w_name]
		if handbrake_input > 0.5 and (w_name == "RL" or w_name == "RR"):
			w.wheel_friction_slip = wheel_friction_slip * 0.2
		else:
			w.wheel_friction_slip = wheel_friction_slip
			
		w.suspension_travel = suspension_travel
		w.suspension_stiffness = suspension_stiffness
		w.suspension_max_force = suspension_max_force
		w.damping_compression = damping_compression
		w.damping_relaxation = damping_relaxation
		
	mass = car_mass
	center_of_mass = Vector3(0, center_of_mass_y, 0)
		
	# Apply steering (speed-sensitive)
	var steer_speed = linear_velocity.length()
	var current_max_steer = max(0.08, max_steer - (steer_speed / 100.0) * 0.32)
	steering = lerp(steering, -steer_input * current_max_steer, 10.0 * delta)
	
	# Apply engine and brakes
	engine_force = 0.0
	brake = 0.0
	
	if accel_input > 0.0:
		engine_force = -accel_input * engine_force_value
	elif brake_input > 0.0:
		# If moving forward, brake. If stopped/reverse, drive backwards
		var forward_speed = -global_transform.basis.z.dot(linear_velocity)
		if forward_speed > 2.0:
			brake = brake_input * brake_force_value
		else:
			engine_force = brake_input * (engine_force_value * 0.5)
	else:
		# Engine braking
		brake = 5.0
		
	if handbrake_input > 0.5:
		brake = brake_force_value * 2.0
		engine_force = 0.0
	
	# Artificial downforce to keep the car glued to the track
	var current_speed = linear_velocity.length()
	apply_central_force(-global_transform.basis.y * (current_speed * downforce_multiplier))
	
	# Update visual models to track the wheels
	update_visuals()

func update_visuals():
	var pivot_FL = get_node_or_null("FrontLeftSteerPivot")
	var pivot_FR = get_node_or_null("FrontRightSteerPivot")
	var pivot_RL = get_node_or_null("RearLeftSteerPivot")
	var pivot_RR = get_node_or_null("RearRightSteerPivot")
	
	# VehicleWheel3D automatically updates its local transform to simulate suspension and rotation!
	# We just copy that transform to our visual meshes.
	if pivot_FL and wheels.has("FL"): pivot_FL.transform = wheels["FL"].transform
	if pivot_FR and wheels.has("FR"): pivot_FR.transform = wheels["FR"].transform
	if pivot_RL and wheels.has("RL"): pivot_RL.transform = wheels["RL"].transform
	if pivot_RR and wheels.has("RR"): pivot_RR.transform = wheels["RR"].transform
