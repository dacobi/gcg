extends Node
func die()->void:
	queue_free()
func hit()->void:
	die()
