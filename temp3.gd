extends Node3D
func _ready():
	var barrier_scn = load("res://assets/models/extra_objects/traffic_barrier.glb")
	var inst = barrier_scn.instantiate()
	for child in inst.find_children("*", "MeshInstance3D", true, false):
		var mat = child.mesh.surface_get_material(0)
		if mat is StandardMaterial3D:
			print(child.name, " | Albedo: ", mat.albedo_color, " | Trans: ", mat.transparency)
	get_tree().quit()
