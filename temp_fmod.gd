extends Node3D

func _ready():
	if ClassDB.class_exists("FmodServer"):
		var methods = ClassDB.class_get_method_list("FmodServer")
		for m in methods:
			if "play" in m["name"] or "one_shot" in m["name"] or "event" in m["name"]:
				print("METHOD: ", m["name"])
				
		var evs = FmodServer.get_all_event_descriptions()
		for e in evs:
			if e.has_method("get_path"):
				print("EVENT: ", e.get_path(), " GUID: ", e.get_guid())
			else:
				print("EVENT GUID: ", e.get_guid())
	else:
		print("NO FMOD SERVER")
	
	await get_tree().create_timer(1.0).timeout
	get_tree().quit()
