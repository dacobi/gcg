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
        # Blend 70% triangle wave and 30% sine wave for a punchy, clean arcade racing tone
        sine_part = math.sin(phase)
        tri_part = 2.0 * abs(2.0 * (t * freq - math.floor(t * freq + 0.5))) - 1.0
        sample = volume * (0.7 * tri_part + 0.3 * sine_part)
        
        # Attack envelope (5ms)
        if i < int(SAMPLE_RATE * 0.005):
            sample *= (i / (SAMPLE_RATE * 0.005))
        # Release envelope (last 30ms)
        elif i > num_samples - int(SAMPLE_RATE * 0.03):
            sample *= (num_samples - i) / (SAMPLE_RATE * 0.03)
            
        data.append(sample)
    return data

if __name__ == "__main__":
    print("Generating countdown audio effects...")
    save_wav("beep_low.wav", make_beep(523.25, 0.15))   # C5 note for 3, 2, 1
    save_wav("beep_high.wav", make_beep(1046.50, 0.35)) # C6 note for GO!
