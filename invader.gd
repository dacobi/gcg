extends Area3D

var is_invader = true
var is_dead = false

func _ready():
	area_entered.connect(_on_hit)
	body_entered.connect(_on_hit)

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
