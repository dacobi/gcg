// ---------------------------------------------------------------------------
// Animated plasma: write directly into a streaming texture each frame
// ---------------------------------------------------------------------------

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3_image/SDL_image.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <algorithm>
#include <vector>
#include <deque>
#include <map>
#include <iostream>
#include <tuple>
#include <regex>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <chrono>

bool plasma_render_tiles = true;
float cur_rel = 800/600;

struct PlasmaParams {
    // X/Y animation speeds (used as multipliers on time)
    float drift_speed_x;   // how fast the field drifts horizontally
    float drift_speed_y;   // how fast the field drifts vertically
    float drift_amp;       // amplitude of the drift

    float scale_base_x;    // base spatial frequency X
    float scale_base_y;    // base spatial frequency Y
    float scale_mod_amp;   // breathing amplitude
    float scale_mod_speed_x;
    float scale_mod_speed_y;

    float rot_speed;       // rotation speed
    float warp_base;       // swirl base intensity
    float warp_amp;        // swirl modulation amplitude
    float warp_speed;      // swirl modulation speed
    float swirl_dist_mul;  // distance multiplier for swirl

    // Palette: three phase offsets for R, G, B colour cosines
    float palette_phase_r;
    float palette_phase_g;
    float palette_phase_b;

    // Darkening multipliers (kept moderate so ImGui stays readable)
    float darken_r;
    float darken_g;
    float darken_b;

    float tile_count;
};

