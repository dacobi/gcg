extends Control

var car: Node3D
var font: Font

func _ready() -> void:
	font = ThemeDB.fallback_font

func _process(_delta: float) -> void:
	queue_redraw()

func _draw() -> void:
	if not is_instance_valid(car): return
	
	var vp_size = get_viewport_rect().size
	var center_rpm = vp_size - Vector2(160, 160)
	var center_speed = vp_size - Vector2(380, 130)
	
	var speed = car.linear_velocity.length() * 3.6
	var rpm = car.get("engine_rpm")
	if rpm == null: rpm = 1000.0
	var gear_sim = car.get("current_gear_sim")
	if gear_sim == null: gear_sim = 0
	var gear_str = str(gear_sim + 1)
	if gear_sim == -1:
		gear_str = "R"
	elif gear_sim == -2:
		gear_str = "N"
	var mode_str = "[MANUAL]" if car.get("manual_transmission") else "[AUTO]"
	
	var cyan = Color(0.0, 1.0, 1.0, 1.0)
	var magenta = Color(1.0, 0.0, 1.0, 1.0)
	var dark_bg = Color(0.05, 0.0, 0.1, 0.9)
	var red = Color(1.0, 0.2, 0.2, 1.0)
	
	# --- RPM GAUGE ---
	draw_circle(center_rpm, 120, dark_bg)
	
	# Synthwave Glow effect for RPM outer ring
	for w in range(1, 5):
		draw_arc(center_rpm, 120, PI*0.75, PI*2.25, 64, Color(magenta.r, magenta.g, magenta.b, 0.3 / w), 4.0 + (w * 3), true)
	draw_arc(center_rpm, 120, PI*0.75, PI*2.25, 64, magenta, 2.0, true)
	
	# Draw RPM ticks
	for i in range(11): # 0 to 10k
		var angle = lerp(PI*0.75, PI*2.25, i / 10.0)
		var p1 = center_rpm + Vector2(cos(angle), sin(angle)) * 105
		var p2 = center_rpm + Vector2(cos(angle), sin(angle)) * 120
		var color = cyan if i < 9 else red
		
		# Glow for ticks
		draw_line(p1, p2, Color(color.r, color.g, color.b, 0.3), 6.0, true)
		draw_line(p1, p2, color, 2.0, true)
		
		# Draw number
		var p_text = center_rpm + Vector2(cos(angle), sin(angle)) * 85
		var str_val = str(i)
		draw_string(font, p_text + Vector2(-5, 5), str_val, HORIZONTAL_ALIGNMENT_CENTER, -1, 16, color)
		
	# RPM Needle
	var rpm_angle = lerp(PI*0.75, PI*2.25, clamp(rpm / 10000.0, 0.0, 1.0))
	var needle_p = center_rpm + Vector2(cos(rpm_angle), sin(rpm_angle)) * 110
	# Needle glow
	for w in range(1, 4):
		draw_line(center_rpm, needle_p, Color(red.r, red.g, red.b, 0.2 / w), 4.0 + (w * 2), true)
	draw_line(center_rpm, needle_p, red, 3.0, true)
	draw_circle(center_rpm, 12, cyan)
	
	# RPM Text
	draw_string(font, center_rpm + Vector2(-30, 40), "RPM x1000", HORIZONTAL_ALIGNMENT_CENTER, -1, 14, magenta)
	draw_string(font, center_rpm + Vector2(-35, 65), "GEAR: " + gear_str, HORIZONTAL_ALIGNMENT_CENTER, -1, 22, cyan)
	draw_string(font, center_rpm + Vector2(-35, 88), mode_str, HORIZONTAL_ALIGNMENT_CENTER, -1, 16, magenta)
	
	# --- SPEED GAUGE ---
	draw_circle(center_speed, 90, dark_bg)
	
	# Speed Glow effect
	for w in range(1, 5):
		draw_arc(center_speed, 90, PI*0.75, PI*2.25, 64, Color(cyan.r, cyan.g, cyan.b, 0.3 / w), 3.0 + (w * 2), true)
	draw_arc(center_speed, 90, PI*0.75, PI*2.25, 64, cyan, 2.0, true)
	
	for i in range(10): # 0 to 450 km/h (steps of 50)
		var angle = lerp(PI*0.75, PI*2.25, i / 9.0)
		var p1 = center_speed + Vector2(cos(angle), sin(angle)) * 80
		var p2 = center_speed + Vector2(cos(angle), sin(angle)) * 90
		
		# Tick glow
		draw_line(p1, p2, Color(magenta.r, magenta.g, magenta.b, 0.3), 4.0, true)
		draw_line(p1, p2, magenta, 2.0, true)
		
		var p_text = center_speed + Vector2(cos(angle), sin(angle)) * 60
		var str_val = str(i * 50)
		draw_string(font, p_text + Vector2(-10, 5), str_val, HORIZONTAL_ALIGNMENT_CENTER, -1, 14, cyan)
		
	# Speed Needle
	var speed_angle = lerp(PI*0.75, PI*2.25, clamp(speed / 450.0, 0.0, 1.0))
	var speed_needle_p = center_speed + Vector2(cos(speed_angle), sin(speed_angle)) * 85
	for w in range(1, 4):
		draw_line(center_speed, speed_needle_p, Color(magenta.r, magenta.g, magenta.b, 0.2 / w), 3.0 + (w * 2), true)
	draw_line(center_speed, speed_needle_p, magenta, 2.0, true)
	draw_circle(center_speed, 8, magenta)
	
	draw_string(font, center_speed + Vector2(-15, 30), "KM/H", HORIZONTAL_ALIGNMENT_CENTER, -1, 14, cyan)
	
	# --- NITROUS GAUGE (Outer Arc around Speed Gauge) ---
	var nitro_sec = float(car.get("nitro_seconds")) if car.get("nitro_seconds") != null else 0.0
	var max_nitro = float(car.get("max_nitro_seconds")) if car.get("max_nitro_seconds") != null else 100.0
	var nitro_active = bool(car.get("nitro_active")) if car.get("nitro_active") != null else false
	
	draw_arc(center_speed, 105, PI*0.75, PI*2.25, 64, Color(dark_bg.r, dark_bg.g, dark_bg.b, 0.8), 10.0, true)
	draw_arc(center_speed, 105, PI*0.75, PI*2.25, 64, Color(cyan.r, cyan.g, cyan.b, 0.2), 2.0, true)
	
	var nitro_ratio = clampf(nitro_sec / max_nitro, 0.0, 1.0)
	if nitro_ratio > 0.0001:
		var nitro_end_angle = lerp(PI*0.75, PI*2.25, nitro_ratio)
		var nitro_col = Color(1.0, 0.5, 0.0) if nitro_active else Color(0.6, 0.1, 0.9)
		var glow_col = Color(1.0, 0.2, 0.0) if nitro_active else Color(0.8, 0.2, 1.0)
		
		for w in range(1, 4):
			draw_arc(center_speed, 105, PI*0.75, nitro_end_angle, 64, Color(glow_col.r, glow_col.g, glow_col.b, 0.3 / w), 10.0 + (w * 3), true)
		draw_arc(center_speed, 105, PI*0.75, nitro_end_angle, 64, nitro_col, 8.0, true)
		
	for i in range(7):
		var tick_ratio = minf(1.0, (i * 15.0) / 100.0)
		var tangle = lerp(PI*0.75, PI*2.25, tick_ratio)
		var tp1 = center_speed + Vector2(cos(tangle), sin(tangle)) * 100
		var tp2 = center_speed + Vector2(cos(tangle), sin(tangle)) * 110
		draw_line(tp1, tp2, Color(0, 0, 0, 0.8), 2.0, true)
		
	var nitro_str = "NITRO: 0s"
	if nitro_sec > 0.01:
		if nitro_sec < 10.0:
			nitro_str = "NITRO: " + str(snapped(nitro_sec, 0.1)) + "s"
		else:
			nitro_str = "NITRO: " + str(int(ceil(nitro_sec))) + "s"
	var text_col = Color(1.0, 0.6, 0.1) if nitro_active else Color(0.8, 0.3, 1.0)
	draw_string(font, center_speed + Vector2(-35, 65), nitro_str, HORIZONTAL_ALIGNMENT_CENTER, -1, 15, text_col)
