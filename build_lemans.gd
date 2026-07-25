extends Node

func _ready():
	var scene = load("res://megaracer_car_rigid.tscn").instantiate()
	scene.name = "SuperCar"
	scene.set_script(load("res://lemans_car.gd"))
	
	# Delete all visual CSG children and leftover suspension rods!
	var nodes_to_delete = []
	for child in scene.get_children():
		if child is CSGShape3D or child is CSGCombiner3D or child.name.begins_with("AxleAnchor"):
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
	
	# --- UNIFIED GLASS CABIN CORE ---
	var mat_glass = StandardMaterial3D.new()
	mat_glass.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	mat_glass.albedo_color = Color(0.05, 0.05, 0.05, 0.95) # Obsidian tint, 95% dense!
	mat_glass.roughness = 0.05 # Extremely glossy
	mat_glass.metallic = 0.9 # Highly reflective
	
	var inner_poly = PackedVector2Array()
	for p in original_poly:
		inner_poly.append(p * 1.20) # Scaled down from 1.25 to leave a flawless 5cm metal shell!

	var build_cabin_shape = func(is_glass: bool):
		var cab_comb = CSGCombiner3D.new()
		cab_comb.name = "CabinGlass" if is_glass else "CabinHole"
		cab_comb.operation = CSGCombiner3D.OPERATION_UNION if is_glass else CSGCombiner3D.OPERATION_SUBTRACTION
		
		var cab_body = CSGPolygon3D.new()
		cab_body.name = "CabinCore"
		cab_body.polygon = inner_poly
		cab_body.mode = CSGPolygon3D.MODE_PATH
		cab_body.path_rotation = CSGPolygon3D.PATH_ROTATION_PATH_FOLLOW
		cab_body.path_interval = 0.05
		cab_body.smooth_faces = true
		cab_body.path_local = true
		if is_glass:
			cab_body.material = mat_glass
		cab_comb.add_child(cab_body)
		
		# Chop off the front (hood)
		var front_chop = CSGBox3D.new()
		front_chop.operation = CSGCombiner3D.OPERATION_SUBTRACTION
		front_chop.size = Vector3(4.0, 4.0, 4.0)
		front_chop.position = Vector3(0, 0, -3.5) # Cuts off everything in front of Z = -1.5
		cab_comb.add_child(front_chop)
		
		# Chop off the back (trunk)
		var back_chop = CSGBox3D.new()
		back_chop.operation = CSGCombiner3D.OPERATION_SUBTRACTION
		back_chop.size = Vector3(4.0, 4.0, 4.0)
		back_chop.position = Vector3(0, 0, 4.0) # Cuts off everything behind Z = 2.0
		cab_comb.add_child(back_chop)
		
		# --- WHEEL WELL PROTECTORS ---
		# Subtracts massive boxes from the cabin core to completely chop out the corners!
		var w_fl = CSGBox3D.new()
		w_fl.operation = CSGCombiner3D.OPERATION_SUBTRACTION
		w_fl.size = Vector3(1.1, 1.5, 1.5) # The 1.1m Golden Box size! Forges a 5cm metal wall without touching the 0.7m glass limit!
		w_fl.position = Vector3(-1.3, -0.125, -1.7)
		cab_comb.add_child(w_fl)
		
		var w_fr = w_fl.duplicate()
		w_fr.position = Vector3(1.3, -0.125, -1.7)
		cab_comb.add_child(w_fr)
		
		var w_rl = w_fl.duplicate()
		w_rl.position = Vector3(-1.3, -0.125, 1.75)
		cab_comb.add_child(w_rl)
		
		var w_rr = w_fl.duplicate()
		w_rr.position = Vector3(1.3, -0.125, 1.75)
		cab_comb.add_child(w_rr)
		
		# Chop off the bottom (floor)
		var bot_chop = CSGBox3D.new()
		bot_chop.operation = CSGCombiner3D.OPERATION_SUBTRACTION
		bot_chop.size = Vector3(4.0, 4.0, 12.0)
		bot_chop.position = Vector3(0, -2.1, 0) # Cuts off everything below Y = -0.1 (30cm up from the floor)
		cab_comb.add_child(bot_chop)
		
		return cab_comb

	var cabin_hole = build_cabin_shape.call(false)
	combiner.add_child(cabin_hole)
	cabin_hole.owner = scene
	for c in cabin_hole.get_children():
		c.owner = scene
	cabin_hole.get_node("CabinCore").path_node = cabin_hole.get_node("CabinCore").get_path_to(path)

	var cabin_glass = build_cabin_shape.call(true)
	cabin_glass.scale = Vector3(0.99, 0.99, 0.99) # 1% smaller to eliminate Z-fighting with the metal walls!
	scene.add_child(cabin_glass)
	cabin_glass.owner = scene
	for c in cabin_glass.get_children():
		c.owner = scene
	cabin_glass.get_node("CabinCore").path_node = cabin_glass.get_node("CabinCore").get_path_to(path)
	
	# --- WINDOW CUTOUTS (HOLE PUNCHERS) ---
	# These shapes strictly slice holes through the metal body to reveal the glass core underneath!
	
	# Front Windshield Hole
	var wind_front = CSGBox3D.new()
	wind_front.name = "WindowFront"
	wind_front.operation = CSGCombiner3D.OPERATION_SUBTRACTION
	wind_front.size = Vector3(1.46, 0.8, 1.925) # Snipped final 10cm off the bottom edge!
	wind_front.rotation_degrees = Vector3(18, 0, 0)
	wind_front.position = Vector3(0, 0.55, -0.4625) # Shifted 5cm to lock top edge!
	combiner.add_child(wind_front)
	wind_front.owner = scene
	
	# Side Window Teardrop Shape
	var side_window_pts = PackedVector2Array()
	# Draw sweeping roof curve (from rear to front)
	for i in range(33):
		var t = float(i) / 32.0 # High-poly 32 segment curve!
		var px = lerpf(0.7, -0.7, t) # Extended by 10cm on both ends! (Total length 1.4m)
		var peak_x = -0.1 # Canopy peak is slightly forward
		var dist = (px - peak_x) / 0.8 if px > peak_x else (px - peak_x) / 0.6
		var py = 0.6 * (1.0 - dist * dist) # Height peaks at 0.6
		if py < 0.0: py = 0.0
		side_window_pts.append(Vector2(px, py))
	# Flat bottom edge (loop cleanly finishes at -0.7, so we just close back to 0.7)
	side_window_pts.append(Vector2(0.7, 0.0))
	
	# Side Window Right Hole
	var wind_right = CSGPolygon3D.new()
	wind_right.name = "WindowRight"
	wind_right.operation = CSGCombiner3D.OPERATION_SUBTRACTION
	wind_right.polygon = side_window_pts
	wind_right.mode = CSGPolygon3D.MODE_DEPTH
	wind_right.depth = 1.5 # Massive depth to guarantee penetration
	wind_right.rotation_degrees = Vector3(0, 90, 0) # Flipped to extrude deeply INWARDS!
	wind_right.position = Vector3(1.4, 0.3, 0.1) # Pulled out to accommodate 1.5m depth
	combiner.add_child(wind_right)
	wind_right.owner = scene
	
	# Side Window Left Hole
	var wind_left = wind_right.duplicate()
	wind_left.name = "WindowLeft"
	wind_left.rotation_degrees = Vector3(0, -90, 0) # Flipped to extrude deeply INWARDS!
	wind_left.position = Vector3(-1.4, 0.3, 0.1)
	combiner.add_child(wind_left)
	wind_left.owner = scene
	
	# Rear Window Hole (New!)
	var wind_rear = CSGBox3D.new()
	wind_rear.name = "WindowRear"
	wind_rear.operation = CSGCombiner3D.OPERATION_SUBTRACTION
	wind_rear.size = Vector3(1.46, 0.8, 1.75) # Snipped final 5cm off the bottom edge!
	wind_rear.rotation_degrees = Vector3(-18, 0, 0)
	wind_rear.position = Vector3(0, 0.45, 1.075) # Shifted 2.5cm to lock top edge!
	combiner.add_child(wind_rear)
	wind_rear.owner = scene
	
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
	spotlight_l.shadow_enabled = true
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
	spotlight_r.shadow_enabled = true
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
	taillight_l.position = Vector3(-w_third, 0.15, 3.30) # Moved up and in!
	combiner.add_child(taillight_l)
	taillight_l.owner = scene

	# Right third (taillight)
	var taillight_r_bar = CSGBox3D.new()
	taillight_r_bar.name = "TaillightBarRight"
	taillight_r_bar.material = mat_taillight
	taillight_r_bar.size = Vector3(w_third, 0.04, 0.05)
	taillight_r_bar.position = Vector3(w_third, 0.15, 3.30)
	combiner.add_child(taillight_r_bar)
	taillight_r_bar.owner = scene

	# Middle third (body color)
	var taillight_m = CSGBox3D.new()
	taillight_m.name = "TaillightBarMiddle"
	taillight_m.material = mat_body
	taillight_m.size = Vector3(w_third, 0.04, 0.05)
	taillight_m.position = Vector3(0.0, 0.15, 3.30)
	combiner.add_child(taillight_m)
	taillight_m.owner = scene

	# Two vertical connector bars dropping straight down into the diffuser panel
	var conn_l = CSGBox3D.new()
	conn_l.name = "TaillightConnectorLeft"
	conn_l.material = mat_body
	conn_l.size = Vector3(0.05, 0.10, 0.05) # 10cm tall vertical standoffs!
	conn_l.position = Vector3(-0.16, 0.10, 3.30)
	combiner.add_child(conn_l)
	conn_l.owner = scene

	var conn_r = CSGBox3D.new()
	conn_r.name = "TaillightConnectorRight"
	conn_r.material = mat_body
	conn_r.size = Vector3(0.05, 0.10, 0.05)
	conn_r.position = Vector3(0.16, 0.10, 3.30)
	combiner.add_child(conn_r)
	conn_r.owner = scene
	
	# --- EXHAUST CUTOUTS, TAILPIPES & DIFFUSER ---
	
	var mat_matte = StandardMaterial3D.new()
	mat_matte.albedo_color = Color(0.05, 0.05, 0.05)
	mat_matte.metallic = 0.0
	mat_matte.roughness = 1.0
	
	# Massive Detached Rear Diffuser Block (Rounded!)
	var diffuser_combiner = CSGCombiner3D.new()
	diffuser_combiner.name = "RearDiffuser"
	scene.add_child(diffuser_combiner)
	diffuser_combiner.owner = scene
	
	var diff_poly = CSGPolygon3D.new()
	diff_poly.name = "DiffuserPanel"
	diff_poly.mode = CSGPolygon3D.MODE_DEPTH
	diff_poly.depth = 0.8
	
	var d_pts = PackedVector2Array()
	# Draw perfectly rounded corners!
	for j in range(9):
		var angle = (float(j)/8.0) * (PI/2.0)
		d_pts.append(Vector2(0.8 + cos(angle)*0.1, 0.1 + sin(angle)*0.1))
	for j in range(9):
		var angle = PI/2.0 + (float(j)/8.0) * (PI/2.0)
		d_pts.append(Vector2(-0.8 + cos(angle)*0.1, 0.1 + sin(angle)*0.1))
	for j in range(9):
		var angle = PI + (float(j)/8.0) * (PI/2.0)
		d_pts.append(Vector2(-0.8 + cos(angle)*0.1, -0.1 + sin(angle)*0.1))
	for j in range(9):
		var angle = 3.0*PI/2.0 + (float(j)/8.0) * (PI/2.0)
		d_pts.append(Vector2(0.8 + cos(angle)*0.1, -0.1 + sin(angle)*0.1))
		
	diff_poly.polygon = d_pts
	diff_poly.position = Vector3(0.0, -0.15, 3.45) # Pulled out aggressively 20cm further!
	diff_poly.material = mat_matte
	diffuser_combiner.add_child(diff_poly)
	diff_poly.owner = scene
	
	var exhaust_idx = 1
	var build_exhaust = func(x_pos):
		# 3-Piece Pill Cutout (Shifted to cut into the protruding diffuser)
		var cut_box = CSGBox3D.new()
		cut_box.name = "CutBox" + str(exhaust_idx)
		cut_box.operation = CSGCombiner3D.OPERATION_SUBTRACTION
		cut_box.size = Vector3(0.2, 0.118, 0.8)
		cut_box.position = Vector3(x_pos, -0.12, 3.6)
		cut_box.material = mat_matte
		diffuser_combiner.add_child(cut_box)
		cut_box.owner = scene
		
		var cut_cyl_l = CSGCylinder3D.new()
		cut_cyl_l.name = "CutCylL" + str(exhaust_idx)
		cut_cyl_l.operation = CSGCombiner3D.OPERATION_SUBTRACTION
		cut_cyl_l.radius = 0.06
		cut_cyl_l.height = 0.8
		cut_cyl_l.sides = 64
		cut_cyl_l.rotation_degrees.x = 90
		cut_cyl_l.position = Vector3(x_pos - 0.1, -0.12, 3.6)
		cut_cyl_l.material = mat_matte
		diffuser_combiner.add_child(cut_cyl_l)
		cut_cyl_l.owner = scene
		
		var cut_cyl_r = CSGCylinder3D.new()
		cut_cyl_r.name = "CutCylR" + str(exhaust_idx)
		cut_cyl_r.operation = CSGCombiner3D.OPERATION_SUBTRACTION
		cut_cyl_r.radius = 0.06
		cut_cyl_r.height = 0.8
		cut_cyl_r.sides = 64
		cut_cyl_r.rotation_degrees.x = 90
		cut_cyl_r.position = Vector3(x_pos + 0.1, -0.12, 3.6)
		cut_cyl_r.material = mat_matte
		diffuser_combiner.add_child(cut_cyl_r)
		cut_cyl_r.owner = scene
		
		# Flattened Chrome Tailpipes
		var pipe_idx = 1
		for offset_x in [-0.06, 0.06]:
			var pipe_out = CSGCylinder3D.new()
			pipe_out.name = "PipeOut" + str(exhaust_idx) + "_" + str(pipe_idx)
			pipe_out.operation = CSGCombiner3D.OPERATION_UNION
			pipe_out.radius = 0.04
			pipe_out.height = 0.20 # Increased to 20cm length
			pipe_out.sides = 64
			pipe_out.rotation_degrees.x = 90
			pipe_out.position = Vector3(x_pos + offset_x, -0.12, 3.45) # Centered on the diffuser face to protrude 10cm!
			pipe_out.material = mat_chrome
			diffuser_combiner.add_child(pipe_out)
			pipe_out.owner = scene
			
			var pipe_in = CSGCylinder3D.new()
			pipe_in.name = "PipeIn" + str(exhaust_idx) + "_" + str(pipe_idx)
			pipe_in.operation = CSGCombiner3D.OPERATION_SUBTRACTION
			pipe_in.radius = 0.03
			pipe_in.height = 0.21 # Sized to puncture the outer pipe
			pipe_in.sides = 64
			pipe_in.rotation_degrees.x = 90
			pipe_in.position = Vector3(x_pos + offset_x, -0.12, 3.46)
			
			var mat_black = StandardMaterial3D.new()
			mat_black.albedo_color = Color(0, 0, 0)
			pipe_in.material = mat_black
			
			diffuser_combiner.add_child(pipe_in)
			pipe_in.owner = scene
			
			pipe_idx += 1
			
		exhaust_idx += 1
		
	# Shift exhaust panels 10cm inwards (0.666 -> 0.566)
	build_exhaust.call(-(w_third - 0.1))
	build_exhaust.call(w_third - 0.1)
	
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
