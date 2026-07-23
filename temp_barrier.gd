extends SceneTree
func _init():
	var file = FileAccess.open("res://assets/models/extra_objects/traffic_barrier.glb", FileAccess.READ)
	file.get_buffer(12)
	var chunk_len = file.get_32()
	file.get_buffer(4)
	print(file.get_buffer(chunk_len).get_string_from_utf8().substr(0, 2000))
	quit()
