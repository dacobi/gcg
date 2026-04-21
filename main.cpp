// SDL3 + Dear ImGui with animated plasma background and transparent text overlay
// Usage: ./imtest [--record output.mp4] [--record-max N] [--no-maximize] [text...]
//   --record FILE     start recording frames to FILE on launch
//   --record-max N    max recording length in seconds (default 59)
//   --no-maximize     don't start the window maximized
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <algorithm>
#include <vector>

// ---------------------------------------------------------------------------
// FFmpeg recording — pipe raw RGBA frames to ffmpeg, produce mp4
// ---------------------------------------------------------------------------
struct Recorder {
    FILE*  pipe       = nullptr;
    int    width      = 0;
    int    height     = 0;
    int    fps        = 60;
    int    frame_count = 0;
    std::string output_path;
};

static bool recorder_start(Recorder& rec, int w, int h, const char* path, int fps = 60) {
    if (rec.pipe) return false; // already recording
    // h264 with yuv420p requires even dimensions
    w &= ~1;
    h &= ~1;
    if (w < 2) w = 2;
    if (h < 2) h = 2;
    rec.width  = w;
    rec.height = h;
    rec.fps    = fps;
    rec.frame_count = 0;
    rec.output_path = path;

    // Build ffmpeg command: read raw RGBA from stdin, encode to h264 mp4
    char cmd[1024];
    std::snprintf(cmd, sizeof(cmd),
        "ffmpeg -y -f rawvideo -pixel_format rgba -video_size %dx%d -framerate %d "
        "-i - -c:v libx264 -preset fast -crf 18 -pix_fmt yuv420p "
        "-movflags +faststart \"%s\"",
        w, h, fps, path);

    rec.pipe = popen(cmd, "w");
    if (!rec.pipe) {
        std::printf("Failed to start ffmpeg for recording\n");
        return false;
    }
    std::printf("Recording started: %s (%dx%d @ %d fps)\n", path, w, h, fps);
    return true;
}

static void recorder_feed_frame(Recorder& rec, SDL_Renderer* renderer) {
    if (!rec.pipe) return;

    // Read back the rendered frame
    SDL_Surface* surf = SDL_RenderReadPixels(renderer, nullptr);
    if (!surf) {
        std::printf("SDL_RenderReadPixels failed: %s\n", SDL_GetError());
        return;
    }

    // Convert to RGBA32 if needed
    SDL_Surface* rgba = SDL_ConvertSurface(surf, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(surf);
    if (!rgba) {
        std::printf("Surface conversion failed: %s\n", SDL_GetError());
        return;
    }

    // If the window was resized, scale the frame back to the original
    // recording dimensions so ffmpeg always receives a consistent size.
    SDL_Surface* final_surf = rgba;
    bool need_free_final = false;
    if (rgba->w != rec.width || rgba->h != rec.height) {
        SDL_Surface* scaled = SDL_CreateSurface(rec.width, rec.height, SDL_PIXELFORMAT_RGBA32);
        if (scaled) {
            SDL_BlitSurfaceScaled(rgba, nullptr, scaled, nullptr, SDL_SCALEMODE_LINEAR);
            SDL_DestroySurface(rgba);
            final_surf = scaled;
            need_free_final = true;
        }
        // If scaling failed, fall back to writing the mismatched frame
        // (better than dropping it entirely)
    }

    // Write raw pixels to ffmpeg pipe
    SDL_LockSurface(final_surf);
    size_t row_bytes = static_cast<size_t>(rec.width) * 4;
    auto* pixels = static_cast<const Uint8*>(final_surf->pixels);
    for (int y = 0; y < rec.height; ++y) {
        fwrite(pixels + y * final_surf->pitch, 1, row_bytes, rec.pipe);
    }
    SDL_UnlockSurface(final_surf);

    if (need_free_final)
        SDL_DestroySurface(final_surf);
    else
        SDL_DestroySurface(rgba);

    rec.frame_count++;
}

static void recorder_stop(Recorder& rec) {
    if (!rec.pipe) return;
    pclose(rec.pipe);
    rec.pipe = nullptr;
    std::printf("Recording stopped: %s (%d frames)\n", rec.output_path.c_str(), rec.frame_count);
}

// ---------------------------------------------------------------------------
// A pre-rendered text texture with its label and dimensions
// ---------------------------------------------------------------------------
struct TextEntry {
    std::string   label;   // the original text string
    SDL_Texture*  tex;
    int           w, h;
};

// ---------------------------------------------------------------------------
// A single bouncing text instance
// ---------------------------------------------------------------------------
struct Bouncer {
    float x, y;
    float vx, vy;
    Uint8 r, g, b;   // random tint colour
    SDL_Texture* tex; // which text texture to use (not owned — shared)
    int tw, th;       // dimensions of that texture
};

// Helper: random float in [lo, hi]
static float rand_range(float lo, float hi) {
    return lo + static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * (hi - lo);
}

// Spawn a new bouncer with random position & velocity
static Bouncer make_bouncer(int win_w, int win_h, SDL_Texture* tex, int tw, int th) {
    Bouncer b;
    float max_x = static_cast<float>(win_w - tw);
    float max_y = static_cast<float>(win_h - th);
    if (max_x < 0) max_x = 0;
    if (max_y < 0) max_y = 0;
    b.x  = rand_range(0, max_x);
    b.y  = rand_range(0, max_y);
    // Random speed 100–350 px/s, random direction
    b.vx = rand_range(100.0f, 350.0f) * (std::rand() % 2 ? 1.0f : -1.0f);
    b.vy = rand_range(100.0f, 350.0f) * (std::rand() % 2 ? 1.0f : -1.0f);
    // Random vivid colour (at least one channel bright, avoid dark/muddy)
    b.r = static_cast<Uint8>(100 + std::rand() % 156);
    b.g = static_cast<Uint8>(100 + std::rand() % 156);
    b.b = static_cast<Uint8>(100 + std::rand() % 156);
    b.tex = tex;
    b.tw  = tw;
    b.th  = th;
    return b;
}

// ---------------------------------------------------------------------------
// Plasma parameters — randomised once at startup for a unique look each run
// ---------------------------------------------------------------------------
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
};

