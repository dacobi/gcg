extends Node

func _ready():
	var scene = load("res://megaracer_car_rigid.tscn").instantiate()
	scene.name = "SuperCar"
	scene.set_script(load("res://lemans_car.gd"))
	
	# Delete all visual CSG children
	var nodes_to_delete = []
	for child in scene.get_children():
		if child is CSGShape3D or child is CSGCombiner3D:
			nodes_to_delete.append(child)
	for child in nodes_to_delete:
		child.queue_free()
		scene.remove_child(child)
		
	# Create materials
	var mat_body = StandardMaterial3D.new()
	mat_body.albedo_color = Color(0.02, 0.08, 0.45) # Dark Cobalt Blue
	mat_body.metallic = 1.0
	mat_body.roughness = 0.05
	mat_body.clearcoat_enabled = true
	mat_body.clearcoat = 1.0
	mat_body.clearcoat_roughness = 0.05
	
	# Create a CSGCombiner3D to hold the sweep and the subtraction boxes
	var combiner = CSGCombiner3D.new()
	combiner.name = "LeMansCombiner"
	scene.add_child(combiner)
	combiner.owner = scene
	
	# --- MAIN SWEEP ---
	var path = Path3D.new()
	path.name = "MainSpine"
	var curve = Curve3D.new()
	
	# Sweeping from Back to Front
	# The nose and tail dive deep underground (Y=-1.0) so the subtraction box slices them perfectly into an aerodynamic wedge!
	curve.add_point(Vector3(0, -1.0, 3.3) * 1.25, Vector3(0, 0, 0.5) * 1.25, Vector3(0, 0, -1.0) * 1.25)  # Deep Tail (Shrunk by 0.5m, Scaled 25%)
	curve.add_point(Vector3(0, 0.1, 2.0) * 1.25, Vector3(0, 0, 0.5) * 1.25, Vector3(0, 0, -1.0) * 1.25)   # Rear deck (Shrunk by 0.5m, Scaled 25%)
	curve.add_point(Vector3(0, 0.6, 0.2) * 1.25, Vector3(0, 0, 1.0) * 1.25, Vector3(0, 0, -1.0) * 1.25)   # Canopy (Scaled 25%)
	curve.add_point(Vector3(0, 0.1, -1.5) * 1.25, Vector3(0, 0, 1.0) * 1.25, Vector3(0, 0, -1.0) * 1.25)  # Hood slope (Scaled 25%)
	curve.add_point(Vector3(0, -1.0, -3.5) * 1.25, Vector3(0, 0, 1.0) * 1.25, Vector3(0, 0, -0.5) * 1.25) # Deep Nose (Scaled 25%)
	
	path.curve = curve
	combiner.add_child(path)
	path.owner = scene
	
	var body = CSGPolygon3D.new()
	body.name = "MainBody"
	combiner.add_child(body)
	body.owner = scene
	
	body.mode = CSGPolygon3D.MODE_PATH
	body.path_node = body.get_path_to(path)
	body.path_rotation = CSGPolygon3D.PATH_ROTATION_PATH_FOLLOW
	body.path_interval = 0.05
	body.smooth_faces = true
	body.path_local = true
	body.material = mat_body
	
	# Full-width cross section with EXTREMELY deep side walls going underground
	var scaled_poly = PackedVector2Array()
	var original_poly = [
		Vector2(-1.1, -2.0),   # Deep left bottom
		Vector2(1.1, -2.0)     # Deep right bottom
	]
	
	# Generate 32 perfectly smooth points for the elliptical roof canopy!
	for i in range(33):
		var t = float(i) / 32.0
		var angle = t * PI # Sweeps from 0 (Right) to PI (Left)
		var px = 1.1 * cos(angle)
		var py = 0.3 * sin(angle)
		original_poly.append(Vector2(px, py))
		
	for p in original_poly:
		scaled_poly.append(p * 1.25)
	body.polygon = scaled_poly
	
	# --- FENDER FLARES (SOLID) ---
	var fender_fl = CSGCylinder3D.new()
	fender_fl.name = "FenderFL"
	fender_fl.operation = CSGShape3D.OPERATION_UNION
	fender_fl.radius = 0.65 # Slightly larger than the cutout to create a shell
	fender_fl.height = 0.75 # Extended deeper into the narrow hood
	fender_fl.sides = 64
	fender_fl.rotation_degrees = Vector3(0, 0, 90)
	fender_fl.position = Vector3(-0.925, -0.125, -1.7) # Shifted inward to maintain outer edge
	fender_fl.material = mat_body
	combiner.add_child(fender_fl)
	fender_fl.owner = scene
	
	var fender_fr = fender_fl.duplicate()
	fender_fr.name = "FenderFR"
	fender_fr.position = Vector3(0.925, -0.125, -1.7)
	combiner.add_child(fender_fr)
	fender_fr.owner = scene
	
	var fender_rl = fender_fl.duplicate()
	fender_rl.name = "FenderRL"
	fender_rl.height = 0.5 # Revert to normal width for rear
	fender_rl.position = Vector3(-1.05, -0.125, 1.75)
	combiner.add_child(fender_rl)
	fender_rl.owner = scene
	
	var fender_rr = fender_fl.duplicate()
	fender_rr.name = "FenderRR"
	fender_rr.height = 0.5 # Revert to normal width for rear
	fender_rr.position = Vector3(1.05, -0.125, 1.75)
	combiner.add_child(fender_rr)
	fender_rr.owner = scene
	
	# --- SUBTRACTION BOX (SLICES BOTTOM) ---
	var cutter = CSGBox3D.new()
	cutter.name = "BottomCutter"
	cutter.operation = CSGShape3D.OPERATION_SUBTRACTION
	cutter.size = Vector3(4.0, 10.0, 12.0) # Much deeper and longer
	# Top edge of cutter is at Y = -0.4 (lowered floor of the chassis)
	# Center is at -5.4, height is 10.0, so top edge is -0.4
	cutter.position = Vector3(0, -5.4, 0)
	combiner.add_child(cutter)
	cutter.owner = scene
	
	# --- WHEEL WELL CUTTERS (ROUNDED ARCHES) ---
	var wheel_cut_fl = CSGCylinder3D.new()
	wheel_cut_fl.name = "WheelCutFL"
	wheel_cut_fl.operation = CSGShape3D.OPERATION_SUBTRACTION
	wheel_cut_fl.radius = 0.605 # Even larger radius to match the massive car body
	wheel_cut_fl.height = 1.0
	wheel_cut_fl.sides = 64 # High-poly for perfectly smooth arches!
	wheel_cut_fl.rotation_degrees = Vector3(0, 0, 90)
	wheel_cut_fl.position = Vector3(-1.3, -0.125, -1.7)
	combiner.add_child(wheel_cut_fl)
	wheel_cut_fl.owner = scene
	
	var wheel_cut_fr = wheel_cut_fl.duplicate()
	wheel_cut_fr.name = "WheelCutFR"
	wheel_cut_fr.position = Vector3(1.3, -0.125, -1.7)
	combiner.add_child(wheel_cut_fr)
	wheel_cut_fr.owner = scene
	
	var wheel_cut_rl = wheel_cut_fl.duplicate()
	wheel_cut_rl.name = "WheelCutRL"
	wheel_cut_rl.position = Vector3(-1.3, -0.125, 1.75)
	combiner.add_child(wheel_cut_rl)
	wheel_cut_rl.owner = scene
	
	var wheel_cut_rr = wheel_cut_fl.duplicate()
	wheel_cut_rr.name = "WheelCutRR"
	wheel_cut_rr.position = Vector3(1.3, -0.125, 1.75)
	combiner.add_child(wheel_cut_rr)
	wheel_cut_rr.owner = scene
	
	# --- WINDOW CUTOUTS (RECESSED GLASS) ---
	var mat_glass = StandardMaterial3D.new()
	mat_glass.albedo_color = Color(0.05, 0.05, 0.05) # Pitch black tint
	mat_glass.roughness = 0.05 # Extremely glossy
	mat_glass.metallic = 0.9 # Highly reflective
	
	# Front Windshield
	var wind_front = CSGBox3D.new()
	wind_front.name = "WindowFront"
	wind_front.operation = CSGShape3D.OPERATION_SUBTRACTION
	wind_front.material = mat_glass
	wind_front.size = Vector3(1.4, 0.2, 1.2) # 1.4 wide, 0.2 thick (cuts shallow recess), 1.2 long
	wind_front.rotation_degrees = Vector3(18, 0, 0) # Angled to match the hood sweep
	wind_front.position = Vector3(0, 0.55, -0.7) # Positioned right on the front canopy slope
	combiner.add_child(wind_front)
	wind_front.owner = scene
	
	# Side Window Right
	var wind_right = CSGBox3D.new()
	wind_right.name = "WindowRight"
	wind_right.operation = CSGShape3D.OPERATION_SUBTRACTION
	wind_right.material = mat_glass
	wind_right.size = Vector3(0.2, 0.4, 1.2) # 0.2 thick, 0.4 high, 1.2 long
	wind_right.rotation_degrees = Vector3(0, 0, 20) # Angled to match the shoulder curve
	wind_right.position = Vector3(0.9, 0.5, 0.1) # Positioned on the right side of canopy
	combiner.add_child(wind_right)
	wind_right.owner = scene
	
	# Side Window Left
	var wind_left = wind_right.duplicate()
	wind_left.name = "WindowLeft"
	wind_left.rotation_degrees = Vector3(0, 0, -20)
	wind_left.position = Vector3(-0.9, 0.5, 0.1)
	combiner.add_child(wind_left)
	wind_left.owner = scene
	
	# --- BACK SPOILER ---
	var mat_spoiler = StandardMaterial3D.new()
	mat_spoiler.albedo_color = Color(0.08, 0.08, 0.08) # Carbon Black
	mat_spoiler.metallic = 0.3
	mat_spoiler.roughness = 0.7

	# Left Support (tilted backwards 22.5 degrees, lengthened to connect trunk to raised wing)
	var support_l = CSGBox3D.new()
	support_l.name = "SpoilerSupportL"
	support_l.material = mat_spoiler
	support_l.size = Vector3(0.05, 0.81, 0.2)
	support_l.position = Vector3(-0.7, 0.395, 2.545)
	support_l.rotation_degrees = Vector3(22.5, 0, 0)
	combiner.add_child(support_l)
	support_l.owner = scene

	# Right Support
	var support_r = support_l.duplicate()
	support_r.name = "SpoilerSupportR"
	support_r.position = Vector3(0.7, 0.395, 2.545)
	combiner.add_child(support_r)
	support_r.owner = scene

	# Main Wing Blade (raised 15cm to Y=0.77)
	var wing = CSGBox3D.new()
	wing.name = "SpoilerWing"
	wing.material = mat_spoiler
	wing.size = Vector3(2.4, 0.05, 0.4)
	wing.position = Vector3(0.0, 0.77, 2.7)
	wing.rotation_degrees = Vector3(-5, 0, 0) # Small angle of attack
	combiner.add_child(wing)
	wing.owner = scene

	# Left Endplate (trapezoidal, small height 0.24m (80% of back height 0.30m) at front)
	var endplate_l = CSGPolygon3D.new()
	endplate_l.name = "SpoilerEndplateL"
	endplate_l.material = mat_spoiler
	endplate_l.mode = CSGPolygon3D.MODE_DEPTH
	endplate_l.depth = 0.05
	endplate_l.polygon = PackedVector2Array([
		Vector2(-0.25, -0.12),  # Front-Bottom (small vertical height: 0.24m)
		Vector2(-0.25, 0.12),   # Front-Top
		Vector2(0.25, 0.15),    # Back-Top (large vertical height: 0.30m)
		Vector2(0.25, -0.15)    # Back-Bottom
	])
	endplate_l.transform = Transform3D(Basis.from_euler(Vector3(0, deg_to_rad(-90), 0)), Vector3(-1.2, 0.77, 2.7))
	combiner.add_child(endplate_l)
	endplate_l.owner = scene

	# Right Endplate
	var endplate_r = endplate_l.duplicate()
	endplate_r.name = "SpoilerEndplateR"
	endplate_r.transform = Transform3D(Basis.from_euler(Vector3(0, deg_to_rad(-90), 0)), Vector3(1.2, 0.77, 2.7))
	combiner.add_child(endplate_r)
	endplate_r.owner = scene

	# --- HEADLIGHTS ---
	# --- HEADLIGHTS ---
	var mat_chrome = StandardMaterial3D.new()
	mat_chrome.albedo_color = Color(0.9, 0.9, 0.9)
	mat_chrome.metallic = 1.0
	mat_chrome.roughness = 0.05

	var mat_headlight_glass = StandardMaterial3D.new()
	mat_headlight_glass.albedo_color = Color(0.7, 0.85, 1.0, 0.55) # Clearer tinted blue
	mat_headlight_glass.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	mat_headlight_glass.roughness = 0.02 # High gloss
	mat_headlight_glass.metallic = 0.9 # High reflection

	var mat_bulb = StandardMaterial3D.new()
	mat_bulb.albedo_color = Color(1.0, 1.0, 0.9)
	mat_bulb.emission_enabled = true
	mat_bulb.emission = Color(1.0, 1.0, 0.8) # Warm white glow
	mat_bulb.emission_energy_multiplier = 5.0

	# Wedge polygon pointing forward (tall flat front vertical face, extended 2x backwards to 0.60m)
	var poly_pts = PackedVector2Array([
		Vector2(0.30, -0.1),     # Front-Bottom
		Vector2(0.30, 0.15),     # Front-Top (taller vertical face)
		Vector2(0.15, 0.18),     # Convex curve sloping up
		Vector2(0.0, 0.19),      # Convex curve sloping up
		Vector2(-0.15, 0.20),    # Convex curve sloping up
		Vector2(-0.30, 0.20),    # Back-Top (tall back face)
		Vector2(-0.30, -0.1)     # Back-Bottom
	])

	# Scaled down polygon points for hollowing out the interior (leaving 0.02m walls)
	var inner_pts = PackedVector2Array([
		Vector2(0.28, -0.08),
		Vector2(0.28, 0.13),
		Vector2(0.14, 0.16),
		Vector2(0.0, 0.17),
		Vector2(-0.14, 0.18),
		Vector2(-0.28, 0.18),
		Vector2(-0.28, -0.08)
	])

	# Left Headlight Assembly Transforms (shifted 10cm down to Y=0.05 and 2cm forward to Z=-2.74)
	var trans_l = Transform3D(Basis(), Vector3(-0.8, 0.05, -2.74))

	var light_l_housing = CSGPolygon3D.new()
	light_l_housing.name = "HeadlightL_Housing"
	light_l_housing.mode = CSGPolygon3D.MODE_DEPTH
	light_l_housing.depth = 0.3
	light_l_housing.polygon = poly_pts
	light_l_housing.material = mat_body
	light_l_housing.transform = trans_l * Transform3D(Basis.from_euler(Vector3(0, deg_to_rad(-90), 0)), Vector3(-0.15, 0, 0))
	combiner.add_child(light_l_housing)
	light_l_housing.owner = scene

	var light_l_cavity = CSGPolygon3D.new()
	light_l_cavity.name = "HeadlightL_Cavity"
	light_l_cavity.operation = CSGShape3D.OPERATION_SUBTRACTION
	light_l_cavity.mode = CSGPolygon3D.MODE_DEPTH
	light_l_cavity.depth = 0.26
	light_l_cavity.polygon = inner_pts
	light_l_cavity.material = mat_chrome
	light_l_cavity.transform = trans_l * Transform3D(Basis.from_euler(Vector3(0, deg_to_rad(-90), 0)), Vector3(-0.13, 0, 0))
	combiner.add_child(light_l_cavity)
	light_l_cavity.owner = scene

	var light_l_cutter = CSGBox3D.new()
	light_l_cutter.name = "HeadlightL_Cutter"
	light_l_cutter.operation = CSGShape3D.OPERATION_SUBTRACTION
	light_l_cutter.material = mat_chrome
	light_l_cutter.size = Vector3(0.24, 0.18, 0.10)
	light_l_cutter.transform = trans_l * Transform3D(Basis(), Vector3(0.0, 0.04, -0.30))
	combiner.add_child(light_l_cutter)
	light_l_cutter.owner = scene

	var light_l_glass = MeshInstance3D.new()
	light_l_glass.name = "HeadlightL_Glass"
	var glass_mesh_l = BoxMesh.new()
	glass_mesh_l.size = Vector3(0.24, 0.18, 0.015)
	light_l_glass.mesh = glass_mesh_l
	light_l_glass.material_override = mat_headlight_glass
	light_l_glass.transform = trans_l * Transform3D(Basis(), Vector3(0.0, 0.04, -0.29))
	scene.add_child(light_l_glass)
	light_l_glass.owner = scene

	# Glowing Bulb mesh Left
	var bulb_l = MeshInstance3D.new()
	bulb_l.name = "HeadlightL_Bulb"
	var bulb_mesh_l = SphereMesh.new()
	bulb_mesh_l.radius = 0.03
	bulb_mesh_l.height = 0.06
	bulb_l.mesh = bulb_mesh_l
	bulb_l.material_override = mat_bulb
	bulb_l.transform = trans_l * Transform3D(Basis(), Vector3(0.0, 0.05, -0.15))
	scene.add_child(bulb_l)
	bulb_l.owner = scene

	var spotlight_l = SpotLight3D.new()
	spotlight_l.name = "HeadlightL_SpotLight"
	spotlight_l.light_color = Color(1.0, 0.95, 0.9)
	spotlight_l.light_energy = 12.0
	spotlight_l.spot_range = 60.0
	spotlight_l.spot_angle = 35.0
	spotlight_l.transform = trans_l * Transform3D(Basis.from_euler(Vector3(deg_to_rad(-5), 0, 0)), Vector3(0.0, 0.05, -0.15))
	scene.add_child(spotlight_l)
	spotlight_l.owner = scene

	# Right Headlight Assembly Transforms (shifted 10cm down to Y=0.05 and 2cm forward to Z=-2.74)
	var trans_r = Transform3D(Basis(), Vector3(0.8, 0.05, -2.74))

	var light_r_housing = CSGPolygon3D.new()
	light_r_housing.name = "HeadlightR_Housing"
	light_r_housing.mode = CSGPolygon3D.MODE_DEPTH
	light_r_housing.depth = 0.3
	light_r_housing.polygon = poly_pts
	light_r_housing.material = mat_body
	light_r_housing.transform = trans_r * Transform3D(Basis.from_euler(Vector3(0, deg_to_rad(-90), 0)), Vector3(-0.15, 0, 0))
	combiner.add_child(light_r_housing)
	light_r_housing.owner = scene

	var light_r_cavity = CSGPolygon3D.new()
	light_r_cavity.name = "HeadlightR_Cavity"
	light_r_cavity.operation = CSGShape3D.OPERATION_SUBTRACTION
	light_r_cavity.mode = CSGPolygon3D.MODE_DEPTH
	light_r_cavity.depth = 0.26
	light_r_cavity.polygon = inner_pts
	light_r_cavity.material = mat_chrome
	light_r_cavity.transform = trans_r * Transform3D(Basis.from_euler(Vector3(0, deg_to_rad(-90), 0)), Vector3(-0.13, 0, 0))
	combiner.add_child(light_r_cavity)
	light_r_cavity.owner = scene

	var light_r_cutter = CSGBox3D.new()
	light_r_cutter.name = "HeadlightR_Cutter"
	light_r_cutter.operation = CSGShape3D.OPERATION_SUBTRACTION
	light_r_cutter.material = mat_chrome
	light_r_cutter.size = Vector3(0.24, 0.18, 0.10)
	light_r_cutter.transform = trans_r * Transform3D(Basis(), Vector3(0.0, 0.04, -0.30))
	combiner.add_child(light_r_cutter)
	light_r_cutter.owner = scene

	var light_r_glass = MeshInstance3D.new()
	light_r_glass.name = "HeadlightR_Glass"
	var glass_mesh_r = BoxMesh.new()
	glass_mesh_r.size = Vector3(0.24, 0.18, 0.015)
	light_r_glass.mesh = glass_mesh_r
	light_r_glass.material_override = mat_headlight_glass
	light_r_glass.transform = trans_r * Transform3D(Basis(), Vector3(0.0, 0.04, -0.29))
	scene.add_child(light_r_glass)
	light_r_glass.owner = scene

	# Glowing Bulb mesh Right
	var bulb_r = MeshInstance3D.new()
	bulb_r.name = "HeadlightR_Bulb"
	var bulb_mesh_r = SphereMesh.new()
	bulb_mesh_r.radius = 0.03
	bulb_mesh_r.height = 0.06
	bulb_r.mesh = bulb_mesh_r
	bulb_r.material_override = mat_bulb
	bulb_r.transform = trans_r * Transform3D(Basis(), Vector3(0.0, 0.05, -0.15))
	scene.add_child(bulb_r)
	bulb_r.owner = scene

	var spotlight_r = SpotLight3D.new()
	spotlight_r.name = "HeadlightR_SpotLight"
	spotlight_r.light_color = Color(1.0, 0.95, 0.9)
	spotlight_r.light_energy = 12.0
	spotlight_r.spot_range = 60.0
	spotlight_r.spot_angle = 35.0
	spotlight_r.transform = trans_r * Transform3D(Basis.from_euler(Vector3(deg_to_rad(-5), 0, 0)), Vector3(0.0, 0.05, -0.15))
	scene.add_child(spotlight_r)
	spotlight_r.owner = scene

	# --- TAILLIGHTS ---
	var mat_taillight = StandardMaterial3D.new()
	mat_taillight.albedo_color = Color(1.0, 0.2, 0.4)
	mat_taillight.emission_enabled = true
	mat_taillight.emission = Color(1.0, 0.1, 0.2) # Glowing Magenta/Red
	mat_taillight.emission_energy_multiplier = 4.0

	var w_third = 2.0 / 3.0

	# Left third (taillight)
	var taillight_l = CSGBox3D.new()
	taillight_l.name = "TaillightBarLeft"
	taillight_l.material = mat_taillight
	taillight_l.size = Vector3(w_third, 0.04, 0.05)
	taillight_l.position = Vector3(-w_third, -0.1, 3.3)
	combiner.add_child(taillight_l)
	taillight_l.owner = scene

	# Right third (taillight)
	var taillight_r_bar = CSGBox3D.new()
	taillight_r_bar.name = "TaillightBarRight"
	taillight_r_bar.material = mat_taillight
	taillight_r_bar.size = Vector3(w_third, 0.04, 0.05)
	taillight_r_bar.position = Vector3(w_third, -0.1, 3.3)
	combiner.add_child(taillight_r_bar)
	taillight_r_bar.owner = scene

	# Middle third (body color)
	var taillight_m = CSGBox3D.new()
	taillight_m.name = "TaillightBarMiddle"
	taillight_m.material = mat_body
	taillight_m.size = Vector3(w_third, 0.04, 0.05)
	taillight_m.position = Vector3(0.0, -0.1, 3.3)
	combiner.add_child(taillight_m)
	taillight_m.owner = scene

	# Two connector bars connecting the middle third to the body
	var conn_l = CSGBox3D.new()
	conn_l.name = "TaillightConnectorLeft"
	conn_l.material = mat_body
	conn_l.size = Vector3(0.05, 0.04, 0.3)
	conn_l.position = Vector3(-0.16, -0.1, 3.15)
	combiner.add_child(conn_l)
	conn_l.owner = scene

	var conn_r = CSGBox3D.new()
	conn_r.name = "TaillightConnectorRight"
	conn_r.material = mat_body
	conn_r.size = Vector3(0.05, 0.04, 0.3)
	conn_r.position = Vector3(0.16, -0.1, 3.15)
	combiner.add_child(conn_r)
	conn_r.owner = scene
	
	# Find front wheel pivots and replace wheels with rear wheels
	var fr = scene.get_node_or_null("FrontRightSteerPivot")
	var fl = scene.get_node_or_null("FrontLeftSteerPivot")
	var rr = scene.get_node_or_null("RearRightSteerPivot")
	var rl = scene.get_node_or_null("RearLeftSteerPivot")
	
	if fr and fl and rr and rl:
		var old_fr_wheel = fr.get_node_or_null("FrontRightWheel")
		var old_fl_wheel = fl.get_node_or_null("FrontLeftWheel")
		if old_fr_wheel:
			fr.remove_child(old_fr_wheel)
			old_fr_wheel.queue_free()
		if old_fl_wheel:
			fl.remove_child(old_fl_wheel)
			old_fl_wheel.queue_free()
			
		var new_fr = rr.get_node("RearRightWheel").duplicate()
		new_fr.name = "FrontRightWheel"
		fr.add_child(new_fr)
		
		var new_fl = rl.get_node("RearLeftWheel").duplicate()
		new_fl.name = "FrontLeftWheel"
		fl.add_child(new_fl)
		
		# Recursively set owner
		_set_owner(new_fr, scene)
		_set_owner(new_fl, scene)
		
	var err = PackedScene.new()
	err.pack(scene)
	ResourceSaver.save(err, "res://lemans_car.tscn")
	print("Saved subtracted lemans_car.tscn permanently to disk!")
	get_tree().quit()

func _set_owner(node, root):
	node.owner = root
	for child in node.get_children():
		_set_owner(child, root)
