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

# Scenic straightaway/bridge start position parameters
var scale_x = 120.0
var scale_z = 240.0
var height_offset = 14.0
var t_car = 0.15

func _ready():
	# Make sure the sweep-based track visuals and their collisions are active
	if road_bed: 
		road_bed.visible = true
		road_bed.use_collision = true
	if border_l: 
		border_l.visible = true
		border_l.use_collision = true
	if border_r: 
		border_r.visible = true
		border_r.use_collision = true
	if center_line: 
		center_line.visible = true
		center_line.use_collision = false

	var curve = Curve3D.new()
	
	# Generate points for a smooth figure 8 (Lemniscate of Gerono/Bernoulli style)
	var num_points = 300
	
	# We also calculate the tilt angle at each point to bank the track on curves!
	for i in range(num_points):
		var t = (float(i) / num_points) * 2.0 * PI
		
		# Figure 8 parametric equations
		var x = scale_x * sin(t)
		var z = scale_z * sin(t) * cos(t)
		var y = height_offset * cos(t)
		
		var pos = Vector3(x, y, z)
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
	if supercar:
		var car_pos = Vector3(scale_x * sin(t_car), height_offset * cos(t_car), scale_z * sin(t_car) * cos(t_car))
		var delta_t = 0.01
		var next_pos = Vector3(scale_x * sin(t_car + delta_t), height_offset * cos(t_car + delta_t), scale_z * sin(t_car + delta_t) * cos(t_car + delta_t))
		var dir = (next_pos - car_pos).normalized()
		
		supercar.position = car_pos + Vector3(0, 0.5, 0)
		supercar.look_at(car_pos + dir * 5.0, Vector3.UP)
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

	spawn_barriers()

