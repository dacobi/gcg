extends Area3D

var speed = 20.0

func _init():
	set_process(true)

func _ready():
	area_entered.connect(_on_hit)
	body_entered.connect(_on_hit)

func _process(delta):
	position.y -= speed * delta
	
	# Polling check for groups (like Bunkers)
	var areas = get_overlapping_areas()
	for area in areas:
		if not area.has_method("hit"):
			if _try_hit_closest_child(area):
				return # Projectile died

func _on_hit(body: Node3D) -> void:
	if body.has_method("hit"):
		body.hit()
		queue_free()
	else:
		_try_hit_closest_child(body)

func _try_hit_closest_child(container: Node) -> bool:
	var closest = null
	var min_dist = 3.0
	
	for child in container.get_children():
		if child.has_method("hit") and child is Node3D:
			var d = child.global_position.distance_to(global_position)
			if d < min_dist:
				min_dist = d
				closest = child
	
	if closest:
		closest.hit()
		queue_free()
		return true
	return false
