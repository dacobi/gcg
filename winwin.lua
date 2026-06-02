setBG("[plasma: 13]")
addBouncer("[phys: 600,600,100,100,1,1][rgb: 255,255,0][fontsize: 0.6]Winner[lf] Winner")
--addBouncer("[phys: 100,600,300,400,1,1][rgb: 0,255,255]Winner")
addBouncer("[phys: 600,600,100,200,1,1][rgb: 255,0,255][fontsize: 0.6]Chicken[lf][rgb: 255,255,255] Dinner[image:ckitt.png]")
--addBouncer("[phys: 600,600,300,400,1,1][rgb: 255,255,255]Dinner[image:ckitt.png]")

setAudioVolume(80)

delayKb(5000)

local clvl = getGlobalVar("level")
clvl = clvl + 1
setGlobalVar("level",clvl)
luaClearAndRun("invaders.lua")

