#include "usd_hydra_renderer.h"

#ifdef USE_USD

#include <iostream>
#include <SDL3/SDL_vulkan.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/base/gf/rotation.h>
#include <pxr/imaging/hgi/tokens.h>
#include <pxr/imaging/hgi/blitCmds.h>
#include <pxr/imaging/hgi/blitCmdsOps.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/driver.h>
#include <pxr/base/gf/half.h>
#include <pxr/imaging/hgi/enums.h>
#include <pxr/usd/usdGeom/camera.h>
#include <pxr/usd/usd/primRange.h>

USDHydraRenderer::USDHydraRenderer(int w, int h) : width(w), height(h) {}

USDHydraRenderer::~USDHydraRenderer() {
    if (glWindow) SDL_DestroyWindow(glWindow);
}

bool USDHydraRenderer::init(const std::string& usdFile) {
    stage = UsdStage::Open(usdFile);
    if (!stage) return false;

    // Create a hidden window for Vulkan surface (some backends might still require it for setup)
    glWindow = SDL_CreateWindow("USD Offscreen", width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN);
    if (!glWindow) return false;

    hgi = pxr::Hgi::CreateNamedHgi(pxr::HgiTokens->Vulkan);
    if (!hgi) {
        std::cerr << "Failed to initialize Vulkan Hgi." << std::endl;
        return false;
    }

    pxr::HdDriver driver;
    driver.name = pxr::HgiTokens->renderDriver;
    driver.driver = pxr::VtValue(hgi.get());

    pxr::UsdImagingGLEngine::Parameters params;
    params.driver = driver;

    engine = std::make_unique<UsdImagingGLEngine>(params);
    engine->SetEnablePresentation(false);
    return true;
}

void USDHydraRenderer::resize(int w, int h) {
    width = w;
    height = h;
}

