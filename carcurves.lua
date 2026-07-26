setBG("[tscn:lemans_scene.tscn]")
selectGodot(-1)
delay(200) -- give it a moment to load

print("\n=== MegaRacer Synthwave Driving Demo Loaded ===")
print("Controls: Up/Down arrow keys to Accelerate and Brake/Reverse.")
print("          Left/Right arrow keys to Steer.")
print("Press ESC to exit.\n")

setAudioVolume(50)

-- Get the supercar node pointer
godotSelectRoot()
local supercar = godotGetNodePointer("SuperCar")

-- Include shared car physics and controls
dofile("car_common.lua")
initCarPhysicsDefaults()

local joy_handle = ioJoystickOpen(0)
if joy_handle >= 0 then
	print("Joystick 0 connected for driving!")
end

local is_paused = false
local orbit_yaw = 0.0
local orbit_pitch = 0.5
local orbit_dist = 12.0

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

		-- Allow entering Globe Camera via ESC
		if ioKBClicked("SDLK_ESCAPE") then
			is_paused = not is_paused
			
			godotSetProperty("is_paused", is_paused, track)
			
			if is_paused then
				godotSetProperty("process_mode", 4, supercar) -- Disable car physics
				ioMouseCapture() -- Hide and lock mouse for infinite panning
			else
				godotSetProperty("process_mode", 0, supercar) -- Re-enable physics
				ioMouseRelease() -- Restore mouse
			end
		end

		if supercar then
			if is_paused then
				-- Globe camera controls
				local dx, dy = ioMouseGetMotion()
				orbit_yaw = orbit_yaw - dx * 0.005
				orbit_pitch = orbit_pitch + dy * 0.005
				
				if orbit_pitch > 1.5 then orbit_pitch = 1.5 end
				if orbit_pitch < -1.5 then orbit_pitch = -1.5 end
				
				-- Process new Mouse Wheel input for zooming
				local wheel = ioMouseWheelMotion()
				if wheel then
					orbit_dist = orbit_dist - wheel * 2.0
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
				updateCarControlsAndPhysics(supercar, joy_handle, track, "reset_game")
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

	delay(10) -- 100 FPS input update loop
end

appQuit()
