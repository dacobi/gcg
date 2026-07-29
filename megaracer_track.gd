extends Node3D

@onready var path_node = $Path3D
@onready var road_bed = $Path3D/RoadBed
@onready var border_l = $Path3D/BorderL
@onready var border_r = $Path3D/BorderR
@onready var center_line = $Path3D/CenterLine
@onready var supercar = $SuperCar

var is_paused = false
var previous_paused = false
var orbit_yaw = 0.0
var orbit_pitch = 0.5
var orbit_dist = 18.0

var cam_rx = 0.0
var cam_ry = 0.0
var current_cam_yaw = 0.0
var current_cam_pitch = 0.0

var mouse_wheel = 0.0

var reset_car = false
var reset_game = false
var powerup_nodes: Array = []

var current_lap: int = 0
var final_laps: int = 3
var countdown_value: int = 4 # 4 (1s silent warmup on boot), 3, 2, 1, 0 (GO!), -1 (done)
var countdown_timer: float = 1.0
var halfway_cleared: bool = false

var hud_layer: CanvasLayer
var lap_label: Label
var time_label: Label
var countdown_label: Label
var beep_player: AudioStreamPlayer
var beep_low_stream: AudioStream
var beep_high_stream: AudioStream
var victory_jingle_player: AudioStreamPlayer

var race_time: float = 0.0
var current_lap_time: float = 0.0
var lap_times: Array = []
var race_finished: bool = false
var victory_lap_timer: float = 0.0
var victory_panel: Control = null
var victory_popup_menu: Control = null

var in_edit_mode = false
var is_square = false
var edit_progress = 0.0
var edit_yaw = 0.0
var edit_pitch = 0.0
var edit_offset = 0.0

var active_ramp = null
var active_ramp_progress = 0.0
var active_ramp_offset = 0.0

var completed_ramps = []

# Properties driven by Lua input
var key_e_pressed = false
var key_d_pressed = false
var key_n_pressed = false
var key_c_pressed = false
var arrow_up_down = 0.0
var arrow_left_right = 0.0
var mouse_dx = 0.0
var mouse_dy = 0.0

# Scenic straightaway/bridge start position parameters
var scale_x = 480.0
var scale_z = 960.0
var height_offset = 56.0
var t_car = 0.15