static void update_plasma_texture1(SDL_Texture* tex, int w, int h, float t,
                                  const PlasmaParams& p)
{
    void* pixels = nullptr;
    int   pitch  = 0;
    if (!SDL_LockTexture(tex, nullptr, &pixels, &pitch))
        return;

    auto* dst = static_cast<Uint8*>(pixels);

    // Animated X/Y properties driven by randomised parameters
    float drift_x  = p.drift_amp * std::sin(t * p.drift_speed_x);
    float drift_y  = p.drift_amp * std::cos(t * p.drift_speed_y);
    float scale_x  = p.scale_base_x + p.scale_mod_amp * std::sin(t * p.scale_mod_speed_x);
    float scale_y  = p.scale_base_y + p.scale_mod_amp * std::cos(t * p.scale_mod_speed_y);
    float rot_sin  = std::sin(t * p.rot_speed);
    float rot_cos  = std::cos(t * p.rot_speed);
    float warp_str = p.warp_base + p.warp_amp * std::sin(t * p.warp_speed);

    float r,g,b;

    for (int y = 0; y < h; ++y) {
        auto* row = reinterpret_cast<Uint32*>(dst + y * pitch);
        float fy = static_cast<float>(y) / static_cast<float>(h);
        for (int x = 0; x < w; ++x) {
            float fx = static_cast<float>(x) / static_cast<float>(w);



             // --- NEW TILE LOGIC ---
            // Snap fx and fy to a fixed grid based on tile_count
            
            if(plasma_render_tiles){
            
                if (p.tile_count > 0.0f) {
                    fy = std::floor(fy * p.tile_count) / (p.tile_count);
                    fx = std::floor(fx * (p.tile_count * cur_rel)) / (p.tile_count * cur_rel);
                        
                }
            }
            // ----------------------

            // Now all calculations for rx, ry, wx, wy, and v 
            // will stay constant for every pixel inside the same tile.
            if(true){

// --- NEW CURVY/JACKY LOGIC ---
        // 1. Create a "Jagged" distortion for the edges
        // Using a high-frequency sine based on X/Y to 'jitter' the lookup
        float jitter = std::sin(fx * 200.0f) * std::cos(fy * 200.0f) * 0.01f;
        
        // 2. Create a "Curvy" warp (Domain Warping)
        // This bends the coordinates before they even hit the plasma math
        float curve_x = std::sin(fy * 10.0f + t) * 0.05f;
        float curve_y = std::cos(fx * 10.0f + t) * 0.05f;

        // Apply these to our base fx/fy
        float warped_fx = fx + curve_x + jitter;
        float warped_fy = fy + curve_y + jitter;
        // ------------------------------

        // Apply drift + rotation using the NEW warped coordinates
        float cx = warped_fx - 0.5f;
        float cy = warped_fy - 0.5f;
        float rx = cx * rot_cos - cy * rot_sin + 0.5f + drift_x * 0.1f;
        float ry = cx * rot_sin + cy * rot_cos + 0.5f + drift_y * 0.1f;

        // Swirl warp
        float dist = std::sqrt(cx * cx + cy * cy);
        float swirl_angle = dist * p.swirl_dist_mul * warp_str;
        float sw_sin = std::sin(swirl_angle);
        float sw_cos = std::cos(swirl_angle);
        float wx = (rx - 0.5f) * sw_cos - (ry - 0.5f) * sw_sin + 0.5f;
        float wy = (rx - 0.5f) * sw_sin + (ry - 0.5f) * sw_cos + 0.5f;

        // Sum of sine plasma waves
        float v = 0.0f;
        v += std::sin(wx * scale_x + t);
        v += std::sin((wy * scale_y + t) * 0.7f);
        v += std::sin((wx * scale_x + wy * scale_y + t) * 0.5f);
        v += std::sin(std::sqrt(wx * wx * 100.0f + wy * wy * 100.0f) + t);
        v *= 0.25f; 

        // --- POSTERIZATION (The "Jacky" Edge Step) ---
        // Instead of smooth gradients, force 'v' into sharp bands.
        // Lower numbers = fewer, bigger, more jagged structures.
        float num_bands = 6.0f; 
        v = std::floor(v * num_bands) / num_bands;
        // ----------------------------------------------

        // Map to color (stays the same)
         r = 0.5f + 0.5f * std::cos(3.14159f * (v + p.palette_phase_r));
         g = 0.5f + 0.5f * std::cos(3.14159f * (v + p.palette_phase_g));
         b = 0.5f + 0.5f * std::cos(3.14159f * (v + p.palette_phase_b));
            } else {


            // Apply drift + rotation to coordinates
            float cx = fx - 0.5f;
            float cy = fy - 0.5f;
            float rx = cx * rot_cos - cy * rot_sin + 0.5f + drift_x * 0.1f;
            float ry = cx * rot_sin + cy * rot_cos + 0.5f + drift_y * 0.1f;

            // Swirl warp — displaces coordinates based on distance from centre
            float dist = std::sqrt(cx * cx + cy * cy);
            float swirl_angle = dist * p.swirl_dist_mul * warp_str;
            float sw_sin = std::sin(swirl_angle);
            float sw_cos = std::cos(swirl_angle);
            float wx = (rx - 0.5f) * sw_cos - (ry - 0.5f) * sw_sin + 0.5f;
            float wy = (rx - 0.5f) * sw_sin + (ry - 0.5f) * sw_cos + 0.5f;

            // sum of several sine plasma waves with animated scale
            float v = 0.0f;
            v += std::sin(wx * scale_x + t);
            v += std::sin((wy * scale_y + t) * 0.7f);
            v += std::sin((wx * scale_x + wy * scale_y + t) * 0.5f);
            v += std::sin(std::sqrt(wx * wx * 100.0f + wy * wy * 100.0f) + t);
            v *= 0.25f; // normalise to roughly [-1, 1]

            // map to colour via phase-shifted cosines (randomised palette)
             r = 0.5f + 0.5f * std::cos(3.14159f * (v + p.palette_phase_r));
             g = 0.5f + 0.5f * std::cos(3.14159f * (v + p.palette_phase_g));
             b = 0.5f + 0.5f * std::cos(3.14159f * (v + p.palette_phase_b));

            }

            // darken so ImGui windows stay readable
            r *= p.darken_r;
            g *= p.darken_g;
            b *= p.darken_b;

            Uint8 R = static_cast<Uint8>(r * 255.0f);
            Uint8 G = static_cast<Uint8>(g * 255.0f);
            Uint8 B = static_cast<Uint8>(b * 255.0f);

            // ARGB8888
            row[x] = (0xFFu << 24) | (R << 16) | (G << 8) | B;
        }
    }

    SDL_UnlockTexture(tex);
}
    
 static void update_plasma_texture2(SDL_Texture* tex, int w, int h, float t,
                                  const PlasmaParams& p)
{
    void* pixels = nullptr;
    int   pitch  = 0;
    // Safety check: If lock fails, we can't draw
    if (!SDL_LockTexture(tex, nullptr, &pixels, &pitch) != 0)
        return;

    auto* dst = static_cast<Uint8*>(pixels);

    for (int y = 0; y < h; ++y) {
        Uint32* row = reinterpret_cast<Uint32*>(dst + y * pitch);
        float fy = static_cast<float>(y) / static_cast<float>(h);
        
        for (int x = 0; x < w; ++x) {
            float fx = static_cast<float>(x) / static_cast<float>(w);

    int    fps        = 60;
    int    frame_count = 0;
    std::string output_path;

    w &= ~1;
            // 1. THE JAGGED JITTER (The "Electric" serration)
            // Using very high frequency sines to "shake" the pixel lookup.
            // We use 't * 10' for that fast, electrical buzzing movement.
            float jitter = std::sin(fx * 120.0f + t * 10.0f) * 0.02f;
            float wx = fx + jitter;
            float wy = fy + jitter;

            // 2. THE RIDGE MATH
            // Instead of standard smooth sine, we use a "V" shape
            // (1.0 - abs(sin)) creates the sharp "filament" look.
            float v1 = 1.0f - std::abs(std::sin(wx * p.scale_base_x + t));
            float v2 = 1.0f - std::abs(std::sin(wy * p.scale_base_y - t));
            
            float v = (v1 + v2) * 0.5f;

            // 3. THE "DOMAIN" SHARPENING
            // We multiply v by itself to make the dark areas darker 
            // and the "lightning" lines pop.
            float electric_v = v * v * v;

            // 4. COLORING
            // Standard cosine palette using our jagged electric_v
            float r = 0.5f + 0.5f * std::cos(3.14159f * (electric_v + p.palette_phase_r));
            float g = 0.5f + 0.5f * std::cos(3.14159f * (electric_v + p.palette_phase_g));
            float b = 0.5f + 0.5f * std::cos(3.14159f * (electric_v + p.palette_phase_b));

            // 5. FINAL PIXEL ASSEMBLY
            // FORCE ALPHA TO 255 (0xFFu) to overwrite billboard shadows.
            Uint8 R = static_cast<Uint8>(r * p.darken_r * 255.0f);
            Uint8 G = static_cast<Uint8>(g * p.darken_g * 255.0f);
            Uint8 B = static_cast<Uint8>(b * p.darken_b * 255.0f);

            // Write: Alpha is ALWAYS 255.
            row[x] = (0xFFu << 24) | (R << 16) | (G << 8) | B;
        }
    }

    SDL_UnlockTexture(tex);
}
    
    
static void update_plasma_texture3(SDL_Texture* tex, int w, int h, float t,
                                  const PlasmaParams& p)
{
    void* pixels = nullptr;
    int   pitch  = 0;
    if (!SDL_LockTexture(tex, nullptr, &pixels, &pitch) != 0)
        return;

    auto* dst = static_cast<Uint8*>(pixels);

    // Hardcode some defaults if params are zero to ensure we see something
    float sX = (p.scale_base_x == 0.0f) ? 10.0f : p.scale_base_x;
    float sY = (p.scale_base_y == 0.0f) ? 10.0f : p.scale_base_y;

    for (int y = 0; y < h; ++y) {
        // Calculate row pointer
        Uint32* row = reinterpret_cast<Uint32*>(dst + y * pitch);
        float fy = static_cast<float>(y) / static_cast<float>(h);
        
        for (int x = 0; x < w; ++x) {
            float fx = static_cast<float>(x) / static_cast<float>(w);

            // 1. JITTERED COORDINATES
            // The "Lightning" look comes from high-frequency offsets
            float jitter = std::sin(fx * 40.0f + fy * 40.0f + t * 3.0f) * 0.01f;
            float wx = fx + jitter;
            float wy = fy + jitter;

            // 2. THE "JACKY" STRUCTURES (Ridge Plasma)
            // Use abs() to create sharp peaks, then combine them
            float v = 0.0f;
            v += 1.0f - std::abs(std::sin(wx * sX + t));
            v += 1.0f - std::abs(std::sin(wy * sY - t));
            v += 1.0f - std::abs(std::sin((wx + wy) * (sX * 0.5f) + t * 0.5f));
            v /= 3.0f; // Range is now 0.0 to 1.0

            // 3. ENHANCE EDGES (The threshold)
            // This makes the "lightning" domains. 
            // If v is high, it stays bright. If low, it goes black.
            v = (v > 0.5f) ? (v - 0.5f) * 2.0f : 0.0f;

            // 4. COLORING
            // Create a sharp electric blue/purple palette
            float r_f = 0.5f + 0.5f * std::cos(3.14f * (v + p.palette_phase_r));
            float g_f = 0.5f + 0.5f * std::cos(3.14f * (v + p.palette_phase_g));
            float b_f = 0.5f + 0.5f * std::cos(3.14f * (v + p.palette_phase_b));

            // Convert to 0-255 and APPLY v to Alpha or Brightness
            // We multiply by 255 and p.darken to respect your settings
            Uint8 R = static_cast<Uint8>(r_f * v * p.darken_r * 255.0f);
            Uint8 G = static_cast<Uint8>(g_f * v * p.darken_g * 255.0f);
            Uint8 B = static_cast<Uint8>(b_f * v * p.darken_b * 255.0f);

            // 5. OUTPUT (Force Alpha to 255 to prevent transparency issues)
            row[x] = (0xFFu << 24) | (R << 16) | (G << 8) | B;
        }
    }

    SDL_UnlockTexture(tex);
}


