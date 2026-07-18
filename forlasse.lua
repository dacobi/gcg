ioMaximizeWindow()
setAudio("ytdlp://https://www.youtube.com/watch?v=iTPNaUsjksM")
setBG("[plasma:11]")
setRecordMax(37)
startRecord("forlasse.mp4")

-- Initial Bouncer: Kære Lasse and dude.png
addBouncer("[layer:1][pos: 50,50][rgb: 255,100,100]Kære Lasse[rgb: 255,255,255][image: dude.png]")

-- 5s delay
delay(5000)

-- Spawn "Du ønskes" below it
addBouncer("[layer:1][pos: 50,200][rgb: 255,100,100]Du ønskes")

-- Explosion of 5 heartemoji.png using the phys tag, starting from (500, 300) with TTL 10s
addBouncer("[layer:2][phys: -400,-400,500,300,1,0.95][ttl:10000][image: heartemoji.png]")
addBouncer("[layer:2][phys: 400,-400,500,300,1,0.95][ttl:10000][image: heartemoji.png]")
addBouncer("[layer:2][phys: -400,400,500,300,1,0.95][ttl:10000][image: heartemoji.png]")
addBouncer("[layer:2][phys: 400,400,500,300,1,0.95][ttl:10000][image: heartemoji.png]")
addBouncer("[layer:2][phys: 0,-500,500,300,1,0.95][ttl:10000][image: heartemoji.png]")

-- Wait 1s
delay(2500)

-- Add "Tillykke Med Fødselsdagen" + heartemoji.png
addBouncer("[layer:1][pos: 50,350][rgb: 255,100,100]Tillykke Med [lf]Fødselsdagen[rgb: 255,255,255][image: heartemoji.png]")

-- Wait remaining 9 seconds for hearts to fade out (1 + 9 = 10s total since heart explosion)
delay(9000)

-- Kitten explosion more to the right of the screen (origin 2200, 500) using 5 kitten12.png with TTL 15s
addBouncer("[layer:2][phys: -400,-400,2200,500,1,0.95][ttl:15000][image: kitten12.png]")
addBouncer("[layer:2][phys: 400,-400,2200,500,1,0.95][ttl:15000][image: kitten12.png]")
addBouncer("[layer:2][phys: -400,400,2200,500,1,0.95][ttl:15000][image: kitten12.png]")
addBouncer("[layer:2][phys: 400,400,2200,500,1,0.95][ttl:15000][image: kitten12.png]")
addBouncer("[layer:2][phys: 0,-500,2200,500,1,0.95][ttl:15000][image: kitten12.png]")

-- Wait 1s
delay(1000)

-- Add centered at the right side of window: Kærlig Hilsen [LF], followed by jada.jpg [LF] and then, Jacob & Ada, followed by ckitt.png
addBouncer("[layer:1][pos: 2200,200][rgb: 255,100,100]Kærlig Hilsen[lf][rgb: 255,255,255][rect:450,500][image: jada.jpg][lf][rgb: 255,100,100]Jacob & Ada[rgb: 255,255,255][image: ckitt.png]")

-- Wait remaining 14 seconds for kittens to fade out (1 + 14 = 15s total since kitten explosion)
delay(14000)

stopRecord(1)
