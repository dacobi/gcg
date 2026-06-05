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

	queue_free()

func _on_hit(node: Node) -> void:
	if "is_bunker" in node:
		return # Pass through/destroy bunkers without dying
		
	if node.has_method("hit"):
		node.hit()
		die()
	elif node.has_method("kill"):
		node.kill()
		die()
