extends Area3D

var is_bunker = true

func _ready():
	area_entered.connect(_on_body_entered)
	body_entered.connect(_on_body_entered)

func _on_body_entered(body: Node) -> void:
	# Ignore self-collisions with other adjacent pixels in the grid
	if body == self or body.get_script() == self.get_script():
		return
	queue_free()

func die()->void:
	var audio = get_node_or_null("../BunkerExplosions")
	if audio:
		audio.play()
	queue_free()

func hit()->void:
	die()
