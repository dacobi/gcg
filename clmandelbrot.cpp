#include "clmandelbrot.h"
#include "fractals.h"
#include <iostream>
#include <vector>

#include "randhelp.h"

MandelbrotOpenCL::MandelbrotOpenCL(int w, int h) : width(w), height(h) {
    backBuffer.resize(w * h, 0xFF000000);
}

MandelbrotOpenCL::~MandelbrotOpenCL() {
    stop();
    cleanup();
}

bool MandelbrotOpenCL::init(int cFractalIDX){


    if(cFractalIDX == -1){
        iFractalIDX = rand_int(6);
    } else {
        iFractalIDX = cFractalIDX;
    }

    switch(iFractalIDX){
        case 0:
                return init(fractal0);             
                break;   
        case 1:
                return init(fractal1);    
                break;   
        case 2:
                return init(fractal2);                    
                break;   
        case 3:
                return init(fractal3);                    
                break;   
        case 4:
                return init(fractal4);                    
                break;   
        case 5:
                return init(fractal5);                
                break;   
        /*case 6:
                return init(fractal6);                
                break;   
        
        
        case 7:
                return init(kernelSource7);    
                break;   
        case 8:
                return init(kernelSource8);    
                break;              */
        default:
            break;
    }

    return init(fractal0);
}

bool MandelbrotOpenCL::init(const char* cKS) {
    SDL_Log("MandelbrotOpenCL::init() starting...");
    cleanup();
    cl_int err;
    err = clGetPlatformIDs(1, &platform, NULL);
    if (err != CL_SUCCESS) { SDL_Log("clGetPlatformIDs Error: %d", err); return false; }
    
    err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);
    if (err != CL_SUCCESS) { SDL_Log("clGetDeviceIDs Error: %d", err); return false; }

    context = clCreateContext(NULL, 1, &device, NULL, NULL, &err);
    if (err != CL_SUCCESS) { SDL_Log("clCreateContext Error: %d", err); return false; }

#if CL_TARGET_OPENCL_VERSION >= 200
    queue = clCreateCommandQueueWithProperties(context, device, NULL, &err);
#else
    queue = clCreateCommandQueue(context, device, 0, &err);
#endif
    if (err != CL_SUCCESS) { SDL_Log("clCreateCommandQueue Error: %d", err); return false; }

    program = clCreateProgramWithSource(context, 1, &cKS, NULL, &err);
    if (err != CL_SUCCESS) { SDL_Log("clCreateProgramWithSource Error: %d", err); return false; }
    
    err = clBuildProgram(program, 1, &device, NULL, NULL, NULL);
    if (err != CL_SUCCESS) {
        char log[2048];
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, sizeof(log), log, NULL);
        SDL_Log("Mandelbrot OpenCL Build Log: %s", log);
        return false;
    }

    kernel = clCreateKernel(program, "fractal_kernel", &err);
    if (err != CL_SUCCESS) {
        SDL_Log("Mandelbrot clCreateKernel Error: %d", err);
        return false;
    }
    clMemBuffer = clCreateBuffer(context, CL_MEM_WRITE_ONLY, width * height * sizeof(uint32_t), NULL, &err);
    if (err != CL_SUCCESS) {
        SDL_Log("Mandelbrot clCreateBuffer Error: %d", err);
        return false;
    }
    SDL_Log("MandelbrotOpenCL::init() success!");
    return true;
}

void MandelbrotOpenCL::start() {
    if (!running) {
        SDL_Log("MandelbrotOpenCL::start() - launching thread");
        running = true;
        workerThread = std::thread(&MandelbrotOpenCL::workerLoop, this);
    }
}

void MandelbrotOpenCL::stop() {
    running = false;
    if (workerThread.joinable()) workerThread.join();
}

void MandelbrotOpenCL::resize(int w, int h) {
    bool wasRunning = running;
    stop();

    std::lock_guard<std::mutex> lock(dataMutex);
    width = w;
    height = h;
    backBuffer.assign(w * h, 0xFF000000);
    frameReady = false;

    init();
    if (wasRunning) start();
}