func spawn_barriers():
	# Wait for the physical collision meshes of the CSG track to generate and sync
	await get_tree().physics_frame
	await get_tree().physics_frame
	
	var shader = Shader.new()
	shader.code = """
	shader_type spatial;
	render_mode cull_disabled;

	uniform vec3 plank_color : source_color = vec3(0.4, 0.35, 0.3);
	uniform vec3 seam_color : source_color = vec3(0.15, 0.1, 0.08);
	uniform float plank_width = 0.5;
	uniform float seam_width = 0.03;

	void fragment() {
		float local_pos = mod(UV.x, plank_width);
		
		bool is_seam = local_pos < seam_width;
		bool is_rail = (UV.y > 0.2 && UV.y < 0.4) || (UV.y > 2.8 && UV.y < 3.0);
		
		if (is_seam && !is_rail) {
			ALBEDO = seam_color;
			ROUGHNESS = 1.0;
		} else {
			float plank_index = floor(UV.x / plank_width);
			float variation = fract(sin(plank_index * 12.9898) * 43758.5453) * 0.05;
			
			ALBEDO = plank_color + vec3(variation);
			ROUGHNESS = 0.9;
		}
	}
	"""
	var mat = ShaderMaterial.new()
	mat.shader = shader
	
	var curve = path_node.curve
	var length = curve.get_baked_length()
	var step = 1.0
	
	var dummy = PathFollow3D.new()
	dummy.rotation_mode = PathFollow3D.ROTATION_ORIENTED
	path_node.add_child(dummy)
	
	var space_state = get_world_3d().direct_space_state
	
	var is_building = false
	var current_left_turn = false
	var st = SurfaceTool.new()
	var st_col = SurfaceTool.new()
	var wall_distance = 0.0
	
	var st_verts = 0
	var col_verts = 0
	
	for offset in range(0, int(length) + 1, int(step)):
		dummy.progress = float(offset)
		var t1 = dummy.global_transform
		
		dummy.progress = float(offset - 6.5)
		var t_prev = dummy.global_transform
		
		dummy.progress = float(offset + 6.5)
		var t_next = dummy.global_transform
		
		var fwd_prev = -t_prev.basis.z.normalized()
		var fwd_next = -t_next.basis.z.normalized()
		
		var turn_rate = fwd_prev.angle_to(fwd_next)
		var is_hard_turn = turn_rate > 0.05
		
		var cross_up = fwd_prev.cross(fwd_next).dot(t1.basis.y)
		var is_left_turn = cross_up > 0
		
		if is_hard_turn:
			if not is_building or is_left_turn != current_left_turn:
				if is_building:
					if st_verts >= 3:
						var mesh = st.commit()
						var mi = MeshInstance3D.new()
						mi.mesh = mesh
						mi.set_surface_override_material(0, mat)
						
						if col_verts >= 3:
							var col_mesh = st_col.commit()
							var shape = col_mesh.create_trimesh_shape()
							if shape:
								var col = CollisionShape3D.new()
								col.shape = shape
								var static_body = StaticBody3D.new()
								static_body.add_child(col)
								mi.add_child(static_body)
						
						add_child(mi)
					st.clear()
					st_col.clear()
				
				is_building = true
				current_left_turn = is_left_turn
				st.begin(Mesh.PRIMITIVE_TRIANGLE_STRIP)
				st_col.begin(Mesh.PRIMITIVE_TRIANGLE_STRIP)
				wall_distance = 0.0
				st_verts = 0
				col_verts = 0
			
			var track_width = 12.3 # Border center is at 12.3
			var outer_offset = track_width if is_left_turn else -track_width
			
			var guess_surface_pos = t1.origin + t1.basis.x * outer_offset + t1.basis.y * 0.4
			var ray_start = guess_surface_pos + t1.basis.y * 10.0
			var ray_end = guess_surface_pos - t1.basis.y * 10.0
			
			var query = PhysicsRayQueryParameters3D.create(ray_start, ray_end)
			var result = space_state.intersect_ray(query)
			
			var hit_pos = guess_surface_pos - t1.basis.y * 0.4
			var hit_normal = t1.basis.y
			
			if result:
				hit_pos = result.position
				hit_normal = result.normal
			
			var top_pos = hit_pos + hit_normal * 3.5
			var bot_pos_col = hit_pos - hit_normal * 1.0
			
			if is_left_turn:
				st.set_uv(Vector2(wall_distance, 3.5))
				st.add_vertex(top_pos)
				st.set_uv(Vector2(wall_distance, 0.0))
				st.add_vertex(hit_pos)
				st_verts += 2
				if offset % 4 == 0 or offset == int(length):
					st_col.add_vertex(top_pos)
					st_col.add_vertex(bot_pos_col)
					col_verts += 2
			else:
				st.set_uv(Vector2(wall_distance, 0.0))
				st.add_vertex(hit_pos)
				st.set_uv(Vector2(wall_distance, 3.5))
				st.add_vertex(top_pos)
				st_verts += 2
				if offset % 4 == 0 or offset == int(length):
					st_col.add_vertex(bot_pos_col)
					st_col.add_vertex(top_pos)
					col_verts += 2
			
			wall_distance += step
		else:
			if is_building:
				if st_verts >= 3:
					var mesh = st.commit()
					var mi = MeshInstance3D.new()
					mi.mesh = mesh
					mi.set_surface_override_material(0, mat)
					
					if col_verts >= 3:
						var col_mesh = st_col.commit()
						var shape = col_mesh.create_trimesh_shape()
						if shape:
							var col = CollisionShape3D.new()
							col.shape = shape
							var static_body = StaticBody3D.new()
							static_body.add_child(col)
							mi.add_child(static_body)
					
					add_child(mi)
				st.clear()
				st_col.clear()
				is_building = false
				st_verts = 0
				col_verts = 0

	if is_building:
		if st_verts >= 3:
			var mesh = st.commit()
			var mi = MeshInstance3D.new()
			mi.mesh = mesh
			mi.set_surface_override_material(0, mat)
			
			if col_verts >= 3:
				var col_mesh = st_col.commit()
				var shape = col_mesh.create_trimesh_shape()
				if shape:
					var col = CollisionShape3D.new()
					col.shape = shape
					var static_body = StaticBody3D.new()
					static_body.add_child(col)
					mi.add_child(static_body)
			
			add_child(mi)
		
	dummy.queue_free()

func _process(delta):
	if reset_car or (supercar and supercar.global_position.y < -30.0):
		reset_car = false
		if supercar:
			var car_pos = Vector3(scale_x * sin(t_car), height_offset * cos(t_car), scale_z * sin(t_car) * cos(t_car))
			var delta_t = 0.01
			var next_pos = Vector3(scale_x * sin(t_car + delta_t), height_offset * cos(t_car + delta_t), scale_z * sin(t_car + delta_t) * cos(t_car + delta_t))
			var dir = (next_pos - car_pos).normalized()
			
			supercar.global_position = car_pos + Vector3(0, 0.5, 0)
			supercar.look_at(car_pos + dir * 5.0, Vector3.UP)
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

func setup_polygons():
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
