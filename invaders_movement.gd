extends Node3D

signal new_score
signal game_won

var speed = 10.0
var direction = 1
var boundary_x = 30.0
var drop_height = 5.0

var march_timer = 0.0

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
@onready var march_timer_node = $MarchTimer


func _init():
	set_process(true)
	

func _ready():
	_reset_fire_timer()
	var march_delay = 8.0 / speed
	march_timer_node.wait_time = march_delay
	march_timer_node.timeout.connect(_on_march_timer_timeout)

func _process(delta):
	# Prevent massive delta spikes from breaking movement
	delta = min(delta, 0.1)
	var level_scale = float(level)

	# --- Difficulty Scaling ---
	speed += speed_increase_per_sec * delta * level_scale
	min_fire_delay *= pow(fire_delay_multiplier_per_sec, delta * level_scale)
	max_fire_delay *= pow(fire_delay_multiplier_per_sec, delta * level_scale)

	# --- Movement Logic ---
	var step = direction * speed * delta
	# Hard cap the maximum movement per frame to prevent boundary tunneling or infinite bounce loops
	if step > 2.0: step = 2.0
	elif step < -2.0: step = -2.0

	position.x += step

	var over_boundary = false
	if position.x > boundary_x:
		position.x = boundary_x
		if direction == 1:
			direction = -1
			over_boundary = true
	elif position.x < -boundary_x:
		position.x = -boundary_x
		if direction == -1:
			direction = 1
			over_boundary = true

	if over_boundary and bAdvance:
		# Drop down one step
		position.y -= drop_height
		# Classic drop speed boost, scaled by level
		speed += 0.5 * level_scale

	# --- Firing Logic ---
	if bAdvance:
		fire_timer += delta * level_scale
		if fire_timer >= next_fire_time:
			_fire_random_projectile()
			_reset_fire_timer()
	

	# Dynamically update the timer's speed as the game gets faster
	


	# --- Marching Sound Logic ---
	#if bAdvance:
		
		#march_timer += delta
		#print("timer: ", march_timer)
		#var march_delay = 8.0 / speed # Delay decreases as speed increases
		#print("delay: ", march_delay)
		#if march_timer >= march_delay:
			#march_timer -= march_delay # Preserve timing overflow for consistent rhythm
			#if march_player:
				#print("Play")
				#march_player.play()

# Connect the MarchTimer's "timeout" signal to this function
func _on_march_timer_timeout():
	print("timeout")
	if bAdvance and march_player:
		march_player.play()
	var march_delay = 8.0 / speed
	if march_timer_node.wait_time != march_delay:
		march_timer_node.wait_time = march_delay

func _reset_fire_timer():
	fire_timer = 0.0
	# Random delay between shots
	next_fire_time = randf() * (max_fire_delay - min_fire_delay) + min_fire_delay

func vaderdie(height):
	vaders = vaders - 1
	# print("vaders: ", vaders)
	score = score + (speed / 10) * level * height
	
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
