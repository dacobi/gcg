extends SceneTree
func _init():
	var curve = Curve3D.new()
	curve.add_point(Vector3(0,0,0))
	curve.add_point(Vector3(1,0,0))
	print(curve.has_method("sample_baked_with_rotation"))
	quit()
