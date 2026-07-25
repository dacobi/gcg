extends StaticBody3D

var respawn_timer: float = 0.0
var base_y: float = 0.0
var visual_group: Node3D
var sparkle_mat: ParticleProcessMaterial

func _ready() -> void:
	collision_layer = 4 # Prop collision layer
	collision_mask = 0
	
	base_y = position.y
	
	var col = CollisionShape3D.new()
	var shape = SphereShape3D.new()
	shape.radius = 1.5
	col.shape = shape
	add_child(col)
	
	visual_group = Node3D.new()
	add_child(visual_group)
	
	# 8-faced glass diamond (octahedron constructed from 4-sided cones)
	var mat = StandardMaterial3D.new()
	mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	mat.albedo_color = Color(0.6, 0.1, 0.9, 0.75) # Purple glass
	mat.roughness = 0.1
	mat.emission_enabled = true
	mat.emission = Color(0.8, 0.2, 1.0)
	mat.emission_energy_multiplier = 2.0
	
	var top_cone = CSGCylinder3D.new()
	top_cone.radius = 0.8
	top_cone.height = 1.0
	top_cone.sides = 4
	top_cone.cone = true
	top_cone.position.y = 0.5
	top_cone.material = mat
	visual_group.add_child(top_cone)
	
	var bottom_cone = CSGCylinder3D.new()
	bottom_cone.radius = 0.8
	bottom_cone.height = 1.0
	bottom_cone.sides = 4
	bottom_cone.cone = true
	bottom_cone.rotation_degrees.x = 180.0
	bottom_cone.position.y = -0.5
	bottom_cone.material = mat
	visual_group.add_child(bottom_cone)
	
	# Prepare sparkle material for collection burst
	sparkle_mat = ParticleProcessMaterial.new()
	sparkle_mat.direction = Vector3(0, 1, 0)
	sparkle_mat.spread = 180.0
	sparkle_mat.initial_velocity_min = 4.0
	sparkle_mat.initial_velocity_max = 8.0
	sparkle_mat.gravity = Vector3(0, -2.0, 0)
	
	var curve = Curve.new()
	curve.add_point(Vector2(0.0, 1.0))
	curve.add_point(Vector2(1.0, 0.0))
	var ctex = CurveTexture.new()
	ctex.curve = curve
	sparkle_mat.scale_curve = ctex
	sparkle_mat.scale_min = 0.2
	sparkle_mat.scale_max = 0.4
	
	var grad = Gradient.new()
	grad.add_point(0.0, Color(1.0, 0.2, 1.0, 1.0))
	grad.add_point(0.5, Color(0.6, 0.0, 1.0, 0.8))
	grad.add_point(1.0, Color(0.0, 1.0, 1.0, 0.0))
	var gtex = GradientTexture1D.new()
	gtex.gradient = grad
	sparkle_mat.color_ramp = gtex

func _process(delta: float) -> void:
	if respawn_timer > 0.0:
		respawn_timer -= delta
		if respawn_timer <= 0.0:
			visible = true
			collision_layer = 4
	else:
		visual_group.rotation.y += 1.5 * delta
		position.y = base_y + sin(Time.get_ticks_msec() * 0.003) * 0.3

func on_nitro_collected(car: Node) -> void:
	if respawn_timer > 0.0 or not visible:
		return
		
	if car.has_method("add_nitro"):
		car.add_nitro(15.0)
		
	# Sparkle burst
	var burst = GPUParticles3D.new()
	burst.amount = 40
	burst.lifetime = 0.6
	burst.one_shot = true
	burst.explosiveness = 0.9
	burst.position = global_position
	burst.process_material = sparkle_mat
	
	var mesh = QuadMesh.new()
	mesh.size = Vector2(0.3, 0.3)
	var bmat = StandardMaterial3D.new()
	bmat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	bmat.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
	bmat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	bmat.vertex_color_use_as_albedo = true
	bmat.billboard_mode = BaseMaterial3D.BILLBOARD_PARTICLES
	mesh.material = bmat
	burst.draw_pass_1 = mesh
	
	get_tree().current_scene.add_child(burst)
	burst.emitting = true

	# Audio feedback
	if ClassDB.class_exists("FmodServer"):
		if FmodServer.has_method("create_event_instance"):
			var ev = FmodServer.create_event_instance("event:/Interactables/Wooden Collision")
			if ev:
				if ev.has_method("set_parameter_by_name"):
					ev.set_parameter_by_name("speed", 50.0)
				if ev.has_method("set_3d_attributes"):
					ev.set_3d_attributes(global_transform)
				if ev.has_method("start"):
					ev.start()
				if ev.has_method("release"):
					ev.release()
					
	visible = false
	collision_layer = 0
	respawn_timer = 30.0