void MandelbrotOpenCL::workerLoop() {
    SDL_Log("Mandelbrot workerLoop started");
    size_t globalSize[2] = { (size_t)width, (size_t)height };
    size_t localSize[2] = { 16, 16 };
    size_t adjGlobalSize[2] = {
        ((globalSize[0] + localSize[0] - 1) / localSize[0]) * localSize[0],
        ((globalSize[1] + localSize[1] - 1) / localSize[1]) * localSize[1]
    };

    std::vector<uint32_t> stagingBuffer(width * height);
    cl_event readEvent = nullptr;
    bool readInProgress = false;

    while (running) {
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
                if (stagingBuffer.size() != (size_t)(width * height)) stagingBuffer.resize(width * height);
            } else {
                SDL_Delay(5);
                continue;
            }
        }

        {
            std::unique_lock<std::mutex> lock(dataMutex);
            if (frameReady) {
                lock.unlock();
                SDL_Delay(10);
                continue;
            }
        }

        CLMandelbrotParams p;
        { std::lock_guard<std::mutex> lock(dataMutex); p = params; }

        clSetKernelArg(kernel, 0, sizeof(cl_mem), &clMemBuffer);
        clSetKernelArg(kernel, 1, sizeof(int), &width);
        clSetKernelArg(kernel, 2, sizeof(int), &height);
        clSetKernelArg(kernel, 3, sizeof(double), &p.x_offset);
        clSetKernelArg(kernel, 4, sizeof(double), &p.y_offset);
        clSetKernelArg(kernel, 5, sizeof(double), &p.zoom);
        clSetKernelArg(kernel, 6, sizeof(int), &p.max_iterations);
        clSetKernelArg(kernel, 7, sizeof(float), &p.palette_phase_r);
        clSetKernelArg(kernel, 8, sizeof(float), &p.palette_phase_g);
        clSetKernelArg(kernel, 9, sizeof(float), &p.palette_phase_b);
        clSetKernelArg(kernel, 10, sizeof(float), &p.color_speed);
        clSetKernelArg(kernel, 11, sizeof(float), &p.transparency);

        cl_int err = clEnqueueNDRangeKernel(queue, kernel, 2, NULL, adjGlobalSize, localSize, 0, NULL, NULL);
        if (err == CL_SUCCESS) {
            err = clEnqueueReadBuffer(queue, clMemBuffer, CL_FALSE, 0, width * height * 4, stagingBuffer.data(), 0, NULL, &readEvent);
            if (err == CL_SUCCESS) {
                clFlush(queue);
                readInProgress = true;
            } else {
                SDL_Log("Mandelbrot clEnqueueReadBuffer Error: %d", err);
            }
        } else {
            SDL_Log("Mandelbrot clEnqueueNDRangeKernel Error: %d", err);
            SDL_Delay(100);
        }
    }

    if (readInProgress && readEvent) {
        clWaitForEvents(1, &readEvent);
        clReleaseEvent(readEvent);
    }
    clFinish(queue);

    SDL_Log("Mandelbrot workerLoop exiting");
}

#include "renderer.h"

void MandelbrotOpenCL::updateTexture(Renderer* renderer, SDL_GPUTexture* tex) {
    if (!tex || !renderer) return;
    std::vector<uint32_t> localBuffer;
    bool updated = false;
    {
        std::lock_guard<std::mutex> lock(dataMutex);
        if (frameReady && !backBuffer.empty()) {
            localBuffer.swap(backBuffer);
            frameReady = false;
            updated = true;
        }
    }
    if (updated) {
        // SDL_Log("Mandelbrot: SDL_UpdateTexture called");
        renderer->updateTexture(tex, width, height, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM, localBuffer.data(), width * 4);
    }
}

void MandelbrotOpenCL::setArgs(const CLMandelbrotParams& p) {
    std::lock_guard<std::mutex> lock(dataMutex);
    params = p;
}

CLMandelbrotParams MandelbrotOpenCL::getArgs() {
    std::lock_guard<std::mutex> lock(dataMutex);
    return params;
}

void MandelbrotOpenCL::cleanup() {
    if (clMemBuffer) { clReleaseMemObject(clMemBuffer); clMemBuffer = nullptr; }
    if (kernel) { clReleaseKernel(kernel); kernel = nullptr; }
    if (program) { clReleaseProgram(program); program = nullptr; }
    if (queue) { clReleaseCommandQueue(queue); queue = nullptr; }
    if (context) { clReleaseContext(context); context = nullptr; }
}