static void update_plasma_texture4(SDL_Texture* tex, int w, int h, float t,
                                  const PlasmaParams& p)
{
    void* pixels = nullptr;
    int   pitch  = 0;
    if (!SDL_LockTexture(tex, nullptr, &pixels, &pitch) != 0)
        return;

    auto* dst = static_cast<Uint8*>(pixels);

    // Drive the "electrical interference" speeds
    float drift_x  = p.drift_amp * std::sin(t * p.drift_speed_x);
    float drift_y  = p.drift_amp * std::cos(t * p.drift_speed_y);
    float scale_x  = p.scale_base_x + p.scale_mod_amp * std::sin(t * p.scale_mod_speed_x);
    float scale_y  = p.scale_base_y + p.scale_mod_amp * std::cos(t * p.scale_mod_speed_y);
    
    // Rotational components
    float rot_sin  = std::sin(t * p.rot_speed);
    float rot_cos  = std::cos(t * p.rot_speed);

    for (int y = 0; y < h; ++y) {
        auto* row = reinterpret_cast<Uint32*>(dst + y * pitch);
        float fy = static_cast<float>(y) / static_cast<float>(h);
        
        for (int x = 0; x < w; ++x) {
            float fx = static_cast<float>(x) / static_cast<float>(w);

            // 1. COORDINATE WARPING (The "Curvy/Lightning" Path)
            // We distort the lookup space to make the lines "wiggle"
            float cx = fx - 0.5f;
            float cy = fy - 0.5f;
            
            // Initial Rotation + Drift
            float rx = cx * rot_cos - cy * rot_sin + 0.5f + drift_x;
            float ry = cx * rot_sin + cy * rot_cos + 0.5f + drift_y;

            // Secondary high-frequency distortion (The "Jagged" edge)
            // Using nested sines to create a pseudo-fractal warp
            float warp_noise = std::sin(rx * 15.0f + t) * std::cos(ry * 15.0f - t);
            float wx = rx + warp_noise * 0.05f;
            float wy = ry + warp_noise * 0.05f;

            // 2. GENERATE JAGGED RIDGES
            // Instead of: v = sin(x)
            // We use: v = 1.0 - abs(sin(x)) to get sharp "peaks"
            float v = 0.0f;
            
            // Layer 1: Horizontal arcs
            v += 1.0f - std::abs(std::sin(wx * scale_x + t));
            
            // Layer 2: Vertical arcs (offset frequency)
            v += 1.0f - std::abs(std::sin(wy * scale_y * 1.2f - t * 0.8f));
            
            // Layer 3: Diagonal interference
            v += 1.0f - std::abs(std::sin((wx + wy) * scale_x * 0.5f + t));

            // Layer 4: Circular "Burst"
            float dist = std::sqrt((wx-0.5f)*(wx-0.5f) + (wy-0.5f)*(wy-0.5f));
            v += 1.0f - std::abs(std::sin(dist * 20.0f - t * 2.0f));

            v /= 4.0f; // Average the layers

            // 3. SHARPEN & CONSTRICT
            // Raising to a power makes the "lightning" thinner and more intense
            v = std::pow(v, 3.5f); 

            // 4. COLOR MAPPING
            // Use the calculated 'v' to drive the phase-shifted palette
            float r = 0.5f + 0.5f * std::cos(3.14159f * (v + p.palette_phase_r));
            float g = 0.5f + 0.5f * std::cos(3.14159f * (v + p.palette_phase_g));
            float b = 0.5f + 0.5f * std::cos(3.14159f * (v + p.palette_phase_b));

            // Apply darkening and brightness boost to the arcs
            float brightness_boost = 1.8f; //p.darken_b *
            Uint8 R = static_cast<Uint8>(std::clamp(r * v * brightness_boost * 255.0f, 0.0f, 255.0f));
            Uint8 G = static_cast<Uint8>(std::clamp(g * v * brightness_boost * 255.0f, 0.0f, 255.0f));
            Uint8 B = static_cast<Uint8>(std::clamp(b * v * brightness_boost *  255.0f, 0.0f, 255.0f));

            // ARGB8888 Write
            row[x] = (0xFFu << 24) | (R << 16) | (G << 8) | B;
        }
    }

    SDL_UnlockTexture(tex);
}

