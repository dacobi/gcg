extends Area3D

func die()->void:
	var parent_node = get_parent()
	

	if parent_node and parent_node.has_method("vaderdie"):
		parent_node.vaderdie()	

	queue_free()
