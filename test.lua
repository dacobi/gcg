-- Sample script for gcg
setBG("betty.mp4")
addBouncer("[pos:100,100,100,100]Hello Lua!")
delay(500)
addBouncer("[pos:400,400,-110,-31][rect: 600,600][fractal:1] Floating Fractal")
delay(500)
addBouncer("[pos:600,300][rect: 500,500][plasma:5] Plasma Bouncer")
delay(10000)
setBG("kitten.png");
delBouncer(0) -- Remove the first bouncer
delay(1000)
delBouncer(0) -- Remove the new first bouncer
