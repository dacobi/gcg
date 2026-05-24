extends Node3D
var speed = 0.0
func _init():
	set_process(true)
func _process(delta):
	position.x += speed * delta
