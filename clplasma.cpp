#include "clplasma.h"
#include <iostream>
#include <cstring>

const char* kernelSource = R"(
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

PlasmaOpenCL::PlasmaOpenCL(int w, int h) : width(w), height(h) {
    backBuffer.resize(w * h, 0xFF000000); // Start with black opaque
}

PlasmaOpenCL::~PlasmaOpenCL() {
    stop();
    cleanup();
}

bool PlasmaOpenCL::init() {
    cl_int err;
    err = clGetPlatformIDs(1, &platform, NULL);
    err |= clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);
    if (err != CL_SUCCESS) return false;

    context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);
    queue = clCreateCommandQueue(context, device, 0, &err);
    program = clCreateProgramWithSource(context, 1, &kernelSource, NULL, &err);
    
    err = clBuildProgram(program, 1, &device, NULL, NULL, NULL);
    if (err != CL_SUCCESS) {
        char log[2048];
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, sizeof(log), log, NULL);
        SDL_Log("OpenCL Build Log: %s", log);
        return false;
    }

    kernel = clCreateKernel(program, "plasma_kernel", &err);
    clMemBuffer = clCreateBuffer(context, CL_MEM_WRITE_ONLY, width * height * sizeof(uint32_t), NULL, &err);
    return (err == CL_SUCCESS);
}

void PlasmaOpenCL::start() {
    if (!running) {
        running = true;
        workerThread = std::thread(&PlasmaOpenCL::workerLoop, this);
    }
}

void PlasmaOpenCL::stop() {
    running = false;
    if (workerThread.joinable()) workerThread.join();
}

void PlasmaOpenCL::resize(int w, int h) {
    bool wasRunning = running;
    stop(); // Thread must be stopped to resize buffers safely

    std::lock_guard<std::mutex> lock(dataMutex);
    width = w;
    height = h;
    backBuffer.assign(w * h, 0xFF000000);
    frameReady = false;

    init(); // Re-init OpenCL buffers for new size
    if (wasRunning) start();
}


void PlasmaOpenCL::workerLoop() {
    size_t globalSize[2] = { (size_t)width, (size_t)height };
    std::vector<uint32_t> stagingBuffer(width * height);
    Uint64 startTicks = SDL_GetTicks();

    while (running) {
        float t = (float)((double)(SDL_GetTicks() - startTicks) / 1000.0);
        CLPlasmaParams p;
        { std::lock_guard<std::mutex> lock(dataMutex); p = params; }

        // Set arguments - Total 24 arguments now
        clSetKernelArg(kernel, 0, sizeof(cl_mem), &clMemBuffer);
        clSetKernelArg(kernel, 1, sizeof(int), &width);
        clSetKernelArg(kernel, 2, sizeof(int), &height);
        clSetKernelArg(kernel, 3, sizeof(float), &t);
        clSetKernelArg(kernel, 4, sizeof(float), &p.drift_amp);
        clSetKernelArg(kernel, 5, sizeof(float), &p.drift_speed_x);
        clSetKernelArg(kernel, 6, sizeof(float), &p.drift_speed_y);
        clSetKernelArg(kernel, 7, sizeof(float), &p.rot_speed);
        clSetKernelArg(kernel, 8, sizeof(float), &p.scale_base_x);
        clSetKernelArg(kernel, 9, sizeof(float), &p.scale_base_y);
        clSetKernelArg(kernel, 10, sizeof(float), &p.palette_phase_r);
        clSetKernelArg(kernel, 11, sizeof(float), &p.palette_phase_g);
        clSetKernelArg(kernel, 12, sizeof(float), &p.palette_phase_b);
        
        // Advanced Params
        clSetKernelArg(kernel, 13, sizeof(float), &p.scale_mod_amp);
        clSetKernelArg(kernel, 14, sizeof(float), &p.scale_mod_speed_x);
        clSetKernelArg(kernel, 15, sizeof(float), &p.scale_mod_speed_y);
        clSetKernelArg(kernel, 16, sizeof(float), &p.warp_base);
        clSetKernelArg(kernel, 17, sizeof(float), &p.warp_amp);
        clSetKernelArg(kernel, 18, sizeof(float), &p.warp_speed);
        clSetKernelArg(kernel, 19, sizeof(float), &p.swirl_dist_mul);
        clSetKernelArg(kernel, 20, sizeof(float), &p.darken_r);
        clSetKernelArg(kernel, 21, sizeof(float), &p.darken_g);
        clSetKernelArg(kernel, 22, sizeof(float), &p.darken_b);
        clSetKernelArg(kernel, 23, sizeof(float), &p.tile_count);

        cl_int err = clEnqueueNDRangeKernel(queue, kernel, 2, NULL, globalSize, NULL, 0, NULL, NULL);
        if (err == CL_SUCCESS) {
            clFinish(queue);
            clEnqueueReadBuffer(queue, clMemBuffer, CL_TRUE, 0, width * height * 4, stagingBuffer.data(), 0, NULL, NULL);
            
            {   // Scope the lock to the bare minimum
                std::lock_guard<std::mutex> lock(dataMutex);
                backBuffer = stagingBuffer;
                frameReady = true;
            }
        }
        SDL_Delay(1); 
    }
}

void PlasmaOpenCL::updateTexture(SDL_Texture* tex) {
    if (!tex) return;
    
    std::unique_lock<std::mutex> lock(dataMutex);
    if (!frameReady || backBuffer.empty()) return;

    void* pixels;
    int pitch;
    // SDL3 Lock returns true on success
    if (SDL_LockTexture(tex, NULL, &pixels, &pitch)) {
        Uint8* dst = static_cast<Uint8*>(pixels);
        Uint8* src = reinterpret_cast<Uint8*>(backBuffer.data());
        size_t bpr = (size_t)width * 4;

        for (int y = 0; y < height; ++y) {
            std::memcpy(dst + ((size_t)y * pitch), src + (y * bpr), bpr);
        }
        
        SDL_UnlockTexture(tex);
        frameReady = false;
    }
}

void PlasmaOpenCL::setParams(const CLPlasmaParams& p) {
    std::lock_guard<std::mutex> lock(dataMutex);
    params = p;
}

void PlasmaOpenCL::cleanup() {
    if (clMemBuffer) clReleaseMemObject(clMemBuffer);
    if (kernel) clReleaseKernel(kernel);
    if (program) clReleaseProgram(program);
    if (queue) clReleaseCommandQueue(queue);
    if (context) clReleaseContext(context);
}
