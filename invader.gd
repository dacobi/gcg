extends Area3D

func _ready():
	area_entered.connect(_on_hit)
	body_entered.connect(_on_hit)
	set_process(true)

func _process(delta):
	# Aggressive shaving: destroy everything we touch every frame
	for b in get_overlapping_bodies():
		_shave_bunker(b)
			
	for a in get_overlapping_areas():
		if a != self:
			_shave_bunker(a)

func die()->void:
	var parent_node = get_parent()
	
	if parent_node and parent_node.has_method("vaderdie"):
		parent_node.vaderdie()	

	queue_free()

func _on_hit(node: Node3D) -> void:
	_shave_bunker(node)

func _shave_bunker(node: Node3D) -> void:
	if node.has_method("hit"):
		node.hit()
		
		# Also shave nearby sibling pixels
		var parent = node.get_parent()
		if parent:
			for sibling in parent.get_children():
				if sibling != node and sibling.has_method("hit") and sibling is Node3D:
					if sibling.global_position.distance_to(global_position) < 5.0:
						sibling.hit()
	else:
		# It's a container (like BunkerGroup Area3D), so check its children
		for child in node.get_children():
			if child.has_method("hit") and child is Node3D:
				# Use a wide radius so the invader's large box sweeps effectively
				if child.global_position.distance_to(global_position) < 5.0:
					child.hit()
