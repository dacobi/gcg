ioMaximizeWindow()
setAudio("ytdlp://https://www.youtube.com/watch?v=EPOIS5taqA8")
setBG("[plasma:6]")
setRecordMax(59) -- Auto-stop after 30s
startRecord("scripted_card.mp4")

addBouncer("[layer:1][pos: 50,50][rgb: 255,100,100]Kære Mor[rgb: 255,255,255][image: heartemoji.png]")

addBouncer("[layer:2][phys: -500,-500,0,0,1,0.95][ttl:15000][image:kitten12.png]")
addBouncer("[layer:2][phys: 500,-500,600,0,1,0.95][ttl:15000][image:kitten12.png]")
addBouncer("[layer:2][phys: -500,500,100,600,1,0.95][ttl:15000][image:kitten12.png]")
addBouncer("[layer:2][phys: 500,500,600,600,1,0.95][ttl:15000][image:kitten12.png]")
addBouncer("[layer:2][phys: 50,50,300,300,1,0.95][ttl:15000][image:kitten12.png]")

delay(3000)
addBouncer("[layer:0][phys: 500,500,0,0,11,0.99][rect:512,512][stencil:softheart.png][image:mk2.png]")
delay(3000)




addBouncer("[layer:2][phys: -500,-500,0,0,1,0.95][ttl:7000][stencil:softstar.png][rect: 384, 384][plasma:-1]")
delay(1000)
addBouncer("[layer:2][phys: 500,-500,600,0,1,0.95][ttl:7000][stencil:softmoon.png][rect: 384, 384][plasma:-1]")
delay(1000)
addBouncer("[layer:2][phys: -500,500,100,600,1,0.95][ttl:7000][stencil:softstar.png][rect: 384, 384][plasma:-1]")
delay(1000)
addBouncer("[layer:2][phys: 500,500,600,600,1,0.95][ttl:7000][stencil:softmoon.png][rect: 384, 384][plasma:-1]")
delay(1000)
addBouncer("[layer:2][phys: 50,50,300,300,1,0.95][ttl:7000][stencil:softheart.png][rect: 384, 384][plasma:-1]")

delay(5000)

delBouncer(0);
addBouncer("[layer:1][pos: 50,50][rgb: 255,100,100]Kære Mor[rgb: 255,255,255][image: heartemoji.png][lf]Vi ønsker dig en[lf]glædelig Morsdag[rgb: 255,255,255][image: ckitt.png]")
delay(1000)
addBouncer("[layer:0][phys: 500,500,0,500,11,0.99][rect:512,512][stencil:softheart.png][image:mk1.png]")
delay(1000)
addBouncer("[layer:0][phys: 500,500,500,0,11,0.99][rect:512,512][stencil:softheart.png][image:mk3.jpg]")
delay(1000)
addBouncer("[layer:0][phys: 500,500,500,500,11,0.99][rect:512,512][stencil:softheart.png][image:mk4.jpg]")

delay(8000)

addBouncer("[layer:2][phys: -500,-500,0,0,1,0.95][ttl:7000][image:kitten12.png]")
delay(500)
addBouncer("[layer:2][phys: 500,-500,600,0,1,0.95][ttl:7000][image:kitten12.png]")
delay(500)
addBouncer("[layer:2][phys: -500,500,100,600,1,0.95][ttl:7000][image:kitten12.png]")
delay(500)
addBouncer("[layer:2][phys: 500,500,600,600,1,0.95][ttl:7000][image:kitten12.png]")
delay(500)
addBouncer("[layer:2][phys: 50,50,300,300,1,0.95][ttl:7000][image:kitten12.png]")

delay(3000)

addBouncer("[layer:1][pos: 1350,0]Kærligst[lf][rect:450,500][image: jada.jpg][lf]Jacob &[lf]Ada")
addBouncer("[layer:1][pos: 1600,775][image: heartemoji.png]");


stopRecord(1)