void USDHydraRenderer::render(void* outPixels) {
    if (!engine || !stage || !hgi) return;

    UsdImagingGLRenderParams params;
    params.drawMode = UsdImagingGLDrawMode::DRAW_SHADED_SMOOTH;
    params.enableLighting = true;
    params.enableSceneLights = true;
    params.colorCorrectionMode = TfToken("sRGB");
    if (backgroundTransparency) {
        params.clearColor = GfVec4f(0.0f, 0.0f, 0.0f, 0.0f);
    } else {
        params.clearColor = GfVec4f(0.1f, 0.1f, 0.1f, 1.0f);
    }

    if (!activeCameraPath.IsEmpty() && !freeCamera) {
        engine->SetCameraPath(activeCameraPath);
    } else {
        GfMatrix4d viewMatrix(1.0);
        viewMatrix.SetTranslate(GfVec3d(0, 0, -cameraDistance));
        
        // Simple projection matrix calculation
        double aspect = (double)width / height;
        double fov = 45.0;
        double near = 0.1;
        double far = 100.0;
        double top = near * tan(fov * M_PI / 360.0);
        double right = top * aspect;
        GfMatrix4d projMatrix(1.0);
        projMatrix[0][0] = near / right;
        projMatrix[1][1] = near / top;
        projMatrix[2][2] = -(far + near) / (far - near);
        projMatrix[3][2] = -2.0 * far * near / (far - near);
        projMatrix[2][3] = -1.0;
        projMatrix[3][3] = 0.0;

        engine->SetCameraState(viewMatrix, projMatrix);
    }

    if (freeCamera) {
        GfMatrix4d rootXform(1.0);
        rootXform.SetRotate(GfRotation(GfVec3d(1, 0, 0), sceneRotation[0]) *
                            GfRotation(GfVec3d(0, 1, 0), sceneRotation[1]) *
                            GfRotation(GfVec3d(0, 0, 1), sceneRotation[2]));
        engine->SetRootTransform(rootXform);
    } else {
        engine->SetRootTransform(GfMatrix4d(1.0));
    }

    engine->SetRenderViewport(GfVec4d(0, 0, width, height));
    engine->SetRenderBufferSize(pxr::GfVec2i(width, height));
    
    engine->SetRendererAov(pxr::HdAovTokens->color);

    engine->Render(stage->GetPseudoRoot(), params);

    pxr::HgiTextureHandle colorAovTexture = engine->GetAovTexture(pxr::HdAovTokens->color);
    if (colorAovTexture) {
        static bool print_format = true;
        if (print_format) {
            std::cout << "Texture format: " << colorAovTexture->GetDescriptor().format << "\n";
            print_format = false;
        }
        pxr::HgiBlitCmdsUniquePtr blitCmds = hgi->CreateBlitCmds();
        pxr::HgiTextureGpuToCpuOp copyOp;
        copyOp.gpuSourceTexture = colorAovTexture;
        copyOp.sourceTexelOffset = pxr::GfVec3i(0, 0, 0);
        copyOp.mipLevel = 0;

        if (colorAovTexture->GetDescriptor().format == pxr::HgiFormatFloat16Vec4) {
            std::vector<pxr::GfHalf> halfBuffer(width * height * 4);
            copyOp.cpuDestinationBuffer = halfBuffer.data();
            copyOp.destinationByteOffset = 0;
            copyOp.destinationBufferByteSize = width * height * 4 * sizeof(pxr::GfHalf);
            
            blitCmds->CopyTextureGpuToCpu(copyOp);
            hgi->SubmitCmds(blitCmds.get(), pxr::HgiSubmitWaitTypeWaitUntilCompleted);
            
            uint8_t* p = static_cast<uint8_t*>(outPixels);
            for (int y = 0; y < height; ++y) {
                int dstY = height - 1 - y; // Vertical flip
                for (int x = 0; x < width; ++x) {
                    int dstX = width - 1 - x; // Horizontal flip (together = 180 deg rotation)
                    int srcIdx = (y * width + x) * 4;
                    int dstIdx = (dstY * width + dstX) * 4;
                    
                    for (int c = 0; c < 4; ++c) {
                        float val = static_cast<float>(halfBuffer[srcIdx + c]);
                        if (val < 0.0f) val = 0.0f;
                        if (val > 1.0f) val = 1.0f;
                        p[dstIdx + c] = static_cast<uint8_t>(val * 255.0f);
                    }
                }
            }
        } else {
            std::vector<uint8_t> tempBuffer(width * height * 4);
            copyOp.cpuDestinationBuffer = tempBuffer.data();
            copyOp.destinationByteOffset = 0;
            copyOp.destinationBufferByteSize = width * height * 4;
            
            blitCmds->CopyTextureGpuToCpu(copyOp);
            hgi->SubmitCmds(blitCmds.get(), pxr::HgiSubmitWaitTypeWaitUntilCompleted);

            uint8_t* p = static_cast<uint8_t*>(outPixels);
            for (int y = 0; y < height; ++y) {
                int dstY = height - 1 - y;
                for (int x = 0; x < width; ++x) {
                    int dstX = width - 1 - x;
                    int srcIdx = (y * width + x) * 4;
                    int dstIdx = (dstY * width + dstX) * 4;
                    std::memcpy(p + dstIdx, tempBuffer.data() + srcIdx, 4);
                }
            }
        }
    } else {
        static bool first_err = true;
        if (first_err) {
            std::cerr << "colorAovTexture is NULL!\n";
            first_err = false;
        }
    }
}

void USDHydraRenderer::setCameraByIndex(int index) {
    if (index < 0) {
        freeCamera = true;
        activeCameraPath = SdfPath();
        return;
    }

    if (!stage) return;

    int current = 0;
    for (auto prim : stage->Traverse()) {
        if (prim.IsA<UsdGeomCamera>()) {
            if (current == index) {
                activeCameraPath = prim.GetPath();
                freeCamera = false;
                return;
            }
            current++;
        }
    }
}

#endif
