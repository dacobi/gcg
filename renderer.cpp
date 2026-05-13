#include "renderer.h"
#include <cstdio>
#include <vector>
#include <cstring>

#include "vert_spv.h"
#include "frag_base_spv.h"
#include "frag_stencil_spv.h"

static SDL_GPUShader* LoadSPIRVShader(SDL_GPUDevice* device, const unsigned char* bytecode, unsigned int size, SDL_ShaderCross_ShaderStage stage, Uint32 num_samplers, Uint32 num_uniform_buffers) {
    SDL_ShaderCross_SPIRV_Info spirv_info = {};
    spirv_info.bytecode = bytecode;
    spirv_info.bytecode_size = size;
    spirv_info.entrypoint = "main";
    spirv_info.shader_stage = stage;
    
    SDL_ShaderCross_GraphicsShaderResourceInfo resource_info = {};
    resource_info.num_samplers = num_samplers;
    resource_info.num_uniform_buffers = num_uniform_buffers;
    
    SDL_GPUShader* shader = SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(device, &spirv_info, &resource_info, 0);
    if (!shader) {
        std::printf("Failed to compile SPIR-V to GPUShader: %s\n", SDL_GetError());
    }
    return shader;
}

Renderer::Renderer() {}

Renderer::~Renderer() {
    shutdown();
}

bool Renderer::init(SDL_Window* win) {
    window = win;
    
    device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL, true, nullptr);
    if (!device) {
        std::printf("Failed to create GPU device: %s\n", SDL_GetError());
        return false;
    }
    
    if (!SDL_ClaimWindowForGPUDevice(device, window)) {
        std::printf("Failed to claim window for GPU device: %s\n", SDL_GetError());
        return false;
    }
    
    if (!SDL_ShaderCross_Init()) {
        std::printf("Failed to init ShaderCross: %s\n", SDL_GetError());
        return false;
    }
    
    if (!initPipelines()) {
        return false;
    }
    
    SDL_GPUSamplerCreateInfo sampler_info = {};
    sampler_info.min_filter = SDL_GPU_FILTER_LINEAR;
    sampler_info.mag_filter = SDL_GPU_FILTER_LINEAR;
    sampler_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
    sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler = SDL_CreateGPUSampler(device, &sampler_info);
    
    return true;
}

bool Renderer::initPipelines() {
    swapchain_format = SDL_GetGPUSwapchainTextureFormat(device, window);
    
    SDL_GPUShader* vs = LoadSPIRVShader(device, vert_spv, vert_spv_len, SDL_SHADERCROSS_SHADERSTAGE_VERTEX, 0, 1);
    if (!vs) return false;
    
    SDL_GPUShader* fs_base = LoadSPIRVShader(device, frag_base_spv, frag_base_spv_len, SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT, 1, 0);
    if (!fs_base) return false;
    
    SDL_GPUShader* fs_stencil = LoadSPIRVShader(device, frag_stencil_spv, frag_stencil_spv_len, SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT, 2, 0);
    if (!fs_stencil) return false;
    
    SDL_GPUGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.vertex_shader = vs;
    pipeline_info.fragment_shader = fs_base;
    pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    
    pipeline_info.target_info.num_color_targets = 1;
    pipeline_info.target_info.color_target_descriptions = new SDL_GPUColorTargetDescription[1];
    
    SDL_GPUColorTargetDescription& color_target = const_cast<SDL_GPUColorTargetDescription&>(pipeline_info.target_info.color_target_descriptions[0]);
    color_target.format = swapchain_format;
    color_target.blend_state.enable_blend = true;
    color_target.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    color_target.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    color_target.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    color_target.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    color_target.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    color_target.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    color_target.blend_state.color_write_mask = 0xF;
    
    pipeline_base = SDL_CreateGPUGraphicsPipeline(device, &pipeline_info);
    if (!pipeline_base) std::printf("Failed to create pipeline_base: %s\n", SDL_GetError());
    
    pipeline_info.fragment_shader = fs_stencil;
    pipeline_stencil = SDL_CreateGPUGraphicsPipeline(device, &pipeline_info);
    if (!pipeline_stencil) std::printf("Failed to create pipeline_stencil: %s\n", SDL_GetError());
    
    delete[] pipeline_info.target_info.color_target_descriptions;
    
    SDL_ReleaseGPUShader(device, vs);
    SDL_ReleaseGPUShader(device, fs_base);
    SDL_ReleaseGPUShader(device, fs_stencil);
    
    return pipeline_base && pipeline_stencil;
}

