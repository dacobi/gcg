import math
import struct
import wave

SAMPLE_RATE = 22050

def save_wav(filename, audio_data):
    with wave.open(filename, 'w') as f:
        f.setnchannels(1) # Mono
        f.setsampwidth(2) # 16-bit
        f.setframerate(SAMPLE_RATE)
        for sample in audio_data:
            int_sample = int(max(-32768, min(32767, sample * 32767)))
            f.writeframesraw(struct.pack('<h', int_sample))
    print(f"Generated: {filename}")

def make_beep(freq, duration, volume=0.35):
    num_samples = int(SAMPLE_RATE * duration)
    data = []
    phase = 0.0
    for i in range(num_samples):
        t = i / SAMPLE_RATE
        phase += (2 * math.pi * freq) / SAMPLE_RATE
        
        # Blend 50% triangle wave and 50% buzzy square wave
        tri_part = 2.0 * abs(2.0 * (t * freq - math.floor(t * freq + 0.5))) - 1.0
        square_part = 1.0 if math.sin(phase) >= 0 else -1.0
        sample = volume * (0.5 * tri_part + 0.5 * square_part)
        
        # 6-bit Bitcrush / quantization for retro arcade crunch (64 discrete step levels)
        sample = round(sample * 32.0) / 32.0
        
        # Attack envelope (5ms)
        if i < int(SAMPLE_RATE * 0.005):
            sample *= (i / (SAMPLE_RATE * 0.005))
        # Release envelope (last 40ms)
        elif i > num_samples - int(SAMPLE_RATE * 0.04):
            sample *= (num_samples - i) / (SAMPLE_RATE * 0.04)
            
        data.append(sample)
    return data

if __name__ == "__main__":
    print("Generating crunchy 8-bit countdown audio effects...")
    save_wav("beep_low.wav", make_beep(523.25, 0.35))   # Longer (350ms) C5 note for 3, 2, 1
    save_wav("beep_high.wav", make_beep(1046.50, 0.65)) # Longer (650ms) C6 note for GO!
