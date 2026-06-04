extends Area3D

func _ready():
	area_entered.connect(_on_body_entered)
	body_entered.connect(_on_body_entered)

func _on_body_entered(body: Node) -> void:
	if body == self or body.get_script() == self.get_script():
		return
	print("Bunker pixel hit by: ", body.name)
	queue_free()

func die()->void:
	queue_free()

func hit()->void:
	die()
