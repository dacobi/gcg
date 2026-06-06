extends Area3D

var is_invader = true
var is_dead = false

@onready var geo_open = get_node_or_null("GeometryOpen")
@onready var geo_closed = get_node_or_null("GeometryClosed")

func _ready():
	area_entered.connect(_on_hit)
	body_entered.connect(_on_hit)

func set_frame(is_open: bool) -> void:
	if not geo_open or not geo_closed:
		print("Invader Error: Missing geometry nodes! Open:", geo_open, " Closed:", geo_closed)
		return
	geo_open.visible = is_open
	geo_closed.visible = not is_open
	# print("Invader frame set to open: ", is_open)

func die()->void:
	if is_dead: return
	is_dead = true
	var parent_node = get_parent()

	if parent_node and parent_node.has_method("vaderdie"):
		parent_node.vaderdie(global_position.y)	

	_spawn_death_particles()
	queue_free()

func _spawn_death_particles():
	var root_node = get_tree().root
	var game_root = root_node.find_child("SpaceInvadersGame", true, false)
	if !game_root: return

	var sparks = CPUParticles3D.new()
	sparks.emitting = false
	sparks.amount = 75
	sparks.lifetime = 0.5
	sparks.one_shot = true
	sparks.explosiveness = 1.0
	sparks.local_coords = false
	
	# Rotate so local Y points down global Z (towards camera)
	sparks.rotation_degrees = Vector3(0, 90, 0) 

	# Process Parameters
	sparks.direction = Vector3(0, 1, 0) # Emit along local Y (global Z)
	sparks.flatness = 1.0 # Squashes spread along local Y (global Z), forcing 2D spread on global XY plane
	sparks.spread = 180.0
	sparks.initial_velocity_min = 10.0
	sparks.initial_velocity_max = 20.0
	sparks.scale_amount_min = 1.0
	sparks.scale_amount_max = 2.0

	var gradient = Gradient.new()
	gradient.set_color(0, Color(8.0, 6.0, 0.0, 1))
	gradient.set_offset(0, 0.0)
	gradient.add_point(0.1, Color(6.0, 3.0, 0, 1))
	gradient.add_point(0.5, Color(8.0, 0.0, 0, 1))
	gradient.set_color(1, Color(3.0, 0, 0, 1)) # Solid dark red, no alpha fade
	gradient.set_offset(1, 1.0)
	
	sparks.color_ramp = gradient

	var mesh = QuadMesh.new()
	mesh.size = Vector2(0.3, 0.3)
	
	var mat = StandardMaterial3D.new()
	mat.billboard_mode = StandardMaterial3D.BILLBOARD_ENABLED
	mat.shading_mode = StandardMaterial3D.SHADING_MODE_UNSHADED
	mat.vertex_color_use_as_albedo = true
	
	mesh.material = mat
	sparks.mesh = mesh

	game_root.add_child(sparks)
	sparks.global_position = global_position
	
	# Clean up automatically when finished
	sparks.finished.connect(sparks.queue_free)
	sparks.emitting = true

func _on_hit(node: Node) -> void:
	if "is_bunker" in node:
		return # Pass through/destroy bunkers without dying
		
	if node.has_method("hit"):
		node.hit()
		die()
	elif node.has_method("kill"):
		node.kill()
		die()
