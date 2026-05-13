#include "clplasma.h"
#include "plasmas.h"
#include <iostream>
#include <cstring>

#include "randhelp.h"

PlasmaOpenCL::PlasmaOpenCL(int w, int h) : width(w), height(h) {
    backBuffer.resize(w * h, 0xFF000000); // Start with black opaque
}

PlasmaOpenCL::~PlasmaOpenCL() {
    stop();
    cleanup();
}

bool PlasmaOpenCL::init(int cPlasmaIDX){


    if(cPlasmaIDX == -1){
        iPlasmaIDX = rand_int(10);
    } else {
        iPlasmaIDX = cPlasmaIDX;
    }

    switch(iPlasmaIDX){
        case 0:
                return init(kernelSource0);             
                break;   
        case 1:
                return init(kernelSource1);    
                break;   
        case 2:
                return init(kernelSource2);                    
                break;   
        case 3:
                return init(kernelSource3);                    
                break;   
        case 4:
                return init(kernelSource4);                    
                break;   
        case 5:
                return init(kernelSource5);                
                break;   
        case 6:
                return init(kernelSource6);    
                break;   
        case 7:
                return init(kernelSource7);    
                break;
        case 8:
                return init(kernelSource8);    
                break;              
        default:
            break;
    }

    return init(kernelSource0);
}

bool PlasmaOpenCL::init(const char* cKS) {
    cleanup();
    cl_int err;
    err = clGetPlatformIDs(1, &platform, NULL);
    err |= clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);
    if (err != CL_SUCCESS) return false;

    context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);
#if CL_TARGET_OPENCL_VERSION >= 200
    queue = clCreateCommandQueueWithProperties(context, device, NULL, &err);
#else
    queue = clCreateCommandQueue(context, device, 0, &err);
#endif
    program = clCreateProgramWithSource(context, 1, &cKS, NULL, &err);
    
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

    init(iPlasmaIDX); // Re-init OpenCL buffers for new size
    if (wasRunning) start();
}


void PlasmaOpenCL::workerLoop() {
    size_t globalSize[2] = { (size_t)width, (size_t)height };
    size_t localSize[2] = { 16, 16 };
    // Ensure globalSize is a multiple of localSize
    size_t adjGlobalSize[2] = {
        ((globalSize[0] + localSize[0] - 1) / localSize[0]) * localSize[0],
        ((globalSize[1] + localSize[1] - 1) / localSize[1]) * localSize[1]
    };

    std::vector<uint32_t> stagingBuffer(width * height);
    Uint64 startTicks = SDL_GetTicks();
    //Uint64 lastHeartbeat = SDL_GetTicks();
    cl_event readEvent = nullptr;
    bool readInProgress = false;

    while (running) {
        //Uint64 frameStart = SDL_GetTicks();
        
        //if (frameStart - lastHeartbeat > 5000) {
         //   SDL_Log("Plasma Worker Heartbeat (Alive)");
         //   lastHeartbeat = frameStart;
       // }

        // 1. Check if an existing async read is finished
        if (readInProgress && readEvent) {
            cl_int status;
            clGetEventInfo(readEvent, CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof(cl_int), &status, NULL);
            if (status == CL_COMPLETE) {
                {
                    std::lock_guard<std::mutex> lock(dataMutex);
                    backBuffer.swap(stagingBuffer);
                    frameReady = true;
                }
                clReleaseEvent(readEvent);
                readEvent = nullptr;
                readInProgress = false;
                
                if (stagingBuffer.size() != (size_t)(width * height)) {
                    stagingBuffer.resize(width * height);
                }
            } else {
                // Read still in progress, yield and wait
                SDL_Delay(5);
                continue;
            }
        }

        // 2. Throttle: Don't start a new frame until the previous one is consumed
        {
            std::unique_lock<std::mutex> lock(dataMutex);
            if (frameReady) {
                lock.unlock();
                SDL_Delay(10);
                continue;
            }
        }

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

        cl_int err = clEnqueueNDRangeKernel(queue, kernel, 2, NULL, adjGlobalSize, localSize, 0, NULL, NULL);
        if (err == CL_SUCCESS) {
            // Start an asynchronous read
            err = clEnqueueReadBuffer(queue, clMemBuffer, CL_FALSE, 0, width * height * 4, stagingBuffer.data(), 0, NULL, &readEvent);
            if (err == CL_SUCCESS) {
                clFlush(queue);
                readInProgress = true;
            }
        } else {
            SDL_Log("OpenCL Kernel Error: %d", err);
        }    
    }
}

#include "renderer.h"

void PlasmaOpenCL::updateTexture(Renderer* renderer, SDL_GPUTexture* tex) {
    if (!tex || !renderer) return;
    
    std::vector<uint32_t> localBuffer;
    {
        std::lock_guard<std::mutex> lock(dataMutex);
        if (!frameReady || backBuffer.empty()) return;
        
        localBuffer.swap(backBuffer);
        frameReady = false;
    }

    renderer->updateTexture(tex, width, height, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM, localBuffer.data(), width * 4);
}

void PlasmaOpenCL::setArgs(const CLPlasmaParams& p) {
    std::lock_guard<std::mutex> lock(dataMutex);
    params = p;
}

CLPlasmaParams PlasmaOpenCL::getArgs() {
    std::lock_guard<std::mutex> lock(dataMutex);
    return params;
}

void PlasmaOpenCL::cleanup() {
    if (clMemBuffer) { clReleaseMemObject(clMemBuffer); clMemBuffer = nullptr; }
    if (kernel) { clReleaseKernel(kernel); kernel = nullptr; }
    if (program) { clReleaseProgram(program); program = nullptr; }
    if (queue) { clReleaseCommandQueue(queue); queue = nullptr; }
    if (context) { clReleaseContext(context); context = nullptr; }
}