func _ready():
	is_square = false
	for arg in OS.get_cmdline_args():
		if "show_car.lua" in arg or "testphysics.lua" in arg:
			is_square = true
			break

	if is_square:
		countdown_value = -1
		if countdown_label:
			countdown_label.visible = false
		if supercar:
			supercar.set("in_countdown", false)
		# Hide default path-extruded track nodes
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

		# Create a large flat square floor box (1200m x 1200m) with no hole in the middle
		# Increased thickness to 10m to prevent high-speed cars from clipping through when landing
		var floor_box = CSGBox3D.new()
		floor_box.name = "ArenaFloor"
		floor_box.size = Vector3(1200.0, 10.0, 1200.0)
		floor_box.position = Vector3(0.0, -5.0, 0.0)
		var shader = Shader.new()
		shader.code = """
		shader_type spatial;
		uniform vec4 color1 : source_color = vec4(0.0, 0.0, 0.0, 1.0);
		uniform vec4 color2 : source_color = vec4(0.0, 1.0, 1.0, 1.0);
		uniform float grid_scale = 120.0;
		void fragment() {
			vec2 pos = floor(UV * grid_scale);
			float pattern = mod(pos.x + pos.y, 2.0);
			ALBEDO = mix(color1.rgb, color2.rgb, pattern);
		}
		"""
		var checker_mat = ShaderMaterial.new()
		checker_mat.shader = shader
		floor_box.material = checker_mat
		floor_box.use_collision = true
		add_child(floor_box)

		# Generate a smooth rounded square path curve (for camera movement, edit mode, and fence)
		var curve = Curve3D.new()
		var size = 600.0
		var r = 80.0
		var num_points_side = 50
		var num_points_corner = 25
		
		# 1. North Side
		for i in range(num_points_side):
			var t = float(i) / num_points_side
			var x = lerp(-size + r, size - r, t)
			curve.add_point(Vector3(x, 0, -size))
			
		# 2. North-East Corner
		for i in range(num_points_corner):
			var t = float(i) / num_points_corner
			var theta = -PI/2.0 + t * (PI/2.0)
			var x = (size - r) + r * cos(theta)
			var z = (-size + r) + r * sin(theta)
			curve.add_point(Vector3(x, 0, z))
			
		# 3. East Side
		for i in range(num_points_side):
			var t = float(i) / num_points_side
			var z = lerp(-size + r, size - r, t)
			curve.add_point(Vector3(size, 0, z))
			
		# 4. South-East Corner
		for i in range(num_points_corner):
			var t = float(i) / num_points_corner
			var theta = 0.0 + t * (PI/2.0)
			var x = (size - r) + r * cos(theta)
			var z = (size - r) + r * sin(theta)
			curve.add_point(Vector3(x, 0, z))
			
		# 5. South Side
		for i in range(num_points_side):
			var t = float(i) / num_points_side
			var x = lerp(size - r, -size + r, t)
			curve.add_point(Vector3(x, 0, size))
			
		# 6. South-West Corner
		for i in range(num_points_corner):
			var t = float(i) / num_points_corner
			var theta = PI/2.0 + t * (PI/2.0)
			var x = (-size + r) + r * cos(theta)
			var z = (size - r) + r * sin(theta)
			curve.add_point(Vector3(x, 0, z))
			
		# 7. West Side
		for i in range(num_points_side):
			var t = float(i) / num_points_side
			var z = lerp(size - r, -size + r, t)
			curve.add_point(Vector3(-size, 0, z))
			
		# 8. North-West Corner
		for i in range(num_points_corner):
			var t = float(i) / num_points_corner
			var theta = PI + t * (PI/2.0)
			var x = (-size + r) + r * cos(theta)
			var z = (-size + r) + r * sin(theta)
			curve.add_point(Vector3(x, 0, z))
			
		# Close the loop
		curve.add_point(Vector3(-size + r, 0, -size))
		path_node.curve = curve

		# Create a 4m tall neon cyan fence along the entire outer edge of the arena
		var outer_fence = CSGPolygon3D.new()
		outer_fence.name = "OuterFence"
		outer_fence.mode = CSGPolygon3D.MODE_PATH
		outer_fence.path_interval = 0.5
		outer_fence.path_rotation = CSGPolygon3D.PATH_ROTATION_PATH_FOLLOW
		outer_fence.path_local = true
		outer_fence.use_collision = true
		outer_fence.material = border_l.material # neon cyan material
		outer_fence.polygon = PackedVector2Array([
			Vector2(-0.25, 0.0),
			Vector2(0.25, 0.0),
			Vector2(0.25, 4.0),
			Vector2(-0.25, 4.0)
		])
		add_child(outer_fence)
		outer_fence.path_node = outer_fence.get_path_to(path_node)

		# Position supercar at the center of the arena facing North
		if supercar:
			supercar.global_position = Vector3(0.0, 0.5, 0.0)
			supercar.global_transform.basis = Basis.IDENTITY
			if "start_transform" in supercar:
				supercar.start_transform = supercar.global_transform
				
		# --- PROCEDURAL CONCRETE MATERIAL ---
		var concrete_mat = StandardMaterial3D.new()
		var noise = FastNoiseLite.new()
		noise.noise_type = FastNoiseLite.TYPE_SIMPLEX
		noise.frequency = 0.05
		var noise_tex = NoiseTexture2D.new()
		noise_tex.noise = noise
		noise_tex.seamless = true
		concrete_mat.albedo_color = Color(0.6, 0.6, 0.6)
		concrete_mat.albedo_texture = noise_tex
		concrete_mat.roughness = 0.9

		# --- SPAWN RAMPS ---
		var ramps_data = [
			{"pos": Vector3(40, 0, -80), "rot": -45, "w": 30, "h": 8, "d": 15},
			{"pos": Vector3(-50, 0, -60), "rot": 30, "w": 20, "h": 5, "d": 10},
			{"pos": Vector3(100, 0, 50), "rot": 135, "w": 40, "h": 12, "d": 25},
			{"pos": Vector3(-120, 0, 80), "rot": 90, "w": 15, "h": 4, "d": 15},
			{"pos": Vector3(0, 0, 150), "rot": 180, "w": 25, "h": 7, "d": 20},
			{"pos": Vector3(-150, 0, -150), "rot": 45, "w": 35, "h": 10, "d": 20},
			{"pos": Vector3(150, 0, -150), "rot": -45, "w": 20, "h": 6, "d": 15},
			{"pos": Vector3(80, 0, 120), "rot": 200, "w": 30, "h": 8, "d": 20},
			{"pos": Vector3(-80, 0, 120), "rot": 160, "w": 30, "h": 8, "d": 20},
			{"pos": Vector3(0, 0, -200), "rot": 0, "w": 50, "h": 15, "d": 30}
		]
		for data in ramps_data:
			var ramp = CSGPolygon3D.new()
			
			var curve_points = PackedVector2Array()
			curve_points.append(Vector2(0, 0))
			var w = float(data["w"])
			var h = float(data["h"])
			curve_points.append(Vector2(w, 0))
			
			# Hybrid 50/50 curve mathematics
			var xc = w / 2.0
			var m = (4.0 * h) / (3.0 * w)
			var A = m / (2.0 * xc)
			var yc = A * xc * xc
			
			var segments = 20
			# Generate curve backwards to maintain counter-clockwise winding order
			for i in range(segments, 0, -1):
				var t = float(i) / float(segments)
				var px = t * w
				var py = 0.0
				
				# Bottom 50%: Smooth parabolic transition
				if px <= xc:
					py = A * px * px
				# Top 50%: Straight constant slope to eliminate launch rotation
				else:
					py = yc + m * (px - xc)
					
				curve_points.append(Vector2(px, py))
				
			ramp.polygon = curve_points
			ramp.depth = data["d"]
			ramp.position = data["pos"]
			ramp.rotation_degrees = Vector3(0, data["rot"], 0)
			ramp.material = concrete_mat
			ramp.use_collision = true
			add_child(ramp)
			
			var nitro_script = load("res://nitro_powerup.gd")
			if nitro_script:
				var powerup = StaticBody3D.new()
				powerup.name = "NitroPowerup_" + str(ramps_data.find(data))
				powerup.set_script(nitro_script)
				
				var px = w * 0.85
				var py = yc + m * (px - xc) + 2.2
				var pz = -float(data["d"]) * 0.5
				
				var local_pos = Vector3(px, py, pz)
				var rot_rad = deg_to_rad(float(data["rot"]))
				var world_pos = data["pos"] + Basis.from_euler(Vector3(0, rot_rad, 0)) * local_pos
				powerup.position = world_pos
				add_child(powerup)
		
		# --- SPAWN MODELS ---
		var barrier_scn = load("res://assets/models/extra_objects/traffic_barrier.glb")
		var cone_scn = load("res://assets/models/extra_objects/traffic_cone.glb")
		var tree_scn = load("res://assets/models/environment/pine_tree_1.glb")
		var tree2_scn = load("res://assets/models/environment/pine_tree_2.glb")
		var bush_scn = load("res://assets/models/environment/bush.glb")
		
		var spawn = func(scn, pos: Vector3, rot_y: float = 0.0, s: float = 1.0, is_rigid: bool = false, mass_val: float = 1.0):
			if scn:
				var inst = scn.instantiate()
				
				# Destroy any rogue cameras, lights, or environments exported from Blender
				var bad_types = ["Camera3D", "DirectionalLight3D", "PointLight3D", "SpotLight3D", "WorldEnvironment", "AnimationPlayer", "ReflectionProbe", "FogVolume", "Decal", "VoxelGI", "LightmapGI"]
				for type_name in bad_types:
					for child in inst.find_children("*", type_name, true, false):
						child.free() 
				
				if is_rigid:
					var parent_node = RigidBody3D.new()
					parent_node.mass = mass_val
					parent_node.position = pos
					parent_node.rotation_degrees.y = rot_y
					parent_node.collision_layer = 4 # Props layer
					parent_node.collision_mask = 15 # Hits World, Car, Props, and Bumper
					
					# Shift center of mass up to stop bottom-heavy weeble-wobble effect
					parent_node.center_of_mass_mode = RigidBody3D.CENTER_OF_MASS_MODE_CUSTOM
					if scn == barrier_scn:
						parent_node.center_of_mass = Vector3(0, 0.6, 0)
					else:
						parent_node.center_of_mass = Vector3(0, 0.4, 0)
					
					inst.scale *= s
					parent_node.add_child(inst)
					add_child(parent_node)
					
					for child in inst.find_children("*", "MeshInstance3D", true, false):
						var shape = CollisionShape3D.new()
						shape.shape = child.mesh.create_convex_shape(true, true)
						# Important: apply the root instance's GLTF conversion scale to the collision shape!
						shape.position = child.position * inst.scale
						shape.rotation = child.rotation
						shape.scale = child.scale * inst.scale
						parent_node.add_child(shape)
				else:
					inst.position = pos
					inst.rotation_degrees.y = rot_y
					inst.scale *= s
					add_child(inst)
					for child in inst.find_children("*", "MeshInstance3D", true, false):
						child.create_multiple_convex_collisions()
					
		# Spawn rigid dynamic barriers and cones
		for i in range(12):
			# Original lines
			spawn.call(barrier_scn, Vector3(20 + i*4, 0.5, 50), 0, 1.0, true, 5.0)
			spawn.call(cone_scn, Vector3(-30 + i*3, 0.2, 40), 0, 1.0, true, 2.0)
			spawn.call(barrier_scn, Vector3(80, 0.5, -20 + i*4), 90, 1.0, true, 5.0)
			
			# Additional lines
			spawn.call(cone_scn, Vector3(10 + i*3, 0.2, -40), 45, 1.0, true, 2.0)
			spawn.call(barrier_scn, Vector3(-80, 0.5, 20 - i*4), 90, 1.0, true, 5.0)
			spawn.call(cone_scn, Vector3(-60 - i*3, 0.2, 80), -45, 1.0, true, 2.0)
			
		for i in range(25):
			# A long slalom line of cones down the middle, with a massive safety gap for the car's spawn
			var z_pos = -107 + i*8
			if absf(z_pos) > 15.0:
				spawn.call(cone_scn, Vector3(0, 0.2, z_pos), 0, 1.0, true, 2.0)
				
		# Foliage spawn helper function
		var is_valid_spawn = func(pos: Vector3) -> bool:
			# Don't spawn within 50m of the exact center
			if Vector2(pos.x, pos.z).length() < 50.0: return false
			
			# Don't spawn anywhere near the ramps!
			for data in ramps_data:
				var dist = Vector2(pos.x - data["pos"].x, pos.z - data["pos"].z).length()
				# Create a huge safe buffer zone around each ramp (ramp size + 35 meters)
				var buffer = max(data["w"], data["d"]) + 35.0
				if dist < buffer:
					return false
			return true

		# Spawn some trees outside the track
		for i in range(40):
			var angle = i * (PI * 2.0 / 40.0)
			var radius = 150.0 + randf_range(-20.0, 20.0)
			var x = cos(angle) * radius
			var z = sin(angle) * radius
			var pos = Vector3(x, 0.0, z)
			if is_valid_spawn.call(pos):
				spawn.call(tree_scn, pos, randf_range(0, 360), randf_range(0.8, 1.5), false)

		# Spawn scattered static foliage safely
		for i in range(150):
			var rx = randf_range(-300, 300)
			var rz = randf_range(-300, 300)
			var pos = Vector3(rx, 0, rz)
			
			if is_valid_spawn.call(pos):
				var prop_type = randi() % 3
				if prop_type == 0:
					spawn.call(tree_scn, pos, randf_range(0, 360), randf_range(0.8, 1.5), false)
				elif prop_type == 1:
					spawn.call(tree2_scn, pos, randf_range(0, 360), randf_range(0.8, 1.3), false)
				else:
					spawn.call(bush_scn, pos, randf_range(0, 360), randf_range(0.5, 2.0), false)

	else:
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
			
			# Set tilt (banking) relative to the curvature (NASCAR cornering, level straightaways)
			var bank = -0.18 * sin(t)
			curve.set_point_tilt(i, bank)
			
		# Close the loop by adding the first point again
		var t_start = 0.0
		var x_start = scale_x * sin(t_start)
		var z_start = scale_z * sin(t_start) * cos(t_start)
		var y_start = height_offset * cos(t_start)
		curve.add_point(Vector3(x_start, y_start, z_start))
		curve.set_point_tilt(num_points, 0.0)
		
		path_node.curve = curve
		setup_polygons()
		build_gate()
		setup_checkpoint_system()
		spawn_random_powerups()
		setup_ui()

		# Position the supercar on the starting grid (10m before gate, Inner Right lane x = +13.0)
		if supercar:
			var grid_transform = path_node.global_transform * path_node.curve.sample_baked_with_rotation(15.0, true, true)
			if grid_transform:
				supercar.global_transform = grid_transform
				supercar.global_position += grid_transform.basis.x * 13.0 + grid_transform.basis.y * 0.5
				if "start_transform" in supercar:
					supercar.start_transform = supercar.global_transform
			
	# Raise the sun by 45 degrees
	var sun = get_node_or_null("DirectionalLight3D")
	if sun:
		sun.rotation_degrees.x -= 45.0
	
		var camera_node = get_node_or_null("Camera3D")
		if camera_node and supercar:
			var offset = Vector3(0, 11.01, 15.69)
			camera_node.global_position = supercar.global_position + supercar.global_transform.basis * offset
			camera_node.look_at(supercar.global_position + supercar.global_transform.basis * Vector3(0, 0.4, -0.5), supercar.global_transform.basis.y)

		if not is_square:
			spawn_barriers()
	print("\n=== EDIT MODE CONTROLS ===")
	print("Press E to enter Edit Mode.")
	print("Press N to spawn a new ramp at the edit camera location.")
	print("Use Arrow Keys to move the edit camera (when no ramp is active) or the active ramp.")
	print("  - UP/DOWN: Move camera/ramp forward/backward along the track.")
	print("  - LEFT/RIGHT: Move ramp left/right relative to the track center.")
	print("Press C to complete/save the active ramp.")
	print("Press D to exit Edit Mode and return to driving.\n")

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
		
		dummy.progress = float(offset - 26.0)
		var t_prev = dummy.global_transform
		
		dummy.progress = float(offset + 26.0)
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
			
			var actual_wall_dist = 49.2 # Border center is at 49.2
			var raycast_dist = 45.0 # Safely inside the asphalt edge to hit solid collision
			
			var raycast_offset = raycast_dist if is_left_turn else -raycast_dist
			
			var guess_surface_pos = t1.origin + t1.basis.x * raycast_offset + t1.basis.y * 0.4
			var ray_start = guess_surface_pos + t1.basis.y * 10.0
			var ray_end = guess_surface_pos - t1.basis.y * 10.0
			
			var query = PhysicsRayQueryParameters3D.create(ray_start, ray_end)
			var result = space_state.intersect_ray(query)
			
			var hit_pos = guess_surface_pos - t1.basis.y * 0.4
			var hit_normal = t1.basis.y
			
			if result:
				hit_pos = result.position
				hit_normal = result.normal
				
			# Push the final position out to the actual wall distance so it sits on the neon line
			var push_out_dist = actual_wall_dist - raycast_dist
			var push_out_vec = t1.basis.x * push_out_dist if is_left_turn else -t1.basis.x * push_out_dist
			hit_pos += push_out_vec
			
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

