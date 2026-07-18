-- Register global variables for the widgets
regGlobalFloat("engine_force", 8000.0)
regGlobalFloat("enable_turbo", 0.0)
regGlobalFloat("spawn_ramp", 0.0)

print("=== Starting Lua ImGui API Test ===")

while true do
    -- Start ImGui Window
    imguiBegin("Lua Script Settings")
    
    imguiText("Control panel from Lua:")
    imguiSeparator()
    
    -- Slider bound to "engine_force"
    imguiSliderFloat("Engine Force", "engine_force", 1000.0, 50000.0)
    
    -- Checkbox bound to "enable_turbo"
    imguiCheckbox("Enable Turbo", "enable_turbo")
    
    imguiSeparator()
    
    -- Button bound to "spawn_ramp"
    imguiButton("Spawn Ramp!", "spawn_ramp")
    
    imguiEnd()
    
    -- Read the values and print them if they change
    local force = getGlobalFloat("engine_force")
    local turbo = getGlobalFloat("enable_turbo")
    local spawn = getGlobalFloat("spawn_ramp")
    
    if spawn > 0.5 then
        print("Spawn Ramp button was CLICKED from ImGui! Resetting click variable.")
        setGlobalFloat("spawn_ramp", 0.0)
    end
    
    delay(50)
end
