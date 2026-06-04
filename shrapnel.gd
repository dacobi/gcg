extends Area3D

var velocity = Vector3.ZERO
var shrapnel_gravity = 20.0
var rotation_speed = Vector3.ZERO
var bottom_limit = -50.0

@onready var mat_red = StandardMaterial3D.new()

func _ready():
	area_entered.connect(_on_hit)
	body_entered.connect(_on_hit)
	
	mat_red.albedo_color = Color(1.0, 0.0, 0.0)
	mat_red.roughness = 0.5
	
	# Random tumbling
	rotation_speed = Vector3(randf_range(-15, 15), randf_range(-15, 15), randf_range(-15, 15))
	
	# Create twisted metal visuals (3 random small boxes)
	var geom = CSGCombiner3D.new()
	for i in range(3):
		var box = CSGBox3D.new()
		box.size = Vector3(0.4, 0.4, 0.4)
		box.position = Vector3(randf_range(-0.2, 0.2), randf_range(-0.2, 0.2), randf_range(-0.2, 0.2))
		box.rotation = Vector3(randf_range(0, PI), randf_range(0, PI), randf_range(0, PI))
		box.material = mat_red
		geom.add_child(box)
	add_child(geom)
	
	# Collision shape
	var col = CollisionShape3D.new()
	var shape = BoxShape3D.new()
	shape.size = Vector3(0.4, 0.4, 0.4)
	col.shape = shape
	add_child(col)

func _process(delta):
	velocity.y -= shrapnel_gravity * delta
	position += velocity * delta
	
	rotate_x(rotation_speed.x * delta)
	rotate_y(rotation_speed.y * delta)
	rotate_z(rotation_speed.z * delta)
	
	if position.y < bottom_limit:
		queue_free()

func _on_hit(node: Node) -> void:
	if node == get_parent(): return
	if "Tardis" in node.name: return # Optional: Ignore Tardis
	if "is_invader" in node: return # Don't hit your own kind!
	
	if node.has_method("die"):
		node.die()
		queue_free()
	elif node.has_method("hit"):
		node.hit()
		queue_free()
