extends Area3D

var speed = 40.0

func _init():
	set_process(true)

func _ready():
	# We'll use polling in _process for more reliable "drilling" through groups,
	# but keep signals for simple one-hit targets like Invaders.
	area_entered.connect(_on_hit)
	body_entered.connect(_on_hit)

func _process(delta):
	position.y += speed * delta
	
	if position.y > 80.0:
		queue_free()
		return

	# Polling check for groups (like Bunkers) that we are currently inside
	var areas = get_overlapping_areas()
	for area in areas:
		if not area.has_method("die"):
			# It's a container group, try to kill a child
			if _try_kill_closest_child(area):
				return # Projectile died

func _on_hit(body: Node3D) -> void:
	if body.has_method("die"):
		body.die()
		queue_free()
	else:
		_try_kill_closest_child(body)

func _try_kill_closest_child(container: Node) -> bool:
	var closest = null
	var min_dist = 3.0 # Slightly larger radius to ensure we catch internal pixels
	
	for child in container.get_children():
		if child.has_method("die") and child is Node3D:
			var d = child.global_position.distance_to(global_position)
			if d < min_dist:
				min_dist = d
				closest = child
	
	if closest:
		closest.die()
		queue_free()
		return true
	return false
