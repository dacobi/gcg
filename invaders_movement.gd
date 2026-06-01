extends Node3D

signal new_score
signal game_won
signal spawn_tardis

var speed = 2.0 # Steps per second
var direction = 1
var boundary_x = 30.0

var step_timer = 0.0
var step_x_size = 5.5
var step_y_size = 5.0
var drop_count = 0
var tardis_destroyed = false

# Firing parameters (Starting at double the previous rate)
var fire_timer = 0.0
var min_fire_delay = 0.25
var max_fire_delay = 1.0
var next_fire_time = 0.5

# Difficulty scaling over time
var speed_increase_per_sec = 0.05
var fire_delay_multiplier_per_sec = 0.99 # Multiply delays by this every second

var projectile_scene = load("res://enemy_projectile.tscn")

@export var vaders: int = 50
@export var bAdvance: bool = true
var bNewScore: bool = false
@export var level: int = 1
@export var score: int = 0

@onready var march_player = $MarchPlayer
@onready var explosion_player = $ExplosionPlayer


func _init():
	set_process(true)
	

func _ready():
	_reset_fire_timer()

func _process(delta):
	# Prevent massive delta spikes from breaking movement
	delta = min(delta, 0.1)
	var level_scale = pow(1.2, level - 1)

	# --- Difficulty Scaling ---
	speed += speed_increase_per_sec * delta * level_scale
	min_fire_delay *= pow(fire_delay_multiplier_per_sec, delta * level_scale)
	max_fire_delay *= pow(fire_delay_multiplier_per_sec, delta * level_scale)

	# --- Movement Logic (Stepped) ---
	if bAdvance:
		step_timer += delta * level_scale
		var step_delay = 1.0 / speed
		
		if step_timer >= step_delay:
			step_timer -= step_delay # Preserve overflow
			
			var next_x = position.x + (direction * step_x_size)
			var boundary_hit = false
			
			if direction == 1 and next_x > boundary_x:
				boundary_hit = true
			elif direction == -1 and next_x < -boundary_x:
				boundary_hit = true
				
			if boundary_hit:
				position.y -= step_y_size
				direction *= -1
				speed += 0.05 * delta * level_scale # Speed boost on drop
				drop_count += 1
				if drop_count >= 5:
					if not tardis_destroyed:
						emit_signal("spawn_tardis")
					drop_count = 0
			else:
				position.x = next_x
			
			# Trigger sound exactly on move
			if march_player:
				march_player.play()

	# --- Firing Logic ---
	if bAdvance:
		fire_timer += delta * level_scale
		if fire_timer >= next_fire_time:
			_fire_random_projectile()
			_reset_fire_timer()

func _reset_fire_timer():
	fire_timer = 0.0
	# Random delay between shots
	next_fire_time = randf() * (max_fire_delay - min_fire_delay) + min_fire_delay

func vaderdie(height):
	if explosion_player:
		explosion_player.play()
	vaders = vaders - 1
	# print("vaders: ", vaders)
	score = score + (6/speed) * level * height
	
	var root_node = get_tree().root
	if !root_node:
		print("no root")

	var my_game = root_node.find_child("SpaceInvadersGame", true, false) 
	
	if !my_game:
		print("no game")

	var my_ship = my_game.find_child("Ship", true, false)
	
	if !my_ship:
		print("no ship")
	
	my_ship.set_score(score)
	
	emit_signal("new_score")
	if vaders <= 0:
		emit_signal("game_won")

func tardis_hit():
	tardis_destroyed = true
	var level_scale = pow(1.33, level - 1)
	score = score + int(1500 * level_scale)
	
	var root_node = get_tree().root
	if !root_node:
		print("no root")

	var my_game = root_node.find_child("SpaceInvadersGame", true, false) 
	
	if !my_game:
		print("no game")

	var my_ship = my_game.find_child("Ship", true, false)
	
	if !my_ship:
		print("no ship")
	else:
		my_ship.set_score(score)
		if my_ship.has_method("reset_lives"):
			my_ship.reset_lives()
	
	emit_signal("new_score")

func clear_all_projectiles():
	_do_clear_projectiles.call_deferred()

func _do_clear_projectiles():
	var root_node = get_tree().root
	var my_game = root_node.find_child("SpaceInvadersGame", true, false)
	if my_game:
		var enemy_projs = my_game.find_child("EnemyProjectiles", true, false)
		if enemy_projs:
			enemy_projs.name = "OldEnemyProjectiles"
			enemy_projs.queue_free()
			var new_enemy_projs = Node3D.new()
			new_enemy_projs.name = "EnemyProjectiles"
			my_game.add_child(new_enemy_projs)
			
		var player_projs = my_game.find_child("Projectiles", true, false)
		if player_projs:
			player_projs.name = "OldProjectiles"
			player_projs.queue_free()
			var new_player_projs = Node3D.new()
			new_player_projs.name = "Projectiles"
			my_game.add_child(new_player_projs)

func _fire_random_projectile():
	var alive_invaders = []
	for child in get_children():
		if child is Area3D:
			alive_invaders.append(child)
	
	if alive_invaders.size() > 0:
		var shooter = alive_invaders[randi() % alive_invaders.size()]
		var proj = projectile_scene.instantiate()
		var container = get_node_or_null("../EnemyProjectiles")
		if container:
			container.add_child(proj)
			proj.global_position = shooter.global_position
		else:
			get_parent().add_child(proj)
			proj.global_position = shooter.global_position
