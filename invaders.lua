print("Space Invaders Logic Started (GDScript Optimized)")

setBG("[tscn:space_invaders_game.tscn]")
selectGodot(-1)
delay(200) -- give it a moment to load

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
                -- Projectiles now move and collide automatically via firep.gd
                if godotLoadNode("res://projectile.tscn") then
                    godotSetPos(sx, sy + 3, sz)
                    -- We don't need to track them in Lua anymore!
                end
            end
        end
    end

    delay(16) -- run at roughly 60 fps
end

print("Space Invaders Logic Ended")