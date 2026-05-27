print("***************************")
print("*        GAME OVER        *")
print("***************************")

setBG("[plasma: 3]")

local score = getGlobalVar("score")
local level = getGlobalVar("level")

if godotIsHighScore(score) then
    -- addBouncer("[layer:1][pos:300,150][rect:400,100][rgb:255,255,0][fontsize: 0.1]NEW HIGH SCORE!")
    addBouncer("[layer:1][rect: 300,350][pos: 360,320][addhscore:18," .. score .. "," .. level .. "]")
    
    --addBouncer("[layer:1][pos:400,350]
    --addBouncer("[layer:1][pos:300,700][rgb:200,200,200][fontsize:0.1]TYPE 3 LETTERS AND PRESS ENTER")
    
    -- Wait for input or timeout
    delay(15000)
    luaClearAndRun("startspace.lua")
else
    addBouncer("[layer:1][rect: 300,350][pos: 360,320][hscore: 18]")
    delay(5000)
    luaClearAndRun("startspace.lua")
end
