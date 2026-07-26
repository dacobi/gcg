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

def make_note(freq, duration, volume=0.25):
    num_samples = int(SAMPLE_RATE * duration)
    data = []
    phase = 0.0
    for i in range(num_samples):
        t = i / SAMPLE_RATE
        phase += (2 * math.pi * freq) / SAMPLE_RATE
        
        # 50% triangle, 50% square for retro synth brass crunch
        tri_part = 2.0 * abs(2.0 * (t * freq - math.floor(t * freq + 0.5))) - 1.0
        square_part = 1.0 if math.sin(phase) >= 0 else -1.0
        sample = volume * (0.6 * tri_part + 0.4 * square_part)
        
        # 6-bit Bitcrush / quantization
        sample = round(sample * 32.0) / 32.0
        
        # Attack (10ms)
        if i < int(SAMPLE_RATE * 0.01):
            sample *= (i / (SAMPLE_RATE * 0.01))
        # Release (last 50ms)
        elif i > num_samples - int(SAMPLE_RATE * 0.05):
            sample *= (num_samples - i) / (SAMPLE_RATE * 0.05)
            
        data.append(sample)
    return data

def make_victory_fanfare():
    # Triumphant arpeggio: C5 -> E5 -> G5 -> C6 -> E6 -> G6 -> C7
    arpeggio_freqs = [523.25, 659.25, 783.99, 1046.50, 1318.51, 1567.98, 2093.00]
    data = []
    
    # Run up arpeggio (70ms per note)
    for f in arpeggio_freqs:
        data.extend(make_note(f, 0.07, 0.28))
        
    # Triumphant final chord hold (C5 + E5 + G5 + C6) for 1.8 seconds
    chord_freqs = [523.25, 659.25, 783.99, 1046.50]
    duration = 1.8
    num_samples = int(SAMPLE_RATE * duration)
    chord_data = [0.0] * num_samples
    
    for f in chord_freqs:
        note_data = make_note(f, duration, 0.12)
        for i in range(num_samples):
            chord_data[i] += note_data[i]
            
    # Add shimmer/tremolo effect (6 Hz beat) to chord
    for i in range(num_samples):
        t = i / SAMPLE_RATE
        tremolo = 0.8 + 0.2 * math.sin(2 * math.pi * 6.0 * t)
        chord_data[i] *= tremolo
        
    data.extend(chord_data)
    
    # Add simple echo/delay (shimmering delay at 150ms)
    delay_samples = int(SAMPLE_RATE * 0.15)
    final_data = list(data)
    for i in range(delay_samples, len(final_data)):
        final_data[i] += data[i - delay_samples] * 0.35
        
    # Normalize and clip
    max_val = max(abs(s) for s in final_data) if final_data else 1.0
    if max_val > 0.9:
        final_data = [s * (0.9 / max_val) for s in final_data]
        
    return final_data

if __name__ == "__main__":
    print("Generating synthwave victory fanfare...")
    save_wav("victory_jingle.wav", make_victory_fanfare())
