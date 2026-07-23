extends Node3D
func _ready():
    if ClassDB.class_exists("FmodEvent"):
        for m in ClassDB.class_get_method_list("FmodEvent"):
            if "volume" in m["name"] or "release" in m["name"]: print("FmodEvent: ", m["name"])
    if ClassDB.class_exists("FmodEventInstance"):
        for m in ClassDB.class_get_method_list("FmodEventInstance"):
            if "volume" in m["name"] or "release" in m["name"]: print("FmodEventInstance: ", m["name"])
    await get_tree().create_timer(1.0).timeout
    get_tree().quit()
