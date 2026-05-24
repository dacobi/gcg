extends CharacterBody3D

var alive: bool = true

func hit()->void:
	if not alive: return
	print("Player Hit! SETTING ALIVE=FALSE")
	alive = false
	# queue_free()
