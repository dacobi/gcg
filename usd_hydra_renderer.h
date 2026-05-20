#pragma once

#include <string>
#include <memory>
#include <vector>

#include <SDL3/SDL.h>
#include <pxr/pxr.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usdImaging/usdImagingGL/engine.h>
#include <pxr/base/gf/matrix4d.h>

PXR_NAMESPACE_USING_DIRECTIVE

class USDHydraRenderer {
public:
    USDHydraRenderer(int width, int height);
    ~USDHydraRenderer();

    bool init(const std::string& usdFile);
    void render(void* outPixels);
    
    int getWidth() const { return width; }
    int getHeight() const { return height; }

    UsdStageRefPtr getStage() const { return stage; }

    void setActiveCamera(const SdfPath& cameraPath) { activeCameraPath = cameraPath; }
    const SdfPath& getActiveCamera() const { return activeCameraPath; }

    bool rotateScene = false;
    float sceneRotation[3] = {0.0f, 0.0f, 0.0f};
    float cameraDistance = 10.0f;

private:
    int width;
    int height;

    SDL_Window* glWindow = nullptr;
    SDL_GLContext glContext = nullptr;
    uint32_t fbo = 0;
    uint32_t colorTex = 0;
    uint32_t depthTex = 0;

    std::unique_ptr<UsdImagingGLEngine> engine;
    UsdStageRefPtr stage;
    SdfPath activeCameraPath;
};
