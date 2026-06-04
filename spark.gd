extends Area3D

var velocity = Vector3.ZERO
var spark_gravity = 25.0
var bottom_limit = -50.0
var rotation_speed = Vector3.ZERO
var hits_remaining = 9

func _ready():
	area_entered.connect(_on_hit)
	body_entered.connect(_on_hit)
	
	# Random tumbling rotation
	rotation_speed = Vector3(
		randf_range(-10, 10),
		randf_range(-10, 10),
		randf_range(-10, 10)
	)

func _process(delta):
	velocity.y -= spark_gravity * delta
	position += velocity * delta
	
	# Apply tumbling
	rotate_x(rotation_speed.x * delta)
	rotate_y(rotation_speed.y * delta)
	rotate_z(rotation_speed.z * delta)
	
	if position.y < bottom_limit:
		queue_free()

func _on_hit(node: Node) -> void:
	# Same logic as player projectiles: destroy target and self
	if node == get_parent(): return
	if "Tardis" in node.name: return # Don't hit the Tardis!
	if "Ship" in node.name: return # Don't hit the player!

	if node.has_method("die") or node.has_method("hit"):
		if node.has_method("die"):
			node.die()
		else:
			node.hit()

		# Survive up to 9 bunker pixel hits
		if "is_bunker" in node:
			hits_remaining -= 1
			if hits_remaining <= 0:
				queue_free()
		else:
			# Invaders or other objects destroy the bolt immediately
			queue_free()


