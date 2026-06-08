print("Space Invaders Logic Started (GDScript Optimized)")

setBG("[tscn:space_invaders_game.tscn]")
selectGodot(-1)
delay(200) -- give it a moment to load

-- Capture mouse for consistent relative control
ioMouseCapture()

local shipX = 0.0
local minX = -55.0
local maxX = 55.0


addBouncer("[layer:1][fontsize:0.2][pos:20,20][rgb: 0,255,255]Level [global:level]")

addBouncer("[layer:1][fontsize:0.2][pos:850,20][rgb: 0,255,255]Score [global:score]")

setAudioVolume(30)


local clvs = getGlobalVar("lives")

godotSelectRoot()
local myInvaders = godotGetNodePointer("Invaders")
if myInvaders then print("Got Waders") end
    
godotSelectRoot()
local myShip = godotGetNodePointer("Ship")
if myShip then print("Got Ship") end
    
godotSetProperty("lives", clvs, myShip)
    
local ltest = godotGetProperty("lives",myShip)
print("lives: ",ltest)

godotSetProperty("vaders", 50, myInvaders)

local vtest = godotGetProperty("vaders",myInvaders)
print("vaders: ",vtest)
    
addBouncer("[layer:1][pos:20,730][rect:40,20][rgb:255,255,255][image:ship_icon.png]")

if clvs > 1 then
    addBouncer("[layer:1][pos:80,730][rect:40,20][rgb:255,255,255][image:ship_icon.png]")
end
if clvs > 2 then
    addBouncer("[layer:1][pos:140,730][rect:40,20][rgb:255,255,255][image:ship_icon.png]")
end

local myGameOverMutex = luaCreateMutex()
local myLivesLostMutex = luaCreateMutex()

function onLivesReset()
    if luaCheckMutex(myGameOverMutex) then return end
    
    luaGetMutex(myLivesLostMutex)
    print("In lives_reset")

    setGlobalVar("lives",3)
    delBouncer(4)
    delBouncer(3)
    delBouncer(2)
    addBouncer("[layer:1][pos:20,730][rect:40,20][rgb:255,255,255][image:ship_icon.png]")
    addBouncer("[layer:1][pos:80,730][rect:40,20][rgb:255,255,255][image:ship_icon.png]")
    addBouncer("[layer:1][pos:140,730][rect:40,20][rgb:255,255,255][image:ship_icon.png]")

    luaReleaseMutex(myLivesLostMutex)
    print("Exit lives_reset")
end

function onGameOver()
    -- We can call this directly from onLiveLost or onLoosing, so it assumes the mutex is already held or tries to grab it
    if not luaTryMutex(myGameOverMutex) then return end
    
    print("***************************")
    print("*        GAME OVER        *")
    print("***************************")
    godotSetProperty("lives",0, myShip)        
    luaClearAndRun("loozer.lua")
end

function onGameWon()
    if not luaTryMutex(myGameOverMutex) then return end
    
    print("***************************")
    print("*        GAME WON!        *")
    print("***************************")
    delay(400)
    luaClearAndRun("winwin.lua")
end

function onLiveLost()
    if luaCheckMutex(myGameOverMutex) then return end
    
    if not luaTryMutex(myLivesLostMutex) then return end
    
    local livesleft = godotGetProperty("lives", myShip)        
    print("livesleft: ",livesleft)
     
    print("***************************")
    print("*        HITMAN!          *")
    print("***************************")
    
    godotSetProperty("bAdvance", false, myInvaders)
    if myInvaders then print("NoAdvance") end
    
    if livesleft == 2 then                
        setGlobalVar("lives",2)
        delBouncer(4)
    end
    if livesleft == 1 then
        setGlobalVar("lives",1)
        delBouncer(3)
    end
    if livesleft <= 0 then
        -- This is the final hit! Grab the Game Over lock IMMEDIATELY to win the race condition.
        if not luaTryMutex(myGameOverMutex) then 
            luaReleaseMutex(myLivesLostMutex)
            return 
        end
        
        setGlobalVar("lives",0)
        delBouncer(2)
        delay(500)
        
        print("***************************")
        print("*        GAME OVER        *")
        print("***************************")
        godotSetProperty("lives",0, myShip)        
        luaClearAndRun("loozer.lua")
        return
    end

    delay(700)
        
    local _, sy, sz = godotGetPos(myShip)
    shipX = 0.0
    godotSetPos(shipX, sy, sz, myShip)
            
    godotSetProperty("bAdvance", true, myInvaders)
    if myInvaders then print("Advance") end            
    
    luaReleaseMutex(myLivesLostMutex)