func _input(event):
	if event.is_action_pressed("ui_cancel"):
		is_paused = not is_paused
		if is_paused:
			Input.set_mouse_mode(Input.MOUSE_MODE_CAPTURED)
		else:
			Input.set_mouse_mode(Input.MOUSE_MODE_VISIBLE)
	
	if event is InputEventMouseMotion:
		mouse_dx += event.relative.x
		mouse_dy += event.relative.y
	elif event is InputEventMouseButton:
		if event.button_index == MOUSE_BUTTON_WHEEL_UP and event.pressed:
			mouse_wheel += 1.0
		elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN and event.pressed:
			mouse_wheel -= 1.0

func _process(delta):

	# Update HUD and Countdown
	if lap_label:
		var display_lap = max(1, min(current_lap, final_laps))
		lap_label.text = "Lap: %d / %d" % [display_lap, final_laps]
		
	if countdown_value >= 0:
		countdown_timer -= delta
		if countdown_timer <= 0.0:
			countdown_timer = 1.0
			countdown_value -= 1
			if beep_player and is_instance_valid(beep_player):
				if countdown_value in [1, 2, 3] and beep_low_stream:
					beep_player.stream = beep_low_stream
					beep_player.play()
				elif countdown_value == 0 and beep_high_stream:
					beep_player.stream = beep_high_stream
					beep_player.play()
			
		if countdown_label:
			countdown_label.visible = true
			if countdown_value == 3:
				countdown_label.text = "3"
				countdown_label.modulate = Color(1.0, 0.2, 0.2) # Red
			elif countdown_value == 2:
				countdown_label.text = "2"
				countdown_label.modulate = Color(1.0, 0.6, 0.1) # Orange
			elif countdown_value == 1:
				countdown_label.text = "1"
				countdown_label.modulate = Color(1.0, 0.9, 0.1) # Yellow
			elif countdown_value == 0:
				countdown_label.text = "GO!"
				countdown_label.modulate = Color(0.2, 1.0, 0.2) # Green
			else:
				countdown_label.visible = false
				
		if supercar:
			supercar.set("in_countdown", countdown_value > 0)
	else:
		if countdown_label and countdown_label.visible:
			countdown_timer -= delta
			if countdown_timer <= 0.0:
				countdown_label.visible = false

	if countdown_value < 0 and not race_finished and current_lap > 0:
		race_time += delta
		current_lap_time += delta

	if time_label:
		time_label.text = "TIME: " + format_time(race_time) + "\nLAP: " + format_time(current_lap_time)

	if race_finished:
		victory_lap_timer += delta
		if victory_lap_timer >= 10.0 and victory_popup_menu and not victory_popup_menu.visible:
			victory_popup_menu.visible = true

	# Handle Edit Mode trigger state changes from Lua properties
	# Read and clear trigger states from Lua immediately every frame to prevent stuck triggers
	var trigger_e = key_e_pressed
	var trigger_d = key_d_pressed
	var trigger_n = key_n_pressed
	var trigger_c = key_c_pressed
	
	key_e_pressed = false
	key_d_pressed = false
	key_n_pressed = false
	key_c_pressed = false

	if trigger_e and not in_edit_mode:
		in_edit_mode = true
		if supercar:
			supercar.process_mode = PROCESS_MODE_DISABLED
			edit_progress = path_node.curve.get_closest_offset(supercar.global_position)
		edit_yaw = 0.0
		edit_pitch = 0.0
		edit_offset = 0.0
		print("Entered Edit Mode via Lua.")
	
	if trigger_d and in_edit_mode:
		if active_ramp:
			active_ramp.queue_free()
			active_ramp = null
		in_edit_mode = false
		if supercar:
			supercar.process_mode = PROCESS_MODE_INHERIT
		print("Exited Edit Mode via Lua.")
		
	if trigger_n and in_edit_mode and not active_ramp:
		active_ramp_progress = edit_progress
		active_ramp_offset = edit_offset
		
		var mat = StandardMaterial3D.new()
		mat.albedo_color = Color(0.4, 0.35, 0.3)
		
		active_ramp = CSGPolygon3D.new()
		active_ramp.polygon = PackedVector2Array([Vector2(-3.0, 0.0), Vector2(3.0, 0.0), Vector2(3.0, 2.5)])
		active_ramp.depth = 6.0
		active_ramp.use_collision = false
		active_ramp.material_override = mat
		
		add_child(active_ramp)
		update_active_ramp()
		print("Spawned new active ramp via Lua.")
		
	if trigger_c and in_edit_mode and active_ramp:
		var static_body = StaticBody3D.new()
		static_body.collision_layer = 1
		static_body.collision_mask = 1
		
		var col_shape = CollisionShape3D.new()
		var shape_created = false
		
		var meshes = active_ramp.get_meshes()
		if meshes.size() >= 2:
			var mesh = meshes[1]
			if mesh:
				var convex_shape = mesh.create_convex_shape()
				if convex_shape:
					col_shape.shape = convex_shape
					shape_created = true
					print("Created collision shape from visual mesh successfully.")
					
		if not shape_created:
			var convex_shape = ConvexPolygonShape3D.new()
			convex_shape.points = PackedVector3Array([
				Vector3(-3.0, 0.0, 0.0),
				Vector3(3.0, 0.0, 0.0),
				Vector3(3.0, 2.5, 0.0),
				Vector3(-3.0, 0.0, -6.0),
				Vector3(3.0, 0.0, -6.0),
				Vector3(3.0, 2.5, -6.0)
			])
			col_shape.shape = convex_shape
			print("Created collision shape from fallback manual points.")
			
		static_body.add_child(col_shape)
		add_child(static_body)
		static_body.global_transform = active_ramp.global_transform
		
		completed_ramps.append(active_ramp)
		active_ramp = null
		print("Completed active ramp via Lua (created dedicated physics body).")

	if in_edit_mode:
		# Process free look camera rotation from Lua mouse deltas
		edit_yaw -= mouse_dx
		edit_pitch -= mouse_dy
		edit_pitch = clamp(edit_pitch, -1.4, 1.4)
		mouse_dx = 0.0
		mouse_dy = 0.0

		var length = path_node.curve.get_baked_length()
		# Process movements from Lua arrow keys properties
		if arrow_up_down != 0.0:
			if active_ramp:
				active_ramp_progress = fmod(active_ramp_progress + 30.0 * arrow_up_down * delta + length, length)
				update_active_ramp()
			else:
				edit_progress = fmod(edit_progress + 50.0 * arrow_up_down * delta + length, length)
		if arrow_left_right != 0.0:
			if active_ramp:
				active_ramp_offset += 10.0 * arrow_left_right * delta
				update_active_ramp()
			else:
				edit_offset += 10.0 * arrow_left_right * delta

		# Update camera position and free view rotation
		var dummy = PathFollow3D.new()
		dummy.rotation_mode = PathFollow3D.ROTATION_ORIENTED
		path_node.add_child(dummy)
		dummy.progress = edit_progress
		
		var camera_node = get_node_or_null("Camera3D")
		if camera_node:
			camera_node.global_position = dummy.global_position + dummy.global_transform.basis.y * 2.5 + dummy.global_transform.basis.x * edit_offset
			camera_node.rotation = Vector3(edit_pitch, edit_yaw, 0.0)
		dummy.queue_free()
		return

	if reset_game:
		reset_game = false
		reset_car = false
		current_lap = 0
		halfway_cleared = false
		race_time = 0.0
		current_lap_time = 0.0
		lap_times.clear()
		race_finished = false
		victory_lap_timer = 0.0
		if victory_panel: victory_panel.visible = false
		if victory_popup_menu: victory_popup_menu.visible = false
		if not is_square:
			countdown_value = 3
			countdown_timer = 1.0
			if beep_player and beep_low_stream:
				beep_player.stream = beep_low_stream
				beep_player.play()
		if supercar:
			supercar.set("nitro_seconds", 100.0)
			if is_square:
				supercar.global_position = Vector3(0.0, 0.5, 0.0)
				supercar.global_transform.basis = Basis.IDENTITY
			else:
				var grid_transform = path_node.global_transform * path_node.curve.sample_baked_with_rotation(15.0, true, true)
				if grid_transform:
					supercar.global_transform = grid_transform
					supercar.global_position += grid_transform.basis.x * 13.0 + grid_transform.basis.y * 0.5
			supercar.linear_velocity = Vector3.ZERO
			supercar.angular_velocity = Vector3.ZERO
		for p in powerup_nodes:
			if is_instance_valid(p):
				p.visible = true
				p.collision_layer = 4
				p.set("respawn_timer", 0.0)

	elif reset_car or (supercar and supercar.global_position.y < -150.0):
		reset_car = false
		if supercar:
			if is_square:
				supercar.global_position = Vector3(0.0, 0.5, 0.0)
				supercar.global_transform.basis = Basis.IDENTITY
			else:
				var closest_prog = path_node.curve.get_closest_offset(supercar.global_position)
				var t_respawn = path_node.global_transform * path_node.curve.sample_baked_with_rotation(closest_prog, true, true)
				if t_respawn:
					supercar.global_transform = t_respawn
					supercar.global_position += t_respawn.basis.y * 1.5
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
			
			# Dynamic FOV warp effect based on physical speed
			var speed = supercar.linear_velocity.length()
			var target_fov = lerp(75.0, 120.0, clamp(speed / 100.0, 0.0, 1.0))
			camera_node.fov = lerp(camera_node.fov, target_fov, 4.0 * delta)

