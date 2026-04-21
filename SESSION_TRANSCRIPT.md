# imtest — Claude Code Session Transcript

**Dates:** 2026-04-12 to 2026-04-21  
**Project:** `/home/klejs/src/imtest/`  
**Stack:** C++17, SDL3, SDL3_ttf, Dear ImGui, CMake

---

## Session Timeline — All Changes Made

### 1. Initial Setup
- Started from a basic SDL3 + Dear ImGui hello-world app
- `CMakeLists.txt` builds ImGui from `~/src/ImGui`, links SDL3 and SDL3_ttf via pkg-config

### 2. TTF Text Overlay — "Sara"
- Added TTF font rendering to display the text "Sara" as a texture overlay
- Rendered using `TTF_RenderText_Blended` with system fonts (Noto, Liberation, Hack, Adwaita)
- Texture set to `SDL_BLENDMODE_BLEND` for transparency over the background

### 3. Animated Plasma Background
- Replaced the static background colour with an **animated plasma effect**
- Created a streaming `SDL_TEXTUREACCESS_STREAMING` texture at 1/4 window resolution for performance
- Plasma computed per-pixel each frame using sum-of-sines algorithm:
  - Multiple sine waves at different scales
  - Phase-shifted cosine palette for R, G, B colour mapping
  - Darkened to keep ImGui text readable
- Made the TTF text overlay transparent so plasma shows through

### 4. CLI-Configurable Overlay Text
- Changed the overlay text from hardcoded "Sara" to **first CLI argument**
- Defaults to `"Cyberpunk"` when no argument is given
- Renamed all internal variables from `sara_tex`/`sara_w`/`sara_h` to `text_tex`/`text_w`/`text_h`

### 5. Text Styling — Dark Cyan, More Opaque
- Changed TTF text colour to **dark cyan** (`{0, 180, 180, 220}`)
- Increased alpha from ~180 to 220 for more opacity

### 6. Bouncing Text (DVD Screensaver Style)
- Replaced static text positioning with **bouncing motion**
- Text moves at a random velocity and bounces off window edges
- Position clamped on window resize
- Only one instance of the text (no tiling)

### 7. Multiple Bouncers via ImGui Button
- Added `struct Bouncer` holding position, velocity, and colour per instance
- ImGui "Bouncer Control" window with **"Add Bouncer" button**
- Each click spawns a new bouncing text instance with random position/velocity
- Counter shows current number of bouncers

### 8. Randomised Bouncer Text Colour
- Each bouncer gets a **random vivid colour** (RGB each in 100–255 range)
- Text rendered in white so `SDL_SetTextureColorMod` can tint it per-bouncer
- Colour mod reset to white after drawing all bouncers

### 9. Animated Plasma X/Y Properties
- Added drift, scale breathing, rotation, and swirl warp to the plasma:
  - `drift_x/y` — field drifts horizontally/vertically over time
  - `scale_x/y` — spatial frequency breathes in/out
  - `rot_speed` — coordinate rotation
  - `warp_base/amp/speed` — swirl distortion based on distance from centre
  - `swirl_dist_mul` — distance multiplier for swirl intensity

### 10. Randomised Plasma Parameters at Startup
- Created `struct PlasmaParams` holding all tuneable plasma parameters
- `randomise_plasma()` generates random values for all params at startup
- Each run looks unique — different colours, speeds, spatial patterns

### 11. Separate Randomise Buttons for Palette and X/Y
- Split randomisation into two helpers:
  - `randomise_plasma_palette(p)` — re-rolls colour phase offsets and darkening
  - `randomise_plasma_xy(p)` — re-rolls drift, scale, rotation, swirl params
- Added two buttons in ImGui: **"Randomise Palette"** and **"Randomise X/Y"**

### 12. Rolling Palette Checkbox + Speed Slider
- Added **"Roll Palette" checkbox** — when enabled, continuously rotates the colour phase offsets each frame
- Each RGB channel advances at a slightly different rate (1.0×, 0.7×, 1.3×) for organic colour cycling
- **"Roll Speed" slider** (0.05–3.0) controls how fast the palette rotates
- Slider only visible when checkbox is ticked

---

## Current Feature Summary

The app (`./imtest [text]`) displays:

1. **Animated plasma background** at 1/4 resolution, stretched to fill the window
   - Sum-of-sines plasma with drift, rotation, swirl warp
   - Phase-shifted cosine palette (randomised at startup)
   - Optional rolling palette animation

2. **Bouncing TTF text** (DVD screensaver style)
   - Text from CLI argument (default: "Cyberpunk")
   - Multiple instances, each with random colour, position, velocity
   - Bounce off window edges

3. **ImGui control panel** ("Bouncer Control"):
   - "Add Bouncer" button + instance count
   - "Randomise Palette" / "Randomise X/Y" buttons
   - "Roll Palette" checkbox + speed slider
   - FPS counter

---

## Files

| File | Purpose |
|------|---------|
| `main.cpp` | Entire application (~500 lines) |
| `CMakeLists.txt` | Build configuration |
| `build/` | CMake build directory |

## Build

```bash
cd build
cmake ..
make -j$(nproc)
./imtest          # uses "Cyberpunk"
./imtest "Hello"  # custom text
```

## Dependencies

- SDL3 (system-installed, pkg-config)
- SDL3_ttf (system-installed, pkg-config)
- Dear ImGui (from `~/src/ImGui`)
- System TTF fonts (Noto, Liberation, Hack, or Adwaita)
