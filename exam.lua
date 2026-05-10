 -- Select the background fractal and zoom in
 	setBG("[fractal:0]")
	selectFractal(-1)
  	setFractalParam("roll_palette", 1) -- Enable rolling
   	setFractalParam("roll_speed", 0.75) -- Set custom speed

	setFractalParam("zoom", 2.0)
    
     -- Add a bouncer and cycle its colors
     	addBouncer("[pos: 100,100,100,50][rect: 500,500][plasma:1]")
     	delay(1000)
     	selectPlasma(0) -- Select the bouncer we just added
     	for i=0, 100 do
		randomizePlasmaPalette()
		--ndomizePlasmaXY()
        	--tPlasmaParam("palette_phase_r", i/100)
        	delay(10)
    	end
	delay(2500)
	selectFractal(-1)
    	setFractalParam("roll_palette", 0) -- Enable rolling

