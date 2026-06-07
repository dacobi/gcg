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

def make_thunder():
    """Deep, rolling, and organic thunder sound using Brownian noise."""
    duration = 2.5  # Thunder needs a long time to roll and fade out
    num_samples = int(SAMPLE_RATE * duration)
    data = []
    
    # Brownian noise accumulator
    brown_noise = 0.0
    
    # Low-pass filter memory
    last_sample = 0.0
    
    for i in range(num_samples):
        t = i / SAMPLE_RATE
        progress = t / duration
        
        # 1. Generate Brownian Noise (integrate random steps)
        # This naturally creates a deep, rumbling 1/f^2 bass profile
        step = random.uniform(-0.1, 0.1)
        brown_noise += step
        
        # Keep the brown noise accumulator from drifting to infinity
        brown_noise = max(-1.0, min(1.0, brown_noise))
        
        # 2. Add structural "rumbles" (simulate echo/clouds using low frequencies)
        # Multiple low-frequency oscillators modulate the volume dynamically
        rumble1 = math.sin(2 * math.pi * 6 * t)  # 6Hz rumble
        rumble2 = math.cos(2 * math.pi * 13 * t) # 13Hz rumble
        amplitude_mod = 0.5 + 0.5 * (rumble1 * rumble2)
        
        # Apply amplitude modulation to the noise
        modulated_noise = brown_noise * amplitude_mod
        
        # 3. Aggressive Low-Pass Filter (simulates distance/air absorption)
        # Strips out any remaining high-frequency hiss, leaving pure bass
        filter_alpha = 0.08  # Lower means more muffled/distant
        filtered_sample = filter_alpha * modulated_noise + (1.0 - filter_alpha) * last_sample
        last_sample = filtered_sample
        
        # 4. Sound Envelope (Sharp clap at start, slow decay for the rolling tail)
        # Exponential decay combined with a linear fade out at the very end
        envelope = math.exp(-2.0 * progress) * (1.0 - progress)
        sample = filtered_sample * envelope
        
        # Boost volume to compensate for heavy filtering, then clamp to avoid clipping
        sample = max(-1.0, min(1.0, sample * 4.0))
        
        data.append(sample)
    return data


def make_compressed_thunder():
    """Short, heavily compressed thunder clap with a dense, punchy tail."""
    duration = 0.6  # Drastically shortened for a quick, snappy impact
    num_samples = int(SAMPLE_RATE * duration)
    data = []
    
    brown_noise = 0.0
    last_sample = 0.0
    
    for i in range(num_samples):
        t = i / SAMPLE_RATE
        progress = t / duration
        
        # 1. Base Brownian Noise for deep texture
        step = random.uniform(-0.15, 0.15) # Increased step size for a more aggressive bite
        brown_noise += step
        brown_noise = max(-1.0, min(1.0, brown_noise))
        
        # 2. Faster amplitude modulation to make the short tail sound chaotic
        rumble = math.sin(2 * math.pi * 25 * t) # Higher frequency (25Hz) for rapid texture
        modulated_noise = brown_noise * (0.6 + 0.4 * rumble)
        
        # 3. Low-Pass Filter (opened up slightly to 0.15 for more initial crisp punch)
        filter_alpha = 0.15  
        filtered_sample = filter_alpha * modulated_noise + (1.0 - filter_alpha) * last_sample
        last_sample = filtered_sample
        
        # 4. Enforce a sharp linear-exponential hybrid decay envelope
        envelope = math.exp(-3.5 * progress) * (1.0 - progress)
        sample = filtered_sample * envelope
        
        # 5. HARD COMPRESSION (Pre-gain amplification + Soft-Clipping)
        # We multiply the signal by 8.0 to squash it against the ceiling, 
        # then use math.tanh to smoothly compress it without harsh digital crackle.
        compressed_sample = math.tanh(sample * 8.0)
        
        data.append(compressed_sample)
    return data


# --- Execution ---
if __name__ == "__main__":
    print("Generating classic arcade audio effects...")
    print("-" * 40)
    save_wav("alien_explosion.wav", make_alien_explosion())
    save_wav("thunder.wav", make_thunder())
    save_wav("compressed_thunder.wav", make_compressed_thunder())
    print("-" * 40)
    print("Done! You can now drag these .wav files straight into Godot.")
