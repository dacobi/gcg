-- Shared Car Physics and Control definitions for MegaRacer
-- Included via dofile("car_common.lua")

function initCarPhysicsDefaults()
	setGlobalFloat("engine_force_value", 90000.0)
	setGlobalFloat("brake_force_value", 400.0)
	setGlobalFloat("max_steer", 1.2)
	setGlobalFloat("wheel_friction_slip", 1.5)
	setGlobalFloat("suspension_travel", 0.25)
	setGlobalFloat("suspension_stiffness", 220.0)
	setGlobalFloat("suspension_max_force", 15000.0)
	setGlobalFloat("damping_compression", 8.0)
	setGlobalFloat("damping_relaxation", 10.0)
	setGlobalFloat("downforce_multiplier", 180.0)
	setGlobalFloat("car_mass", 1000.0)
	setGlobalFloat("center_of_mass_y", -0.35)
	setGlobalFloat("center_of_mass_z", 0.5)
	setGlobalFloat("max_speed", 210.0)
	setGlobalFloat("over_extend", 0.05)
	setGlobalFloat("z_traction", 0.15)
	setGlobalFloat("radius_front", 0.5)
	setGlobalFloat("radius_rear", 0.5)
	setGlobalFloat("use_shapecast", 1.0)
	setGlobalFloat("drivetrain_mode", 0.0)
	setGlobalFloat("tire_turn_speed", 15.0)
	setGlobalFloat("telemetry_slip_FL", 0.0)
	setGlobalFloat("telemetry_slip_FR", 0.0)
	setGlobalFloat("telemetry_slip_RL", 0.0)
	setGlobalFloat("telemetry_slip_RR", 0.0)
	setGlobalFloat("show_collision_debug", 0.0)
	setGlobalFloat("esp_max_yaw_damping", 1.5)
	setGlobalFloat("aero_drag_coeff", 0.3)
	setGlobalFloat("steer_speed_limit_max_speed", 80.0)
	setGlobalFloat("steer_speed_limit_min_mult", 0.2)

	-- Load saved car settings on startup if present
	godotLoadCarSettings()
end

function renderCarPhysicsUI()
	if getGlobalFloat("show_car_physics_ui") > 0.5 then
		imguiBegin("Car Physics Settings")
		
		imguiSliderFloat("Engine Force", "engine_force_value", 1000.0, 100000.0)
		imguiSliderFloat("Brake Force", "brake_force_value", 50.0, 1000.0)
		imguiSliderFloat("Max Steer", "max_steer", 0.1, 1.5)
		imguiSliderFloat("Wheel Friction", "wheel_friction_slip", 1.0, 20.0)
		imguiSliderFloat("Susp. Travel", "suspension_travel", 0.1, 1.0)
		imguiSliderFloat("Susp. Stiffness", "suspension_stiffness", 10.0, 300.0)
		imguiSliderFloat("Susp. Max Force", "suspension_max_force", 1000.0, 30000.0)
		imguiSliderFloat("Damp Compress", "damping_compression", 1.0, 20.0)
		imguiSliderFloat("Damp Relax", "damping_relaxation", 1.0, 20.0)
		imguiSliderFloat("Downforce Mult", "downforce_multiplier", 0.0, 500.0)
		imguiSliderFloat("Car Mass", "car_mass", 500.0, 3000.0)
		imguiSliderFloat("Center of Mass Y", "center_of_mass_y", -1.0, 1.0)
		imguiSliderFloat("Center of Mass Z", "center_of_mass_z", -1.0, 1.0)
		imguiSliderFloat("Max Speed", "max_speed", 10.0, 1000.0)
		imguiSliderFloat("Over Extend", "over_extend", 0.0, 1.0)
		imguiSliderFloat("Longitudinal Traction", "z_traction", 0.0, 1.0)
		imguiSliderFloat("Radius Front", "radius_front", 0.1, 2.0)
		imguiSliderFloat("Radius Rear", "radius_rear", 0.1, 2.0)
		imguiCheckbox("Use Shapecast", "use_shapecast")
		imguiCheckbox("Show Collision Mesh", "show_collision_debug")
		imguiSliderFloat("Steer Speed", "tire_turn_speed", 1.0, 30.0)
		
		imguiSeparator()
		imguiText("Handling & Drifting")
		imguiSliderFloat("ESP Yaw Damp", "esp_max_yaw_damping", 0.0, 5.0)
		imguiSliderFloat("Aero Drag", "aero_drag_coeff", 0.0, 2.0)
		imguiSliderFloat("Steer Limit Speed", "steer_speed_limit_max_speed", 10.0, 200.0)
		imguiSliderFloat("Steer Limit Min", "steer_speed_limit_min_mult", 0.0, 1.0)
		
		imguiSeparator()
		imguiText("Tire Slip Telemetry")
		imguiProgressBar("FL", "telemetry_slip_FL")
		imguiProgressBar("FR", "telemetry_slip_FR")
		imguiProgressBar("RL", "telemetry_slip_RL")
		imguiProgressBar("RR", "telemetry_slip_RR")
		
		imguiText("Engine RPM Telemetry")
		imguiProgressBar("RPM", "telemetry_engine_rpm_normalized")
		
		imguiSeparator()
		
		imguiButton("Save Settings", "save_settings_clicked")
		imguiButton("Load Settings", "load_settings_clicked")
		
		imguiEnd()
		
		if getGlobalFloat("save_settings_clicked") > 0.5 then
			setGlobalFloat("save_settings_clicked", 0.0)
			godotSaveCarSettings()
		end
		if getGlobalFloat("load_settings_clicked") > 0.5 then
			setGlobalFloat("load_settings_clicked", 0.0)
			godotLoadCarSettings()
		end
	end
