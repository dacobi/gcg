extends Area3D

var speed = 40.0
var dead = false

func _init():
	set_process(true)

func _ready():
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
