extends Node3D
func _ready():
	var barrier_scn = load("res://assets/models/extra_objects/traffic_barrier.glb")
	var inst = barrier_scn.instantiate()
	for child in inst.find_children("*", "MeshInstance3D", true, false):
		print("Child: ", child.name, " Visible: ", child.visible)
	get_tree().quit()
