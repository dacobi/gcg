-- test_input.lua
print("Input Test Script Started")
while true do
    if ioKBClicked("SDLK_SPACE") then
        print("Space Clicked!")
    end
    if ioKBDown("SDLK_SPACE") then
        print("Space is DOWN")
    end
    if ioKBUp("SDLK_SPACE") then
        print("Space Released!")
    end

    if ioMouseMoved() then
        local x, y = ioMousePos()
        local rx, ry = ioMouseGetMotion()
        print(string.format("Mouse Moved: %d,%d (Delta: %d,%d)", x, y, rx, ry))
    end

    if ioMouseBTNClicked(1) then
        print("Left Mouse Button Clicked!")
    end

    if ioMouseBTNDown(1) then
        print("Left Mouse Button Down!")
    end

    if ioMouseBTNDown(3) then
        print("Right Mouse Button Down!")
    end



    delay(16) -- ~60fps
end
