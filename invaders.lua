print("Space Invaders Logic Started")

setBG("[tscn:space_invaders_game.tscn]")
selectGodot(-1)
delay(100) -- give it a moment to load

local projectiles = {}
local proj_id = 1

while true do
    -- Allow exiting via ESC
    if ioKBClicked("SDLK_ESCAPE") then
        print("Exiting game logic...")
        break
    end
    
    -- Mouse movement to control ship
    local rx, ry = ioMouseGetMotion()
    if rx ~= 0 then
        godotSelectRoot()
        if godotSearchNode("Ship") then
            godotMoveX(rx / 10.0) -- scale mouse movement down a bit
        end
    end

    -- Left mouse click to fire
    if ioMouseBTNClicked(1) then
        godotSelectRoot()
        if godotSearchNode("Ship") then
            local sx, sy, sz = godotGetPos()
            godotSelectRoot()
            if godotSearchNode("Projectiles") then
                if godotLoadNode("res://projectile.tscn") then
                    local p_name = "Proj_" .. proj_id
                    godotRenameNode(p_name)
                    -- Spawn slightly above the ship
                    godotSetPos(sx, sy + 3, sz)
                    table.insert(projectiles, p_name)
                    proj_id = proj_id + 1
                end
            end
        end
    end

    -- Update active projectiles
    for i = #projectiles, 1, -1 do
        local p_name = projectiles[i]
        godotSelectRoot()
        if godotSearchNode(p_name) then
            godotMoveY(1.0) -- projectile speed per frame
            local px, py, pz = godotGetPos()
            
            -- Check for collisions
            local hit = false
            local overlaps = godotGetOverlappingAreas()
            if overlaps then
                for _, area_name in ipairs(overlaps) do
                    if string.sub(area_name, 1, 3) == "Inv" then
                        print("Hit Invader: " .. area_name)
                        godotSelectRoot()
                        if godotSearchNode(area_name) then
                            godotDeleteNode()
                        end
                        hit = true
                        break
                    end
                end
            end

            if hit or py > 80.0 then -- despawn threshold at top of screen
                godotSelectRoot()
                if godotSearchNode(p_name) then
                    godotDeleteNode()
                end
                table.remove(projectiles, i)
            end
        else
            -- If node somehow disappeared, remove from tracking
            table.remove(projectiles, i)
        end
    end

    --delay(16) -- run at roughly 60 fps
end

print("Space Invaders Logic Ended")