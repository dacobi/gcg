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

def make_laser():
    """Fast downward pitch sweep using a square wave."""
    duration = 0.15 # 150ms
    num_samples = int(SAMPLE_RATE * duration)
    data = []
    phase = 0.0
    
    for i in range(num_samples):
        t = i / SAMPLE_RATE
        # Sweep frequency down from 900Hz to 200Hz
        freq = 900 - (700 * (t / duration))
        
        phase += (2 * math.pi * freq) / SAMPLE_RATE
        # Square wave logic: +1 or -1
        sample = 0.2 if math.sin(phase) >= 0 else -0.2
        
        # Quick fade out at the very end to prevent clicking
        if i > num_samples - 200:
            sample *= (num_samples - i) / 200.0
        data.append(sample)
    return data

def make_alien_laser():
    """A more subdued, lower-pitched laser for the enemies."""
    duration = 0.18 # Slightly longer but softer
    num_samples = int(SAMPLE_RATE * duration)
    data = []
    phase = 0.0
    
    for i in range(num_samples):
        t = i / SAMPLE_RATE
        # Sweeps down from 500Hz to 150Hz (lower than the player's 900Hz->200Hz)
        freq = 500 - (350 * (t / duration))
        
        phase += (2 * math.pi * freq) / SAMPLE_RATE
        # Using a triangle wave instead of a square wave makes it sound "subdued" and smoother
        sample = 0.25 * (abs((phase % (2 * math.pi)) - math.pi) / math.pi - 0.5)
        
        # Linear fade out to prevent sharp clicks
        sample *= (1.0 - (t / duration))
        data.append(sample)
    return data

def make_march():
    """The iconic short, mechanical, clonky sound of moving aliens."""
    duration = 0.06 # Cut duration in half for a snappier step
    num_samples = int(SAMPLE_RATE * duration)
    data = []
    phase = 0.0
    
    for i in range(num_samples):
        t = i / SAMPLE_RATE
        # Higher initial pitch with a steeper pitch drop for a metallic "clonk"
        freq = 300 - (220 * (t / duration))
        
        phase += (2 * math.pi * freq) / SAMPLE_RATE
        # Square wave creates a harsh, robotic, mechanical digital tone
        sample = 0.3 if math.sin(phase) >= 0 else -0.3
        
        # Linear fade out prevents clicking while maintaining the harsh stop
        sample *= (1.0 - (t / duration))
        data.append(sample)
    return data

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

def make_bunker_hit():
    """A short, heavy, metallic thud for a bunker taking damage."""
    duration = 0.15 # Very brief impact
    num_samples = int(SAMPLE_RATE * duration)
    data = []
    
    for i in range(num_samples):
        t = i / SAMPLE_RATE
        # Mix a low-frequency impact pitch (80Hz) with a tiny bit of metallic noise
        pitch = math.sin(2 * math.pi * 80 * t)
        noise = random.uniform(-1.0, 1.0) if i < (num_samples * 0.3) else 0 # Noise only at the start instant
        
        # Combine them (heavy on the low-end thud)
        sample = (0.4 * pitch) + (0.15 * noise)
        
        # Sharp exponential decay so it feels like a solid impact
        sample *= math.exp(-12 * t)
        data.append(sample)
    return data

def make_player_explosion():
    """Heavy, distorted sub-bass explosion with a massive initial impact slam."""
    duration = 0.9
    num_samples = int(SAMPLE_RATE * duration)
    data = []
    
    prev_sample = 0.0
    
    for i in range(num_samples):
        t = i / SAMPLE_RATE
        
        # 1. Immediate Impact Transient (A heavy 90Hz square wave thump that decays in 30ms)
        impact_env = math.exp(-120 * t)
        impact = (0.5 if math.sin(2 * math.pi * 90 * t) >= 0 else -0.5) * impact_env
        
        # 2. Constant Sub-Bass Tone (Deep 42Hz foundation)
        sub_bass = math.sin(2 * math.pi * 42 * t)
        
        # 3. Tremolo Shudder (Fluctuates the volume at 16Hz for a fast ground shake)
        rumble_shake = 0.4 + 0.6 * math.sin(2 * math.pi * 16 * t)
        modulated_rumble = sub_bass * rumble_shake
        
        # 4. Low-Frequency Noise (Crunchy baseline)
        noise = random.uniform(-0.5, 0.5)
        
        # Combine the elements
        raw_sample = (0.6 * modulated_rumble) + (0.25 * noise) + impact
        
        # 5. Simple Low-Pass Filter (Keeps it dark and heavy, removes bubble clicks)
        filtered_sample = 0.22 * raw_sample + 0.78 * prev_sample
        prev_sample = filtered_sample
        
        # 6. Drive & Soft Clipping (The "Oomph" / Saturation)
        # Multiplying pushes the signal into overdrive, math.tanh crushes it smoothly
        driven_sample = filtered_sample * 2.2
        clipped_sample = math.tanh(driven_sample)
        
        # Master volume envelope to trail off nicely
        envelope = math.pow(1.0 - (t / duration), 1.5)
        data.append(clipped_sample * envelope)
        
    return data


# --- Execution ---
if __name__ == "__main__":
    print("Generating classic arcade audio effects...")
    print("-" * 40)
    save_wav("player_laser.wav", make_laser())
    save_wav("alien_laser.wav", make_alien_laser())
    save_wav("alien_march.wav", make_march())
    save_wav("alien_explosion.wav", make_alien_explosion())
    save_wav("bunker_hit.wav", make_bunker_hit())
    save_wav("player_explosion.wav", make_player_explosion())
    print("-" * 40)
    print("Done! You can now drag these .wav files straight into Godot.")