extends Area3D

var velocity = Vector3.ZERO
var shrapnel_gravity = 25.0
var bottom_limit = -50.0
var rotation_speed = Vector3.ZERO
var material_override: StandardMaterial3D = null

func _ready():
	area_entered.connect(_on_hit)
	body_entered.connect(_on_hit)
	
	# Random tumbling
	rotation_speed = Vector3(randf_range(-15, 15), randf_range(-15, 15), randf_range(-15, 15))
	
	# Create twisted metal visuals (composed of 3 random boxes)
	var geom = CSGCombiner3D.new()
	for i in range(3):
		var box = CSGBox3D.new()
		box.size = Vector3(0.5, 0.5, 0.5)
		box.position = Vector3(randf_range(-0.3, 0.3), randf_range(-0.3, 0.3), randf_range(-0.3, 0.3))
		box.rotation = Vector3(randf_range(0, PI), randf_range(0, PI), randf_range(0, PI))
		if material_override:
			box.material = material_override
		geom.add_child(box)
	add_child(geom)
	
	# Collision shape
	var col = CollisionShape3D.new()
	var shape = BoxShape3D.new()
	shape.size = Vector3(0.6, 0.6, 0.6)
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
	if "Ship" in node.name: return # Don't hit self
	if "is_bunker" in node: return # Don't hit own bunkers
	
	if node.has_method("die"):
		node.die()
		queue_free()
	elif node.has_method("hit"):
		node.hit()
		queue_free()