func update_active_ramp():
	if not active_ramp: return
	
	var curve = path_node.curve
	var length = curve.get_baked_length()
	
	var dummy = PathFollow3D.new()
	dummy.rotation_mode = PathFollow3D.ROTATION_ORIENTED
	path_node.add_child(dummy)
	
	dummy.progress = active_ramp_progress
	var t1 = dummy.global_transform
	
	var center_pos = t1.origin + t1.basis.x * active_ramp_offset
	
	var corners = [
		Vector3(-3.0, 0.0, -3.0),
		Vector3(3.0, 0.0, -3.0),
		Vector3(-3.0, 0.0, 3.0),
		Vector3(3.0, 0.0, 3.0)
	]
	
	var p = []
	var space_state = get_world_3d().direct_space_state
	
	for i in range(corners.size()):
		var corner_global = center_pos + t1.basis.x * corners[i].x + t1.basis.z * corners[i].z
		var ray_start = corner_global + t1.basis.y * 5.0
		var ray_end = corner_global - t1.basis.y * 5.0
		
		var query = PhysicsRayQueryParameters3D.create(ray_start, ray_end)
		query.collision_mask = 1
		var result = space_state.intersect_ray(query)
		
		if result:
			p.append(result.position)
		else:
			p.append(corner_global)
			
	var dir_z = ((p[2] - p[0]) + (p[3] - p[1])).normalized()
	var dir_x = ((p[1] - p[0]) + (p[3] - p[2])).normalized()
	var dir_y = dir_z.cross(dir_x).normalized()
	dir_z = dir_y.cross(dir_x).normalized()
	
	var avg_pos = (p[0] + p[1] + p[2] + p[3]) / 4.0
	var final_pos = avg_pos - dir_y * 0.5
	
	var angle = deg_to_rad(90.0)
	var rotated_basis = Basis(dir_x, dir_y, dir_z).rotated(dir_y, angle)
	var origin_pos = final_pos - rotated_basis.z * 3.0
	
	active_ramp.global_transform = Transform3D(rotated_basis, origin_pos)
	dummy.queue_free()

