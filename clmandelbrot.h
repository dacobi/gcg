#pragma once

#include <SDL3/SDL.h>

struct CLMandelbrotParams {
    double x_offset = -0.5;
    double y_offset = 0.0;
    double zoom = 1.0;
    int max_iterations = 256;
    float palette_phase_r = 0.0f;
    float palette_phase_g = 0.33f;
    float palette_phase_b = 0.66f;
    float color_speed = 1.0f;
    float transparency = 4.0f;
    };

#ifdef USE_OPENCL
#define CL_TARGET_OPENCL_VERSION 300
#include <CL/cl.h>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>

class MandelbrotOpenCL {
public:
    const char* fractal0 = R"(
#pragma OPENCL EXTENSION cl_khr_fp64 : enable

kernel void fractal_kernel(
    global uint* pixels, int w, int h,
    double x_off, double y_off, double zoom, int max_iter,
    float p_r, float p_g, float p_b, float c_s, float transparency) 
{
    int x = get_global_id(0);
    int y = get_global_id(1);
    if (x >= w || y >= h) return;

    double aspect = (double)w / (double)h;
    double scale = 2.0 / zoom;
    
    double cx = x_off + (double)(x - w/2) / (double)w * scale * aspect;
    double cy = y_off + (double)(y - h/2) / (double)h * scale;

    double zx = 0.0, zy = 0.0;
    int iter = 0;
    while (zx*zx + zy*zy < 4.0 && iter < max_iter) {
        double xtemp = zx*zx - zy*zy + cx;
        zy = 2.0*zx*zy + cy;
        zx = xtemp;
        iter++;
    }

    uint R, G, B;
    uint A = 255u;
    if (iter == max_iter || iter < (int)transparency) {
        R = 0; G = 0; B = 0;
        A = 0u;
    } else {
        float v = (float)iter / (float)max_iter * c_s;
        R = (uint)((0.5f + 0.5f * cos(6.28318f * (v + p_r))) * 255.0f);
        G = (uint)((0.5f + 0.5f * cos(6.28318f * (v + p_g))) * 255.0f);
        B = (uint)((0.5f + 0.5f * cos(6.28318f * (v + p_b))) * 255.0f);
    }
    pixels[y * w + x] = (A << 24) | (R << 16) | (G << 8) | B;
})";

    MandelbrotOpenCL(int w, int h);
    ~MandelbrotOpenCL();

    bool init(int cFractalIXD = -1);
    bool init(const char* cKS);
    void start();
    void stop();
    void resize(int w, int h);
    
    void updateTexture(class Renderer* renderer, SDL_GPUTexture* tex);
    void setArgs(const CLMandelbrotParams& p);
    CLMandelbrotParams getArgs();
    int iFractalIDX = -1;

private:
    void workerLoop();
    void cleanup();

    int width, height;
    std::atomic<bool> running{false};
    std::thread workerThread;
    
    std::mutex dataMutex;
    std::vector<uint32_t> backBuffer;
    bool frameReady = false;

    cl_platform_id platform = nullptr;
    cl_device_id device = nullptr;
    cl_context context = nullptr;
    cl_command_queue queue = nullptr;
    cl_program program = nullptr;
    cl_kernel kernel = nullptr;
    cl_mem clMemBuffer = nullptr;

    CLMandelbrotParams params;
};
#else
class MandelbrotOpenCL {
public:
    MandelbrotOpenCL(int w, int h) {}
    ~MandelbrotOpenCL() {}
    bool init(int cFractalIXD = -1) { return false; }
    bool init(const char* cKS) { return false; }
    void start() {}
    void stop() {}
    void resize(int w, int h) {}
    void updateTexture(class Renderer* renderer, SDL_GPUTexture* tex) {}
    void setArgs(const CLMandelbrotParams& p) { params = p; }
    CLMandelbrotParams getArgs() { return params; }
    void cleanup() {}
    int iFractalIDX = -1;
    CLMandelbrotParams params;
};
#endif
