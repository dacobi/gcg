setBG("[tscn:megaracer_car.tscn]")
selectGodot(-1)
delay(200) -- give it a moment to load

print("\n=== MegaRacer Synthwave Driving Demo Loaded ===")
print("Controls: Up/Down arrow keys to Accelerate and Brake/Reverse.")
print("          Left/Right arrow keys to Steer.")
print("Press ESC to exit.\n")

-- Get the supercar node pointer
godotSelectRoot()
local supercar = godotGetNodePointer("SuperCar")

local joy_handle = ioJoystickOpen(0)
if joy_handle >= 0 then
	print("Joystick 0 connected for driving!")
end

local is_paused = false
local orbit_yaw = 0.0
local orbit_pitch = 0.5
local orbit_dist = 12.0

while true do
	-- Handle Q to quit (moved from ESCAPE)
	if ioKBClicked("SDLK_q") then
		print("Exiting game logic...")
		break
	end

	-- Allow entering Globe Camera via ESC
	if ioKBClicked("SDLK_ESCAPE") then
		is_paused = not is_paused
		
		-- track is the MegaRacerScene root
		local track = godotGetNodePointer("MegaRacerScene")
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
			
			local track = godotGetNodePointer("MegaRacerScene")
			godotSetProperty("orbit_yaw", orbit_yaw, track)
			godotSetProperty("orbit_pitch", orbit_pitch, track)
			godotSetProperty("orbit_dist", orbit_dist, track)
		else
			local accel = 0.0
			local brake = 0.0
			local steer = 0.0

			if joy_handle >= 0 then
				-- Read analog values (Triggers: -1.0 to 1.0 -> 0.0 to 1.0)
				local r2 = ioJoystickGetAxis(joy_handle, 5)
				local l2 = ioJoystickGetAxis(joy_handle, 4)
				accel = (r2 + 1.0) * 0.5
				brake = (l2 + 1.0) * 0.5
				steer = ioJoystickGetAxis(joy_handle, 0)
				
				-- Tiny deadzones
				if accel < 0.05 then accel = 0.0 end
				if brake < 0.05 then brake = 0.0 end
				if steer > -0.1 and steer < 0.1 then steer = 0.0 end
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
			end

			-- Pass controls to the Godot CharacterBody3D node properties
			godotSetProperty("accel_input", accel, supercar)
			godotSetProperty("brake_input", brake, supercar)
			godotSetProperty("steer_input", steer, supercar)
		end
	end

	delay(10) -- 100 FPS input update loop
end

appQuit()
