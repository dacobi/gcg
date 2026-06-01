extends Node3D

var rotation_speed = 2.0

@onready var visuals = get_node_or_null("Visuals")
@onready var spotlight_container = get_node_or_null("Visuals/Structure/Lamp/SpotLightContainer")

func _process(delta: float) -> void:
	# Rotate the visuals around the local Y axis (preserving the root's tilt)
	if visuals:
		visuals.rotate_object_local(Vector3.UP, rotation_speed * delta)
	
	# Rotate the spotlight independently
	if spotlight_container:
		spotlight_container.rotate_y(4.0 * delta)
