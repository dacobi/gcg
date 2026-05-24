setBG("[plasma: 13]")
addBouncer("[phys: 100,100,300,400,1,1][rgb: 255,255,0]Winner")
addBouncer("[phys: 100,600,300,400,1,1][rgb: 0,255,255]Winner")
addBouncer("[phys: 600,100,300,400,1,1][rgb: 255,0,255]Chicken")
addBouncer("[phys: 600,600,300,400,1,1][rgb: 255,255,255]WDinner[image:ckitt.png]")


delay(3500)

luaClearAndRun("startspace.lua")

