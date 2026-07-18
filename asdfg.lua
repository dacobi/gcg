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

function onLivesReset()
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

--local gameIsOver = false

local myGameOverMutex = luaCreateMutex()
local myLivesLostMutex = luaCreateMutex();

function onGameOver()
    if not luaTryMutex(myGameOverMutex) then 
        print("onGameOver mutex failed") 
        return
    end
    
    if luaCheckMutex(myGameOverMutex) then print("onGameOver got myGameOverMutex") end

    print("***************************")
    print("*        GAME OVER        *")
    print("***************************")
    godotSetProperty("lives",0, myShip)        
    luaClearAndRun("loozer.lua")
end

function onGameWon()
    if not luaTryMutex(myGameOverMutex) then 
        print("onGameWon mutex failed") 
        return
    end

    if luaCheckMutex(myGameOverMutex) then print("onGameWon got myGameOverMutex") end
    
    print("***************************")
    print("*        GAME WON!        *")
    print("***************************")
    luaClearAndRun("winwin.lua")
end


function onLiveLost()
    if not luaTryMutex(myLivesLostMutex) or luaCheckMutex(myGameOverMutex) then 
        print("onLiveLost mutexes failed") 
        return 
    end
    
    if luaCheckMutex(myLivesLostMutex) then print("onLiveLost got myLivesLostMutex") end

    local livesleft = 3
        
    livesleft = godotGetProperty("lives", myShip)        
    print("livesleft: ",livesleft)
    
     
    print("***************************")
    print("*        HITMAN!          *")
    print("***************************")
    
    godotSetProperty("bAdvance", false, myInvaders) -- Disable advancing and firing
    if myInvaders then print("NoAdvance") end
    
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
        delay(500)
        onGameOver()        
        return
    end

    delay(700)
        
    local _, sy, sz = godotGetPos(myShip)
    shipX = 0.0
        
    godotSetPos(shipX, sy, sz, myShip)
            
    godotSetProperty("bAdvance", true, myInvaders) -- Enable advancing and firing
    if myInvaders then print("Advance") end            
    luaReleaseMutex(myLivesLostMutex)
end


local bGotTex = false
function onLoosing()    
    print("bGotTex: ", bGotTex)
    if not bGotTex then
        if luaTryMutex(myGameOverMutex) then
            luaGetMutex(myLivesLostMutex)
            bGotTex = true
            print("onLoosing Got Mutexes: ", bGotTex)
        else
            print("onLoosing mutexes failed") 
            return
        end        
    end

    if luaCheckMutex(myLivesLostMutex) then print("onLoosing got myLivesLostMutex") end
    if luaCheckMutex(myGameOverMutex) then print("onLoosing got myGameOverMutex") end

    local livesleft = 3
        
    livesleft = godotGetProperty("lives", myShip)        
    print("livesleft: ",livesleft)
    
     
    print("***************************")
    print("*        HITMAN!          *")
    print("***************************")
    
    godotSetProperty("bAdvance", false, myInvaders) -- Disable advancing and firing
    if myInvaders then print("NoAdvance") end
    
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
        delay(300)
    
        print("***************************")
        print("*        GAME OVER        *")
        print("***************************")
        godotSetProperty("lives",0, myShip)        
        luaClearAndRun("loozer.lua")

        return
    end
        
    
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

    if not luaCheckMutex(myGameOverMutex) then
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
                    godotLoadNode("res://projectile.tscn", sx, sy + 3, sz, myFire)                          
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
