extends SceneTree
func _init():
    var w = VehicleWheel3D.new()
    for p in w.get_property_list():
        print(p.name)
    quit()