void Renderer::shutdown() {
    if (sampler) {
        SDL_ReleaseGPUSampler(device, sampler);
        sampler = nullptr;
    }
    if (pipeline_base) {
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline_base);
        pipeline_base = nullptr;
    }
    if (pipeline_stencil) {
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline_stencil);
        pipeline_stencil = nullptr;
    }
    if (device) {
        SDL_ShaderCross_Quit();
        if (window) {
            SDL_ReleaseWindowFromGPUDevice(device, window);
        }
        SDL_DestroyGPUDevice(device);
        device = nullptr;
    }
    window = nullptr;
}

void Renderer::beginFrame() {
    current_cmd_buf = SDL_AcquireGPUCommandBuffer(device);
    if (!current_cmd_buf) return;
    
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(current_cmd_buf, window, &swapchain_texture, nullptr, nullptr)) {
        SDL_SubmitGPUCommandBuffer(current_cmd_buf);
        current_cmd_buf = nullptr;
        return;
    }
}

void Renderer::beginRenderPass() {
    if (!current_cmd_buf || !swapchain_texture) return;
    
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    if (w != target_width || h != target_height || !color_target) {
        if (color_target) {
            SDL_ReleaseGPUTexture(device, color_target);
        }
        SDL_GPUTextureCreateInfo tex_info = {};
        tex_info.type = SDL_GPU_TEXTURETYPE_2D;
        tex_info.format = swapchain_format;
        tex_info.width = w;
        tex_info.height = h;
        tex_info.layer_count_or_depth = 1;
        tex_info.num_levels = 1;
        tex_info.sample_count = SDL_GPU_SAMPLECOUNT_1;
        tex_info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
        color_target = SDL_CreateGPUTexture(device, &tex_info);
        target_width = w;
        target_height = h;
    }
    
    SDL_GPUColorTargetInfo color_target_info = {};
    color_target_info.texture = color_target;
    color_target_info.clear_color = {0.10f, 0.08f, 0.15f, 1.0f};
    color_target_info.load_op = SDL_GPU_LOADOP_CLEAR;
    color_target_info.store_op = SDL_GPU_STOREOP_STORE;
    
    current_render_pass = SDL_BeginGPURenderPass(current_cmd_buf, &color_target_info, 1, nullptr);
}

void Renderer::endRenderPass() {
    if (current_render_pass) {
        SDL_EndGPURenderPass(current_render_pass);
        current_render_pass = nullptr;
    }
}

void Renderer::blitToSwapchain() {
    if (current_cmd_buf && color_target && swapchain_texture) {
        SDL_GPUBlitInfo blit_info = {};
        
        blit_info.source.texture = color_target;
        blit_info.source.w = target_width;
        blit_info.source.h = target_height;
        
        blit_info.destination.texture = swapchain_texture;
        blit_info.destination.w = target_width;
        blit_info.destination.h = target_height;
        
        blit_info.load_op = SDL_GPU_LOADOP_DONT_CARE;
        blit_info.clear_color = {0, 0, 0, 1};
        blit_info.flip_mode = SDL_FLIP_NONE;
        blit_info.filter = SDL_GPU_FILTER_NEAREST;
        
        SDL_BlitGPUTexture(current_cmd_buf, &blit_info);
    }
}