static void update_plasma_texture5(SDL_Texture* tex, int w, int h, float t,
                                  const PlasmaParams& p)
{
    void* pixels = nullptr;
    int   pitch  = 0;
    
    // The "!" is back! 
    if (!SDL_LockTexture(tex, nullptr, &pixels, &pitch) != 0)
        return;

    auto* dst = static_cast<Uint8*>(pixels);

    for (int y = 0; y < h; ++y) {
        Uint32* row = reinterpret_cast<Uint32*>(dst + y * pitch);
        float fy = static_cast<float>(y) / static_cast<float>(h);
        
        for (int x = 0; x < w; ++x) {
            float fx = static_cast<float>(x) / static_cast<float>(w);

            // 1. JAGGED JITTER (The "Electric" serration)
            // A very fast, high-frequency "buzz" to the coordinates.
            float jitter = std::sin(fx * 150.0f + t * 20.0f) * 0.012f;
            float wx = fx + jitter;
            float wy = fy + jitter;

            // 2. RIDGE MATH (Sharp Lightning Arcs)
            // 1.0 - abs(sin) creates a sharp "peak" instead of a smooth wave.
            float v1 = 1.0f - std::abs(std::sin(wx * p.scale_base_x + t));
            float v2 = 1.0f - std::abs(std::sin(wy * p.scale_base_y - t * 1.3f));
            float v3 = 1.0f - std::abs(std::sin((wx + wy) * (p.scale_base_x * 0.4f) + t));
            
            float v = (v1 + v2 + v3) / 3.0f;

            // 3. ELECTRIC CONTRAST
            // Pushing the value into "domains" so you get clear paths.
            // Adjust the 0.4f to control the thickness of the "bolts".
            float electric_v = std::max(0.0f, v - 0.4f) * 1.8f;

            // 4. COLORING
            float r_val = 0.5f + 0.5f * std::cos(3.14159f * (electric_v + p.palette_phase_r));
            float g_val = 0.5f + 0.5f * std::cos(3.14159f * (electric_v + p.palette_phase_g));
            float b_val = 0.5f + 0.5f * std::cos(3.14159f * (electric_v + p.palette_phase_b));

            // 5. FINAL PIXEL (ARGB8888)
            // Multiplying by electric_v again keeps the "background" dark.
            Uint8 R = static_cast<Uint8>(r_val * electric_v  * 255.0f);
            Uint8 G = static_cast<Uint8>(g_val * electric_v  * 255.0f);
            Uint8 B = static_cast<Uint8>(b_val * electric_v  * 255.0f);

            row[x] = (0xFFu << 24) | (R << 16) | (G << 8) | B;
        }
    }

    SDL_UnlockTexture(tex);
}

