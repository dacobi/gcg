setAudio("betty.mp4")
stopAudio()
stopAudio()
delay(2000)
playAudio()
delay(2000)
setAudioVolume(10) -- Start at half volume
delay(2000)
playAudio()
delay(10000)
setAudioVolume(50)
delay(10000)
stopAudio() -- Pause after 2 seconds
delay(10000)
playAudio() -- Resume
rewindAudio() -- Start over
setAudioVolume(100)