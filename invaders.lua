print("Space Invaders Logic Started (GDScript Optimized)")

setBG("[tscn:space_invaders_game.tscn]")
selectGodot(-1)
delay(200) -- give it a moment to load

local projectiles = {}
local proj_id_seq = 1

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

    -- Update active projectiles list (prune those that died in Godot)
    for i = #projectiles, 1, -1 do
        local pname = projectiles[i]
        godotSelectRoot()
        if not godotSearchNode(pname) then
            -- Node was deleted (hit something or left screen)
            table.remove(projectiles, i)
	    print("projtl removed", #projectiles)
        end
    end

    -- Left mouse click to fire (Limit to 7)
    if ioMouseBTNClicked(1) then
        if #projectiles < 7 then
            godotSelectRoot()
            if godotSearchNode("Ship") then
                local sx, sy, sz = godotGetPos()
                godotSelectRoot()
                if godotSearchNode("Projectiles") then
                    -- Load and position atomically
                    if godotLoadNode("res://projectile.tscn", sx, sy + 3, sz) then
                        local pname = "PlayerProj_" .. proj_id_seq
                        godotRenameNode(pname)
                        table.insert(projectiles, pname)
                        proj_id_seq = proj_id_seq + 1
                    end
                end
            end
        else
            -- Optional: print("Max projectiles active!")
        end
    end

    delay(16) -- run at roughly 60 fps
end

print("Space Invaders Logic Ended")

appQuit()
