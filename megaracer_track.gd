extends Node3D

@onready var path_node = $Path3D
@onready var road_bed = $Path3D/RoadBed
@onready var border_l = $Path3D/BorderL
@onready var border_r = $Path3D/BorderR
@onready var center_line = $Path3D/CenterLine
@onready var supercar = $SuperCar

func _ready():
	var curve = Curve3D.new()
	
	# Generate points for a smooth figure 8 (Lemniscate of Gerono/Bernoulli style)
	# with another 2x larger dimensions (X scale = 120, Z scale = 240, height = 14.0)
	var num_points = 300
	var scale_x = 120.0
	var scale_z = 240.0
	var height_offset = 14.0
	
	# We also calculate the tilt angle at each point to bank the track on curves!
	for i in range(num_points):
		var t = (float(i) / num_points) * 2.0 * PI
		
		# Figure 8 parametric equations
		var x = scale_x * sin(t)
		var z = scale_z * sin(t) * cos(t)
		var y = height_offset * cos(t)
		
		var pos = Vector3(x, y, z)
		
		# Add point to the curve. In Godot 4, add_point takes position, in, out
		curve.add_point(pos)
		
		# Set tilt (banking) relative to the curvature
		var bank = 0.5 * sin(t * 2.0)
		curve.set_point_tilt(i, bank)
		
	# Close the loop by adding the first point again
	var t_start = 0.0
	var x_start = scale_x * sin(t_start)
	var z_start = scale_z * sin(t_start) * cos(t_start)
	var y_start = height_offset * cos(t_start)
	curve.add_point(Vector3(x_start, y_start, z_start))
	curve.set_point_tilt(num_points, 0.0)
	
	path_node.curve = curve
	
	# Set CSGPolygon3D properties to extrude along the Path3D
	setup_polygons()
	
	# Position the supercar on a scenic straightaway/bridge (e.g. t = 0.15)
	var t_car = 0.15
	var car_pos = Vector3(scale_x * sin(t_car), height_offset * cos(t_car), scale_z * sin(t_car) * cos(t_car))
	
	var delta_t = 0.01
	var next_pos = Vector3(scale_x * sin(t_car + delta_t), height_offset * cos(t_car + delta_t), scale_z * sin(t_car + delta_t) * cos(t_car + delta_t))
	var dir = (next_pos - car_pos).normalized()
	
	supercar.position = car_pos + Vector3(0, 0.1, 0)
	supercar.look_at(car_pos + dir * 5.0, Vector3.UP)
	
	# Initialize camera position behind the car
	var camera_node = get_node_or_null("Camera3D")
	if camera_node and supercar:
		var offset = Vector3(0, 2.2, 6.0)
		camera_node.global_position = supercar.global_position + supercar.global_transform.basis * offset
		camera_node.look_at(supercar.global_position + supercar.global_transform.basis * Vector3(0, 0.4, -0.5), supercar.global_transform.basis.y)

func _physics_process(delta):
	var camera_node = get_node_or_null("Camera3D")
	if supercar and camera_node:
		# Camera target: slightly behind and above the car relative to its basis
		var offset = Vector3(0, 2.2, 6.0)
		var target_pos = supercar.global_position + supercar.global_transform.basis * offset
		
		# Smoothly interpolate position
		camera_node.global_position = camera_node.global_position.lerp(target_pos, 5.0 * delta)
		
		# Look at the car (using the car's local up vector to tilt/bank with it)
		var target_look = supercar.global_position + supercar.global_transform.basis * Vector3(0, 0.4, -0.5)
		camera_node.look_at(target_look, supercar.global_transform.basis.y)

func setup_polygons():
	# Configure the road bed (dark grey surface with collision enabled)
	# 24.0 units wide (Vector2 range from -12.0 to 12.0)
	road_bed.mode = CSGPolygon3D.MODE_PATH
	road_bed.path_node = road_bed.get_path_to(path_node)
	road_bed.path_interval = 0.5
	road_bed.path_rotation = CSGPolygon3D.PATH_ROTATION_PATH_FOLLOW
	road_bed.path_local = true
	road_bed.path_continuous_u = true
	road_bed.path_u_distance = 16.0
	road_bed.use_collision = true
	road_bed.polygon = PackedVector2Array([
		Vector2(-12.0, 0.0),
		Vector2(12.0, 0.0),
		Vector2(12.0, -0.32),
		Vector2(-12.0, -0.32)
	])
	
	# Configure left border (cyan neon with collision enabled)
	border_l.mode = CSGPolygon3D.MODE_PATH
	border_l.path_node = border_l.get_path_to(path_node)
	border_l.path_interval = 0.5
	border_l.path_rotation = CSGPolygon3D.PATH_ROTATION_PATH_FOLLOW
	border_l.path_local = true
	border_l.path_continuous_u = true
	border_l.path_u_distance = 16.0
	border_l.use_collision = true
	border_l.polygon = PackedVector2Array([
		Vector2(-12.6, 0.2),
		Vector2(-12.0, 0.2),
		Vector2(-12.0, -0.08),
		Vector2(-12.6, -0.08)
	])
	
	# Configure right border (cyan neon with collision enabled)
	border_r.mode = CSGPolygon3D.MODE_PATH
	border_r.path_node = border_r.get_path_to(path_node)
	border_r.path_interval = 0.5
	border_r.path_rotation = CSGPolygon3D.PATH_ROTATION_PATH_FOLLOW
	border_r.path_local = true
	border_r.path_continuous_u = true
	border_r.path_u_distance = 16.0
	border_r.use_collision = true
	border_r.polygon = PackedVector2Array([
		Vector2(12.0, 0.2),
		Vector2(12.6, 0.2),
		Vector2(12.6, -0.08),
		Vector2(12.0, -0.08)
	])
	
	# Configure center line (magenta grid line)
	center_line.mode = CSGPolygon3D.MODE_PATH
	center_line.path_node = center_line.get_path_to(path_node)
	center_line.path_interval = 0.5
	center_line.path_rotation = CSGPolygon3D.PATH_ROTATION_PATH_FOLLOW
	center_line.path_local = true
	center_line.path_continuous_u = true
	center_line.path_u_distance = 8.0
	center_line.polygon = PackedVector2Array([
		Vector2(-0.24, 0.02),
		Vector2(0.24, 0.02),
		Vector2(0.24, -0.02),
		Vector2(-0.24, -0.02)
	])