func setup_polygons():
	road_bed.mode = CSGPolygon3D.MODE_PATH
	road_bed.path_node = road_bed.get_path_to(path_node)
	road_bed.path_interval = 2.0
	road_bed.path_rotation = CSGPolygon3D.PATH_ROTATION_PATH_FOLLOW
	road_bed.path_local = true
	road_bed.path_continuous_u = true
	road_bed.path_u_distance = 16.0
	road_bed.use_collision = true
	road_bed.polygon = PackedVector2Array([
		Vector2(-52.0, 0.0),
		Vector2(52.0, 0.0),
		Vector2(52.0, -0.32),
		Vector2(-52.0, -0.32)
	])
	
	border_l.mode = CSGPolygon3D.MODE_PATH
	border_l.path_node = border_l.get_path_to(path_node)
	border_l.path_interval = 2.0
	border_l.path_rotation = CSGPolygon3D.PATH_ROTATION_PATH_FOLLOW
	border_l.path_local = true
	border_l.path_continuous_u = true
	border_l.path_u_distance = 16.0
	border_l.use_collision = false
	border_l.polygon = PackedVector2Array([
		Vector2(-50.4, 0.2),
		Vector2(-48.0, 0.2),
		Vector2(-48.0, -0.08),
		Vector2(-50.4, -0.08)
	])
	
	border_r.mode = CSGPolygon3D.MODE_PATH
	border_r.path_node = border_r.get_path_to(path_node)
	border_r.path_interval = 2.0
	border_r.path_rotation = CSGPolygon3D.PATH_ROTATION_PATH_FOLLOW
	border_r.path_local = true
	border_r.path_continuous_u = true
	border_r.path_u_distance = 16.0
	border_r.use_collision = false
	border_r.polygon = PackedVector2Array([
		Vector2(48.0, 0.2),
		Vector2(50.4, 0.2),
		Vector2(50.4, -0.08),
		Vector2(48.0, -0.08)
	])
	
	center_line.mode = CSGPolygon3D.MODE_PATH
	center_line.path_node = center_line.get_path_to(path_node)
	center_line.path_interval = 2.0
	center_line.path_rotation = CSGPolygon3D.PATH_ROTATION_PATH_FOLLOW
	center_line.path_local = true
	center_line.path_continuous_u = true
	center_line.path_u_distance = 8.0
	center_line.polygon = PackedVector2Array([
		Vector2(-1.2, 0.15),
		Vector2(1.2, 0.15),
		Vector2(1.2, 0.05),
		Vector2(-1.2, 0.05)
	])

