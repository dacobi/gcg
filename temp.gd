extends Node3D

func _ready():
	if ClassDB.class_exists("FmodServer"):
		var evs = FmodServer.get_all_event_descriptions()
		for e in evs:
			var path = ""
			if e.has_method("get_path"):
				path = e.get_path()
			else:
				path = str(e)
			print("EVENT_PATH: ", path)
	
	await get_tree().create_timer(1.0).timeout
	get_tree().quit()
