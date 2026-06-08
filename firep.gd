extends Area3D

var speed = 40.0
var dead = false

func _init():
	set_process(true)

func _ready():
	# Race condition fix: if the ship is hit, do not allow a laser to spawn
	var root_node = get_tree().root
	var my_game = root_node.find_child("SpaceInvadersGame", true, false)
	if my_game:
		var my_ship = my_game.find_child("Ship", true, false)
		if my_ship and "is_hit_anim" in my_ship and my_ship.is_hit_anim:
			var laser_sound = get_node_or_null("LaserSound")
			if laser_sound:
				laser_sound.stop()
			queue_free()
			return

	area_entered.connect(_on_hit)
	body_entered.connect(_on_hit)

func _process(delta):
	position.y += speed * delta
	
	if position.y > 80.0:
		queue_free()
		return

func _on_hit(body: Node3D) -> void:
	if dead: return
	if body.has_method("die"):
		body.die()
		dead = true
		queue_free()
