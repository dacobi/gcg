extends Node3D

@onready var path_node = $Path3D
@onready var road_bed = $Path3D/RoadBed
@onready var border_l = $Path3D/BorderL
@onready var border_r = $Path3D/BorderR
@onready var center_line = $Path3D/CenterLine
@onready var supercar = $SuperCar

var is_paused = false
var orbit_yaw = 0.0
var orbit_pitch = 0.5
var orbit_dist = 12.0

var cam_rx = 0.0
var cam_ry = 0.0
var current_cam_yaw = 0.0
var current_cam_pitch = 0.0

var reset_car = false

func _ready():
	# Hide the old sweep-based track visuals and disable their collision
	if road_bed: 
		road_bed.visible = false
		road_bed.use_collision = false
	if border_l: 
		border_l.visible = false
		border_l.use_collision = false
	if border_r: 
		border_r.visible = false
		border_r.use_collision = false
	if center_line: 
		center_line.visible = false
		center_line.use_collision = false

	# Position the supercar safely above the floor, aligned with Ramp 1
	if supercar:
		supercar.position = Vector3(-4.5, 1.0, 50)
		supercar.look_at(Vector3(-4.5, 1.0, 0), Vector3.UP)
		if "start_transform" in supercar:
			supercar.start_transform = supercar.global_transform
			
	# Raise the sun by 45 degrees
	var sun = get_node_or_null("DirectionalLight3D")
	if sun:
		sun.rotation_degrees.x -= 45.0
	
	# Initialize camera position behind the car
	var camera_node = get_node_or_null("Camera3D")
	if camera_node and supercar:
		var offset = Vector3(0, 11.01, 15.69)
		camera_node.global_position = supercar.global_position + supercar.global_transform.basis * offset
		camera_node.look_at(supercar.global_position + supercar.global_transform.basis * Vector3(0, 0.4, -0.5), supercar.global_transform.basis.y)
		
	build_arena()

func build_arena():
	var mat = StandardMaterial3D.new()
	mat.albedo_color = Color(0.4, 0.35, 0.3)
	
	var fence_mat = StandardMaterial3D.new()
	fence_mat.albedo_color = Color(0.0, 0.9, 0.9)
	fence_mat.emission_enabled = true
	fence_mat.emission = Color(0.0, 0.9, 0.9)
	fence_mat.emission_energy_multiplier = 0.5
	
	var floor_size = 200.0
	var h = floor_size / 2.0
	
	# Create floor
	var floor = CSGBox3D.new()
	floor.size = Vector3(floor_size, 1.0, floor_size)
	floor.position = Vector3(0, -0.5, 0)
	floor.use_collision = true
	floor.material_override = mat
	add_child(floor)
	
	# Create fences
	# North fence
	var fn = CSGBox3D.new()
	fn.size = Vector3(floor_size, 4.0, 1.0)
	fn.position = Vector3(0, 2.0, -h)
	fn.use_collision = true
	fn.material_override = fence_mat
	add_child(fn)
	
	# South fence
	var fs = CSGBox3D.new()
	fs.size = Vector3(floor_size, 4.0, 1.0)
	fs.position = Vector3(0, 2.0, h)
	fs.use_collision = true
	fs.material_override = fence_mat
	add_child(fs)
	
	# East fence
	var fe = CSGBox3D.new()
	fe.size = Vector3(1.0, 4.0, floor_size)
	fe.position = Vector3(h, 2.0, 0)
	fe.use_collision = true
	fe.material_override = fence_mat
	add_child(fe)
	
	# West fence
	var fw = CSGBox3D.new()
	fw.size = Vector3(1.0, 4.0, floor_size)
	fw.position = Vector3(-h, 2.0, 0)
	fw.use_collision = true
	fw.material_override = fence_mat
	add_child(fw)
	
	# Create Ramp 1
	var ramp1 = CSGPolygon3D.new()
	ramp1.polygon = PackedVector2Array([Vector2(0, 0), Vector2(6, 0), Vector2(6, 2.5)])
	ramp1.depth = 6.0
	ramp1.position = Vector3(-1.5, 0, 3.0)
	ramp1.rotation_degrees = Vector3(0, -90, 0)
	ramp1.use_collision = true
	ramp1.material_override = mat
	add_child(ramp1)
	
	# Create Ramp 2
	var ramp2 = CSGPolygon3D.new()
	ramp2.polygon = PackedVector2Array([Vector2(0, 0), Vector2(6, 0), Vector2(6, 2.5)])
	ramp2.depth = 6.0
	ramp2.position = Vector3(1.5, 0, -3.0)
	ramp2.rotation_degrees = Vector3(0, 90, 0)
	ramp2.use_collision = true
	ramp2.material_override = mat
	add_child(ramp2)

func _process(delta):
	if reset_car or (supercar and supercar.global_position.y < -30.0):
		reset_car = false
		if supercar:
			# Respawn the car safely within the arena
			supercar.global_position = Vector3(-4.5, 1.0, 50)
			supercar.look_at(Vector3(-4.5, 1.0, 0), Vector3.UP)
			supercar.linear_velocity = Vector3.ZERO
			supercar.angular_velocity = Vector3.ZERO

	var camera_node = get_node_or_null("Camera3D")
	if camera_node and supercar:
		if is_paused:
			var offset = Vector3(
				sin(orbit_yaw) * cos(orbit_pitch),
				sin(orbit_pitch),
				cos(orbit_yaw) * cos(orbit_pitch)
			) * orbit_dist
			camera_node.global_position = supercar.global_position + offset
			camera_node.look_at(supercar.global_position, Vector3.UP)
		else:
			current_cam_yaw = lerp(current_cam_yaw, -cam_rx * 2.0, 5.0 * delta)
			current_cam_pitch = lerp(current_cam_pitch, cam_ry * 1.0, 5.0 * delta)
			
			var forward = supercar.global_transform.basis.z.normalized()
			var up = Vector3.UP
			
			var rotated_forward = forward.rotated(up, current_cam_yaw)
			var offset = rotated_forward * 12.0 + Vector3(0, 4.0 + current_cam_pitch * 4.0, 0)
			var target_pos = supercar.global_position + offset
			
			camera_node.global_position = camera_node.global_position.lerp(target_pos, 10.0 * delta)
			var look_target = supercar.global_position + Vector3(0, 1.5, 0) + supercar.linear_velocity * 0.1
			var target_transform = camera_node.global_transform.looking_at(look_target, Vector3.UP)
			camera_node.global_transform = camera_node.global_transform.interpolate_with(target_transform, 15.0 * delta)
