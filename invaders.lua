print("Space Invaders Logic Started (GDScript Optimized)")

setBG("[tscn:space_invaders_game.tscn]")
selectGodot(-1)



delay(200) -- give it a moment to load

godotWatchProperty("Ship", "alive", false, "game_over.lua")
godotWatchProperty("Invaders", "vaders", 0, "game_won.lua", 3)


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

    -- Left mouse click to fire (Limit to 7 active projectiles)
    if ioMouseBTNClicked(1) then
        godotSelectRoot()
        if godotSearchNode("Projectiles") then
            -- Use the new native child count for the limit
            local count = godotGetChildCount()
            if count < 7 then
                godotSelectRoot()
                if godotSearchNode("Ship") then
                    local sx, sy, sz = godotGetPos()
                    godotSelectRoot()
                    if godotSearchNode("Projectiles") then
                        -- Load and position atomically
                        godotLoadNode("res://projectile.tscn", sx, sy + 3, sz)
                    end
                end
            end
        end
    end

    delay(16) -- run at roughly 60 fps
end

print("Space Invaders Logic Ended")