static void update_plasma_texture6(SDL_Texture* tex, int w, int h, float t,
                                  const PlasmaParams& p)
{
    void* pixels = nullptr;
    int   pitch  = 0;
    if (!SDL_LockTexture(tex, nullptr, &pixels, &pitch) != 0)
        return;

    auto* dst = static_cast<Uint8*>(pixels);

    // Speed constants for the "electric" jitter
    float drift_x  = p.drift_amp * std::sin(t * p.drift_speed_x);
    float drift_y  = p.drift_amp * std::cos(t * p.drift_speed_y);
    float rot_sin  = std::sin(t * p.rot_speed);
    float rot_cos  = std::cos(t * p.rot_speed);

    for (int y = 0; y < h; ++y) {
        auto* row = reinterpret_cast<Uint32*>(dst + y * pitch);
        float fy = static_cast<float>(y) / static_cast<float>(h);
        
        for (int x = 0; x < w; ++x) {
            float fx = static_cast<float>(x) / static_cast<float>(w);

            // 1. COORDINATE WARP (The "Lightning" Path)
            float cx = fx - 0.5f;
            float cy = fy - 0.5f;
            
            // Rotate and Drift
            float rx = cx * rot_cos - cy * rot_sin + 0.5f + drift_x;
            float ry = cx * rot_sin + cy * rot_cos + 0.5f + drift_y;

            // JAGGED DISTORTION: We use a high-freq sine to "jitter" the lookups
            // This creates the broken, electric edges.
            float jitter = std::sin(rx * 50.0f + t * 2.0f) * 0.02f;
            float wx = rx + jitter;
            float wy = ry + jitter;

            // 2. RIDGE CALCULATION (The "Electric" Arcs)
            // We use the absolute value of sines to create sharp peaks
            float v1 = 1.0f - std::abs(std::sin(wx * p.scale_base_x + t));
            float v2 = 1.0f - std::abs(std::sin(wy * p.scale_base_y - t * 0.5f));
            float v3 = 1.0f - std::abs(std::sin((wx + wy) * (p.scale_base_x * 0.5f) + t));
            
            // Combine them
            float v = (v1 + v2 + v3) / 3.0f;

            // 3. SHARPEN THE EDGES
            // Instead of pow (which is risky), we use a sharp contrast curve.
            // This keeps values between 0 and 1 but makes them "punchy".
            v = std::max(0.0f, v - 0.4f) * 1.6f; 

            // 4. COLOR MAPPING
            // We drive the cosine palette using our jagged value 'v'
            float r_val = 0.5f + 0.5f * std::cos(3.14159f * (v + p.palette_phase_r));
            float g_val = 0.5f + 0.5f * std::cos(3.14159f * (v + p.palette_phase_g));
            float b_val = 0.5f + 0.5f * std::cos(3.14159f * (v + p.palette_phase_b));

            // Apply darkening and scale back to 255
            // Multiply by 'v' again to make the "background" between lightning dark
            Uint8 R = static_cast<Uint8>(std::min(255.0f, r_val * v  * 255.0f));
            Uint8 G = static_cast<Uint8>(std::min(255.0f, g_val * v  * 255.0f));
            Uint8 B = static_cast<Uint8>(std::min(255.0f, b_val * v  * 255.0f));

            // ARGB8888
            row[x] = (0xFFu << 24) | (R << 16) | (G << 8) | B;
        }
    }

    SDL_UnlockTexture(tex);
}

