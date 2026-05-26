extends Area3D

func _ready():
	area_entered.connect(_on_body_entered)
	body_entered.connect(_on_body_entered)

func _on_body_entered(body: Node) -> void:
	# Ignore self-collisions with other adjacent pixels in the grid
	if body == self or body.get_script() == self.get_script():
		return
	queue_free()

func die()->void:
	queue_free()

func hit()->void:
	die()
