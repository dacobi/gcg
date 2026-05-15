#pragma once

#include <SDL3/SDL.h>
#include <vector>
#include <string>
#include <mutex>
#include "clplasma.h" // Reuse CLPlasmaParams

class PlasmaShader {
public:
    PlasmaShader(int w, int h);
    ~PlasmaShader();

    bool init(SDL_GPUTextureFormat format, int cPlasmaIDX = -1);
    void resize(int w, int h);
    void updateTexture(class Renderer* renderer, SDL_GPUTexture* tex);
    void setArgs(const CLPlasmaParams& p);
    CLPlasmaParams getArgs();
    SDL_GPUTextureFormat getTargetFormat() const { return targetFormat; }

    int iPlasmaIDX = 0;

private:
    int width, height;
    SDL_GPUTextureFormat targetFormat;
    CLPlasmaParams params;
    std::mutex dataMutex;
    Uint64 startTicks;

    SDL_GPUGraphicsPipeline* pipeline = nullptr;

    void cleanup();
};