func build_gate():
	var gate_center = 25.0
	var t1_gate = path_node.global_transform * path_node.curve.sample_baked_with_rotation(gate_center, true, true)
	if not t1_gate:
		return
		
	var gate_node = Node3D.new()
	gate_node.global_transform = t1_gate
	add_child(gate_node)
	
	# The road goes from x = -52.0 to x = +52.0.
	# With a 5m gap from the track sides, inner edge of pillar is at 52.0 + 5.0 = 57.0m.
	# With an 8m wide pillar (size x = 8.0), center is at 57.0 + 4.0 = 61.0m.
	var build_pillar = func(x_pos):
		var pillar = CSGBox3D.new()
		pillar.size = Vector3(8.0, 44.0, 8.0)
		pillar.position = Vector3(x_pos, 14.0, 0.0)
		pillar.material = road_bed.material
		pillar.use_collision = true
		
		var glow_band = CSGBox3D.new()
		glow_band.size = Vector3(8.5, 2.0, 8.5)
		glow_band.position = Vector3(0, 5.0, 0)
		glow_band.material = border_l.material
		pillar.add_child(glow_band)
		return pillar
		
	gate_node.add_child(build_pillar.call(-61.0))
	gate_node.add_child(build_pillar.call(61.0))
	
	# Top beam connecting pillar tops (y = 33.0)
	var beam_top = CSGBox3D.new()
	beam_top.size = Vector3(130.0, 6.0, 8.0)
	beam_top.position = Vector3(0.0, 33.0, 0.0)
	beam_top.material = road_bed.material
	beam_top.use_collision = true
	
	var beam_top_glow = CSGBox3D.new()
	beam_top_glow.size = Vector3(130.6, 2.0, 8.5)
	beam_top_glow.position = Vector3(0.0, 0.0, 0.0)
	beam_top_glow.material = center_line.material
	beam_top.add_child(beam_top_glow)
	
	gate_node.add_child(beam_top)
	
	# Bottom beam 5m under the track (center at y = -5.0, forming a complete square frame)
	var beam_bottom = CSGBox3D.new()
	beam_bottom.size = Vector3(130.0, 6.0, 8.0)
	beam_bottom.position = Vector3(0.0, -5.0, 0.0)
	beam_bottom.material = road_bed.material
	beam_bottom.use_collision = true
	
	var beam_bottom_glow = CSGBox3D.new()
	beam_bottom_glow.size = Vector3(130.6, 2.0, 8.5)
	beam_bottom_glow.position = Vector3(0.0, 0.0, 0.0)
	beam_bottom_glow.material = border_l.material
	beam_bottom.add_child(beam_bottom_glow)
	
	gate_node.add_child(beam_bottom)

