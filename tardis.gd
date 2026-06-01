extends Area3D

signal tardis_hit

var rotation_speed = 2.0
var is_dead = false
var flash_timer = 0.0

@onready var visuals = get_node_or_null("Visuals")
@onready var spotlight_container = get_node_or_null("Visuals/Structure/Lamp/SpotLightContainer")

var orange_mat: StandardMaterial3D

func _ready():
	orange_mat = StandardMaterial3D.new()
	orange_mat.albedo_color = Color(1.0, 0.5, 0.0)
	orange_mat.emission_enabled = true
	orange_mat.emission = Color(1.0, 0.4, 0.0)
	orange_mat.emission_energy_multiplier = 8.0

func die():
	if is_dead: return
	is_dead = true
	emit_signal("tardis_hit")
	
	# Swap material on Structure
	var structure = get_node_or_null("Visuals/Structure")
	if structure:
		structure.material_override = orange_mat
	
	# Hide faces so orange shows through
	for face in ["FaceFront", "FaceBack", "FaceLeft", "FaceRight"]:
		var f = get_node_or_null("Visuals/" + face)
		if f: f.visible = false

func _process(delta: float) -> void:
	if is_dead:
		flash_timer += delta
		var structure = get_node_or_null("Visuals/Structure")
		if structure:
			# Flash between normal and invisible (or orange) every 0.1s
			if int(flash_timer * 10) % 2 == 0:
				structure.visible = true
			else:
				structure.visible = false

	# Rotate the visuals around the local Y axis (preserving the root's tilt)
	if visuals:
		visuals.rotate_object_local(Vector3.UP, rotation_speed * delta)
	
	# Rotate the spotlight independently
	if spotlight_container:
		spotlight_container.rotate_y(4.0 * delta)
