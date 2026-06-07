extends Area3D

var is_bunker = true
var is_dead = false

func _ready():
	area_entered.connect(_on_body_entered)
	body_entered.connect(_on_body_entered)

func _on_body_entered(body: Node) -> void:
	if is_dead: return
	# Ignore self-collisions with other adjacent pixels in the grid
	if body == self or body.get_script() == self.get_script():
		return
	die()

func die()->void:
	if is_dead: return
	is_dead = true
	var audio = get_node_or_null("../BunkerExplosions")
	if audio:
		audio.play()
	queue_free()

func hit()->void:
	die()
