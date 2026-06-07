extends Area3D

signal tardis_hit

var rotation_speed = 2.0
var is_dead = false
var flash_timer = 0.0

@onready var visuals = get_node_or_null("Visuals")
#@onready var spotlight_container = get_node_or_null("Visuals/Structure/Lamp/SpotLightContainer")

var orange_mat: StandardMaterial3D
var is_spawning_sparks = false
var spark_timer = 0.0
var spark_script = preload("res://spark.gd")

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
	
	is_spawning_sparks = true
	
	# Swap material on Structure
	var structure = get_node_or_null("Visuals/Structure")
	if structure:
		structure.material_override = orange_mat
	
	# Hide faces so orange shows through
	for face in ["FaceFront", "FaceBack", "FaceLeft", "FaceRight"]:
		var f = get_node_or_null("Visuals/" + face)
		if f: f.visible = false

func _spawn_one_spark():
	var game_root = get_parent()
	if !game_root: return

	var spark = Area3D.new()
	spark.set_script(spark_script)
	
	# Visual geometry for the spark: A mechanical bolt
	var geom = CSGCombiner3D.new()
	var boltscale = 2.0
	# Shaft
	var shaft = CSGCylinder3D.new()
	shaft.radius = 0.15 * boltscale # 0.03 * 5
	shaft.height = 1.0 * boltscale  # 0.2 * 5
	shaft.material = orange_mat
	geom.add_child(shaft)

	# Hex Head
	var head = CSGCylinder3D.new()
	head.radius = 0.4 * boltscale  # 0.08 * 5
	head.height = 0.3 * boltscale  # 0.06 * 5
	head.sides = 6 # Hexagonal shape
	head.position.y = 0.5 # 0.1 * 5
	head.material = orange_mat
	geom.add_child(head)

	spark.add_child(geom)

	# Collision shape (larger for better coverage of bunker pixels)
	var col = CollisionShape3D.new()
	var shape = BoxShape3D.new()
	shape.size = Vector3(0.6, 0.6, 0.6)
	col.shape = shape
	spark.add_child(col)

	
	# Set velocity: Ejected backwards relative to orientation
	# rotation.y == 0 (L->R) -> back is -X
	# rotation.y == PI (R->L) -> back is +X
	var x_dir = -1.0
	if abs(rotation.y - PI) < 0.1:
		x_dir = 1.0
	
	spark.velocity = Vector3(x_dir * randf_range(5.0, 15.0), randf_range(2.0, 10.0), 0.0)
	
	game_root.add_child(spark)

	# Position at Tardis (backside relative to motion)
	spark.global_position = global_position
	# Move slightly back relative to motion (x_dir is the spark velocity direction, which is backwards)
	spark.global_translate(Vector3(x_dir * 1.5, 0.5, 0.0))
	spark.global_position.z = 0.0

func _process(delta: float) -> void:
	if is_spawning_sparks:
		if position.x < 90:
			spark_timer += delta
			if spark_timer >= 0.2: # 5 sparks per second
				_spawn_one_spark()
				spark_timer = 0.0
		else:
			is_spawning_sparks = false

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
	#if spotlight_container:
	#	spotlight_container.rotate_y(4.0 * delta)
