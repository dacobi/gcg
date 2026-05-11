--addBouncer("[phys: 250,250,0,0,1,0.95][rect: 1024, 768][fractal:2]")
setBG("[fractal:2]")
selectFractal(-1)
setFractalParam("color_speed",7)
setFractalParam("zoom",0.4)
setFractalParam("palette_phase_r", 0.442)
setFractalParam("palette_phase_g", 0.373)
setFractalParam("palette_phase_b", 0.519)

setFractalParam("roll_palette",1)


local tasks = {}
     function spawn(f) table.insert(tasks, coroutine.create(f)) end
    
     function run()
         while true do
             for i = #tasks, 1, -1 do
                 local ok, err = coroutine.resume(tasks[i])
                 if not ok then print("Lua Error: " .. err) end
                if coroutine.status(tasks[i]) == "dead" then table.remove(tasks, i) end
            end
            delay(16) -- Sync to ~60fps
            if #tasks == 0 then break end
        end
    end
   
   spawn(function()
	  for i=1, 5 do coroutine.yield() end
   
        selectFractal(-1) -- Select the first bouncer with a fractal
	      local base = 0 
	 while true do
           -- Be careful with large increments: 0.1 * 5000 is 500. 
           -- The fractal will move off-screen very quickly!
           for i = 1, 300, 1 do 
                setFractalParam("roll_speed", base + (0.01 * i))
                coroutine.yield() -- Wait for the next frame
           end
            for i = 1, 300, 1 do 
               setFractalParam("roll_speed", base + (0.01 * (300-i)))
               coroutine.yield()
           end
       end
   end
  ) 
	 spawn(function()
	  for i=1, 5 do coroutine.yield() end
   
        selectFractal(-1) -- Select the first bouncer with a fractal
	      local base = 5
	 while true do
           -- Be careful with large increments: 0.1 * 5000 is 500. 
           -- The fractal will move off-screen very quickly!
           for i = 1, 150, 1 do 
                setFractalParam("zoom", base + (0.1 * i))
                coroutine.yield() -- Wait for the next frame
           end
            for i = 1, 150, 1 do 
               setFractalParam("zoom", base + 0.1 * (150-i))
               coroutine.yield()
           end
       end
   end
   )

   spawn(function()
        -- IMPORTANT: If you added the bouncer via CLI or a previous command,
        -- it might take a moment for the main thread to process it.
        -- We wait a few frames to ensure it exists before selecting it.
        for i=1, 5 do coroutine.yield() end
   
        selectFractal(-1) -- Select the first bouncer with a fractal
      local base = 0.3
       
       while true do
           -- Be careful with large increments: 0.1 * 5000 is 500. 
           -- The fractal will move off-screen very quickly!
           for i = 1, 150, 1 do 
                setFractalParam("x_offset", base + (0.001 * i))
               setFractalParam("y_offset", base + (0.001 * i))
                coroutine.yield() -- Wait for the next frame
           end
            for i = 1, 150, 1 do 
               setFractalParam("x_offset", base + 0.001 * (150-i))
               setFractalParam("y_offset", base + 0.001 * (150-i))
               coroutine.yield()
           end
       end
    end)
   
  
   run()



