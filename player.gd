extends CharacterBody3D

signal live_lost

var alive: bool = true
var livelost: bool = false
var lives: int = 3
@export var cscore: int = 0
var hit_glow: bool = false
var _glow_timer: float = 0.0

@onready var main_geometry = get_node_or_null("Geometry")
@onready var glow_light = get_node_or_null("HitGlowLight")
@onready var glow_mesh = get_node_or_null("GlowGeometry")


func _process(delta: float) -> void:
	if hit_glow:
		_glow_timer += delta
		var is_visible = int(_glow_timer * 10.0) % 2 == 0
		if glow_light: glow_light.visible = is_visible
		if glow_mesh: glow_mesh.visible = is_visible
		# Optional: Turn off main geometry when glow is on to avoid Z-fighting
		if main_geometry: main_geometry.visible = !is_visible
	else:
		_glow_timer = 0.0
		if glow_light: glow_light.visible = false
		if glow_mesh: glow_mesh.visible = false
		if main_geometry: main_geometry.visible = true


func set_score(newScore: int) -> void:	
	cscore = newScore

func kill() -> void:
	if not alive: return
	lives = 0
	alive = false
	livelost = true
	emit_signal("live_lost")
	print("Player Hit by Invader! SETTING ALIVE=FALSE")

func hit()->void:
	if not alive: return
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