func setup_checkpoint_system():
	var length = path_node.curve.get_baked_length()
	
	# Gate Area3D (at progress = 25.0)
	var t_gate = path_node.global_transform * path_node.curve.sample_baked_with_rotation(25.0, true, true)
	var gate_area = Area3D.new()
	gate_area.name = "GateCheckpoint"
	gate_area.global_transform = t_gate
	gate_area.collision_layer = 16
	gate_area.collision_mask = 16
	var gate_col = CollisionShape3D.new()
	var gate_box = BoxShape3D.new()
	gate_box.size = Vector3(110.0, 20.0, 10.0)
	gate_col.shape = gate_box
	gate_area.add_child(gate_col)
	add_child(gate_area)
	gate_area.area_entered.connect(_on_gate_area_entered)
	
	# Halfway Checkpoint Area3D (at progress = fmod(25.0 + length * 0.5, length))
	var halfway_prog = fmod(25.0 + length * 0.5, length)
	var t_half = path_node.global_transform * path_node.curve.sample_baked_with_rotation(halfway_prog, true, true)
	var half_area = Area3D.new()
	half_area.name = "HalfwayCheckpoint"
	half_area.global_transform = t_half
	half_area.collision_layer = 16
	half_area.collision_mask = 16
	var half_col = CollisionShape3D.new()
	var half_box = BoxShape3D.new()
	half_box.size = Vector3(110.0, 20.0, 10.0)
	half_col.shape = half_box
	half_area.add_child(half_col)
	add_child(half_area)
	half_area.area_entered.connect(_on_halfway_area_entered)

func _on_halfway_area_entered(area: Area3D):
	if area.name == "CheckpointSphere" or (supercar and area.get_parent() == supercar):
		halfway_cleared = true

func format_time(t: float) -> String:
	var mins = int(t) / 60
	var secs = int(t) % 60
	var millis = int((t - float(int(t))) * 100.0)
	return "%02d:%02d.%02d" % [mins, secs, millis]

func spawn_fireworks():
	var colors = [Color(0.0, 1.0, 1.0), Color(1.0, 0.0, 1.0), Color(1.0, 0.9, 0.1), Color(0.2, 1.0, 0.2)]
	for i in range(8):
		var particles = CPUParticles3D.new()
		particles.name = "CelebrationFireworks_" + str(i)
		add_child(particles)
		var gate_t = path_node.global_transform * path_node.curve.sample_baked_with_rotation(15.0, true, true)
		var offset = Vector3(randf_range(-40.0, 40.0), randf_range(15.0, 40.0), randf_range(-30.0, 30.0))
		particles.global_position = gate_t.origin + offset if gate_t else Vector3(offset.x, offset.y, offset.z)
		particles.emitting = true
		particles.amount = 120
		particles.one_shot = false
		particles.explosiveness = 0.8
		particles.lifetime = 2.5
		particles.spread = 180.0
		particles.initial_velocity_min = 15.0
		particles.initial_velocity_max = 30.0
		particles.gravity = Vector3(0, -9.8, 0)
		particles.scale_amount_min = 0.4
		particles.scale_amount_max = 1.0
		particles.color = colors[i % colors.size()]

func _on_gate_area_entered(area: Area3D):
	if area.name == "CheckpointSphere" or (supercar and area.get_parent() == supercar):
		if (halfway_cleared or current_lap == 0) and not race_finished:
			if current_lap > 0:
				lap_times.append(current_lap_time)
				current_lap_time = 0.0
			current_lap += 1
			halfway_cleared = false
			print("LAP COMPLETED! Current Lap: %d / %d" % [current_lap, final_laps])
			
			if current_lap > final_laps:
				race_finished = true
				print("RACE FINISHED! Total Time: ", format_time(race_time))
				if victory_jingle_player:
					victory_jingle_player.play()
				spawn_fireworks()
				if victory_panel:
					victory_panel.visible = true
					var vic_label = victory_panel.get_node_or_null("VictoryText")
					if vic_label:
						var best_lap = lap_times[0] if lap_times.size() > 0 else 0.0
						for lt in lap_times:
							if lt < best_lap: best_lap = lt
						var txt = "--- VICTORY! ---\n"
						txt += "1st PLACE / FINISHED!\n\n"
						txt += "TOTAL TIME:  " + format_time(race_time) + "\n"
						txt += "BEST LAP:    " + format_time(best_lap) + "\n\n"
						for l_idx in range(lap_times.size()):
							txt += "Lap %d: %s\n" % [l_idx + 1, format_time(lap_times[l_idx])]
						vic_label.text = txt

func spawn_random_powerups():
	var nitro_script = load("res://nitro_powerup.gd")
	if not nitro_script:
		return
		
	var length = path_node.curve.get_baked_length()
	var num_powerups = 25
	
	for i in range(num_powerups):
		var progress = float(i + 1) * (length / float(num_powerups + 1))
		progress = fmod(progress + randf_range(-10.0, 10.0) + length, length)
		
		# Don't spawn within 30 meters of starting grid / gate (progress 15.0 and 25.0)
		if abs(progress - 15.0) < 30.0 or abs(progress - 25.0) < 20.0:
			continue
			
		var t = path_node.global_transform * path_node.curve.sample_baked_with_rotation(progress, true, true)
		if not t:
			continue
			
		var lane_offset = randf_range(-42.0, 42.0)
		var world_pos = t.origin + t.basis.x * lane_offset + t.basis.y * 20.0
		
		var powerup = StaticBody3D.new()
		powerup.name = "RandomNitroPowerup_" + str(i)
		powerup.set_script(nitro_script)
		
		# CRITICAL: add_child BEFORE setting spatial position to avoid !is_inside_tree() errors
		add_child(powerup)
		powerup.global_position = world_pos
		powerup_nodes.append(powerup)

