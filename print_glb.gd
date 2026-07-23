extends SceneTree

func _init():
    var gltf = GLTFDocument.new()
    var state = GLTFState.new()
    var err = gltf.append_from_file("res://assets/models/extra_objects/traffic_cone.glb", state)
    if err == OK:
        var root = gltf.generate_scene(state)
        print_tree(root, "")
    else:
        print("Failed to load GLB")
    quit()

func print_tree(node: Node, indent: String):
    print(indent + node.name + " (" + node.get_class() + ")")
    if node is MeshInstance3D:
        var aabb = node.mesh.get_aabb()
        print(indent + "  AABB: " + str(aabb.size))
    for child in node.get_children():
        print_tree(child, indent + "  ")
