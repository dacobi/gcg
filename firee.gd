extends Area3D

var speed = 20.0
var dead = false

func _init():
	set_process(true)

func _ready():
	area_entered.connect(_on_hit)
	body_entered.connect(_on_hit)

func _process(delta):
	position.y -= speed * delta

func _on_hit(body: Node3D) -> void:
	if dead: return
	if body.has_method("hit"):
		body.hit()
		dead = true
		queue_free()
