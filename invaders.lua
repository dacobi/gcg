print("Space Invaders Logic Started (GDScript Optimized)")

setBG("[tscn:space_invaders_game.tscn]")
selectGodot(-1)
delay(200) -- give it a moment to load

-- Capture mouse for consistent relative control
ioMouseCapture()

local shipX = 0.0
local minX = -55.0
local maxX = 55.0


addBouncer("[layer:1][fontsize:0.2][pos:20,20][rgb: 0,255,255]Level [global:\"level\"]")

addBouncer("[layer:1][fontsize:0.2][pos:850,20][rgb: 0,255,255]Score [global:\"score\"]")

setAudioVolume(30)

local clvs = getGlobalVar("lives")

    godotSelectRoot()
    if godotSearchNode("Ship") then
        godotSetProperty("lives", clvs)
    end

    godotSelectRoot()
    if godotSearchNode("Invaders") then
        godotSetProperty("vaders", 50)
        local vtest = godotGetProperty("vaders")
        print("vaders: ",vtest)
    end

addBouncer("[layer:1][pos:20,730][rect:40,20][rgb:255,255,255][image:ship_icon.png]")

if clvs > 1 then
    addBouncer("[layer:1][pos:80,730][rect:40,20][rgb:255,255,255][image:ship_icon.png]")
end
if clvs > 2 then
    addBouncer("[layer:1][pos:140,730][rect:40,20][rgb:255,255,255][image:ship_icon.png]")
end

function onLivesReset()
    setGlobalVar("lives",3)

    delBouncer(4)
    delBouncer(3)
    delBouncer(2)

    addBouncer("[layer:1][pos:20,730][rect:40,20][rgb:255,255,255][image:ship_icon.png]")

    addBouncer("[layer:1][pos:80,730][rect:40,20][rgb:255,255,255][image:ship_icon.png]")

    addBouncer("[layer:1][pos:140,730][rect:40,20][rgb:255,255,255][image:ship_icon.png]")
end

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

local inLiveLost = false
function onLiveLost()
    if inLiveLost then return end
    inLiveLost = true
    local livesleft = 3
    
    godotSelectRoot()
    if godotSearchNode("Ship") then
        livesleft = godotGetProperty("lives")        
        print("livesleft: ",livesleft)
    end
    
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
            godotSetProperty("hit_glow", true)
        end    
        
        for i = 1,7 do
            godotSelectRoot()
            if godotSearchNode("Ship") then
                -- godotSetVisible(true)
                godotSetScale(1.2, 1.2, 1.2)
            end
            delay(40)
            godotSelectRoot()
            if godotSearchNode("Ship") then
                godotSetScale(1.0, 1.0, 1.0)
            end
            delay(40)
            godotSelectRoot()
            if godotSearchNode("Ship") then
                -- godotSetVisible(false)
            end
            delay(40)
        end

        godotSelectRoot()
        if godotSearchNode("Ship") then
            godotSetProperty("hit_glow", false)
        end

        if livesleft == 2 then                
                setGlobalVar("lives",2)
                delBouncer(4)
            end
            if livesleft == 1 then
                setGlobalVar("lives",1)
                delBouncer(3)
            end
            if livesleft == 0 then
                setGlobalVar("lives",0)
                delBouncer(2)
                onGameOver()
                inLiveLost = false
                return
            end

            delay(700)
            godotSelectRoot()
            if godotSearchNode("Ship") then
                local _, sy, sz = godotGetPos()
                shipX = 0.0
            godotSelectRoot()
            if godotSearchNode("Ship") then
                godotSetPos(shipX, sy, sz)
            end
            godotSelectRoot()
            if godotSearchNode("Ship") then
                godotSetVisible(true)
            end
            --godotSelectRoot()
            --if godotSearchNode("Ship") then
            --    godotSetProperty("livelost", false)
            --end
        

            
            godotSelectRoot()
            if godotSearchNode("Invaders") then
                godotSetProperty("bAdvance", true) -- Enable advancing and firing
            end
        end 
    inLiveLost = false
end

function onNewScore()
    
    godotSelectRoot()
    if godotSearchNode("Ship") then        
          local lscore = godotGetProperty("cscore")  
         setGlobalVar("score",lscore)
        --godotSetProperty("bNewScore", false) -- Enable next score update

    end
end


-- Watchers are now polled natively via godotWatchSignal on selected nodes
godotSelectRoot()
if godotSearchNode("Ship") then
    godotWatchSignal("live_lost", "onLiveLost")
    godotWatchSignal("lives_reset", "onLivesReset")
end

godotSelectRoot()
if godotSearchNode("Invaders") then
    godotWatchSignal("new_score", "onNewScore")
    godotWatchSignal("game_won", "onGameWon")
end

godotSelectRoot()
    if godotSearchNode("Invaders") then
        local mlvl = getGlobalVar("level")
        if mlvl == 0 then mlvl = 1 end
        godotSetProperty("level", mlvl) -- Enable advancing and firing
        local mscore = getGlobalVar("score")
        godotSetProperty("score", mscore) -- Transfer score from last level
    end

while true do
    -- Allow exiting via ESC
    if ioKBClicked("SDLK_ESCAPE") then
        print("Exiting game logic...")
        break
    end

    -- Mouse movement to control ship
    local rx, ry = ioMouseGetMotion()
    if rx ~= 0 then
        --print("rx: ",rx)
        --print("ry: ",ry)
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

    delay(5) -- run at roughly 60 fps
end

-- Release mouse on exit
ioMouseRelease()

print("Space Invaders Logic Ended")
luaClearAndRun("startspace.lua")