void Renderer::endFrame() {
    endRenderPass();
    blitToSwapchain();
    if (current_cmd_buf) {
        SDL_SubmitGPUCommandBuffer(current_cmd_buf);
        current_cmd_buf = nullptr;
    }
}

SDL_GPUTexture* Renderer::createAndUploadTexture(int width, int height, SDL_GPUTextureFormat format, const void* pixels, int pitch) {
    if (!device) return nullptr;
    
    SDL_GPUTextureCreateInfo tex_info = {};
    tex_info.type = SDL_GPU_TEXTURETYPE_2D;
    tex_info.format = format;
    tex_info.width = width;
    tex_info.height = height;
    tex_info.layer_count_or_depth = 1;
    tex_info.num_levels = 1;
    tex_info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    tex_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    
    SDL_GPUTexture* tex = SDL_CreateGPUTexture(device, &tex_info);
    if (!tex) {
        std::printf("Failed to create texture: %s\n", SDL_GetError());
        return nullptr;
    }
    
    Uint32 tex_size = height * pitch;
    
    SDL_GPUTransferBufferCreateInfo tb_info = {};
    tb_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tb_info.size = tex_size;
    
    SDL_GPUTransferBuffer* tb = SDL_CreateGPUTransferBuffer(device, &tb_info);
    if (!tb) {
        SDL_ReleaseGPUTexture(device, tex);
        return nullptr;
    }
    
    void* map = SDL_MapGPUTransferBuffer(device, tb, false);
    if (map) {
        std::memcpy(map, pixels, tex_size);
        SDL_UnmapGPUTransferBuffer(device, tb);
        
        SDL_GPUCommandBuffer* cmd = current_cmd_buf;
        bool own_cmd = false;
        if (!cmd) {
            cmd = SDL_AcquireGPUCommandBuffer(device);
            own_cmd = true;
        }

        SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(cmd);
        
        SDL_GPUTextureTransferInfo src = {};
        src.transfer_buffer = tb;
        src.offset = 0;
        // Manual calculation for now: 4 for RGBA8/BGRA8, 1 for A8
        int bpp = 4;
        if (format == SDL_GPU_TEXTUREFORMAT_A8_UNORM) bpp = 1;
        src.pixels_per_row = pitch / bpp;
        
        SDL_GPUTextureRegion dst = {};
        dst.texture = tex;
        dst.w = width;
        dst.h = height;
        dst.d = 1;
        
        SDL_UploadToGPUTexture(copy_pass, &src, &dst, false);
        SDL_EndGPUCopyPass(copy_pass);
        
        if (own_cmd) {
            SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
            SDL_WaitForGPUFences(device, true, &fence, 1);
            SDL_ReleaseGPUFence(device, fence);
        }
    }
    
    SDL_ReleaseGPUTransferBuffer(device, tb);
    
    return tex;
}

SDL_GPUTexture* Renderer::createTexture(int width, int height, SDL_GPUTextureFormat format) {
    if (!device) return nullptr;
    
    SDL_GPUTextureCreateInfo tex_info = {};
    tex_info.type = SDL_GPU_TEXTURETYPE_2D;
    tex_info.format = format;
    tex_info.width = width;
    tex_info.height = height;
    tex_info.layer_count_or_depth = 1;
    tex_info.num_levels = 1;
    tex_info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    tex_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    
    SDL_GPUTexture* tex = SDL_CreateGPUTexture(device, &tex_info);
    if (!tex) {
        std::printf("Failed to create texture: %s\n", SDL_GetError());
        return nullptr;
    }
    return tex;
}