end

local bOnLoosingLocked = false
function onLoosing()    
    -- The invasion triggered. This is an immediate game over.
    -- Grab the Game Over lock IMMEDIATELY to beat out any simultaneous player/invader hits.
    if not bOnLoosingLocked then
        if not luaTryMutex(myGameOverMutex) then return end
    end
    bOnLoosingLocked = true
    
    -- We won the race! The game is officially over. We can safely do our animations.
    local livesleft = godotGetProperty("lives", myShip)        
    print("livesleft: ",livesleft)
     
    print("***************************")
    print("*      INVASION OVER      *")
    print("***************************")
    
    godotSetProperty("bAdvance", false, myInvaders)
    
    if livesleft == 2 then                
        delBouncer(4)
        return
    end
    if livesleft == 1 then
        delBouncer(3)
        return
    end
    if livesleft == 0 then
        setGlobalVar("lives",0)
        delBouncer(2)
    end
    
    delay(500)

    print("***************************")
    print("*        GAME OVER        *")
    print("***************************")
    godotSetProperty("lives",0, myShip)        
    luaClearAndRun("loozer.lua")
end

function onNewScore()
    local lscore = godotGetProperty("cscore", myShip)  
    setGlobalVar("score",lscore)
end


-- Watchers are now polled natively via godotWatchSignal on selected nodes
    godotWatchSignal("live_lost", "onLiveLost", myShip)
    godotWatchSignal("lives_reset", "onLivesReset", myShip)
    godotWatchSignal("loosing", "onLoosing", myShip)

    godotWatchSignal("new_score", "onNewScore", myInvaders)
    godotWatchSignal("game_won", "onGameWon", myInvaders)

    local mlvl = getGlobalVar("level")
    if mlvl == 0 then mlvl = 1 end
    godotSetProperty("level", mlvl, myInvaders) -- Enable advancing and firing
    local mscore = getGlobalVar("score")
    godotSetProperty("score", mscore, myInvaders) -- Transfer score from last level
    

while true do
    -- Allow exiting via ESC
    if ioKBClicked("SDLK_ESCAPE") then
        print("Exiting game logic...")
        break
    end

    if not luaCheckMutex(myGameOverMutex) and not luaCheckMutex(myLivesLostMutex) then
        -- Mouse movement to control ship
        local rx, ry = ioMouseGetMotion()
        if rx ~= 0 then        
            shipX = shipX + (rx / 10.0)
            -- Clamp ship position
            if shipX < minX then shipX = minX end
            if shipX > maxX then shipX = maxX end
                
            local _, sy, sz = godotGetPos(myShip)
            godotSetPos(shipX, sy, sz,myShip)        
        end

        -- Left mouse click to fire (Limit to 7 active projectiles)
        if ioMouseBTNClicked(1) then
            godotSelectRoot()
            local myFire = godotGetNodePointer("Projectiles")
            local sx, sy, sz = godotGetPos(myShip)
            if myFire then
                local count = godotGetChildCount(myFire)
                if count < 7 then
                    if not luaCheckMutex(myLivesLostMutex) then
                        myFire = godotGetNodePointer("Projectiles")
                        if myFire then   
                            local myAlive = godotGetProperty("alive",myShip)
                            local nyHit = godotGetProperty("is_hit_anim", myShip)
                            if myAlive and not myHit then
                                godotLoadNode("res://projectile.tscn", sx, sy + 3, 0.0, myFire)
                            end
                        end
                    end
                end
            end
        end
    end

    delay(5) 
end

-- Release mouse on exit
ioMouseRelease()

print("Space Invaders Logic Ended")
luaClearAndRun("startspace.lua")
