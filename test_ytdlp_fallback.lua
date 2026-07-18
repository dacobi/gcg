local success = setAudio("ytdlp://https://www.youtube.com/watch?v=invalid_id")
if not success then
    print("yt-dlp failed, falling back to local file")
    setAudio("alien_march.wav")
end
setAudioVolume(50)
