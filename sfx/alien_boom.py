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
    """Deep, boomy, and bassy arcade explosion."""
    duration = 0.6  # Extended duration so the bass rumble can fade out
    num_samples = int(SAMPLE_RATE * duration)
    data = []
    
    # Track previous noise to create a true low-pass filter effect
    last_noise = 0.0
    
    for i in range(num_samples):
        t = i / SAMPLE_RATE
        progress = t / duration
        
        # 1. Pitch-dropping sub-bass hum (starts at 120Hz, drops quickly to 40Hz)
        current_freq = 120 * (1.0 - progress) + 40
        # Integrate frequency over time for a smooth phase transition
        phase = 2 * math.pi * current_freq * t
        sub_bass = math.sin(phase)
        
        # 2. Heavy lo-fi low-pass filter on white noise
        raw_noise = random.uniform(-1.0, 1.0)
        # Average with previous sample to smooth out high frequencies (muffle the noise)
        filtered_noise = (raw_noise + last_noise) / 2.0
        last_noise = filtered_noise
        
        # 3. Combine elements: heavy bass mixed with muffled impact noise
        sample = (0.5 * sub_bass) + (0.5 * filtered_noise)
        
        # 4. Exponential decay envelope for a more natural, punchy explosion tail
        envelope = math.exp(-4.5 * progress)
        sample *= envelope
        
        # 5. Prevent digital clipping
        sample = max(-1.0, min(1.0, sample))
        
        data.append(sample)
    return data


# --- Execution ---
if __name__ == "__main__":
    print("Generating classic arcade audio effects...")
    print("-" * 40)
    save_wav("alien_explosion.wav", make_alien_explosion())
    print("-" * 40)
    print("Done! You can now drag these .wav files straight into Godot.")
