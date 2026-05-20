#include "usd_hydra_renderer.h"
#include <iostream>
#include <SDL3/SDL_opengl.h>
#include <pxr/usd/usd/prim.h>

// Helper to get GL function pointers without GLEW
typedef void (APIENTRYP PFNGLGENFRAMEBUFFERSPROC) (GLsizei n, GLuint *framebuffers);
typedef void (APIENTRYP PFNGLBINDFRAMEBUFFERPROC) (GLenum target, GLuint framebuffer);
typedef void (APIENTRYP PFNGLFRAMEBUFFERTEXTURE2DPROC) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void (APIENTRYP PFNGLDELETEFRAMEBUFFERSPROC) (GLsizei n, const GLuint *framebuffers);
typedef GLenum (APIENTRYP PFNGLCHECKFRAMEBUFFERSTATUSPROC) (GLenum target);

USDHydraRenderer::USDHydraRenderer(int w, int h) : width(w), height(h) {}

USDHydraRenderer::~USDHydraRenderer() {
    if (glContext) {
        SDL_GL_MakeCurrent(glWindow, glContext);
        glDeleteTextures(1, &colorTex);
        glDeleteTextures(1, &depthTex);
        PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC)SDL_GL_GetProcAddress("glDeleteFramebuffers");
        if (glDeleteFramebuffers) glDeleteFramebuffers(1, &fbo);
        SDL_GL_DestroyContext(glContext);
    }
    if (glWindow) SDL_DestroyWindow(glWindow);
}

bool USDHydraRenderer::init(const std::string& usdFile) {
    stage = UsdStage::Open(usdFile);
    if (!stage) return false;

    // Create a hidden window for OpenGL context
    glWindow = SDL_CreateWindow("USD Offscreen", width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if (!glWindow) return false;

    glContext = SDL_GL_CreateContext(glWindow);
    if (!glContext) return false;

    SDL_GL_MakeCurrent(glWindow, glContext);

    // Basic GL setup for offscreen rendering
    glGenTextures(1, &colorTex);
    glBindTexture(GL_TEXTURE_2D, colorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    
    glGenTextures(1, &depthTex);
    glBindTexture(GL_TEXTURE_2D, depthTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, width, height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);

    PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC)SDL_GL_GetProcAddress("glGenFramebuffers");
    PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)SDL_GL_GetProcAddress("glBindFramebuffer");
    PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC)SDL_GL_GetProcAddress("glFramebufferTexture2D");
    PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC)SDL_GL_GetProcAddress("glCheckFramebufferStatus");

    if (glGenFramebuffers && glBindFramebuffer && glFramebufferTexture2D) {
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depthTex, 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "FBO incomplete" << std::endl;
        }
    }

    engine = std::make_unique<UsdImagingGLEngine>();
    return true;
}

void USDHydraRenderer::render(void* outPixels) {
    if (!engine || !stage || !glContext) return;

    SDL_GL_MakeCurrent(glWindow, glContext);
    
    PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)SDL_GL_GetProcAddress("glBindFramebuffer");
    if (glBindFramebuffer) glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glViewport(0, 0, width, height);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    UsdImagingGLRenderParams params;
    params.drawMode = UsdImagingGLDrawMode::DRAW_SHADED_SMOOTH;
    params.enableLighting = true;
    params.clearColor = GfVec4f(0.1f, 0.1f, 0.1f, 1.0f);

    GfMatrix4d viewMatrix(1.0);
    viewMatrix.SetTranslate(GfVec3d(0, 0, -10)); // Move back a bit more
    
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
    engine->SetRenderViewport(GfVec4d(0, 0, width, height));

    engine->Render(stage->GetPseudoRoot(), params);

    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, outPixels);
    
    if (glBindFramebuffer) glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
