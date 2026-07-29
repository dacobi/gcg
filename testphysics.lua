godotSingleContext("lemans_scene.tscn")
delay(200) -- give it a moment to load

print("\n=== MegaRacer Synthwave Driving Demo Loaded ===")
print("Controls: Up/Down arrow keys to Accelerate and Brake/Reverse.")
print("          Left/Right arrow keys to Steer.")
print("Press ESC to exit.\n")

setAudioVolume(50)

-- Get the supercar node pointer
godotSelectRoot()
local supercar = godotGetNodePointer("SuperCar")
print("SUPERCAR POINTER IS: ", supercar)

-- Include shared car physics and controls
dofile("car_common.lua")
initCarPhysicsDefaults()

local joy_handle = ioJoystickOpen(0)
if joy_handle >= 0 then
	print("Joystick 1 connected for driving!")
end

local is_paused = false
local esc_held_state = false
local orbit_yaw = 0.0
local orbit_pitch = 0.5
local orbit_dist = 12.0
local last_mouse_dx = 0.0
local last_mouse_dy = 0.0
local last_mouse_wheel = 0.0

local frame_count = 0

while true do
	-- Draw Car Physics dialog if show_car_physics_ui is enabled
	renderCarPhysicsUI()

	-- Handle Q to quit (moved from ESCAPE)
	if ioKBClicked("SDLK_q") then
		print("Exiting game logic...")
		break
	end

	local track = godotGetNodePointer("MegaRacerScene")
	local in_edit = false
	if track then
		local res = godotGetProperty("in_edit_mode", track)
		if res == "True" or res == "true" or res == 1.0 or res == true then
			in_edit = true
		end
	end

	if not in_edit then
		if ioKBClicked("SDLK_e") then
			godotSetProperty("key_e_pressed", true, track)
			godotSetProperty("key_e_pressed", false, track)
			ioMouseCapture()
		end

		if track then
		    local p = godotGetProperty("is_paused", track)
		    if p == "True" or p == "true" or p == 1.0 or p == true then
		        is_paused = true
		    else
		        is_paused = false
		    end
		end
		
		if supercar then
		    if is_paused then
		        godotSetProperty("process_mode", 4, supercar) -- Disable car physics
		    else
		        godotSetProperty("process_mode", 0, supercar) -- Re-enable physics
		    end
		end

		if supercar then
			if is_paused then
				-- Globe camera controls
				local dx = godotGetProperty("mouse_dx", track) or 0.0
				local dy = godotGetProperty("mouse_dy", track) or 0.0
				
				local delta_dx = dx - (last_mouse_dx or 0.0)
				local delta_dy = dy - (last_mouse_dy or 0.0)
				last_mouse_dx = dx
				last_mouse_dy = dy
				
				orbit_yaw = orbit_yaw - delta_dx * 0.005
				orbit_pitch = orbit_pitch + delta_dy * 0.005
				
				if orbit_pitch > 1.5 then orbit_pitch = 1.5 end
				if orbit_pitch < -1.5 then orbit_pitch = -1.5 end
				
				-- Process new Mouse Wheel input for zooming
				local wheel = godotGetProperty("mouse_wheel", track) or 0.0
				local delta_wheel = wheel - (last_mouse_wheel or 0.0)
				last_mouse_wheel = wheel
				if delta_wheel ~= 0.0 then
					orbit_dist = orbit_dist - delta_wheel * 2.0
				end
				
				-- Keep keyboard W/S fallbacks
				if ioKBDown("w") then orbit_dist = orbit_dist - 0.5 end
				if ioKBDown("s") then orbit_dist = orbit_dist + 0.5 end
				
				if orbit_dist < 3.0 then orbit_dist = 3.0 end
				if orbit_dist > 50.0 then orbit_dist = 50.0 end
				
				godotSetProperty("orbit_yaw", orbit_yaw, track)
				godotSetProperty("orbit_pitch", orbit_pitch, track)
				godotSetProperty("orbit_dist", orbit_dist, track)
			else
				updateCarControlsAndPhysics(supercar, joy_handle, track, "reset_car")
				frame_count = frame_count + 1
				updateCarTelemetry(supercar, frame_count)
			end
		end
	else
		-- In Edit Mode
		if ioKBClicked("SDLK_d") then
			godotSetProperty("key_d_pressed", true, track)
			godotSetProperty("key_d_pressed", false, track)
			ioMouseRelease()
		elseif ioKBClicked("SDLK_n") then
			godotSetProperty("key_n_pressed", true, track)
			godotSetProperty("key_n_pressed", false, track)
		elseif ioKBClicked("SDLK_c") then
			godotSetProperty("key_c_pressed", true, track)
			godotSetProperty("key_c_pressed", false, track)
		end
		
		-- Pass mouse motion for free look camera
		local dx, dy = ioMouseGetMotion()
		godotSetProperty("mouse_dx", dx * 0.003, track)
		godotSetProperty("mouse_dy", dy * 0.003, track)
		
		-- Pass arrow keys for movement controls
		local arrow_up_down = 0.0
		if ioKBDown("Up") then
			arrow_up_down = 1.0
		elseif ioKBDown("Down") then
			arrow_up_down = -1.0
		end
		godotSetProperty("arrow_up_down", arrow_up_down, track)
		
		local arrow_left_right = 0.0
		if ioKBDown("Right") then
			arrow_left_right = 1.0
		elseif ioKBDown("Left") then
			arrow_left_right = -1.0
		end
		godotSetProperty("arrow_left_right", arrow_left_right, track)
	end

	delay(1) -- High-frequency input update loop
end

appQuit()
