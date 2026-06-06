print("***************************")
print("*        GAME OVER        *")
print("***************************")

setBG("[plasma: 3]")

local score = getGlobalVar("score")
local level = getGlobalVar("level")

setAudioVolume(100)

if godotIsHighScore(score) then
    -- addBouncer("[layer:1][pos:300,150][rect:400,100][rgb:255,255,0][fontsize: 0.1]NEW HIGH SCORE!")
    addBouncer("[layer:1][rect: 270,350][pos: 380,200][addhscore:18," .. score .. "," .. level .. "]")
    --addBouncer("[layer:1][rect: 270,350][pos: 380,200][hscore: 18]")
    addBouncer("[layer:2][ttl:10000][phys: 250,250,50,50,1,1][fontsize: 0.6][rgb:255,122,255]You Got[lf][rgb:251,222,155]High Score[rgb:255,255,255][image:geekt.png]")
    --addBouncer("[layer:1][pos:400,350]
    --addBouncer("[layer:1][pos:300,700][rgb:200,200,200][fontsize:0.1]TYPE 3 LETTERS AND PRESS ENTER")
    
    -- Wait for input or timeout
    delayKb(10000)
    rewindAudio()
    luaClearAndRun("startspace.lua")
else
	addBouncer("[layer:2][phys: 150,500,200,100,1,1][rgb: 255,255,0]You Snooze")
	addBouncer("[layer:2][phys: 150,500,200,400,1,1][rgb: 0,255,255]You Looze")
	addBouncer("[layer:2][phys: 600,600,300,400,1,1][image:kitt.png]")
    addBouncer("[layer:1][rect: 270,350][pos: 380,200][hscore: 18]")
    delayKb(5000)
    rewindAudio()
    luaClearAndRun("startspace.lua")
end

