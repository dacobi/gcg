import math
import struct
import wave
import random

SAMPLE_RATE = 22050 # 22.05kHz is perfect for that crunchy retro lo-fi sound

def save_wav(filename, audio_data):
    """Saves a list of floating-point audio samples into a 16-bit Mono WAV file."""
    with wave.open(filename, 'w') as f:
        f.setnchannels(1) # Mono
        f.setsampwidth(2) # 16-bit
        f.setframerate(SAMPLE_RATE)
        
        # Convert floats (-1.0 to 1.0) to 16-bit integers
        for sample in audio_data:
            int_sample = int(max(-32768, min(32767, sample * 32767)))
            f.writeframesraw(struct.pack('<h', int_sample))
    print(f"Generated: {filename}")


def make_alien_explosion():
    """High-pitched noise burst."""
    duration = 0.25 
    num_samples = int(SAMPLE_RATE * duration)
    data = []
    
    for i in range(num_samples):
        t = i / SAMPLE_RATE
        # White noise (random numbers)
        noise = random.uniform(-1.0, 1.0)
        
        # Fake a low-pass filter by mixing with a low frequency hum
        hum = math.sin(2 * math.pi * 150 * t)
        sample = 0.3 * noise * hum
        
        # Fade out
        sample *= (1.0 - (t / duration))
        data.append(sample)
    return data



# --- Execution ---
if __name__ == "__main__":
    print("Generating classic arcade audio effects...")
    print("-" * 40)
    save_wav("alien_explosion.wav", make_alien_explosion())
    print("-" * 40)
    print("Done! You can now drag these .wav files straight into Godot.")