void Renderer::updateTexture(SDL_GPUTexture* tex, int width, int height, SDL_GPUTextureFormat format, const void* pixels, int pitch) {
    if (!device || !tex) return;
    
    Uint32 tex_size = height * pitch;
    
    SDL_GPUTransferBufferCreateInfo tb_info = {};
    tb_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tb_info.size = tex_size;
    
    SDL_GPUTransferBuffer* tb = SDL_CreateGPUTransferBuffer(device, &tb_info);
    if (!tb) return;
    
    void* map = SDL_MapGPUTransferBuffer(device, tb, false);
    if (map) {
        std::memcpy(map, pixels, tex_size);
        SDL_UnmapGPUTransferBuffer(device, tb);
        
        SDL_GPUCommandBuffer* cmd = current_cmd_buf;
        bool own_cmd = false;
        if (!cmd) {
            cmd = SDL_AcquireGPUCommandBuffer(device);
            own_cmd = true;
        }

        SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(cmd);
        
        SDL_GPUTextureTransferInfo src = {};
        src.transfer_buffer = tb;
        src.offset = 0;
        // Manual calculation for now: 4 for RGBA8/BGRA8, 1 for A8
        int bpp = 4;
        if (format == SDL_GPU_TEXTUREFORMAT_A8_UNORM) bpp = 1;
        src.pixels_per_row = pitch / bpp;
        
        SDL_GPUTextureRegion dst = {};
        dst.texture = tex;
        dst.w = width;
        dst.h = height;
        dst.d = 1;
        
        SDL_UploadToGPUTexture(copy_pass, &src, &dst, false);
        SDL_EndGPUCopyPass(copy_pass);

        if (own_cmd) {
            SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
            SDL_WaitForGPUFences(device, true, &fence, 1);
            SDL_ReleaseGPUFence(device, fence);
        }
    }
    
    SDL_ReleaseGPUTransferBuffer(device, tb);
}

SDL_Surface* Renderer::readPixels() {
    if (!device || !color_target || !current_cmd_buf) return nullptr;
    
    blitToSwapchain();
    
    int w = target_width;
    int h = target_height;
    
    SDL_GPUTransferBufferCreateInfo tb_info = {};
    tb_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    tb_info.size = w * h * 4; // Assuming 4 bytes per pixel
    
    SDL_GPUTransferBuffer* tb = SDL_CreateGPUTransferBuffer(device, &tb_info);
    if (!tb) return nullptr;
    
    SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(current_cmd_buf);
    
    SDL_GPUTextureRegion src = {};
    src.texture = color_target;
    src.w = w;
    src.h = h;
    src.d = 1;
    
    SDL_GPUTextureTransferInfo dst = {};
    dst.transfer_buffer = tb;
    dst.offset = 0;
    
    SDL_DownloadFromGPUTexture(copy_pass, &src, &dst);
    SDL_EndGPUCopyPass(copy_pass);
    
    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(current_cmd_buf);
    current_cmd_buf = nullptr; // prevent endFrame from submitting again
    
    SDL_WaitForGPUFences(device, true, &fence, 1);
    SDL_ReleaseGPUFence(device, fence);
    
    SDL_Surface* surf = nullptr;
    void* map = SDL_MapGPUTransferBuffer(device, tb, false);
    if (map) {
        SDL_PixelFormat pix_fmt = SDL_PIXELFORMAT_RGBA32;
        if (swapchain_format == SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM) {
            pix_fmt = SDL_PIXELFORMAT_BGRA32;
        }
        
        surf = SDL_CreateSurfaceFrom(w, h, pix_fmt, map, w * 4);
        if (surf) {
            SDL_Surface* copy = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
            SDL_BlitSurface(surf, nullptr, copy, nullptr);
            SDL_DestroySurface(surf);
            surf = copy;
        }
        SDL_UnmapGPUTransferBuffer(device, tb);
    }
    
    SDL_ReleaseGPUTransferBuffer(device, tb);
    return surf;
}
struct Transform {
    float projection[16];
    float dst_rect[4];
    float color[4];
};

