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
	var gear = car.get("current_gear_sim")
	if gear == null: gear = 0
	gear += 1
	
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
	draw_string(font, center_rpm + Vector2(-35, 70), "GEAR: " + str(gear), HORIZONTAL_ALIGNMENT_CENTER, -1, 24, cyan)
	
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
