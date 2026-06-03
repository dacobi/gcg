extends Area3D

var velocity = Vector3.ZERO
var spark_gravity = 25.0
var bottom_limit = -50.0
var rotation_speed = Vector3.ZERO

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
	
	if node.has_method("die"):
		node.die()
		queue_free()
	elif node.has_method("hit"):
		node.hit()
		queue_free()
