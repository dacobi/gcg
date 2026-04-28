#pragma once

#include <SDL3/SDL.h>
#include <CL/cl.h>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>

// Helper: random float in [lo, hi]
static float rand_range(float lo, float hi) {
    return lo + static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * (hi - lo);
}

static int rand_int(int cval) {
    return static_cast<int>((std::rand()) / static_cast<float>(RAND_MAX) * (float)cval) -1;
}

struct CLPlasmaParams {
    float drift_amp = 0.05f;
    float drift_speed_x = 0.5f;
    float drift_speed_y = 0.3f;
    float rot_speed = 0.2f;
    float scale_base_x = 4.0f;
    float scale_base_y = 4.0f;
    float palette_phase_r = 0.0f;
    float palette_phase_g = 0.33f;
    float palette_phase_b = 0.66f;

    // Advanced Parameters
    float scale_mod_amp = 0.1f;
    float scale_mod_speed_x = 0.4f;
    float scale_mod_speed_y = 0.3f;
    float warp_base = 0.5f;
    float warp_amp = 0.2f;
    float warp_speed = 0.3f;
    float swirl_dist_mul = 1.0f;
    float darken_r = 1.0f;
    float darken_g = 1.0f;
    float darken_b = 1.0f;
    float tile_count = 0.0f; // 0.0 = disabled
};


class PlasmaOpenCL {
public:
    const char* kernelSource0 = R"(
kernel void plasma_kernel(
    global uint* pixels, int w, int h, float t,
    float d_amp, float d_sx, float d_sy, float r_s,
    float s_bx, float s_by, float p_r, float p_g, float p_b,
    float s_ma, float s_msx, float s_msy,
    float w_b, float w_a, float w_s, float s_dm,
    float d_r, float d_g, float d_b,
    float t_c) 
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    if (x >= w || y >= h) return;

    float fx = (float)x / (float)w;
    float fy = (float)y / (float)h;

    // Optional tiling
    if (t_c > 0.0f) {
        float cur_rel = (float)w / (float)h;
        fy = floor(fy * t_c) / t_c;
        fx = floor(fx * (t_c * cur_rel)) / (t_c * cur_rel);
    }

    float dx = d_amp * sin(t * d_sx);
    float dy = d_amp * cos(t * d_sy);
    float rs = sin(t * r_s), rc = cos(t * r_s);

    float jitter = sin(fx * 140.0f + t * 15.0f) * cos(fy * 240.0f - t * 10.0f) * 0.02f;
    float cx = (fx + jitter) - 0.5f;
    float cy = (fy + jitter) - 0.5f;

    float rx = cx * rc - cy * rs + 0.5f + dx;
    float ry = cx * rs + cy * rc + 0.5f + dy;

    // Swirl logic
    float dist = sqrt(cx * cx + cy * cy);
    float warp_str = w_b + w_a * sin(t * w_s);
    float swirl_angle = dist * s_dm * warp_str;
    float sw_sin = sin(swirl_angle), sw_cos = cos(swirl_angle);
    float wx = (rx - 0.5f) * sw_cos - (ry - 0.5f) * sw_sin + 0.5f;
    float wy = (rx - 0.5f) * sw_sin + (ry - 0.5f) * sw_cos + 0.5f;

    // Animated scale
    float scale_x = s_bx + s_ma * sin(t * s_msx);
    float scale_y = s_by + s_ma * cos(t * s_msy);

    float v = (sin(wx * scale_x + t) + sin(wy * scale_y - t * 0.5f) + 
               sin((wx + wy) * (scale_x * 0.5f) + t) + 
               sin(sqrt(wx * wx + wy * wy) * 10.0f + t)) * 0.25f;

    uint R = (uint)((0.5f + 0.5f * cos(6.28318f * (v + p_r))) * d_r * 255.0f);
    uint G = (uint)((0.5f + 0.5f * cos(6.28318f * (v + p_g))) * d_g * 255.0f);
    uint B = (uint)((0.5f + 0.5f * cos(6.28318f * (v + p_b))) * d_b * 255.0f);

    pixels[y * w + x] = (0xFFu << 24) | (R << 16) | (G << 8) | B;
})";


    PlasmaOpenCL(int w, int h);
    ~PlasmaOpenCL();

    bool init();
    bool init(const char* cKS);
    void start();
    void stop();
    void resize(int w, int h);

    
    // Call this from the main SDL thread
    void updateTexture(SDL_Texture* tex);
    void setParams(const CLPlasmaParams& p);

private:
    void workerLoop();
    void cleanup();

    int width, height;
    std::atomic<bool> running{false};
    std::thread workerThread;
    
    // Synchronization
    std::mutex dataMutex;
    std::vector<uint32_t> backBuffer;
    bool frameReady = false;

    // OpenCL Handles
    cl_platform_id platform = nullptr;
    cl_device_id device = nullptr;
    cl_context context = nullptr;
    cl_command_queue queue = nullptr;
    cl_program program = nullptr;
    cl_kernel kernel = nullptr;
    cl_mem clMemBuffer = nullptr;

    CLPlasmaParams params;
};