static PlasmaParams randomise_plasma() {
    PlasmaParams p;
    p.drift_speed_x    = rand_range(0.15f, 0.60f);
    p.drift_speed_y    = rand_range(0.15f, 0.60f);
    p.drift_amp        = rand_range(1.0f, 3.5f);

    p.scale_base_x     = rand_range(6.0f, 16.0f);
    p.scale_base_y     = rand_range(6.0f, 16.0f);
    p.scale_mod_amp    = rand_range(1.0f, 5.0f);
    p.scale_mod_speed_x = rand_range(0.10f, 0.40f);
    p.scale_mod_speed_y = rand_range(0.10f, 0.40f);

    p.rot_speed         = rand_range(0.05f, 0.25f);
    p.warp_base         = rand_range(0.05f, 0.25f);
    p.warp_amp          = rand_range(0.05f, 0.20f);
    p.warp_speed        = rand_range(0.20f, 0.60f);
    p.swirl_dist_mul    = rand_range(3.0f, 10.0f);

    // Random palette — each phase in [0, 1) gives wildly different colour combos
    p.palette_phase_r   = rand_range(0.0f, 1.0f);
    p.palette_phase_g   = rand_range(0.0f, 1.0f);
    p.palette_phase_b   = rand_range(0.0f, 1.0f);

    // Darkening: keep each channel between 0.25 and 0.60 so it's visible but not blinding
    p.darken_r          = rand_range(0.25f, 0.60f);
    p.darken_g          = rand_range(0.25f, 0.60f);
    p.darken_b          = rand_range(0.25f, 0.60f);

    return p;
}

// Re-randomise only the palette (colour) fields
static void randomise_plasma_palette(PlasmaParams& p) {
    p.palette_phase_r = rand_range(0.0f, 1.0f);
    p.palette_phase_g = rand_range(0.0f, 1.0f);
    p.palette_phase_b = rand_range(0.0f, 1.0f);
    p.darken_r        = rand_range(0.25f, 0.60f);
    p.darken_g        = rand_range(0.25f, 0.60f);
    p.darken_b        = rand_range(0.25f, 0.60f);
}

// Re-randomise only the X/Y spatial / animation fields
static void randomise_plasma_xy(PlasmaParams& p) {
    p.drift_speed_x     = rand_range(0.15f, 0.60f);
    p.drift_speed_y     = rand_range(0.15f, 0.60f);
    p.drift_amp         = rand_range(1.0f, 3.5f);
    p.scale_base_x      = rand_range(6.0f, 16.0f);
    p.scale_base_y      = rand_range(6.0f, 16.0f);
    p.scale_mod_amp     = rand_range(1.0f, 5.0f);
    p.scale_mod_speed_x = rand_range(0.10f, 0.40f);
    p.scale_mod_speed_y = rand_range(0.10f, 0.40f);
    p.rot_speed          = rand_range(0.05f, 0.25f);
    p.warp_base          = rand_range(0.05f, 0.25f);
    p.warp_amp           = rand_range(0.05f, 0.20f);
    p.warp_speed         = rand_range(0.20f, 0.60f);
    p.swirl_dist_mul     = rand_range(3.0f, 10.0f);
}

