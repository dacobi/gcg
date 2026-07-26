extends StaticBody3D

var respawn_timer: float = 0.0
var base_y: float = 0.0
var visual_group: Node3D
var sparkle_mat: ParticleProcessMaterial
var snapped_to_track: bool = false
var snap_attempts: int = 0

func _ready() -> void:
	collision_layer = 4 # Prop collision layer
	collision_mask = 0
	
	base_y = position.y
	
	var col = CollisionShape3D.new()
	var shape = SphereShape3D.new()
	shape.radius = 2.25 # 50% larger collision sphere
	col.shape = shape
	add_child(col)
	
	visual_group = Node3D.new()
	add_child(visual_group)
	
	# Procedural cellular noise for crystalline granular texture
	var noise = FastNoiseLite.new()
	noise.noise_type = FastNoiseLite.TYPE_CELLULAR
	noise.frequency = 0.08
	noise.cellular_distance_function = FastNoiseLite.DISTANCE_EUCLIDEAN
	noise.cellular_return_type = FastNoiseLite.RETURN_CELL_VALUE
	
	var ntex = NoiseTexture2D.new()
	ntex.noise = noise
	ntex.width = 256
	ntex.height = 256
	ntex.seamless = true
	
	# 8-faced glass diamond (50% larger octahedron constructed from 4-sided cones)
	var mat = StandardMaterial3D.new()
	mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	mat.albedo_color = Color(0.08, 0.0, 0.25, 0.9) # Dark royal purple glass
	mat.roughness = 0.2
	mat.roughness_texture = ntex
	mat.normal_enabled = true
	mat.normal_scale = 0.6
	mat.normal_texture = ntex
	mat.emission_enabled = true
	mat.emission = Color(0.35, 0.02, 0.7) # Deep darker ultraviolet glow
	mat.emission_energy_multiplier = 6.0 # 3x emission glow (was 2.0)
	
	var top_cone = CSGCylinder3D.new()
	top_cone.radius = 1.2 # 50% larger (was 0.8)
	top_cone.height = 1.5 # 50% larger (was 1.0)
	top_cone.sides = 4
	top_cone.cone = true
	top_cone.position.y = 0.75 # 50% larger offset (was 0.5)
	top_cone.material = mat
	visual_group.add_child(top_cone)
	
	var bottom_cone = CSGCylinder3D.new()
	bottom_cone.radius = 1.2 # 50% larger
	bottom_cone.height = 1.5 # 50% larger
	bottom_cone.sides = 4
	bottom_cone.cone = true
	bottom_cone.rotation_degrees.x = 180.0
	bottom_cone.position.y = -0.75 # 50% larger offset
	bottom_cone.material = mat
	visual_group.add_child(bottom_cone)
	
	# Prepare sparkle material for collection burst
	sparkle_mat = ParticleProcessMaterial.new()
	sparkle_mat.direction = Vector3(0, 1, 0)
	sparkle_mat.spread = 180.0
	sparkle_mat.initial_velocity_min = 5.0
	sparkle_mat.initial_velocity_max = 10.0
	sparkle_mat.gravity = Vector3(0, -2.0, 0)
	
	var curve = Curve.new()
	curve.add_point(Vector2(0.0, 1.0))
	curve.add_point(Vector2(1.0, 0.0))
	var ctex = CurveTexture.new()
	ctex.curve = curve
	sparkle_mat.scale_curve = ctex
	sparkle_mat.scale_min = 0.3 # Larger sparkles
	sparkle_mat.scale_max = 0.6
	
	var grad = Gradient.new()
	grad.add_point(0.0, Color(0.45, 0.05, 0.85, 1.0))
	grad.add_point(0.5, Color(0.2, 0.0, 0.5, 0.9))
	grad.add_point(1.0, Color(0.05, 0.0, 0.2, 0.0))
	var gtex = GradientTexture1D.new()
	gtex.gradient = grad
	sparkle_mat.color_ramp = gtex

func _physics_process(_delta: float) -> void:
	if not snapped_to_track and snap_attempts < 10:
		snap_attempts += 1
		var space_state = get_world_3d().direct_space_state
		if space_state:
			var query = PhysicsRayQueryParameters3D.create(global_position + Vector3(0, 50.0, 0), global_position - Vector3(0, 100.0, 0))
			query.collision_mask = 1 # Road collision layer
			var result = space_state.intersect_ray(query)
			if result:
				# Bottom cone tip is 1.5m below center (y = -1.5 in local space).
				# To sit exactly 1.5m above the track surface: center = result.position + 1.5m (bottom tip offset) + 1.5m (hover gap) = result.position + 3.0m along normal.
				global_position = result.position + result.normal * 3.0
				base_y = position.y
				snapped_to_track = true

func _process(delta: float) -> void:
	if respawn_timer > 0.0:
		respawn_timer -= delta
		if respawn_timer <= 0.0:
			visible = true
			collision_layer = 4
	else:
		visual_group.rotation.y += 1.5 * delta
		position.y = base_y + sin(Time.get_ticks_msec() * 0.003) * 0.4 # Slightly wider bobbing (±0.4m)

func on_nitro_collected(car: Node) -> void:
	if respawn_timer > 0.0 or not visible:
		return
		
	if car.has_method("add_nitro"):
		car.add_nitro(15.0)
		
	# Sparkle burst
	var burst = GPUParticles3D.new()
	burst.amount = 60 # Scaled up particle count
	burst.lifetime = 0.6
	burst.one_shot = true
	burst.explosiveness = 0.9
	burst.process_material = sparkle_mat
	
	var mesh = QuadMesh.new()
	mesh.size = Vector2(0.4, 0.4) # Larger quad particles
	var bmat = StandardMaterial3D.new()
	bmat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	bmat.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
	bmat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	bmat.vertex_color_use_as_albedo = true
	bmat.billboard_mode = BaseMaterial3D.BILLBOARD_PARTICLES
	mesh.material = bmat
	burst.draw_pass_1 = mesh
	
	var parent_node = get_parent() if get_parent() else get_tree().root
	parent_node.add_child(burst)
	burst.global_position = global_position
	burst.emitting = true
	
	# Automatically clean up particles after lifetime
	var timer = get_tree().create_timer(1.0)
	timer.timeout.connect(burst.queue_free)

	# Audio feedback
	if ClassDB.class_exists("FmodServer"):
		if FmodServer.has_method("create_event_instance"):
			var ev = FmodServer.create_event_instance("event:/Interactables/ping")
			if ev:
				# if ev.has_method("set_parameter_by_name"):
				# 	ev.set_parameter_by_name("speed", 50.0)
				if ev.has_method("set_3d_attributes"):
					ev.set_3d_attributes(global_transform)
				if ev.has_method("start"):
					ev.start()
				if ev.has_method("release"):
					ev.release()
					
	visible = false
	collision_layer = 0
	respawn_timer = 30.0
