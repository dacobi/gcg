setBG("dalekd.png")
imGuiHide()
ioResizeEnabled(false)

regGlobalVar("lives",3)
regGlobalVar("level",1)
regGlobalVar("score",0)

godotLoadHighScore()

--setAudio("drwho.mp3")
setAudio("ytdlp://https://www.youtube.com/watch?v=DsAVx0u9Cw4")
-- rewindAudio()
-- skipAudio(20)

delay(2500)

luaClearAndRun("startspace.lua")
