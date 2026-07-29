#pragma once

#include <SDL3/SDL.h>
#include <SDL3_shadercross/SDL_shadercross.h>

class Renderer {
public:
    Renderer();
    ~Renderer();

    bool init(SDL_Window* window);
    void shutdown();

    void beginFrame();
    void beginRenderPass(bool transparent = false);
    void beginSwapchainRenderPass();
    void endRenderPass();
    void blitToSwapchain();
    void endFrame();

    SDL_GPUDevice* getDevice() const { return device; }

    // Helper for creating and uploading textures
    SDL_GPUTexture* createAndUploadTexture(int width, int height, SDL_GPUTextureFormat format, const void* pixels, int pitch);
    SDL_GPUTexture* createTexture(int width, int height, SDL_GPUTextureFormat format, SDL_GPUTextureUsageFlags usage = SDL_GPU_TEXTUREUSAGE_SAMPLER);
    void updateTexture(SDL_GPUTexture* tex, int width, int height, SDL_GPUTextureFormat format, const void* pixels, int pitch);

    // Bouncer drawing
    void drawBouncer(SDL_GPUTexture* tex, const SDL_FRect& dst, Uint8 r, Uint8 g, Uint8 b, Uint8 a, SDL_GPUTexture* stencil_tex = nullptr, bool transparent = false);

    // 3D drawing
#ifdef USE_USD
    void drawObject3D(class Object3D* obj, const struct Light3D& light, const float* viewMatrix, const float* projMatrix);
#endif

    // Background drawing
    void drawBackground(SDL_GPUTexture* tex);

    // Solid color rectangle
    void drawRect(const SDL_FRect& rect, Uint8 r, Uint8 g, Uint8 b, Uint8 a);

    // Current pass and command buffer for ImGui or other direct usage
    SDL_GPUCommandBuffer* getCommandBuffer() const { return current_cmd_buf; }
    SDL_GPURenderPass* getRenderPass() const { return current_render_pass; }

    SDL_Surface* readPixels(int dx, int dy, int dw, int dh);
    int getTargetWidth() const { return target_width; }
    int getTargetHeight() const { return target_height; }

private:
    SDL_GPUDevice* device = nullptr;
    SDL_Window* window = nullptr;

    SDL_GPUCommandBuffer* current_cmd_buf = nullptr;
    SDL_GPUTexture* swapchain_texture = nullptr;
    SDL_GPURenderPass* current_render_pass = nullptr;

    SDL_GPUTexture* color_target = nullptr;
    SDL_GPUTexture* depth_texture = nullptr;
    int target_width = 0;
    int target_height = 0;

    SDL_GPUGraphicsPipeline* pipeline_base = nullptr;
    SDL_GPUGraphicsPipeline* pipeline_stencil = nullptr;
    SDL_GPUGraphicsPipeline* pipeline_trans = nullptr;
    SDL_GPUGraphicsPipeline* pipeline_3d = nullptr;

    SDL_GPUSampler* sampler = nullptr;
    SDL_GPUTexture* white_tex = nullptr;
    SDL_GPUTextureFormat swapchain_format = SDL_GPU_TEXTUREFORMAT_INVALID;

    bool initPipelines();
};
