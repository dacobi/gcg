#pragma once

#include <SDL3/SDL.h>
#include <CL/cl.h>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>

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
};

class PlasmaOpenCL {
public:
    PlasmaOpenCL(int w, int h);
    ~PlasmaOpenCL();

    bool init();
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

