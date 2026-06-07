setBG("dalekd.png")
imGuiHide()
ioResizeEnabled(false)

regGlobalVar("lives",3)
regGlobalVar("level",1)
regGlobalVar("score",0)

godotLoadHighScore()

local loads = setAudio("ytdlp://https://www.youtube.com/watch?v=DsAVx0u9Cw4")

if not loads then
	print("Using Fallback Audio")
	setAudio("slingshot.mp3")
else
	print("\n\nThe Timelords have spoken")
end

luaClearAndRun("startspace.lua")
