extends Node3D

var speed = 10.0
var direction = 1
var boundary_x = 30.0
var drop_height = 5.0

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

func _init():
	set_process(true)

func _ready():
	_reset_fire_timer()

func _process(delta):
	# --- Difficulty Scaling ---
	speed += speed_increase_per_sec * delta
	min_fire_delay *= pow(fire_delay_multiplier_per_sec, delta)
	max_fire_delay *= pow(fire_delay_multiplier_per_sec, delta)

	# --- Movement Logic ---
	position.x += direction * speed * delta
	
	if abs(position.x) > boundary_x:
		# Clamp to boundary to prevent jitter
		position.x = boundary_x * (1 if position.x > 0 else -1)
		# Reverse direction
		direction *= -1
		# Drop down one step
		position.y -= drop_height
		# Classic drop speed boost
		speed += 0.5

	# --- Firing Logic ---
	fire_timer += delta
	if fire_timer >= next_fire_time:
		_fire_random_projectile()
		_reset_fire_timer()

func _reset_fire_timer():
	fire_timer = 0.0
	# Random delay between shots
	next_fire_time = randf() * (max_fire_delay - min_fire_delay) + min_fire_delay

func vaderdie():
	vaders = vaders - 1
	print("vaders: ", vaders)

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
