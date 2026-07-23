extends SceneTree

func _init():
    var methods = ClassDB.class_get_method_list("FmodServer")
    for m in methods:
        if m["name"] == "play_one_shot":
            print(m)
    quit()