func setup_ui():
	hud_layer = CanvasLayer.new()
	hud_layer.name = "GameUI"
	add_child(hud_layer)
	
	# Top-Left Lap Indicator HUD
	lap_label = Label.new()
	lap_label.name = "LapLabel"
	lap_label.position = Vector2(40, 30)
	var lap_settings = LabelSettings.new()
	lap_settings.font_size = 42
	lap_settings.font_color = Color(1.0, 0.9, 0.1) # Neon yellow/gold
	lap_settings.outline_size = 8
	lap_settings.outline_color = Color(0.0, 0.0, 0.0, 0.9)
	lap_label.label_settings = lap_settings
	lap_label.text = "Lap: 1 / 3"
	hud_layer.add_child(lap_label)
	
	# Center Screen Countdown
	countdown_label = Label.new()
	countdown_label.name = "CountdownLabel"
	countdown_label.anchor_left = 0.5
	countdown_label.anchor_right = 0.5
	countdown_label.anchor_top = 0.35
	countdown_label.anchor_bottom = 0.35
	countdown_label.grow_horizontal = Control.GROW_DIRECTION_BOTH
	countdown_label.grow_vertical = Control.GROW_DIRECTION_BOTH
	countdown_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	
	var count_settings = LabelSettings.new()
	count_settings.font_size = 150
	count_settings.font_color = Color(1.0, 0.2, 0.2)
	count_settings.outline_size = 16
	count_settings.outline_color = Color(0.0, 0.0, 0.0, 0.9)
	countdown_label.label_settings = count_settings
	countdown_label.text = ""
	countdown_label.visible = false
	hud_layer.add_child(countdown_label)
	
	beep_low_stream = load("res://beep_low.wav")
	beep_high_stream = load("res://beep_high.wav")
	beep_player = AudioStreamPlayer.new()
	beep_player.name = "CountdownBeepPlayer"
	add_child(beep_player)

	# Top-Right Time Indicator HUD
	time_label = Label.new()
	time_label.name = "TimeLabel"
	time_label.anchor_left = 1.0
	time_label.anchor_right = 1.0
	time_label.position = Vector2(-380, 30)
	var time_settings = LabelSettings.new()
	time_settings.font_size = 36
	time_settings.font_color = Color(0.0, 1.0, 1.0) # Neon cyan
	time_settings.outline_size = 8
	time_settings.outline_color = Color(0.0, 0.0, 0.0, 0.9)
	time_label.label_settings = time_settings
	time_label.text = "TIME: 00:00.00\nLAP: 00:00.00"
	hud_layer.add_child(time_label)

	# Center Screen Victory Panel
	victory_panel = Control.new()
	victory_panel.name = "VictoryPanel"
	victory_panel.anchor_left = 0.5
	victory_panel.anchor_right = 0.5
	victory_panel.anchor_top = 0.35
	victory_panel.anchor_bottom = 0.35
	victory_panel.visible = false
	hud_layer.add_child(victory_panel)

	var bg_box = ColorRect.new()
	bg_box.color = Color(0.05, 0.02, 0.12, 0.9)
	bg_box.position = Vector2(-350, -180)
	bg_box.size = Vector2(700, 360)
	victory_panel.add_child(bg_box)

	var border = ColorRect.new()
	border.color = Color(1.0, 0.84, 0.0, 1.0) # Gold border
	border.position = Vector2(-350, -180)
	border.size = Vector2(700, 6)
	victory_panel.add_child(border)

	var vic_label = Label.new()
	vic_label.name = "VictoryText"
	vic_label.position = Vector2(-350, -160)
	vic_label.size = Vector2(700, 340)
	vic_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	var vic_settings = LabelSettings.new()
	vic_settings.font_size = 32
	vic_settings.font_color = Color(1.0, 0.9, 0.2) # Gold
	vic_settings.outline_size = 8
	vic_settings.outline_color = Color(0.0, 0.0, 0.0, 0.9)
	vic_label.label_settings = vic_settings
	victory_panel.add_child(vic_label)

	# 10s Popup Menu
	victory_popup_menu = Control.new()
	victory_popup_menu.name = "VictoryPopupMenu"
	victory_popup_menu.anchor_left = 0.5
	victory_popup_menu.anchor_right = 0.5
	victory_popup_menu.anchor_top = 0.78
	victory_popup_menu.anchor_bottom = 0.78
	victory_popup_menu.visible = false
	hud_layer.add_child(victory_popup_menu)

	var pop_bg = ColorRect.new()
	pop_bg.color = Color(0.8, 0.0, 0.5, 0.95) # Magenta
	pop_bg.position = Vector2(-280, -40)
	pop_bg.size = Vector2(560, 80)
	victory_popup_menu.add_child(pop_bg)

	var pop_label = Label.new()
	pop_label.position = Vector2(-280, -30)
	pop_label.size = Vector2(560, 60)
	pop_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	var pop_settings = LabelSettings.new()
	pop_settings.font_size = 28
	pop_settings.font_color = Color(1.0, 1.0, 1.0)
	pop_settings.outline_size = 6
	pop_settings.outline_color = Color(0.0, 0.0, 0.0, 0.9)
	pop_label.label_settings = pop_settings
	pop_label.text = "Press [RESET / R] to Play Again!"
	victory_popup_menu.add_child(pop_label)

	victory_jingle_player = AudioStreamPlayer.new()
	victory_jingle_player.name = "VictoryJinglePlayer"
	var jingle_res = load("res://victory_jingle.wav")
	if jingle_res:
		victory_jingle_player.stream = jingle_res
	add_child(victory_jingle_player)
