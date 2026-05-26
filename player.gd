extends CharacterBody3D

signal live_lost

var alive: bool = true
var livelost: bool = false
var lives: int = 3

func kill() -> void:
	if not alive: return
	lives = 0
	alive = false
	livelost = true
	emit_signal("live_lost")
	print("Player Hit by Invader! SETTING ALIVE=FALSE")

func hit()->void:
	if not alive: return
	lives -= 1
	if lives == 0:
		alive = false
		livelost = true
		emit_signal("live_lost")
		print("Player Hit! SETTING ALIVE=FALSE")
	else:
		livelost = true
		emit_signal("live_lost")
		print("Player Hit! Removing one Live")
