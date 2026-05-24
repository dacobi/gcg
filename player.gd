extends CharacterBody3D

func hit()->void:
	print("Player Hit!")
	queue_free()