static void make_ortho(float* m, float left, float right, float bottom, float top, float near_val, float far_val) {
    m[0] = 2.0f / (right - left); m[4] = 0.0f; m[8] = 0.0f; m[12] = -(right + left) / (right - left);
    m[1] = 0.0f; m[5] = 2.0f / (top - bottom); m[9] = 0.0f; m[13] = -(top + bottom) / (top - bottom);
    m[2] = 0.0f; m[6] = 0.0f; m[10] = -2.0f / (far_val - near_val); m[14] = -(far_val + near_val) / (far_val - near_val);
    m[3] = 0.0f; m[7] = 0.0f; m[11] = 0.0f; m[15] = 1.0f;
}

void Renderer::drawBackground(SDL_GPUTexture* tex) {
    if (!current_render_pass || !tex) return;
    
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    
    SDL_GPUViewport viewport = { 0.0f, 0.0f, (float)w, (float)h, 0.0f, 1.0f };
    SDL_SetGPUViewport(current_render_pass, &viewport);
    
    SDL_BindGPUGraphicsPipeline(current_render_pass, pipeline_base);
    
    SDL_GPUTextureSamplerBinding binds[1];
    binds[0].texture = tex;
    binds[0].sampler = sampler;
    SDL_BindGPUFragmentSamplers(current_render_pass, 0, binds, 1);
    
    Transform t = {};
    make_ortho(t.projection, 0.0f, (float)w, (float)h, 0.0f, -1.0f, 1.0f);
    t.dst_rect[0] = 0.0f;
    t.dst_rect[1] = 0.0f;
    t.dst_rect[2] = (float)w;
    t.dst_rect[3] = (float)h;
    t.color[0] = 1.0f;
    t.color[1] = 1.0f;
    t.color[2] = 1.0f;
    t.color[3] = 1.0f;
    
    SDL_PushGPUVertexUniformData(current_cmd_buf, 0, &t, sizeof(t));
    SDL_DrawGPUPrimitives(current_render_pass, 6, 1, 0, 0);
}

void Renderer::drawBouncer(SDL_GPUTexture* tex, const SDL_FRect& dst, Uint8 r, Uint8 g, Uint8 b, Uint8 a, SDL_GPUTexture* stencil_tex) {
    if (!current_render_pass || !tex) return;
    
    int win_w, win_h;
    SDL_GetWindowSize(window, &win_w, &win_h);

    SDL_GPUViewport viewport = { 0.0f, 0.0f, (float)win_w, (float)win_h, 0.0f, 1.0f };
    SDL_SetGPUViewport(current_render_pass, &viewport);
    
    Transform t = {};
    make_ortho(t.projection, 0.0f, (float)win_w, (float)win_h, 0.0f, -1.0f, 1.0f);
    t.dst_rect[0] = dst.x;
    t.dst_rect[1] = dst.y;
    t.dst_rect[2] = dst.w;
    t.dst_rect[3] = dst.h;
    t.color[0] = r / 255.0f;
    t.color[1] = g / 255.0f;
    t.color[2] = b / 255.0f;
    t.color[3] = a / 255.0f;
    
    SDL_PushGPUVertexUniformData(current_cmd_buf, 0, &t, sizeof(t));
    
    if (stencil_tex) {
        SDL_BindGPUGraphicsPipeline(current_render_pass, pipeline_stencil);
        SDL_GPUTextureSamplerBinding binds[2];
        binds[0].texture = tex;
        binds[0].sampler = sampler;
        binds[1].texture = stencil_tex;
        binds[1].sampler = sampler;
        SDL_BindGPUFragmentSamplers(current_render_pass, 0, binds, 2);
    } else {
        SDL_BindGPUGraphicsPipeline(current_render_pass, pipeline_base);
        SDL_GPUTextureSamplerBinding binds[1];
        binds[0].texture = tex;
        binds[0].sampler = sampler;
        SDL_BindGPUFragmentSamplers(current_render_pass, 0, binds, 1);
    }
    
    SDL_DrawGPUPrimitives(current_render_pass, 6, 1, 0, 0);
}
