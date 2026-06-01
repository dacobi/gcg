extends Node3D

@export var speed: float = 40.0
@export var spawn_position: Vector3 = Vector3(-90, 49.375, 0)
@export var right_boundary: float = 90.0

var tardis_scene = preload("res://tardis.tscn")
var active_tardis: Node3D = null

@onready var audio_player = $TardisAudio

func _on_spawn_tardis():
	if active_tardis != null:
		return # Tardis already active

	active_tardis = tardis_scene.instantiate()
	active_tardis.position = spawn_position
	active_tardis.scale = Vector3(5, 5, 5)
	add_child(active_tardis)
	
	if audio_player:
		audio_player.play()

func _process(delta: float) -> void:
	if active_tardis != null:
		active_tardis.position.x += speed * delta
		
		if active_tardis.position.x > right_boundary:
			active_tardis.queue_free()
			active_tardis = null
			if audio_player:
				audio_player.stop()
