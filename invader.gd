extends Area3D

func _ready():
	area_entered.connect(_on_hit)
	body_entered.connect(_on_hit)

func die()->void:
	var parent_node = get_parent()

	if parent_node and parent_node.has_method("vaderdie"):
		parent_node.vaderdie(global_position.y)	

	queue_free()

func _on_hit(node: Node3D) -> void:
	if node.has_method("hit"):
		node.hit()
	elif node.name == "BunkerGroup":
		# If we hit the group itself, we don't do anything here.
		# Rely on direct body_entered signals for individual pixels.
		pass