// ---------------------------------------------------------------------------
// Animated plasma: write directly into a streaming texture each frame
// ---------------------------------------------------------------------------
static void update_plasma_texture(SDL_Texture* tex, int w, int h, float t,
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

    for (int y = 0; y < h; ++y) {
        auto* row = reinterpret_cast<Uint32*>(dst + y * pitch);
        float fy = static_cast<float>(y) / static_cast<float>(h);
        for (int x = 0; x < w; ++x) {
            float fx = static_cast<float>(x) / static_cast<float>(w);

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
            float r = 0.5f + 0.5f * std::cos(3.14159f * (v + p.palette_phase_r));
            float g = 0.5f + 0.5f * std::cos(3.14159f * (v + p.palette_phase_g));
            float b = 0.5f + 0.5f * std::cos(3.14159f * (v + p.palette_phase_b));

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

// ---------------------------------------------------------------------------
// Single text texture — just the rendered text, no tiling
// Returns the texture; writes dimensions into *out_w / *out_h.
// ---------------------------------------------------------------------------
static SDL_Texture* create_text_texture(SDL_Renderer* renderer,
                                        const char* text,
                                        int* out_w, int* out_h)
{
    if (!TTF_Init()) {
        std::printf("TTF_Init error: %s\n", SDL_GetError());
        return nullptr;
    }

    const char* font_paths[] = {
        "/usr/share/fonts/noto/NotoSans-Bold.ttf",
        "/usr/share/fonts/liberation/LiberationSans-Bold.ttf",
        "/usr/share/fonts/TTF/Hack-Bold.ttf",
        "/usr/share/fonts/Adwaita/AdwaitaSans-Regular.ttf",
    };

    TTF_Font* font = nullptr;
    for (auto path : font_paths) {
        font = TTF_OpenFont(path, 120.0f);
        if (font) break;
    }
    if (!font) {
        std::printf("Could not open any font: %s\n", SDL_GetError());
        return nullptr;
    }

    // White text, semi-transparent — colour modulation will tint per-bouncer
    SDL_Color fg = {255, 255, 255, 200};
    SDL_Surface* text_surf = TTF_RenderText_Blended(font, text, 0, fg);
    if (!text_surf) {
        std::printf("TTF_RenderText_Blended error: %s\n", SDL_GetError());
        TTF_CloseFont(font);
        return nullptr;
    }

    *out_w = text_surf->w;
    *out_h = text_surf->h;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, text_surf);
    if (texture) {
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    }

    SDL_DestroySurface(text_surf);
    TTF_CloseFont(font);
    return texture;
}

// ---------------------------------------------------------------------------
int main(int argc, char** argv)
{
    // --- Parse CLI arguments ---
    // Usage: ./imtest [--record output.mp4] [text...]
    std::vector<std::string> cli_texts;
    std::string cli_record_path;
    int cli_record_max = -1;  // -1 = not specified on CLI
    bool cli_no_maximize = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--record") == 0 && i + 1 < argc) {
            cli_record_path = argv[++i];
        } else if (std::strcmp(argv[i], "--record-max") == 0 && i + 1 < argc) {
            cli_record_max = std::atoi(argv[++i]);
            if (cli_record_max < 1) cli_record_max = 1;
        } else if (std::strcmp(argv[i], "--no-maximize") == 0) {
            cli_no_maximize = true;
        } else {
            cli_texts.push_back(argv[i]);
        }
    }
    if (cli_texts.empty()) {
        cli_texts.push_back("Cyberpunk");
        cli_texts.push_back("Neon");
        cli_texts.push_back("Vaporwave");
        cli_texts.push_back("Synthwave");
        cli_texts.push_back("Retro");
        cli_texts.push_back("Glitch");
    }
    for (const auto& t : cli_texts)
        std::printf("Overlay text: \"%s\"\n", t.c_str());
    if (!cli_record_path.empty())
        std::printf("Will record to: %s\n", cli_record_path.c_str());

    // --- SDL init ---
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::printf("SDL_Init error: %s\n", SDL_GetError());
        return 1;
    }

    float scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());

    int win_w = static_cast<int>(1024 * scale);
    int win_h = static_cast<int>(768 * scale);

    SDL_WindowFlags win_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (!cli_no_maximize)
        win_flags |= SDL_WINDOW_MAXIMIZED;

    SDL_Window* window = SDL_CreateWindow(
        "SDL/ImGui Greeting Card",
        win_w, win_h,
        win_flags
    );
    if (!window) {
        std::printf("SDL_CreateWindow error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    SDL_SetRenderVSync(renderer, 1);
    if (!renderer) {
        std::printf("SDL_CreateRenderer error: %s\n", SDL_GetError());
        return 1;
    }

    // --- Plasma texture (streaming, updated every frame) ---
    // Use a reduced resolution for performance — will stretch to fill window
    int plasma_w = win_w / 4;
    int plasma_h = win_h / 4;
    SDL_Texture* plasma_tex = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
        plasma_w, plasma_h
    );

    // --- Pre-render a texture for each CLI text argument ---
    std::vector<TextEntry> cli_entries;
    for (const auto& t : cli_texts) {
        TextEntry e;
        e.label = t;
        e.tex = create_text_texture(renderer, t.c_str(), &e.w, &e.h);
        if (e.tex)
            cli_entries.push_back(std::move(e));
    }

    // Seed RNG and create one bouncer per CLI text
    std::srand(static_cast<unsigned>(SDL_GetPerformanceCounter()));
    std::vector<Bouncer> bouncers;
    for (const auto& e : cli_entries)
        bouncers.push_back(make_bouncer(win_w, win_h, e.tex, e.w, e.h));

    // Custom bouncer text — checkbox + input field state
    bool  use_custom_text = false;
    char  custom_text_buf[256] = "";

    // Keep track of all created textures so we can clean them up
    std::vector<SDL_Texture*> extra_textures;

    // Randomise plasma palette & X/Y properties for this run
    PlasmaParams plasma_params = randomise_plasma();

    int prev_win_w = win_w;
    int prev_win_h = win_h;

    // --- ImGui init ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(scale);
    style.FontScaleDpi = scale;

    // Make ImGui windows slightly transparent so background shows through
    style.Colors[ImGuiCol_WindowBg].w = 0.80f;

    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    // --- State ---
    bool  running = true;
    float time_acc = 0.0f;
    bool  roll_palette = false;
    float roll_palette_speed = 0.5f;  // how fast the palette phases rotate

    // Recording state
    Recorder recorder;
    char record_path_buf[256] = "output.mp4";
    float record_time = 0.0f;  // elapsed recording time in seconds
    float record_frame_accum = 0.0f;  // accumulator for fixed-rate frame capture
    bool  record_max_enabled = true;   // whether max-length auto-stop is active
    int   record_max_seconds = 59;     // max recording length in seconds

    // Apply CLI overrides for recording max
    if (cli_record_max > 0) {
        record_max_seconds = cli_record_max;
        record_max_enabled = true;
    }

    // Start recording immediately if --record was passed
    if (!cli_record_path.empty()) {
        std::snprintf(record_path_buf, sizeof(record_path_buf), "%s", cli_record_path.c_str());
        int out_w = 0, out_h = 0;
        SDL_GetRenderOutputSize(renderer, &out_w, &out_h);
        recorder_start(recorder, out_w, out_h, cli_record_path.c_str());
    }

    Uint64 last_ticks = SDL_GetPerformanceCounter();
    Uint64 freq       = SDL_GetPerformanceFrequency();

    // --- Main loop ---
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            ImGui_ImplSDL3_ProcessEvent(&ev);
            if (ev.type == SDL_EVENT_QUIT)
                running = false;
            if (ev.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                ev.window.windowID == SDL_GetWindowID(window))
                running = false;
        }

        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
            SDL_Delay(10);
            continue;
        }

        // Delta time
        Uint64 now = SDL_GetPerformanceCounter();
        float dt = static_cast<float>(now - last_ticks) / static_cast<float>(freq);
        last_ticks = now;
        time_acc += dt * 1.5f;  // speed multiplier for the plasma

        // Handle resize — recreate plasma texture when window size changes
        int cur_w, cur_h;
        SDL_GetWindowSize(window, &cur_w, &cur_h);
        if (cur_w != prev_win_w || cur_h != prev_win_h) {
            // Recreate plasma at new reduced size
            if (plasma_tex) SDL_DestroyTexture(plasma_tex);
            plasma_w = cur_w / 4;
            plasma_h = cur_h / 4;
            if (plasma_w < 1) plasma_w = 1;
            if (plasma_h < 1) plasma_h = 1;
            plasma_tex = SDL_CreateTexture(
                renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                plasma_w, plasma_h
            );

            // Clamp all bouncers so they stay inside the new window
            for (auto& b : bouncers) {
                float max_x = static_cast<float>(cur_w - b.tw);
                float max_y = static_cast<float>(cur_h - b.th);
                if (max_x < 0) max_x = 0;
                if (max_y < 0) max_y = 0;
                if (b.x > max_x) b.x = max_x;
                if (b.y > max_y) b.y = max_y;
            }

            prev_win_w = cur_w;
            prev_win_h = cur_h;
        }

        // --- Bounce all text instances (DVD screensaver style) ---
        {
            for (auto& b : bouncers) {
                b.x += b.vx * dt;
                b.y += b.vy * dt;

                float right_edge  = static_cast<float>(cur_w - b.tw);
                float bottom_edge = static_cast<float>(cur_h - b.th);
                if (right_edge  < 0) right_edge  = 0;
                if (bottom_edge < 0) bottom_edge = 0;

                if (b.x <= 0.0f)         { b.x = 0.0f;         b.vx = -b.vx; }
                if (b.x >= right_edge)    { b.x = right_edge;   b.vx = -b.vx; }
                if (b.y <= 0.0f)          { b.y = 0.0f;         b.vy = -b.vy; }
                if (b.y >= bottom_edge)   { b.y = bottom_edge;  b.vy = -b.vy; }
            }
        }

        // Roll palette: smoothly rotate the colour phase offsets each frame
        if (roll_palette) {
            float step = roll_palette_speed * dt;
            plasma_params.palette_phase_r = std::fmod(plasma_params.palette_phase_r + step,        2.0f);
            plasma_params.palette_phase_g = std::fmod(plasma_params.palette_phase_g + step * 0.7f, 2.0f);
            plasma_params.palette_phase_b = std::fmod(plasma_params.palette_phase_b + step * 1.3f, 2.0f);
        }

        // Update plasma pixels
        if (plasma_tex)
            update_plasma_texture(plasma_tex, plasma_w, plasma_h, time_acc, plasma_params);

        // New frame
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // --- Main Menu Bar ---
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Bouncers")) {
                ImGui::Text("Bouncer Texts (%d):", static_cast<int>(cli_entries.size()));
                {
                    int del_text_idx = -1;
                    for (int ti = 0; ti < static_cast<int>(cli_entries.size()); ++ti) {
                        ImGui::PushID(ti);
                        if (ImGui::SmallButton("X")) del_text_idx = ti;
                        ImGui::SameLine();
                        ImGui::BulletText("\"%s\"", cli_entries[static_cast<size_t>(ti)].label.c_str());
                        ImGui::PopID();
                    }
                    if (del_text_idx >= 0) {
                        // Remove all bouncers that use this texture
                        SDL_Texture* dead_tex = cli_entries[static_cast<size_t>(del_text_idx)].tex;
                        bouncers.erase(
                            std::remove_if(bouncers.begin(), bouncers.end(),
                                [dead_tex](const Bouncer& b) { return b.tex == dead_tex; }),
                            bouncers.end());
                        // Destroy the texture and remove the entry
                        if (dead_tex) SDL_DestroyTexture(dead_tex);
                        cli_entries.erase(cli_entries.begin() + del_text_idx);
                    }
                }
                ImGui::Separator();
                ImGui::Checkbox("Custom Text", &use_custom_text);
                if (use_custom_text) {
                    ImGui::SetNextItemWidth(200.0f);
                    ImGui::InputText("##custom", custom_text_buf, sizeof(custom_text_buf));
                }
                if (ImGui::MenuItem("Add Bouncer")) {
                    SDL_Texture* spawn_tex = nullptr;
                    int spawn_w = 0, spawn_h = 0;
                    if (use_custom_text && custom_text_buf[0] != '\0') {
                        int cw = 0, ch = 0;
                        SDL_Texture* ct = create_text_texture(renderer, custom_text_buf, &cw, &ch);
                        if (ct) {
                            TextEntry ne;
                            ne.label = custom_text_buf;
                            ne.tex = ct;
                            ne.w = cw;
                            ne.h = ch;
                            cli_entries.push_back(std::move(ne));
                            spawn_tex = ct;
                            spawn_w = cw;
                            spawn_h = ch;
                        }
                    } else if (!cli_entries.empty()) {
                        const auto& e = cli_entries[static_cast<size_t>(std::rand()) % cli_entries.size()];
                        spawn_tex = e.tex;
                        spawn_w = e.w;
                        spawn_h = e.h;
                    }
                    if (spawn_tex)
                        bouncers.push_back(make_bouncer(cur_w, cur_h, spawn_tex, spawn_w, spawn_h));
                }
                ImGui::Text("Count: %d", static_cast<int>(bouncers.size()));
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Plasma")) {
                if (ImGui::MenuItem("Randomise Palette"))
                    randomise_plasma_palette(plasma_params);
                if (ImGui::MenuItem("Randomise X/Y"))
                    randomise_plasma_xy(plasma_params);
                ImGui::Separator();
                ImGui::Checkbox("Roll Palette", &roll_palette);
                if (roll_palette)
                    ImGui::SliderFloat("Roll Speed", &roll_palette_speed, 0.05f, 3.0f);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Record")) {
                bool is_recording = (recorder.pipe != nullptr);
                if (!is_recording) {
                    ImGui::SetNextItemWidth(200.0f);
                    ImGui::InputText("File", record_path_buf, sizeof(record_path_buf));
                    if (ImGui::MenuItem("Start Recording")) {
                        int out_w = 0, out_h = 0;
                        SDL_GetRenderOutputSize(renderer, &out_w, &out_h);
                        recorder_start(recorder, out_w, out_h, record_path_buf);
                        record_time = 0.0f;
                        record_frame_accum = 0.0f;
                    }
                } else {
                    int mins = static_cast<int>(record_time) / 60;
                    int secs = static_cast<int>(record_time) % 60;
                    ImGui::Text("REC  %02d:%02d  (%d frames)", mins, secs, recorder.frame_count);
                    ImGui::Text("File: %s", recorder.output_path.c_str());
                    ImGui::Text("Size: %dx%d @ %d fps", recorder.width, recorder.height, recorder.fps);
                    if (record_max_enabled) {
                        int remaining = record_max_seconds - static_cast<int>(record_time);
                        if (remaining < 0) remaining = 0;
                        ImGui::Text("Auto-stop in: %ds", remaining);
                    }
                    if (ImGui::MenuItem("Stop Recording"))
                        recorder_stop(recorder);
                }
                ImGui::Separator();
                ImGui::Checkbox("Max Length", &record_max_enabled);
                if (record_max_enabled) {
                    ImGui::SetNextItemWidth(200.0f);
                    ImGui::SliderInt("Seconds", &record_max_seconds, 1, 300);
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (recorder.pipe) {
                record_time += dt;
                // Auto-stop recording when max length reached
                if (record_max_enabled && record_time >= static_cast<float>(record_max_seconds))
                    recorder_stop(recorder);
            }
            if (recorder.pipe) {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "REC");
                ImGui::SameLine();
            }
            ImGui::Text("%.1f FPS", io.Framerate);
            ImGui::EndMainMenuBar();
        }

        // Render
        ImGui::Render();
        SDL_SetRenderScale(renderer, io.DisplayFramebufferScale.x,
                                     io.DisplayFramebufferScale.y);

        // 1) Draw animated plasma background (stretches from reduced res)
        if (plasma_tex) {
            SDL_RenderTexture(renderer, plasma_tex, nullptr, nullptr);
        } else {
            SDL_SetRenderDrawColorFloat(renderer, 0.10f, 0.08f, 0.15f, 1.0f);
            SDL_RenderClear(renderer);
        }

        // 2) Draw all bouncing text instances (each with its own texture & colour)
        for (const auto& b : bouncers) {
            if (!b.tex) continue;
            SDL_SetTextureColorMod(b.tex, b.r, b.g, b.b);
            SDL_FRect dst_rect = { b.x, b.y, static_cast<float>(b.tw), static_cast<float>(b.th) };
            SDL_RenderTexture(renderer, b.tex, nullptr, &dst_rect);
            SDL_SetTextureColorMod(b.tex, 255, 255, 255);
        }

        // 3) ImGui on top of everything
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);

        // 4) Capture frame for recording (must happen before Present)
        recorder_feed_frame(recorder, renderer);

        SDL_RenderPresent(renderer);
    }

    // --- Cleanup ---
    recorder_stop(recorder);
    for (auto* et : extra_textures) SDL_DestroyTexture(et);
    for (auto& e : cli_entries) { if (e.tex) SDL_DestroyTexture(e.tex); }
    if (plasma_tex) SDL_DestroyTexture(plasma_tex);
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    return 0;
}
