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

while true do
	-- Allow exiting via ESC
	if ioKBClicked("SDLK_ESCAPE") then
		print("Exiting game logic...")
		break
	end

	if supercar then
		-- Read arrow keys
		local accel = 0.0
		local brake = 0.0
		local steer = 0.0

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

		-- Pass controls to the Godot CharacterBody3D node properties
		godotSetProperty("accel_input", accel, supercar)
		godotSetProperty("brake_input", brake, supercar)
		godotSetProperty("steer_input", steer, supercar)
	end

	delay(10) -- 100 FPS input update loop
end

appQuit()