static void update_plasma_texture7(SDL_Texture* tex, int w, int h, float t,
                                  const PlasmaParams& p)
{
    void* pixels = nullptr;
    int pitch = 0;
    // Back to the working logic!
    if (!SDL_LockTexture(tex, nullptr, &pixels, &pitch) != 0)
        return;

    auto* dst = static_cast<Uint8*>(pixels);

    float drift_x = p.drift_amp * std::sin(t * p.drift_speed_x);
    float drift_y = p.drift_amp * std::cos(t * p.drift_speed_y);
    float rot_sin = std::sin(t * p.rot_speed);
    float rot_cos = std::cos(t * p.rot_speed);

    for (int y = 0; y < h; ++y) {
        auto* row = reinterpret_cast<Uint32*>(dst + y * pitch);
        float fy = static_cast<float>(y) / static_cast<float>(h);
        
        for (int x = 0; x < w; ++x) {
            float fx = static_cast<float>(x) / static_cast<float>(w);

            float cx = fx - 0.5f;
            float cy = fy - 0.5f;
            
            float rx = cx * rot_cos - cy * rot_sin + 0.5f + drift_x;
            float ry = cx * rot_sin + cy * rot_cos + 0.5f + drift_y;

            // 1. HIGH-VOLTAGE JITTER
            // Mixing two high frequencies (130 and 210) creates that non-repeating "crackling" edge.
            float buzz = std::sin(rx * 130.0f + t * 15.0f) * std::cos(ry * 210.0f - t * 10.0f);
            float wx = rx + buzz * 0.015f;
            float wy = ry + buzz * 0.015f;

            // 2. RIDGE CALCULATION
            // We use standard plasma math but with the 1.0 - abs() "ridge" trick.
            float v1 = 1.0f - std::abs(std::sin(wx * p.scale_base_x + t));
            float v2 = 1.0f - std::abs(std::sin(wy * p.scale_base_y - t * 0.5f));
            float v3 = 1.0f - std::abs(std::sin((wx + wy) * (p.scale_base_x * 0.5f) + t));
            
            float v = (v1 + v2 + v3) / 3.0f;

            // 3. BOLT SHARPENING
            // Pushing the threshold higher (0.5 instead of 0.4) makes the "bolts" thinner.
            // Using a square (v*v) helps emphasize the bright centers of the arcs.
            float electric_v = std::max(0.0f, v - 0.5f) * 2.0f;
            electric_v *= electric_v; 

            // 4. COLOR MAPPING
            float r_val = 0.5f + 0.5f * std::cos(3.14159f * (electric_v + p.palette_phase_r));
            float g_val = 0.5f + 0.5f * std::cos(3.14159f * (electric_v + p.palette_phase_g));
            float b_val = 0.5f + 0.5f * std::cos(3.14159f * (electric_v + p.palette_phase_b));

            // Final intensities
            Uint8 R = static_cast<Uint8>(std::min(255.0f, r_val * electric_v  * 255.0f));
            Uint8 G = static_cast<Uint8>(std::min(255.0f, g_val * electric_v  * 255.0f));
            Uint8 B = static_cast<Uint8>(std::min(255.0f, b_val * electric_v  * 255.0f));

            row[x] = (0xFFu << 24) | (R << 16) | (G << 8) | B;
        }
    }

    SDL_UnlockTexture(tex);
}

