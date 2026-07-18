setBG("[tscn:tardis.tscn]")
selectGodot(-1)
delay(100)
godotSetProperty("rotation_speed", 0.0)

-- We can't call die() directly, but we can set is_dead=true 
-- Actually we can't set sparks.emitting easily without a method.
-- Let's just create a test trigger in tardis.gd

startRecord("test_sparks.mp4", 1)
setRecordMax(5)

delay(5000)
stopRecord()