end

function updateCarControlsAndPhysics(supercar, joy_handle, track, reset_prop_name)
	local accel = 0.0
	local brake = 0.0
	local steer = 0.0
	local rx = 0.0
	local ry = 0.0

	if joy_handle >= 0 then
		-- Read analog values (Triggers: -1.0 to 1.0 -> 0.0 to 1.0)
		local r2 = ioJoystickGetAxis(joy_handle, 5)
		local l2 = ioJoystickGetAxis(joy_handle, 4)
		accel = (r2 + 1.0) * 0.5
		brake = (l2 + 1.0) * 0.5
		steer = ioJoystickGetAxis(joy_handle, 0)
		rx = ioJoystickGetAxis(joy_handle, 2)
		ry = ioJoystickGetAxis(joy_handle, 3)
		
		-- Tiny deadzones
		if accel < 0.05 then accel = 0.0 end
		if brake < 0.05 then brake = 0.0 end
		if steer > -0.1 and steer < 0.1 then steer = 0.0 end
		if rx > -0.1 and rx < 0.1 then rx = 0.0 end
		if ry > -0.1 and ry < 0.1 then ry = 0.0 end
		
		-- Check for right face button (PS5 'O' button is index 1 in SDL) to reset car/game
		if ioJoystickGetButtonHit(joy_handle, 1) then
			if track and reset_prop_name then
				godotSetProperty(reset_prop_name, true, track)
			end
		end
	else
		-- Read arrow keys for driving fallback
		if ioKBDown("Up") then
			accel = 1.0
		end
		if ioKBDown("Down") then
			brake = 1.0
		end
		if ioKBDown("Left") then
			steer = -1.0
		elseif ioKBDown("Right") then
			steer = 1.0
		end
		if (ioKBHit("r") or ioKBHit("R")) and track and reset_prop_name then
			godotSetProperty(reset_prop_name, true, track)
		end
	end

	-- Pass controls to the Godot CharacterBody3D node properties
	godotSetProperty("accel_input", accel, supercar)
	godotSetProperty("brake_input", brake, supercar)
	godotSetProperty("steer_input", steer, supercar)
	
	local handbrake = 0.0
	if joy_handle >= 0 and ioJoystickGetButtonDown(joy_handle, 0) then
		handbrake = 1.0
	elseif ioKBDown("Space") then
		handbrake = 1.0
	end
	godotSetProperty("handbrake_input", handbrake, supercar)
	
	local nitro = 0.0
	if joy_handle >= 0 and ioJoystickGetButtonDown(joy_handle, 2) then
		nitro = 1.0
	elseif ioKBDown("Left Shift") or ioKBDown("LSHIFT") or ioKBDown("n") or ioKBDown("N") then
		nitro = 1.0
	end
	godotSetProperty("nitro_input", nitro, supercar)
	
	godotSetProperty("engine_force_value", getGlobalFloat("engine_force_value"), supercar)
	godotSetProperty("brake_force_value", getGlobalFloat("brake_force_value"), supercar)
	godotSetProperty("max_steer", getGlobalFloat("max_steer"), supercar)
	godotSetProperty("wheel_friction_slip", getGlobalFloat("wheel_friction_slip"), supercar)
	godotSetProperty("suspension_travel", getGlobalFloat("suspension_travel"), supercar)
	godotSetProperty("suspension_stiffness", getGlobalFloat("suspension_stiffness"), supercar)
	godotSetProperty("suspension_max_force", getGlobalFloat("suspension_max_force"), supercar)
	godotSetProperty("damping_compression", getGlobalFloat("damping_compression"), supercar)
	godotSetProperty("damping_relaxation", getGlobalFloat("damping_relaxation"), supercar)
	godotSetProperty("downforce_multiplier", getGlobalFloat("downforce_multiplier"), supercar)
	godotSetProperty("car_mass", getGlobalFloat("car_mass"), supercar)
	godotSetProperty("center_of_mass_y", getGlobalFloat("center_of_mass_y"), supercar)
	godotSetProperty("center_of_mass_z", getGlobalFloat("center_of_mass_z"), supercar)
	godotSetProperty("slider_max_speed_kmh", getGlobalFloat("max_speed"), supercar)
	godotSetProperty("over_extend", getGlobalFloat("over_extend"), supercar)
	godotSetProperty("z_traction", getGlobalFloat("z_traction"), supercar)
	godotSetProperty("radius_front", getGlobalFloat("radius_front"), supercar)
	godotSetProperty("radius_rear", getGlobalFloat("radius_rear"), supercar)
	godotSetProperty("use_shapecast", getGlobalFloat("use_shapecast"), supercar)
	godotSetProperty("drivetrain_mode", getGlobalFloat("drivetrain_mode"), supercar)
	godotSetProperty("tire_turn_speed", getGlobalFloat("tire_turn_speed"), supercar)
	godotSetProperty("show_collision_debug", getGlobalFloat("show_collision_debug"), supercar)
	godotSetProperty("esp_max_yaw_damping", getGlobalFloat("esp_max_yaw_damping"), supercar)
	godotSetProperty("aero_drag_coeff", getGlobalFloat("aero_drag_coeff"), supercar)
	godotSetProperty("steer_speed_limit_max_speed", getGlobalFloat("steer_speed_limit_max_speed"), supercar)
	godotSetProperty("steer_speed_limit_min_mult", getGlobalFloat("steer_speed_limit_min_mult"), supercar)
	if track then
		godotSetProperty("cam_rx", rx, track)
		godotSetProperty("cam_ry", ry, track)
	end
end

function updateCarTelemetry(supercar, frame_count)
	if frame_count % 2 == 0 then
		setGlobalFloat("telemetry_slip_FL", godotGetProperty("slip_FL", supercar) or 0.0)
		setGlobalFloat("telemetry_slip_FR", godotGetProperty("slip_FR", supercar) or 0.0)
		setGlobalFloat("telemetry_slip_RL", godotGetProperty("slip_RL", supercar) or 0.0)
		setGlobalFloat("telemetry_slip_RR", godotGetProperty("slip_RR", supercar) or 0.0)
		
		local rpm = tonumber(godotGetProperty("engine_rpm", supercar)) or 1000.0
		setGlobalFloat("telemetry_engine_rpm", rpm)
		setGlobalFloat("telemetry_engine_rpm_normalized", (rpm - 1000.0) / 9000.0)
	end
end
