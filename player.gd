extends CharacterBody3D

signal live_lost
signal lives_reset
signal loosing

var alive: bool = true
var livelost: bool = false
var lives: int = 3
@export var cscore: int = 0
var is_hit_anim: bool = false
var hit_anim_timer: float = 0.0
var shrapnel_script = preload("res://ship_shrapnel.gd")

@onready var main_geometry = get_node_or_null("Geometry")
@onready var glow_light = get_node_or_null("HitGlowLight")
@onready var glow_mesh = get_node_or_null("GlowGeometry")
@onready var explosion_player = get_node_or_null("ExplosionPlayer")


func _process(delta: float) -> void:
	if is_hit_anim:
		hit_anim_timer += delta
		var phase = int(hit_anim_timer / 0.04) # 40ms steps
		if phase < 21: # 3 steps * 7 loops = 21 phases
			var loop_step = phase % 3
			if loop_step == 0:
				scale = Vector3(1.2, 1.2, 1.2)
			else:
				scale = Vector3(1.0, 1.0, 1.0)
			
			# Flash logic (toggle every 40ms)
			var is_glow = phase % 2 == 0
			if glow_light: glow_light.visible = is_glow
			if glow_mesh: glow_mesh.visible = is_glow
			if main_geometry: main_geometry.visible = !is_glow
		else:
			# End animation
			is_hit_anim = false
			scale = Vector3(1.0, 1.0, 1.0)
			if glow_light: glow_light.visible = false
			if glow_mesh: glow_mesh.visible = false
			if main_geometry: main_geometry.visible = true
	else:
		# Ensure normal state if not animating
		if scale != Vector3(1.0, 1.0, 1.0):
			scale = Vector3(1.0, 1.0, 1.0)
		if main_geometry and !main_geometry.visible:
			main_geometry.visible = true
		if glow_light and glow_light.visible:
			glow_light.visible = false
		if glow_mesh and glow_mesh.visible:
			glow_mesh.visible = false


func set_score(newScore: int) -> void:	
	cscore = newScore

func reset_lives() -> void:
	lives = 3
	emit_signal("lives_reset")

func invasion_kill() -> void:
	if not alive: return
	alive = false # Disable further interaction
	
	# Sequentially explode for every remaining life
	while lives > 0:
		if explosion_player: explosion_player.play()
		_spawn_explosion_shrapnel()
		
		# Trigger the visual flashing/pulsing animation
		is_hit_anim = true
		hit_anim_timer = 0.0
		
		lives -= 1
		emit_signal("loosing")
		
		# If we still have lives to "burn", wait 700ms
		if lives > 0:
			await get_tree().create_timer(0.7).timeout
	
	print("Invasion destruction complete.")

func kill() -> void:
	if not alive: return
	if explosion_player: explosion_player.play()
	_spawn_explosion_shrapnel()
	is_hit_anim = true
	hit_anim_timer = 0.0
	lives = 0
	alive = false
	livelost = true
	emit_signal("live_lost")
	print("Player Hit by Invader! SETTING ALIVE=FALSE")

func hit()->void:
	if not alive: return
	if explosion_player: explosion_player.play()
	_spawn_explosion_shrapnel()
	is_hit_anim = true
	hit_anim_timer = 0.0
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

func _spawn_explosion_shrapnel():
	var game_root = get_parent()
	if !game_root: return
	
	var ship_mat = null
	if main_geometry:
		ship_mat = main_geometry.material_override
	
	var count = 12
	for i in range(count):
		var s = Area3D.new()
		s.set_script(shrapnel_script)
		s.material_override = ship_mat
		
		# Semi-circle: angle from 0 to PI
		var angle = (float(i) / float(count - 1)) * PI
		var dir = Vector3(cos(angle), sin(angle), 0.0)
		
		# Upward expanding speed
		var force = randf_range(22.5, 37.5)
		s.velocity = dir * force
		
		game_root.add_child(s)
		s.global_position = global_position
