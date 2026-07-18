-- test_godot_manip.lua
print("Godot Manip Test Started")

-- Set invaders game as background
setBG("[tscn:space_invaders_game.tscn]")
selectGodot(-1) -- Select background for manipulation

delay(1000) -- Wait for load

godotSelectRoot()
print("Root Type: " .. godotGetNodeType())

if godotSearchNode("Camera3D") then
    print("Found Camera3D!")
    godotSetCamera()
    local x,y,z = godotGetPos()
    print(string.format("Camera Pos: %f, %f, %f", x, y, z))
    
    -- Move camera back
    godotMoveX(50)
    local nx,ny,nz = godotGetPos()
    print(string.format("New Camera Pos: %f, %f, %f", nx, ny, nz))
else
    print("Camera3D NOT found!")
end

if godotSearchNode("Invaders") then
    print("Found Invaders node!")
    godotMoveY(20) -- Move them up
end

print("Testing Node Loading...")
if godotLoadNode("res://ship.tscn") then
    print("Loaded ship.tscn, current type: " .. godotGetNodeType())
    godotSetPos(-20, 0, 0)
    local x,y,z = godotGetPos()
    print(string.format("Ship Pos: %f, %f, %f", x, y, z))
end

print("Testing Node Creation...")
if godotCreateNode("DynamicNode") then
    print("Created 'DynamicNode', current type: " .. godotGetNodeType())
    godotSetPos(10, 20, 30)
    local x,y,z = godotGetPos()
    print(string.format("DynamicNode Pos: %f, %f, %f", x, y, z))
    
    print("Deleting DynamicNode...")
    godotDeleteNode()
    print("Back to parent type: " .. godotGetNodeType())
end

print("Testing GDScript integration...")
godotSelectRoot()
if godotCreateNode("ScriptNode") then
    if godotAttachScript("res://test_script.gd") then
        print("Attached test_script.gd")
        godotSetProperty("speed", 50.0)
        print("Set speed to 50.0, current pos:")
        local x1,y1,z1 = godotGetPos()
        print(string.format("  Start: %f, %f, %f", x1, y1, z1))
        
        delay(1000) -- Wait 1 second
        
        local x2,y2,z2 = godotGetPos()
        print(string.format("  End after 1s: %f, %f, %f", x2, y2, z2))
        
        local cur_speed = godotGetProperty("speed")
        print("Verified speed property: " .. cur_speed)
    else
        print("FAILED to attach script!")
    end
end

print("Godot Manip Test Finished")
