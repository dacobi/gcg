print("Space Invaders Logic Started (GDScript Optimized)")

setBG("[tscn:space_invaders_game.tscn]")
selectGodot(-1)
delay(200) -- give it a moment to load

-- Capture mouse for consistent relative control
ioMouseCapture()

local shipX = 0.0
local minX = -55.0
local maxX = 55.0

addBouncer("[layer:1][fontsize:0.4][pos:50,50]Level [global:\"level\"]")

addBouncer("[layer:1][pos:20,730][rect:40,20][rgb:255,255,255][image:ship_icon.png]")
addBouncer("[layer:1][pos:80,730][rect:40,20][rgb:255,255,255][image:ship_icon.png]")
addBouncer("[layer:1][pos:140,730][rect:40,20][rgb:255,255,255][image:ship_icon.png]")

function onGameOver()
    print("***************************")
    print("*        GAME OVER        *")
    print("***************************")
    luaClearAndRun("loozer.lua")
end

function onGameWon()
    print("***************************")
    print("*        GAME WON!        *")
    print("***************************")
    luaClearAndRun("winwin.lua")
end

function onLiveLost()
    print("***************************")
    print("*        HITMAN!          *")
    print("***************************")
    
    godotSelectRoot()
    if godotSearchNode("Invaders") then
        godotSetProperty("bAdvance", false) -- Disable advancing and firing
    end

    -- remove all enemy projectiles
    godotSelectRoot()
    if godotSearchNode("EnemyProjectiles") then
        godotDeleteNode()
        godotSelectRoot()
        godotCreateNode("EnemyProjectiles")
    end

    -- remove all player projectiles
    godotSelectRoot()
    if godotSearchNode("Projectiles") then
        godotDeleteNode()
        godotSelectRoot()
        godotCreateNode("Projectiles")
    end

    godotSelectRoot()
        if godotSearchNode("Ship") then


            local livesleft = godotGetProperty("lives")
            print("livesleft: ",livesleft)
            for i = 1,10 do
                godotSetVisible(true)
                godotSetScale(1.2, 1.2, 1.2)
                delay(40)
                godotSetScale(1.0, 1.0, 1.0)
                delay(40)
                godotSetVisible(false)
                delay(40)
            end            
            if livesleft == 2 then
                setGlobalVar(lives,2)
                delBouncer(3)
            end
            if livesleft == 1 then
                setGlobalVar(lives,1)
                delBouncer(2)
            end
            if livesleft == 0 then
                setGlobalVar(lives,0)
                delBouncer(1)
                onGameOver()
            end

            delay(900)
            
            local _, sy, sz = godotGetPos()
            shipX = 0.0
            godotSetPos(shipX, sy, sz)
            godotSetVisible(true)
            godotSetProperty("livelost", false)
            godotWatchProperty("Ship", "livelost", true, "onLiveLost")            
            godotSelectRoot()
            if godotSearchNode("Invaders") then
                godotSetProperty("bAdvance", true) -- Enable advancing and firing
            end
        end

    
end


--godotWatchProperty("Ship", "alive", false, "onGameOver")
godotWatchProperty("Ship", "livelost", true, "onLiveLost")
godotWatchProperty("Invaders", "vaders", 0, "onGameWon", 3)

while true do
    -- Allow exiting via ESC
    if ioKBClicked("SDLK_ESCAPE") then
        print("Exiting game logic...")
        break
    end
    
    -- Mouse movement to control ship
    local rx, ry = ioMouseGetMotion()
    if rx ~= 0 then
        shipX = shipX + (rx / 10.0)
        -- Clamp ship position
        if shipX < minX then shipX = minX end
        if shipX > maxX then shipX = maxX end

        godotSelectRoot()
        if godotSearchNode("Ship") then
            local _, sy, sz = godotGetPos()
            godotSetPos(shipX, sy, sz)
        end
    end

    -- Left mouse click to fire (Limit to 7 active projectiles)
    if ioMouseBTNClicked(1) then
        godotSelectRoot()
        if godotSearchNode("Projectiles") then
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

-- Release mouse on exit
ioMouseRelease()

print("Space Invaders Logic Ended")
