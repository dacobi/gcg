extends Node3D

var speed = 10.0
var direction = 1
var boundary_x = 30.0
var drop_height = 5.0

# Firing parameters
var fire_timer = 0.0
var min_fire_delay = 0.5
var max_fire_delay = 2.0
var next_fire_time = 1.0

var projectile_scene = load("res://enemy_projectile.tscn")

func _init():
	set_process(true)

func _ready():
	_reset_fire_timer()

func _process(delta):
	# --- Movement Logic ---
	position.x += direction * speed * delta
	
	if abs(position.x) > boundary_x:
		# Clamp to boundary to prevent jitter
		position.x = boundary_x * (1 if position.x > 0 else -1)
		# Reverse direction
		direction *= -1
		# Drop down one step
		position.y -= drop_height
		# Optionally increase speed slightly
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

func _fire_random_projectile():
	var alive_invaders = []
	for child in get_children():
		# All active children of the Invaders node are invaders
		if child is Area3D:
			alive_invaders.append(child)
	
	if alive_invaders.size() > 0:
		# Pick a random surviving invader
		var shooter = alive_invaders[randi() % alive_invaders.size()]
		
		# Instance the red projectile
		var proj = projectile_scene.instantiate()
		
		# Add to the EnemyProjectiles group node (sibling of Invaders)
		var container = get_node_or_null("../EnemyProjectiles")
		if container:
			container.add_child(proj)
			# Set position atomically
			proj.global_position = shooter.global_position
		else:
			# Fallback if container missing
			get_parent().add_child(proj)
			proj.global_position = shooter.global_position
