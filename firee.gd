extends Area3D

var speed = 20.0
var dead = false
var hits_remaining = 3
var shrapnel_script = preload("res://shrapnel.gd")

func _init():
	set_process(true)

func _ready():
	area_entered.connect(_on_hit)
	body_entered.connect(_on_hit)

func _process(delta):
	position.y -= speed * delta
	if position.y < -50.0:
		queue_free()

func explode():
	if dead: return
	dead = true
	
	var game_root = get_parent()
	if !game_root:
		queue_free()
		return
		
	var count = randi_range(1, 4)
	for i in range(count):
		var s = Area3D.new()
		s.set_script(shrapnel_script)
		# Eject in various directions
		s.velocity = Vector3(randf_range(-10, 10), randf_range(5, 15), 0.0)
		game_root.add_child(s)
		s.global_position = global_position
		
	queue_free()

func _on_hit(body: Node3D) -> void:
	if dead: return
	
	if "is_invader" in body:
		return # Don't hit your own kind!

	if "Ship" in body.name:
		if body.has_method("hit"):
			body.hit()
			dead = true
			queue_free()
			return

	if body.has_method("die") or body.has_method("hit"):
		if "is_bunker" in body:
			if body.has_method("die"):
				body.die()
			else:
				body.hit()
			
			hits_remaining -= 1
			if hits_remaining <= 0:
				explode()
		else:
			# If it hits an invader (unlikely for enemy shot) or something else
			if body.has_method("die"):
				body.die()
			else:
				body.hit()
			dead = true
			queue_free()