static void update_plasma_texture8(SDL_Texture* tex, int w, int h, float t,
                                  const PlasmaParams& p)
{
    void* pixels = nullptr;
    int pitch = 0;
    if (!SDL_LockTexture(tex, nullptr, &pixels, &pitch) != 0)
        return;

    auto* dst = static_cast<Uint8*>(pixels);

    float drift_x = p.drift_amp * std::sin(t * p.drift_speed_x);
    float drift_y = p.drift_amp * std::cos(t * p.drift_speed_y);
    float rot_sin = std::sin(t * p.rot_speed);
    float rot_cos = std::cos(t * p.rot_speed);

    for (int y = 0; y < h; ++y) {
        auto* row = reinterpret_cast<Uint32*>(dst + y * pitch);
        float fy = static_cast<float>(y) / static_cast<float>(h);
        
        for (int x = 0; x < w; ++x) {
            float fx = static_cast<float>(x) / static_cast<float>(w);

            // 1. HIGH-FREQ JITTER (The "Electric" serration)
            // Increased frequency for a sharper, more jagged "buzz"
            float jitter = std::sin(fx * 140.0f + t * 15.0f) * std::cos(fy * 240.0f - t * 10.0f) * 0.02f;
            
            float cx = (fx + jitter) - 0.5f;
            float cy = (fy + jitter) - 0.5f;

            // 2. COORDINATE TRANSFORM
            float rx = cx * rot_cos - cy * rot_sin + 0.5f + drift_x;
            float ry = cx * rot_sin + cy * rot_cos + 0.5f + drift_y;

            // 3. FULL-COLOR PLASMA MATH
            // We use standard sines here (not abs) to keep the full color range
            float v = 0.0f;
            v += std::sin(rx * p.scale_base_x + t);
            v += std::sin(ry * p.scale_base_y - t * 0.5f);
            v += std::sin((rx + ry) * (p.scale_base_x * 0.5f) + t);
            v += std::sin(std::sqrt(rx * rx + ry * ry) * 10.0f + t);
            v *= 0.25f; // Normalise to roughly [-1, 1]

            // 4. VIBRANT COLOR MAPPING
            // Use a higher multiplier (6.28 instead of 3.14) if you want the 
            // colors to cycle even faster/more intensely.
            float r_f = 0.5f + 0.5f * std::cos(6.28318f * (v + p.palette_phase_r));
            float g_f = 0.5f + 0.5f * std::cos(6.28318f * (v + p.palette_phase_g));
            float b_f = 0.5f + 0.5f * std::cos(6.28318f * (v + p.palette_phase_b));

            // 5. FINAL PIXEL (No Darkening, Full Opaque)
            Uint8 R = static_cast<Uint8>(r_f * 255.0f);
            Uint8 G = static_cast<Uint8>(g_f * 255.0f);
            Uint8 B = static_cast<Uint8>(b_f * 255.0f);

            row[x] = (0xFFu << 24) | (R << 16) | (G << 8) | B;
        }
    }

    SDL_UnlockTexture(tex);
}

