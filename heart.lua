--addBouncer("[phys: 250,250,0,0,1,0.95][rect: 1024, 768][fractal:2]")
setBG("[plasma:12]")
addBouncer("[pos: 100,100,100,100][tusd:heart.usdc]");

addBouncer("[stencil:softstar.png][pos: 300,600,100,100][usd:cube.usda]");

selectUSD(-1);
setUSDParam("camera",2);


selectUSD(0);
setUSDParam("camera",-1);
setUSDParam("dist",35);


selectUSD(1);
setUSDParam("camera",-1);
setUSDParam("dist",15);


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
--	selectUSD(0)

        --lectUSD(0) -- Select the first bouncer with a fractal
	      local base = 0 
	 while true do
--	selectUSD(0);

           -- Be careful with large increments: 0.1 * 5000 is 500. 
           -- The fractal will move off-screen very quickly!
           --for i = 1, 360, 1 do 
             --   setUSDParam("rot_y",i);
--              --  coroutine.yield() -- Wait for the next frame
           --end

	---selectUSD(1);

           -- Be careful with large increments: 0.1 * 5000 is 500. 
           -- The fractal will move off-screen very quickly!
           for i = 1, 360, 1 do 
	selectUSD(0);
                setUSDParam("rot_y",i);
		
	selectUSD(1);
	
		   setUSDParam("rot_y",i);
		 setUSDParam("rot_x",i);
		coroutine.yield() 
	   end
           -- for i = 1, 300, 1 do 
            --   setUSDParam("rot_y", base + (0.5 * (300-i)))
            --   coroutine.yield()
           --end
       end
   end
  ) 

   run()



