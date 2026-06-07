setBG("dalekd.png")
imGuiHide()
ioResizeEnabled(false)

regGlobalVar("lives",3)
regGlobalVar("level",1)
regGlobalVar("score",0)

godotLoadHighScore()

setAudio("drwho.mp3")

luaClearAndRun("startspace.lua")
