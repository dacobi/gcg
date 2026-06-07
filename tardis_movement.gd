extends Node3D

@export var speed: float = 40.0
@export var spawn_position: Vector3 = Vector3(-90, 49.375, 0)
@export var right_boundary: float = 90.0
@export var auto_hit_test: bool = false

var tardis_scene = preload("res://tardis.tscn")
var active_tardis: Node3D = null
var current_speed: float = 0.0

@onready var audio_player = $TardisAudio
@onready var explode_player = $TardisExplode

func _on_spawn_tardis():
	if active_tardis != null:
		return # Tardis already active

	active_tardis = tardis_scene.instantiate()
	
	# Randomize direction
	if randf() > 0.5:
		# Start on right, move left
		active_tardis.position = Vector3(90, 49.375, 0)
		active_tardis.rotation.y = PI # Flip 180 degrees
		current_speed = -speed
	else:
		# Start on left, move right
		active_tardis.position = spawn_position
		active_tardis.rotation.y = 0
		current_speed = speed

	active_tardis.scale = Vector3(5, 5, 5)
	
	# Prune auxiliary nodes that shouldn't affect the main game scene
	for node_name in ["WorldEnvironment", "DirectionalLight3D", "Camera3D"]:
		var aux_node = active_tardis.get_node_or_null(node_name)
		if aux_node:
			aux_node.queue_free()
	
	# Remove any extra lights from the visuals (like the lamp's OmniLight3D)
	var extra_light = active_tardis.find_child("OmniLight3D", true, false)
	if extra_light:
		extra_light.queue_free()

	active_tardis.connect("tardis_hit", _on_tardis_hit)
	add_child(active_tardis)
	
	if audio_player:
		audio_player.play()

func _on_tardis_hit():
	if audio_player:
		audio_player.stop()
	if explode_player:
		explode_player.play()
	
	var invaders = get_node_or_null("../Invaders")
	if invaders and invaders.has_method("tardis_hit"):
		invaders.tardis_hit()

func _process(delta: float) -> void:
	if active_tardis != null:
		active_tardis.position.x += current_speed * delta
		
		# Auto-hit debug mode
		if auto_hit_test and abs(active_tardis.position.x) < 70:
			if active_tardis.has_method("die"):
				active_tardis.die()
		
		if abs(active_tardis.position.x) > 100:
			active_tardis.queue_free()
			active_tardis = null
			if audio_player:
				audio_player.stop()
