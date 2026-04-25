// SDL3 + Dear ImGui with animated plasma background and transparent text overlay
// Usage: ./imtest [--record output.mp4] [--record-max N] [--no-maximize] [text...]
//   --record FILE     start recording frames to FILE on launch
//   --record-max N    max recording length in seconds (default 59)
//   --no-maximize     don't start the window maximized
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3_image/SDL_image.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <algorithm>
#include <vector>
#include <iostream>
#include <tuple>
#include <regex>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <SDL2/SDL_render.h>
}

class NvencEncoder {
private:
    AVFormatContext* fmt_ctx = nullptr;
    AVCodecContext* codec_ctx = nullptr;
    AVStream* stream = nullptr;
    AVFrame* frame = nullptr;
    SwsContext* sws_ctx = nullptr;
    int width, height;
    int64_t frame_pts = 0;

public:
    NvencEncoder(int w, int h, int fps, const std::string& path) : width(w), height(h) {
        // 1. Initialize Output Context (-y and path)
        avformat_alloc_output_context2(&fmt_ctx, nullptr, nullptr, path.c_str());

        // 2. Find and Configure NVENC (-c:v h264_nvenc)
        const AVCodec* codec = avcodec_find_encoder_by_name("h264_nvenc");
        if (!codec) throw std::runtime_error("NVENC not found");

        stream = avformat_new_stream(fmt_ctx, codec);
        codec_ctx = avcodec_alloc_context3(codec);

        // Core Parameters (-video_size, -framerate, -pix_fmt)
        codec_ctx->width = w;
        codec_ctx->height = h;
        codec_ctx->time_base = {1, fps};
        codec_ctx->framerate = {fps, 1};
        //codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
        codec_ctx->pix_fmt = AV_PIX_FMT_RGBA;

        // Rate Control & Quality (-preset fast, -rc vbr, -cq 18, -b:v 0)
        codec_ctx->bit_rate = 0; 
        av_opt_set(codec_ctx->priv_data, "preset", "fast", 0);
        av_opt_set(codec_ctx->priv_data, "rc", "vbr", 0);
        av_opt_set_int(codec_ctx->priv_data, "cq", 18, 0);

        if (fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
            codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        avcodec_open2(codec_ctx, codec, nullptr);
        avcodec_parameters_from_context(stream->codecpar, codec_ctx);

        // 3. Setup Scaler (RGBA input -> YUV420P output)
        sws_ctx = sws_getContext(w, h, AV_PIX_FMT_RGBA, 
                                 w, h, AV_PIX_FMT_YUV420P, 
                                 SWS_BILINEAR, nullptr, nullptr, nullptr);

        // 4. Open File and Write Header (+faststart)
        avio_open(&fmt_ctx->pb, path.c_str(), AVIO_FLAG_WRITE);
        
        AVDictionary* muxer_opts = nullptr;
        av_dict_set(&muxer_opts, "movflags", "faststart", 0); // -movflags +faststart
        if (avformat_write_header(fmt_ctx, &muxer_opts) < 0) {
            av_dict_free(&muxer_opts);
            throw std::runtime_error("Could not write header");
        }
        av_dict_free(&muxer_opts);

        // Prepare working frame
        frame = av_frame_alloc();
        frame->format = codec_ctx->pix_fmt;
        frame->width = w;
        frame->height = h;
        av_frame_get_buffer(frame, 32);
    }

void encodeFrame(const uint8_t* rgba_data) {
    // 1. Ensure the frame is writable
    av_frame_make_writable(frame);

    // 2. Copy raw RGBA data into the frame's data plane
    // RGBA is a packed format, so all data is in data[0]
    int line_size = width * 4; 
    for (int y = 0; y < height; y++) {
        memcpy(frame->data[0] + y * frame->linesize[0], 
               rgba_data + y * line_size, 
               line_size);
    }

    // 3. Set Timestamps
    frame->pts = frame_pts++;
    
    // 4. Send to encoder
    int ret = avcodec_send_frame(codec_ctx, frame);
    if (ret < 0) return;

    AVPacket* pkt = av_packet_alloc();
    while (avcodec_receive_packet(codec_ctx, pkt) == 0) {
        av_packet_rescale_ts(pkt, codec_ctx->time_base, stream->time_base);
        pkt->stream_index = stream->index;
        av_interleaved_write_frame(fmt_ctx, pkt);
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);
}

    ~NvencEncoder() {
        // Flush and Close
        avcodec_send_frame(codec_ctx, nullptr); 
        av_write_trailer(fmt_ctx);
        
        sws_freeContext(sws_ctx);
        av_frame_free(&frame);
        avcodec_free_context(&codec_ctx);
        avio_closep(&fmt_ctx->pb);
        avformat_free_context(fmt_ctx);
    }
};

class NvdecDecode {
private:
    AVFormatContext* fmt_ctx = nullptr;
    AVCodecContext* codec_ctx = nullptr;
    int video_stream_idx = -1;
    AVFrame* frame = nullptr;
    AVFrame* frame_rgba = nullptr;
    AVPacket* packet = nullptr;
    SwsContext* sws_ctx = nullptr;
    uint8_t* rgba_buffer = nullptr;
    int width = 0, height = 0;

public:
    NvdecDecode(const std::string& path) {
        if (avformat_open_input(&fmt_ctx, path.c_str(), nullptr, nullptr) < 0)
            throw std::runtime_error("Could not open source file: " + path);

        if (avformat_find_stream_info(fmt_ctx, nullptr) < 0)
            throw std::runtime_error("Could not find stream information");

        const AVCodec* codec = nullptr;
        for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
            if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                video_stream_idx = i;
                AVCodecID codec_id = fmt_ctx->streams[i]->codecpar->codec_id;

                // Attempt to find an NVIDIA CUVID hardware decoder
                std::string cuvid_name;
                switch (codec_id) {
                    case AV_CODEC_ID_H264:  cuvid_name = "h264_cuvid"; break;
                    case AV_CODEC_ID_HEVC:  cuvid_name = "hevc_cuvid"; break;
                    case AV_CODEC_ID_VP8:   cuvid_name = "vp8_cuvid"; break;
                    case AV_CODEC_ID_VP9:   cuvid_name = "vp9_cuvid"; break;
                    case AV_CODEC_ID_AV1:   cuvid_name = "av1_cuvid"; break;
                    case AV_CODEC_ID_VC1:   cuvid_name = "vc1_cuvid"; break;
                    case AV_CODEC_ID_MPEG4: cuvid_name = "mpeg4_cuvid"; break;
                    case AV_CODEC_ID_MPEG2VIDEO: cuvid_name = "mpeg2_cuvid"; break;
                    case AV_CODEC_ID_MPEG1VIDEO: cuvid_name = "mpeg1_cuvid"; break;
                    case AV_CODEC_ID_MJPEG: cuvid_name = "mjpeg_cuvid"; break;
                    default: break;
                }

                if (!cuvid_name.empty()) {
                    codec = avcodec_find_decoder_by_name(cuvid_name.c_str());
                    if (codec) std::printf("NvdecDecode: Using hardware decoder %s\n", cuvid_name.c_str());
                }

                if (!codec) {
                    codec = avcodec_find_decoder(codec_id);
                    if (codec) std::printf("NvdecDecode: Falling back to software decoder %s\n", codec->name);
                }
                break;
            }
        }

        if (video_stream_idx == -1 || !codec)
            throw std::runtime_error("Could not find suitable video stream or decoder");

        codec_ctx = avcodec_alloc_context3(codec);
        avcodec_parameters_to_context(codec_ctx, fmt_ctx->streams[video_stream_idx]->codecpar);
        
        // Some HW decoders need threading disabled or specific options
        // codec_ctx->thread_count = 1;

        if (avcodec_open2(codec_ctx, codec, nullptr) < 0)
            throw std::runtime_error("Could not open codec");

        width = codec_ctx->width;
        height = codec_ctx->height;

        frame = av_frame_alloc();
        frame_rgba = av_frame_alloc();
        packet = av_packet_alloc();

        int num_bytes = av_image_get_buffer_size(AV_PIX_FMT_RGBA, width, height, 32);
        rgba_buffer = (uint8_t*)av_malloc(num_bytes * sizeof(uint8_t));
        av_image_fill_arrays(frame_rgba->data, frame_rgba->linesize, rgba_buffer, AV_PIX_FMT_RGBA, width, height, 32);

        // We'll defer SwsContext creation until we have the first frame's format
    }

    ~NvdecDecode() {
        if (sws_ctx) sws_freeContext(sws_ctx);
        if (rgba_buffer) av_free(rgba_buffer);
        av_frame_free(&frame_rgba);
        av_frame_free(&frame);
        av_packet_free(&packet);
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&fmt_ctx);
    }

    bool getNextFrame(SDL_Texture* texture) {
        while (av_read_frame(fmt_ctx, packet) >= 0) {
            if (packet->stream_index == video_stream_idx) {
                if (avcodec_send_packet(codec_ctx, packet) == 0) {
                    while (avcodec_receive_frame(codec_ctx, frame) == 0) {
                        // Initialize sws_ctx if not already done (or if format changed)
                        if (!sws_ctx) {
                            sws_ctx = sws_getContext(width, height, (AVPixelFormat)frame->format,
                                                     width, height, AV_PIX_FMT_RGBA,
                                                     SWS_BILINEAR, nullptr, nullptr, nullptr);
                        }

                        if (sws_ctx) {
                            sws_scale(sws_ctx, frame->data, frame->linesize, 0, height,
                                      frame_rgba->data, frame_rgba->linesize);

                            // Update SDL Texture
                            SDL_UpdateTexture(texture, nullptr, rgba_buffer, width * 4);
                            
                            av_packet_unref(packet);
                            return true;
                        }
                    }
                }
            }
            av_packet_unref(packet);
        }
        return false; // EOF or error
    }

    int getWidth() const { return width; }
    int getHeight() const { return height; }
};

bool  plasma_render_tiles = false;
float cur_rel;
int cur_w, cur_h;
SDL_Window* window;
SDL_Renderer* renderer;

// ---------------------------------------------------------------------------
// FFmpeg recording — pipe raw RGBA frames to ffmpeg, produce mp4
// ---------------------------------------------------------------------------
struct Recorder {
    FILE*  pipe       = nullptr;
    int    width      = 0;
    int    height     = 0;
    int    fps        = 60;
    int    frame_count = 0;
    std::string output_path;
};
NvencEncoder* myNvec = NULL;


static bool recorder_start(Recorder& rec, int w, int h, const char* path, int fps = 60) {
    if (myNvec != NULL) return false; // already recording
    // h264 with yuv420p requires even dimensions
    w &= ~1;
    h &= ~1;
    if (w < 2) w = 2;
    if (h < 2) h = 2;
    rec.width  = w;
    rec.height = h;
    rec.fps    = fps;
    rec.frame_count = 0;
    rec.output_path = path;

    myNvec = new NvencEncoder(w, h, fps, std::string(path));

    SDL_SetWindowResizable(window, false);


    // Build ffmpeg command: read raw RGBA from stdin, encode to h264 mp4
   /* char cmd[1024];
    std::snprintf(cmd, sizeof(cmd),
        "ffmpeg -y -f rawvideo -pixel_format rgba -video_size %dx%d -framerate %d "
        "-i - -c:v  h264_nvenc -preset fast -rc vbr -cq 18 -b:v 0 -pix_fmt yuv420p "
        "-movflags +faststart \"%s\"",
        w, h, fps, path);
*/
/*

 std::snprintf(cmd, sizeof(cmd),
 "ffmpeg -y -f rawvideo -pixel_format rgba -video_size %dx%d -framerate %d"
"-i - -c:v hevc_nvenc -preset fast -rc vbr -cq 18 -b:v 0 -pix_fmt yuv420p"
"-movflags +faststart \"%s\"");
*/
/*
    rec.pipe = popen(cmd, "w"); 
    if (!rec.pipe) {
        std::printf("Failed to start ffmpeg for recording\n");
        return false;
    }
*/

    std::printf("Recording started: %s (%dx%d @ %d fps)\n", path, w, h, fps);
    return true;
}

static void recorder_feed_frame(Recorder& rec, SDL_Renderer* renderer) {
    if (myNvec == NULL) return;

    // Read back the rendered frame
    SDL_Surface* surf = SDL_RenderReadPixels(renderer, nullptr);
    if (!surf) {
        std::printf("SDL_RenderReadPixels failed: %s\n", SDL_GetError());
        return;
    }

    // Convert to RGBA32 if needed
    SDL_Surface* rgba = SDL_ConvertSurface(surf, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(surf);
    if (!rgba) {
        std::printf("Surface conversion failed: %s\n", SDL_GetError());
        return;
    }

    // If the window was resized, scale the frame back to the original
    // recording dimensions so ffmpeg always receives a consistent size.
    
    SDL_Surface* final_surf = rgba;
    /*
    bool need_free_final = false;
    if (rgba->w != rec.width || rgba->h != rec.height) {
        SDL_Surface* scaled = SDL_CreateSurface(rec.width, rec.height, SDL_PIXELFORMAT_RGBA32);
        if (scaled) {
            SDL_BlitSurfaceScaled(rgba, nullptr, scaled, nullptr, SDL_SCALEMODE_LINEAR);
            SDL_DestroySurface(rgba);
            final_surf = scaled;
            need_free_final = true;
        }
        // If scaling failed, fall back to writing the mismatched frame
        // (better than dropping it entirely)
    }
    */
    // Write raw pixels to ffmpeg pipe
    SDL_LockSurface(final_surf);
    size_t row_bytes = static_cast<size_t>(rec.width) * 4;
    auto* pixels = static_cast<const Uint8*>(final_surf->pixels);
    //for (int y = 0; y < rec.height; ++y) {
    //    fwrite(pixels + y * final_surf->pitch, 1, row_bytes, rec.pipe);
    //}
    myNvec->encodeFrame(pixels);
    SDL_UnlockSurface(final_surf);

    //if (need_free_final)
    //    SDL_DestroySurface(final_surf);
   // else
        SDL_DestroySurface(rgba);

    rec.frame_count++;
}

static void recorder_stop(Recorder& rec) {
    if (myNvec == NULL) return;
    

    std::printf("Recording stopped: %s (%d frames)\n", rec.output_path.c_str(), rec.frame_count);
        
    
    delete myNvec;
    myNvec = NULL;
    SDL_SetWindowResizable(window, true);
}

// ---------------------------------------------------------------------------
// A pre-rendered text texture with its label and dimensions
// ---------------------------------------------------------------------------
struct TextEntry {
    std::string   label;   // the original text string
    SDL_Texture*  tex;
    int           w, h;
    bool           bNoColor = false;
};

// ---------------------------------------------------------------------------
// A single bouncing text instance
// ---------------------------------------------------------------------------


/*
class ContentParser {
public:
    
    static std::vector<std::tuple<std::string, bool>> parse(const std::string& input) {
        std::vector<std::tuple<std::string, bool>> results;
        
        // Regex to match [image:filename.ext]
        // Group 1 captures the filename inside the tags
        std::regex imageRegex(R"(\[image:([^\]]+)\])");
        
        auto words_begin = std::sregex_iterator(input.begin(), input.end(), imageRegex);
        auto words_end = std::sregex_iterator();

        size_t lastPos = 0;

        for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
            std::smatch match = *i;
            size_t matchPos = match.position();

            // 1. Extract text before the match (if any)
            if (matchPos > lastPos) {
                results.emplace_back(input.substr(lastPos, matchPos - lastPos), false);
            }

            // 2. Extract the filename from the capture group
            results.emplace_back(match[1].str(), true);

            // Update position to after the full match
            lastPos = matchPos + match.length();
        }

        // 3. Extract remaining text after the last match
        if (lastPos < input.length()) {
            results.emplace_back(input.substr(lastPos), false);
        }

        for(auto& rs : results){
            bool isFile = std::get<1>(rs);
            if(isFile){ std:: cout << "File: ";}
            std::cout << std::get<0>(rs) << std::endl;

        }

        return results;
    }
};

*/
struct ParsedSegment {
    int posx, posy, velox, veloy;
    bool bIsStatic;
    int r, g, b; 
    std::string content;
    int bIsFile; // 0: None, 1: PNG, 2: Video
    std::string fullInput;
    int over_w = 0, over_h = 0;
};

class ContentParser {
public:
    static std::vector<ParsedSegment> parse(const std::string& input) {
        std::vector<ParsedSegment> results;

        // Initial State
        int px = 0, py = 0, vx = 0, vy = 0;
        int cr = 255, cg = 255, cb = 255;
        int ow = 0, oh = 0;
        bool bIsStatic = false;
        size_t cursor = 0;

        // 1. Parse the [pos: ...] prefix
        std::regex posRegex(R"(^\[pos:\s*([^\]]+)\])");
        std::smatch posMatch;
        if (std::regex_search(input, posMatch, posRegex)) {
            std::vector<std::string> tokens = tokenize(posMatch.str(1));
            if (tokens.size() >= 5) {
                px = std::stoi(tokens[0]); py = std::stoi(tokens[1]);
                vx = std::stoi(tokens[2]); vy = std::stoi(tokens[3]);
                bIsStatic = (std::stoi(tokens[4]) == 1);
            } else if (tokens.size() >= 3) {
                px = std::stoi(tokens[0]); py = std::stoi(tokens[1]);
                vx = 0; vy = 0;
                bIsStatic = (std::stoi(tokens[2]) == 1);
            }
            cursor = posMatch.length();
        }

        // 2. Scan remaining string for [image:...], [video:...], [rect:...], and [rgb:...]
        std::string body = input.substr(cursor);
        std::regex tagRegex(R"(\[(image|video|rgb|rect):\s*([^\]]+)\])");
        auto tags_begin = std::sregex_iterator(body.begin(), body.end(), tagRegex);
        auto tags_end = std::sregex_iterator();

        size_t lastPos = 0;
        for (std::sregex_iterator i = tags_begin; i != tags_end; ++i) {
            std::smatch match = *i;
            size_t matchPos = match.position();

            // Text segment before a tag
            if (matchPos > lastPos) {
                results.push_back({px, py, vx, vy, bIsStatic, cr, cg, cb, body.substr(lastPos, matchPos - lastPos), 0, input, 0, 0});
            }

            std::string tagType = match.str(1);
            std::string tagContent = match.str(2);

            if (tagType == "rgb") {
                std::vector<std::string> rgbTokens = tokenize(tagContent);
                if (rgbTokens.size() >= 3) {
                    cr = std::stoi(rgbTokens[0]);
                    cg = std::stoi(rgbTokens[1]);
                    cb = std::stoi(rgbTokens[2]);
                }
            } else if (tagType == "rect") {
                std::vector<std::string> rectTokens = tokenize(tagContent);
                if (rectTokens.size() >= 2) {
                    ow = std::stoi(rectTokens[0]);
                    oh = std::stoi(rectTokens[1]);
                }
            } else if (tagType == "image") {
                results.push_back({px, py, vx, vy, bIsStatic, cr, cg, cb, tagContent, 1, input, ow, oh});
                ow = 0; oh = 0; // Reset after use
            } else if (tagType == "video") {
                results.push_back({px, py, vx, vy, bIsStatic, cr, cg, cb, tagContent, 2, input, ow, oh});
                ow = 0; oh = 0; // Reset after use
            }

            lastPos = matchPos + match.length();
        }

        // 3. Final trailing text
        if (lastPos < body.length()) {
            results.push_back({px, py, vx, vy, bIsStatic, cr, cg, cb, body.substr(lastPos), 0, input, 0, 0});
        }

        return results;
    }

void processAndPrint(const std::string& input) {
    auto segments = ContentParser::parse(input);
    std::cout << "\nInput: " << input << "\n";
    for (const auto& s : segments) {
        std::printf("  Pos:(%d,%d) Velo:(%d,%d) Static:%s RGB:(%d,%d,%d) Rect:(%d,%d) | Type:%d | Content: \"%s\"\n",
                    s.posx, s.posy, s.velox, s.veloy,
                    s.bIsStatic ? "Y" : "N",
                    s.r, s.g, s.b,
                    s.over_w, s.over_h,
                    s.bIsFile, s.content.c_str());
    }
}


private:
    static std::vector<std::string> tokenize(const std::string& s) {
        std::vector<std::string> res;
        std::stringstream ss(s);
        std::string item;
        while (std::getline(ss, item, ',')) {
            item.erase(0, item.find_first_not_of(" "));
            item.erase(item.find_last_not_of(" ") + 1);
            res.push_back(item);
        }
        return res;
    }
};




struct Bouncer {
    float x, y;
    float vx, vy;
    Uint8 r, g, b;   // random tint colour
    SDL_Texture* tex; // which text texture to use (not owned — shared)
    int tw, th;       // dimensions of that texture()
    NvdecDecode* decoder = nullptr;
};

// ------------------------------------------------
// Single text texture — just the rendered text, no tiling
// Returns the texture; writes dimensions into *out_w / *out_h.
// ---------------------------------------------------------------------------
static SDL_Texture* create_png_texture(SDL_Renderer* renderer,
                                        const char* text,
                                        int* out_w, int* out_h)
{
 
    // White text, semi-transparent — colour modulation will tint per-bouncer
    SDL_Color fg = {255, 255, 255, 200};
    SDL_Surface* text_surf = IMG_Load(text);
    if (!text_surf) {
        std::printf("IMG_Load error: %s\n", SDL_GetError());
        return nullptr;
    }

    *out_w = text_surf->w;
    *out_h = text_surf->h;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, text_surf);
    if (texture) {
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    }

    SDL_DestroySurface(text_surf);
    
    return texture;
}



// ---------------------------------------------------------------------------
// Single text texture — just the rendered text, no tiling
// Returns the texture; writes dimensions into *out_w / *out_h.
// ---------------------------------------------------------------------------
static SDL_Texture* create_text_texture(SDL_Renderer* renderer,
                                        const char* text,
                                        int* out_w, int* out_h)
{
    if (!TTF_Init()) {
        std::printf("TTF_Init error: %s\n", SDL_GetError());
        return nullptr;
    }

    const char* font_paths[] = {
        "/usr/share/fonts/noto/NotoSans-Bold.ttf",
        "/usr/share/fonts/liberation/LiberationSans-Bold.ttf",
        "/usr/share/fonts/TTF/Hack-Bold.ttf",
        "/usr/share/fonts/Adwaita/AdwaitaSans-Regular.ttf",
    };

    TTF_Font* font = nullptr;
    for (auto path : font_paths) {
        font = TTF_OpenFont(path, 120.0f);
        if (font) break;
    }
    if (!font) {
        std::printf("Could not open any font: %s\n", SDL_GetError());
        return nullptr;
    }

    // White text, semi-transparent — colour modulation will tint per-bouncer
    SDL_Color fg = {255, 255, 255, 200};
    SDL_Surface* text_surf = TTF_RenderText_Blended(font, text, 0, fg);
    if (!text_surf) {
        std::printf("TTF_RenderText_Blended error: %s\n", SDL_GetError());
        TTF_CloseFont(font);
        return nullptr;
    }

    *out_w = text_surf->w;
    *out_h = text_surf->h;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, text_surf);
    if (texture) {
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    }

    SDL_DestroySurface(text_surf);
    TTF_CloseFont(font);
    return texture;
}


class BDdisplay {
private:
    std::vector<Bouncer> bouncers;
    SDL_FRect boundingBox = {0.0f, 0.0f, 0.0f, 0.0f};   
    float groupVx = 20.0f; // Default horizontal speed
    float groupVy = 20.0f; // Default vertical speed

    std::string cli_input;

    bool bAmNotMoving = false;

    /*
    void updateBoundingBox(const Bouncer& b) {
        if (bouncers.size() == 1) {
            boundingBox.x = b.x;
            boundingBox.y = b.y;
            boundingBox.w = b.tw;
            boundingBox.h = b.th;
        } else {
            // New elements are added to the left (smaller X)
            float currentRight = boundingBox.x + cur_w; 
            float currentBottom = boundingBox.y + cur_h;

            float newX = b.x;
            float newY = std::min(boundingBox.y, b.y);
            
            float newRight = currentRight; 
            float newBottom = std::max(currentBottom, b.y + b.th);

            boundingBox.x = newX;
            boundingBox.y = newY;
            boundingBox.w = newRight - newX;
            boundingBox.h = newBottom - newY;
        }
    }
    */
public:
    ~BDdisplay() {
        for (auto& b : bouncers) {
            if (b.decoder) delete b.decoder;
            if (b.tex) SDL_DestroyTexture(b.tex);
        }
    }

    void recalculateBoundingBox() {
        if (bouncers.empty()) return;

        float minX = bouncers[0].x;
        float minY = bouncers[0].y;
        float maxX = bouncers[0].x + bouncers[0].tw;
        float maxY = bouncers[0].y + bouncers[0].th;

        for (const auto& b : bouncers) {
            minX = std::min(minX, b.x);
            minY = std::min(minY, b.y);
            maxX = std::max(maxX, b.x + b.tw);
            maxY = std::max(maxY, b.y + b.th);
        }

        boundingBox.x = minX;
        boundingBox.y = minY;
        boundingBox.w = maxX - minX;
        boundingBox.h = maxY - minY;
    }


    void update(float deltaTime, int windowW, int windowH) {
        if (bouncers.empty()) return;

        // Update video frames
        for (auto& b : bouncers) {
            if (b.decoder && b.tex) {
                b.decoder->getNextFrame(b.tex);
            }
        }

        if(bAmNotMoving) return;

        // 1. Move all elements by the group velocity
        for (auto& b : bouncers) {
            b.x += groupVx * deltaTime;
            b.y += groupVy * deltaTime;
        }

        // 2. Update the bounding box position
        recalculateBoundingBox();

        // 3. Check for window collisions using the bounding box
        // Check Left/Right
        if (boundingBox.x < 0) {
            groupVx = std::abs(groupVx); // Force positive
            float offset = -boundingBox.x;
            for (auto& b : bouncers) b.x += offset;
        } else if (boundingBox.x + boundingBox.w > windowW) {
            groupVx = -std::abs(groupVx); // Force negative
            float offset = (boundingBox.x + boundingBox.w) - windowW;
            for (auto& b : bouncers) b.x -= offset;
        }

        // Check Top/Bottom
        if (boundingBox.y < 0) {
            groupVy = std::abs(groupVy); // Force positive
            float offset = -boundingBox.y;
            for (auto& b : bouncers) b.y += offset;
        } else if (boundingBox.y + boundingBox.h > windowH) {
            groupVy = -std::abs(groupVy); // Force negative
            float offset = (boundingBox.y + boundingBox.h) - windowH;
            for (auto& b : bouncers) b.y -= offset;
        }
        
        // Final sync of bounding box after correction
        recalculateBoundingBox();
    }


    bool add(ParsedSegment pd) {
        SDL_Texture* tex = NULL;
        Bouncer newB;

        if (pd.bIsFile == 1) { // PNG
            tex = create_png_texture(renderer, pd.content.c_str(), &newB.tw, &newB.th);
        } else if (pd.bIsFile == 2) { // Video
            try {
                newB.decoder = new NvdecDecode(pd.content);
                newB.tw = newB.decoder->getWidth();
                newB.th = newB.decoder->getHeight();
                // Create streaming texture for video (RGBA is preferred for SDL_UpdateTexture)
                tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, newB.tw, newB.th);
                if (tex) {
                    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
                    newB.decoder->getNextFrame(tex); // load first frame
                }
            } catch (const std::exception& e) {
                std::printf("Video load error: %s\n", e.what());
                return false;
            }
        } else { // Text (pd.bIsFile == 0)
            tex = create_text_texture(renderer, pd.content.c_str(), &newB.tw, &newB.th);
        }

        if(tex == NULL) return false;

        // Apply overrides if provided
        if (pd.over_w > 0) newB.tw = pd.over_w;
        if (pd.over_h > 0) newB.th = pd.over_h;

        bAmNotMoving = pd.bIsStatic;

        newB.vx = pd.velox;
        newB.vy = pd.veloy;
        newB.r = pd.r;
        newB.g = pd.g;
        newB.b = pd.b;
        newB.tex = tex;
        
        if (bouncers.empty()) {
            // Initial spawn point
            newB.x = pd.posx;
            newB.y = pd.posy;
            groupVx = pd.velox;
            groupVy = pd.veloy;
            cli_input = pd.fullInput;
        } else {
            // First entry in vector is the anchor
            const Bouncer& first = bouncers[0];
            
            // Draw 2nd to the left of the first, etc.
            // We use the current boundingBox.x to keep stacking left
            newB.x = boundingBox.x + boundingBox.w;
            
            // Center around the middle horizontal line of the first entry
            float firstMidline = first.y + (first.th / 2.0f);
            newB.y = firstMidline - (newB.th / 2.0f);
        }

        bouncers.push_back(newB);
        
        recalculateBoundingBox();
        return true;
    }

    std::string getInput(){
        return cli_input;
    }


    void draw(SDL_Renderer* renderer) {
        for (auto& b : bouncers) {
            SDL_FRect dst = { b.x, b.y, static_cast<float>(b.tw), static_cast<float>(b.th) };
            
            // Set tint (SDL3 uses Uint8 0-255)
            SDL_SetTextureColorMod(b.tex, b.r, b.g, b.b);
            
            // SDL3 API change: RenderCopyF -> RenderTexture
            SDL_RenderTexture(renderer, b.tex, NULL, &dst);
        }

        // Reset tint for the shared texture
        if (!bouncers.empty()) {
            SDL_SetTextureColorMod(bouncers[0].tex, 255, 255, 255);
        }
    }

    const SDL_FRect& getBounds() const {
        return boundingBox;
    }
};

// Helper: random float in [lo, hi]
static float rand_range(float lo, float hi) {
    return lo + static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) * (hi - lo);
}

// Spawn a new bouncer with random position & velocity
static Bouncer make_bouncer(int win_w, int win_h, SDL_Texture* tex, int tw, int th, bool bNoColor=false) {
    Bouncer b;
    float max_x = static_cast<float>(win_w - tw);
    float max_y = static_cast<float>(win_h - th);
    if (max_x < 0) max_x = 0;
    if (max_y < 0) max_y = 0;
    b.x  = rand_range(0, max_x);
    b.y  = rand_range(0, max_y);
    // Random speed 100–350 px/s, random direction
    b.vx = rand_range(100.0f, 350.0f) * (std::rand() % 2 ? 1.0f : -1.0f);
    b.vy = rand_range(100.0f, 350.0f) * (std::rand() % 2 ? 1.0f : -1.0f);
    // Random vivid colour (at least one channel bright, avoid dark/muddy)
    if(bNoColor == true){
        b.r = static_cast<Uint8>(255);
        b.g = static_cast<Uint8>(255);
        b.b = static_cast<Uint8>(255);
    } else {
        b.r = static_cast<Uint8>(100 + std::rand() % 156);
        b.g = static_cast<Uint8>(100 + std::rand() % 156);
        b.b = static_cast<Uint8>(100 + std::rand() % 156);
    }   
    b.tex = tex;
    b.tw  = tw;
    b.th  = th;
    return b;
}

// ---------------------------------------------------------------------------
// Plasma parameters — randomised once at startup for a unique look each run
// ---------------------------------------------------------------------------
struct PlasmaParams {
    // X/Y animation speeds (used as multipliers on time)
    float drift_speed_x;   // how fast the field drifts horizontally
    float drift_speed_y;   // how fast the field drifts vertically
    float drift_amp;       // amplitude of the drift

    float scale_base_x;    // base spatial frequency X
    float scale_base_y;    // base spatial frequency Y
    float scale_mod_amp;   // breathing amplitude
    float scale_mod_speed_x;
    float scale_mod_speed_y;

    float rot_speed;       // rotation speed
    float warp_base;       // swirl base intensity
    float warp_amp;        // swirl modulation amplitude
    float warp_speed;      // swirl modulation speed
    float swirl_dist_mul;  // distance multiplier for swirl

    // Palette: three phase offsets for R, G, B colour cosines
    float palette_phase_r;
    float palette_phase_g;
    float palette_phase_b;

    // Darkening multipliers (kept moderate so ImGui stays readable)
    float darken_r;
    float darken_g;
    float darken_b;

    float tile_count;
};


PlasmaParams plasma_params;

static PlasmaParams randomise_plasma() {
    PlasmaParams p;
    p.drift_speed_x    = rand_range(0.15f, 0.60f);
    p.drift_speed_y    = rand_range(0.15f, 0.60f);
    p.drift_amp        = rand_range(1.0f, 3.5f);

    p.scale_base_x     = rand_range(6.0f, 16.0f);
    p.scale_base_y     = rand_range(6.0f, 16.0f);
    p.scale_mod_amp    = rand_range(1.0f, 5.0f);
    p.scale_mod_speed_x = rand_range(0.10f, 0.40f);
    p.scale_mod_speed_y = rand_range(0.10f, 0.40f);

    p.rot_speed         = rand_range(0.05f, 0.25f);
    p.warp_base         = rand_range(0.05f, 0.25f);
    p.warp_amp          = rand_range(0.05f, 0.20f);
    p.warp_speed        = rand_range(0.20f, 0.60f);
    p.swirl_dist_mul    = rand_range(3.0f, 10.0f);

    // Random palette — each phase in [0, 1) gives wildly different colour combos
    p.palette_phase_r   = rand_range(0.0f, 1.0f);
    p.palette_phase_g   = rand_range(0.0f, 1.0f);
    p.palette_phase_b   = rand_range(0.0f, 1.0f);

    // Darkening: keep each channel between 0.25 and 0.60 so it's visible but not blinding
    p.darken_r          = rand_range(0.25f, 0.60f);
    p.darken_g          = rand_range(0.25f, 0.60f);
    p.darken_b          = rand_range(0.25f, 0.60f);

    p.tile_count        = rand_range(10.0f, 100.0f);

    return p;
}

// Re-randomise only the palette (colour) fields
static void randomise_plasma_palette(PlasmaParams& p) {
    p.palette_phase_r = rand_range(0.0f, 1.0f);
    p.palette_phase_g = rand_range(0.0f, 1.0f);
    p.palette_phase_b = rand_range(0.0f, 1.0f);
    p.darken_r        = rand_range(0.25f, 0.60f);
    p.darken_g        = rand_range(0.25f, 0.60f);
    p.darken_b        = rand_range(0.25f, 0.60f);
}

// Re-randomise only the X/Y spatial / animation fields
static void randomise_plasma_xy(PlasmaParams& p) {
    p.drift_speed_x     = rand_range(0.15f, 0.60f);
    p.drift_speed_y     = rand_range(0.15f, 0.60f);
    p.drift_amp         = rand_range(1.0f, 3.5f);
    p.scale_base_x      = rand_range(6.0f, 16.0f);
    p.scale_base_y      = rand_range(6.0f, 16.0f);
    p.scale_mod_amp     = rand_range(1.0f, 5.0f);
    p.scale_mod_speed_x = rand_range(0.10f, 0.40f);
    p.scale_mod_speed_y = rand_range(0.10f, 0.40f);
    p.rot_speed          = rand_range(0.05f, 0.25f);
    p.warp_base          = rand_range(0.05f, 0.25f);
    p.warp_amp           = rand_range(0.05f, 0.20f);
    p.warp_speed         = rand_range(0.20f, 0.60f);
    p.swirl_dist_mul     = rand_range(3.0f, 10.0f);

    p.tile_count        = rand_range(10.0f, 100.0f);
}

// ---------------------------------------------------------------------------
// Animated plasma: write directly into a streaming texture each frame
// ---------------------------------------------------------------------------

/*
static void update_plasma_texture(SDL_Texture* tex, int w, int h, float t,
                                  const PlasmaParams& p)
{
    void* pixels = nullptr;
    int   pitch  = 0;
    if (!SDL_LockTexture(tex, nullptr, &pixels, &pitch))
        return;

    auto* dst = static_cast<Uint8*>(pixels);

    // Animated X/Y properties driven by randomised parameters
    float drift_x  = p.drift_amp * std::sin(t * p.drift_speed_x);
    float drift_y  = p.drift_amp * std::cos(t * p.drift_speed_y);
    float scale_x  = p.scale_base_x + p.scale_mod_amp * std::sin(t * p.scale_mod_speed_x);
    float scale_y  = p.scale_base_y + p.scale_mod_amp * std::cos(t * p.scale_mod_speed_y);
    float rot_sin  = std::sin(t * p.rot_speed);
    float rot_cos  = std::cos(t * p.rot_speed);
    float warp_str = p.warp_base + p.warp_amp * std::sin(t * p.warp_speed);

    float r,g,b;

    for (int y = 0; y < h; ++y) {
        auto* row = reinterpret_cast<Uint32*>(dst + y * pitch);
        float fy = static_cast<float>(y) / static_cast<float>(h);
        for (int x = 0; x < w; ++x) {
            float fx = static_cast<float>(x) / static_cast<float>(w);



             // --- NEW TILE LOGIC ---
            // Snap fx and fy to a fixed grid based on tile_count
            
            if(plasma_render_tiles){
            
                if (p.tile_count > 0.0f) {
                    fy = std::floor(fy * p.tile_count) / (p.tile_count);
                    fx = std::floor(fx * (p.tile_count * cur_rel)) / (p.tile_count * cur_rel);
                        
                }
            }
            // ----------------------

            // Now all calculations for rx, ry, wx, wy, and v 
            // will stay constant for every pixel inside the same tile.
            if(true){

// --- NEW CURVY/JACKY LOGIC ---
        // 1. Create a "Jagged" distortion for the edges
        // Using a high-frequency sine based on X/Y to 'jitter' the lookup
        float jitter = std::sin(fx * 200.0f) * std::cos(fy * 200.0f) * 0.01f;
        
        // 2. Create a "Curvy" warp (Domain Warping)
        // This bends the coordinates before they even hit the plasma math
        float curve_x = std::sin(fy * 10.0f + t) * 0.05f;
        float curve_y = std::cos(fx * 10.0f + t) * 0.05f;

        // Apply these to our base fx/fy
        float warped_fx = fx + curve_x + jitter;
        float warped_fy = fy + curve_y + jitter;
        // ------------------------------

        // Apply drift + rotation using the NEW warped coordinates
        float cx = warped_fx - 0.5f;
        float cy = warped_fy - 0.5f;
        float rx = cx * rot_cos - cy * rot_sin + 0.5f + drift_x * 0.1f;
        float ry = cx * rot_sin + cy * rot_cos + 0.5f + drift_y * 0.1f;

        // Swirl warp
        float dist = std::sqrt(cx * cx + cy * cy);
        float swirl_angle = dist * p.swirl_dist_mul * warp_str;
        float sw_sin = std::sin(swirl_angle);
        float sw_cos = std::cos(swirl_angle);
        float wx = (rx - 0.5f) * sw_cos - (ry - 0.5f) * sw_sin + 0.5f;
        float wy = (rx - 0.5f) * sw_sin + (ry - 0.5f) * sw_cos + 0.5f;

        // Sum of sine plasma waves
        float v = 0.0f;
        v += std::sin(wx * scale_x + t);
        v += std::sin((wy * scale_y + t) * 0.7f);
        v += std::sin((wx * scale_x + wy * scale_y + t) * 0.5f);
        v += std::sin(std::sqrt(wx * wx * 100.0f + wy * wy * 100.0f) + t);
        v *= 0.25f; 

        // --- POSTERIZATION (The "Jacky" Edge Step) ---
        // Instead of smooth gradients, force 'v' into sharp bands.
        // Lower numbers = fewer, bigger, more jagged structures.
        float num_bands = 6.0f; 
        v = std::floor(v * num_bands) / num_bands;
        // ----------------------------------------------

        // Map to color (stays the same)
         r = 0.5f + 0.5f * std::cos(3.14159f * (v + p.palette_phase_r));
         g = 0.5f + 0.5f * std::cos(3.14159f * (v + p.palette_phase_g));
         b = 0.5f + 0.5f * std::cos(3.14159f * (v + p.palette_phase_b));
            } else {


            // Apply drift + rotation to coordinates
            float cx = fx - 0.5f;
            float cy = fy - 0.5f;
            float rx = cx * rot_cos - cy * rot_sin + 0.5f + drift_x * 0.1f;
            float ry = cx * rot_sin + cy * rot_cos + 0.5f + drift_y * 0.1f;

            // Swirl warp — displaces coordinates based on distance from centre
            float dist = std::sqrt(cx * cx + cy * cy);
            float swirl_angle = dist * p.swirl_dist_mul * warp_str;
            float sw_sin = std::sin(swirl_angle);
            float sw_cos = std::cos(swirl_angle);
            float wx = (rx - 0.5f) * sw_cos - (ry - 0.5f) * sw_sin + 0.5f;
            float wy = (rx - 0.5f) * sw_sin + (ry - 0.5f) * sw_cos + 0.5f;

            // sum of several sine plasma waves with animated scale
            float v = 0.0f;
            v += std::sin(wx * scale_x + t);
            v += std::sin((wy * scale_y + t) * 0.7f);
            v += std::sin((wx * scale_x + wy * scale_y + t) * 0.5f);
            v += std::sin(std::sqrt(wx * wx * 100.0f + wy * wy * 100.0f) + t);
            v *= 0.25f; // normalise to roughly [-1, 1]

            // map to colour via phase-shifted cosines (randomised palette)
             r = 0.5f + 0.5f * std::cos(3.14159f * (v + p.palette_phase_r));
             g = 0.5f + 0.5f * std::cos(3.14159f * (v + p.palette_phase_g));
             b = 0.5f + 0.5f * std::cos(3.14159f * (v + p.palette_phase_b));

            }

            // darken so ImGui windows stay readable
            r *= p.darken_r;
            g *= p.darken_g;
            b *= p.darken_b;

            Uint8 R = static_cast<Uint8>(r * 255.0f);
            Uint8 G = static_cast<Uint8>(g * 255.0f);
            Uint8 B = static_cast<Uint8>(b * 255.0f);

            // ARGB8888
            row[x] = (0xFFu << 24) | (R << 16) | (G << 8) | B;
        }
    }

    SDL_UnlockTexture(tex);
}
    
 static void update_plasma_texture(SDL_Texture* tex, int w, int h, float t,
                                  const PlasmaParams& p)
{
    void* pixels = nullptr;
    int   pitch  = 0;
    // Safety check: If lock fails, we can't draw
    if (!SDL_LockTexture(tex, nullptr, &pixels, &pitch) != 0)
        return;

    auto* dst = static_cast<Uint8*>(pixels);

    for (int y = 0; y < h; ++y) {
        Uint32* row = reinterpret_cast<Uint32*>(dst + y * pitch);
        float fy = static_cast<float>(y) / static_cast<float>(h);
        
        for (int x = 0; x < w; ++x) {
            float fx = static_cast<float>(x) / static_cast<float>(w);

    int    fps        = 60;
    int    frame_count = 0;
    std::string output_path;
};

static bool recorder_start(Recorder& rec, int w, int h, const char* path, int fps = 60) {
    if (rec.pipe) return false; // already recording
    // h264 with yuv420p requires even dimensions
    w &= ~1;
            // 1. THE JAGGED JITTER (The "Electric" serration)
            // Using very high frequency sines to "shake" the pixel lookup.
            // We use 't * 10' for that fast, electrical buzzing movement.
            float jitter = std::sin(fx * 120.0f + t * 10.0f) * 0.02f;
            float wx = fx + jitter;
            float wy = fy + jitter;

            // 2. THE RIDGE MATH
            // Instead of standard smooth sine, we use a "V" shape
            // (1.0 - abs(sin)) creates the sharp "filament" look.
            float v1 = 1.0f - std::abs(std::sin(wx * p.scale_base_x + t));
            float v2 = 1.0f - std::abs(std::sin(wy * p.scale_base_y - t));
            
            float v = (v1 + v2) * 0.5f;

            // 3. THE "DOMAIN" SHARPENING
            // We multiply v by itself to make the dark areas darker 
            // and the "lightning" lines pop.
            float electric_v = v * v * v;

            // 4. COLORING
            // Standard cosine palette using our jagged electric_v
            float r = 0.5f + 0.5f * std::cos(3.14159f * (electric_v + p.palette_phase_r));
            float g = 0.5f + 0.5f * std::cos(3.14159f * (electric_v + p.palette_phase_g));
            float b = 0.5f + 0.5f * std::cos(3.14159f * (electric_v + p.palette_phase_b));

            // 5. FINAL PIXEL ASSEMBLY
            // FORCE ALPHA TO 255 (0xFFu) to overwrite billboard shadows.
            Uint8 R = static_cast<Uint8>(r * p.darken_r * 255.0f);
            Uint8 G = static_cast<Uint8>(g * p.darken_g * 255.0f);
            Uint8 B = static_cast<Uint8>(b * p.darken_b * 255.0f);

            // Write: Alpha is ALWAYS 255.
            row[x] = (0xFFu << 24) | (R << 16) | (G << 8) | B;
        }
    }

    SDL_UnlockTexture(tex);
}
    */
    /*
static void update_plasma_texture(SDL_Texture* tex, int w, int h, float t,
                                  const PlasmaParams& p)
{
    void* pixels = nullptr;
    int   pitch  = 0;
    if (!SDL_LockTexture(tex, nullptr, &pixels, &pitch) != 0)
        return;

    auto* dst = static_cast<Uint8*>(pixels);

    // Hardcode some defaults if params are zero to ensure we see something
    float sX = (p.scale_base_x == 0.0f) ? 10.0f : p.scale_base_x;
    float sY = (p.scale_base_y == 0.0f) ? 10.0f : p.scale_base_y;

    for (int y = 0; y < h; ++y) {
        // Calculate row pointer
        Uint32* row = reinterpret_cast<Uint32*>(dst + y * pitch);
        float fy = static_cast<float>(y) / static_cast<float>(h);
        
        for (int x = 0; x < w; ++x) {
            float fx = static_cast<float>(x) / static_cast<float>(w);

            // 1. JITTERED COORDINATES
            // The "Lightning" look comes from high-frequency offsets
            float jitter = std::sin(fx * 40.0f + fy * 40.0f + t * 3.0f) * 0.01f;
            float wx = fx + jitter;
            float wy = fy + jitter;

            // 2. THE "JACKY" STRUCTURES (Ridge Plasma)
            // Use abs() to create sharp peaks, then combine them
            float v = 0.0f;
            v += 1.0f - std::abs(std::sin(wx * sX + t));
            v += 1.0f - std::abs(std::sin(wy * sY - t));
            v += 1.0f - std::abs(std::sin((wx + wy) * (sX * 0.5f) + t * 0.5f));
            v /= 3.0f; // Range is now 0.0 to 1.0

            // 3. ENHANCE EDGES (The threshold)
            // This makes the "lightning" domains. 
            // If v is high, it stays bright. If low, it goes black.
            v = (v > 0.5f) ? (v - 0.5f) * 2.0f : 0.0f;

            // 4. COLORING
            // Create a sharp electric blue/purple palette
            float r_f = 0.5f + 0.5f * std::cos(3.14f * (v + p.palette_phase_r));
            float g_f = 0.5f + 0.5f * std::cos(3.14f * (v + p.palette_phase_g));
            float b_f = 0.5f + 0.5f * std::cos(3.14f * (v + p.palette_phase_b));

            // Convert to 0-255 and APPLY v to Alpha or Brightness
            // We multiply by 255 and p.darken to respect your settings
            Uint8 R = static_cast<Uint8>(r_f * v * p.darken_r * 255.0f);
            Uint8 G = static_cast<Uint8>(g_f * v * p.darken_g * 255.0f);
            Uint8 B = static_cast<Uint8>(b_f * v * p.darken_b * 255.0f);

            // 5. OUTPUT (Force Alpha to 255 to prevent transparency issues)
            row[x] = (0xFFu << 24) | (R << 16) | (G << 8) | B;
        }
    }

    SDL_UnlockTexture(tex);
}
*/

static void update_plasma_texture2(SDL_Texture* tex, int w, int h, float t,
                                  const PlasmaParams& p)
{
    void* pixels = nullptr;
    int   pitch  = 0;
    if (!SDL_LockTexture(tex, nullptr, &pixels, &pitch) != 0)
        return;

    auto* dst = static_cast<Uint8*>(pixels);

    // Drive the "electrical interference" speeds
    float drift_x  = p.drift_amp * std::sin(t * p.drift_speed_x);
    float drift_y  = p.drift_amp * std::cos(t * p.drift_speed_y);
    float scale_x  = p.scale_base_x + p.scale_mod_amp * std::sin(t * p.scale_mod_speed_x);
    float scale_y  = p.scale_base_y + p.scale_mod_amp * std::cos(t * p.scale_mod_speed_y);
    
    // Rotational components
    float rot_sin  = std::sin(t * p.rot_speed);
    float rot_cos  = std::cos(t * p.rot_speed);

    for (int y = 0; y < h; ++y) {
        auto* row = reinterpret_cast<Uint32*>(dst + y * pitch);
        float fy = static_cast<float>(y) / static_cast<float>(h);
        
        for (int x = 0; x < w; ++x) {
            float fx = static_cast<float>(x) / static_cast<float>(w);

            // 1. COORDINATE WARPING (The "Curvy/Lightning" Path)
            // We distort the lookup space to make the lines "wiggle"
            float cx = fx - 0.5f;
            float cy = fy - 0.5f;
            
            // Initial Rotation + Drift
            float rx = cx * rot_cos - cy * rot_sin + 0.5f + drift_x;
            float ry = cx * rot_sin + cy * rot_cos + 0.5f + drift_y;

            // Secondary high-frequency distortion (The "Jagged" edge)
            // Using nested sines to create a pseudo-fractal warp
            float warp_noise = std::sin(rx * 15.0f + t) * std::cos(ry * 15.0f - t);
            float wx = rx + warp_noise * 0.05f;
            float wy = ry + warp_noise * 0.05f;

            // 2. GENERATE JAGGED RIDGES
            // Instead of: v = sin(x)
            // We use: v = 1.0 - abs(sin(x)) to get sharp "peaks"
            float v = 0.0f;
            
            // Layer 1: Horizontal arcs
            v += 1.0f - std::abs(std::sin(wx * scale_x + t));
            
            // Layer 2: Vertical arcs (offset frequency)
            v += 1.0f - std::abs(std::sin(wy * scale_y * 1.2f - t * 0.8f));
            
            // Layer 3: Diagonal interference
            v += 1.0f - std::abs(std::sin((wx + wy) * scale_x * 0.5f + t));

            // Layer 4: Circular "Burst"
            float dist = std::sqrt((wx-0.5f)*(wx-0.5f) + (wy-0.5f)*(wy-0.5f));
            v += 1.0f - std::abs(std::sin(dist * 20.0f - t * 2.0f));

            v /= 4.0f; // Average the layers

            // 3. SHARPEN & CONSTRICT
            // Raising to a power makes the "lightning" thinner and more intense
            v = std::pow(v, 3.5f); 

            // 4. COLOR MAPPING
            // Use the calculated 'v' to drive the phase-shifted palette
            float r = 0.5f + 0.5f * std::cos(3.14159f * (v + p.palette_phase_r));
            float g = 0.5f + 0.5f * std::cos(3.14159f * (v + p.palette_phase_g));
            float b = 0.5f + 0.5f * std::cos(3.14159f * (v + p.palette_phase_b));

            // Apply darkening and brightness boost to the arcs
            float brightness_boost = 1.8f; //p.darken_b *
            Uint8 R = static_cast<Uint8>(std::clamp(r * v * brightness_boost * 255.0f, 0.0f, 255.0f));
            Uint8 G = static_cast<Uint8>(std::clamp(g * v * brightness_boost * 255.0f, 0.0f, 255.0f));
            Uint8 B = static_cast<Uint8>(std::clamp(b * v * brightness_boost *  255.0f, 0.0f, 255.0f));

            // ARGB8888 Write
            row[x] = (0xFFu << 24) | (R << 16) | (G << 8) | B;
        }
    }

    SDL_UnlockTexture(tex);
}

static void update_plasma_texture3(SDL_Texture* tex, int w, int h, float t,
                                  const PlasmaParams& p)
{
    void* pixels = nullptr;
    int   pitch  = 0;
    
    // The "!" is back! 
    if (!SDL_LockTexture(tex, nullptr, &pixels, &pitch) != 0)
        return;

    auto* dst = static_cast<Uint8*>(pixels);

    for (int y = 0; y < h; ++y) {
        Uint32* row = reinterpret_cast<Uint32*>(dst + y * pitch);
        float fy = static_cast<float>(y) / static_cast<float>(h);
        
        for (int x = 0; x < w; ++x) {
            float fx = static_cast<float>(x) / static_cast<float>(w);

            // 1. JAGGED JITTER (The "Electric" serration)
            // A very fast, high-frequency "buzz" to the coordinates.
            float jitter = std::sin(fx * 150.0f + t * 20.0f) * 0.012f;
            float wx = fx + jitter;
            float wy = fy + jitter;

            // 2. RIDGE MATH (Sharp Lightning Arcs)
            // 1.0 - abs(sin) creates a sharp "peak" instead of a smooth wave.
            float v1 = 1.0f - std::abs(std::sin(wx * p.scale_base_x + t));
            float v2 = 1.0f - std::abs(std::sin(wy * p.scale_base_y - t * 1.3f));
            float v3 = 1.0f - std::abs(std::sin((wx + wy) * (p.scale_base_x * 0.4f) + t));
            
            float v = (v1 + v2 + v3) / 3.0f;

            // 3. ELECTRIC CONTRAST
            // Pushing the value into "domains" so you get clear paths.
            // Adjust the 0.4f to control the thickness of the "bolts".
            float electric_v = std::max(0.0f, v - 0.4f) * 1.8f;

            // 4. COLORING
            float r_val = 0.5f + 0.5f * std::cos(3.14159f * (electric_v + p.palette_phase_r));
            float g_val = 0.5f + 0.5f * std::cos(3.14159f * (electric_v + p.palette_phase_g));
            float b_val = 0.5f + 0.5f * std::cos(3.14159f * (electric_v + p.palette_phase_b));

            // 5. FINAL PIXEL (ARGB8888)
            // Multiplying by electric_v again keeps the "background" dark.
            Uint8 R = static_cast<Uint8>(r_val * electric_v  * 255.0f);
            Uint8 G = static_cast<Uint8>(g_val * electric_v  * 255.0f);
            Uint8 B = static_cast<Uint8>(b_val * electric_v  * 255.0f);

            row[x] = (0xFFu << 24) | (R << 16) | (G << 8) | B;
        }
    }

    SDL_UnlockTexture(tex);
}

static void update_plasma_texture4(SDL_Texture* tex, int w, int h, float t,
                                  const PlasmaParams& p)
{
    void* pixels = nullptr;
    int   pitch  = 0;
    if (!SDL_LockTexture(tex, nullptr, &pixels, &pitch) != 0)
        return;

    auto* dst = static_cast<Uint8*>(pixels);

    // Speed constants for the "electric" jitter
    float drift_x  = p.drift_amp * std::sin(t * p.drift_speed_x);
    float drift_y  = p.drift_amp * std::cos(t * p.drift_speed_y);
    float rot_sin  = std::sin(t * p.rot_speed);
    float rot_cos  = std::cos(t * p.rot_speed);

    for (int y = 0; y < h; ++y) {
        auto* row = reinterpret_cast<Uint32*>(dst + y * pitch);
        float fy = static_cast<float>(y) / static_cast<float>(h);
        
        for (int x = 0; x < w; ++x) {
            float fx = static_cast<float>(x) / static_cast<float>(w);

            // 1. COORDINATE WARP (The "Lightning" Path)
            float cx = fx - 0.5f;
            float cy = fy - 0.5f;
            
            // Rotate and Drift
            float rx = cx * rot_cos - cy * rot_sin + 0.5f + drift_x;
            float ry = cx * rot_sin + cy * rot_cos + 0.5f + drift_y;

            // JAGGED DISTORTION: We use a high-freq sine to "jitter" the lookups
            // This creates the broken, electric edges.
            float jitter = std::sin(rx * 50.0f + t * 2.0f) * 0.02f;
            float wx = rx + jitter;
            float wy = ry + jitter;

            // 2. RIDGE CALCULATION (The "Electric" Arcs)
            // We use the absolute value of sines to create sharp peaks
            float v1 = 1.0f - std::abs(std::sin(wx * p.scale_base_x + t));
            float v2 = 1.0f - std::abs(std::sin(wy * p.scale_base_y - t * 0.5f));
            float v3 = 1.0f - std::abs(std::sin((wx + wy) * (p.scale_base_x * 0.5f) + t));
            
            // Combine them
            float v = (v1 + v2 + v3) / 3.0f;

            // 3. SHARPEN THE EDGES
            // Instead of pow (which is risky), we use a sharp contrast curve.
            // This keeps values between 0 and 1 but makes them "punchy".
            v = std::max(0.0f, v - 0.4f) * 1.6f; 

            // 4. COLOR MAPPING
            // We drive the cosine palette using our jagged value 'v'
            float r_val = 0.5f + 0.5f * std::cos(3.14159f * (v + p.palette_phase_r));
            float g_val = 0.5f + 0.5f * std::cos(3.14159f * (v + p.palette_phase_g));
            float b_val = 0.5f + 0.5f * std::cos(3.14159f * (v + p.palette_phase_b));

            // Apply darkening and scale back to 255
            // Multiply by 'v' again to make the "background" between lightning dark
            Uint8 R = static_cast<Uint8>(std::min(255.0f, r_val * v  * 255.0f));
            Uint8 G = static_cast<Uint8>(std::min(255.0f, g_val * v  * 255.0f));
            Uint8 B = static_cast<Uint8>(std::min(255.0f, b_val * v  * 255.0f));

            // ARGB8888
            row[x] = (0xFFu << 24) | (R << 16) | (G << 8) | B;
        }
    }

    SDL_UnlockTexture(tex);
}

static void update_plasma_texture5(SDL_Texture* tex, int w, int h, float t,
                                  const PlasmaParams& p)
{
    void* pixels = nullptr;
    int pitch = 0;
    // Back to the working logic!
    if (!SDL_LockTexture(tex, nullptr, &pixels, &pitch) != 0)
        return;

    auto* dst = static_cast<Uint8*>(pixels);

    float drift_x = p.drift_amp * std::sin(t * p.drift_speed_x);
    float drift_y = p.drift_amp * std::cos(t * p.drift_speed_y);
    float rot_sin = std::sin(t * p.rot_speed);
    float rot_cos = std::cos(t * p.rot_speed);

    for (int y = 0; y < h; ++y) {
        auto* row = reinterpret_cast<Uint32*>(dst + y * pitch);
        float fy = static_cast<float>(y) / static_cast<float>(h);
        
        for (int x = 0; x < w; ++x) {
            float fx = static_cast<float>(x) / static_cast<float>(w);

            float cx = fx - 0.5f;
            float cy = fy - 0.5f;
            
            float rx = cx * rot_cos - cy * rot_sin + 0.5f + drift_x;
            float ry = cx * rot_sin + cy * rot_cos + 0.5f + drift_y;

            // 1. HIGH-VOLTAGE JITTER
            // Mixing two high frequencies (130 and 210) creates that non-repeating "crackling" edge.
            float buzz = std::sin(rx * 130.0f + t * 15.0f) * std::cos(ry * 210.0f - t * 10.0f);
            float wx = rx + buzz * 0.015f;
            float wy = ry + buzz * 0.015f;

            // 2. RIDGE CALCULATION
            // We use standard plasma math but with the 1.0 - abs() "ridge" trick.
            float v1 = 1.0f - std::abs(std::sin(wx * p.scale_base_x + t));
            float v2 = 1.0f - std::abs(std::sin(wy * p.scale_base_y - t * 0.5f));
            float v3 = 1.0f - std::abs(std::sin((wx + wy) * (p.scale_base_x * 0.5f) + t));
            
            float v = (v1 + v2 + v3) / 3.0f;

            // 3. BOLT SHARPENING
            // Pushing the threshold higher (0.5 instead of 0.4) makes the "bolts" thinner.
            // Using a square (v*v) helps emphasize the bright centers of the arcs.
            float electric_v = std::max(0.0f, v - 0.5f) * 2.0f;
            electric_v *= electric_v; 

            // 4. COLOR MAPPING
            float r_val = 0.5f + 0.5f * std::cos(3.14159f * (electric_v + p.palette_phase_r));
            float g_val = 0.5f + 0.5f * std::cos(3.14159f * (electric_v + p.palette_phase_g));
            float b_val = 0.5f + 0.5f * std::cos(3.14159f * (electric_v + p.palette_phase_b));

            // Final intensities
            Uint8 R = static_cast<Uint8>(std::min(255.0f, r_val * electric_v  * 255.0f));
            Uint8 G = static_cast<Uint8>(std::min(255.0f, g_val * electric_v  * 255.0f));
            Uint8 B = static_cast<Uint8>(std::min(255.0f, b_val * electric_v  * 255.0f));

            row[x] = (0xFFu << 24) | (R << 16) | (G << 8) | B;
        }
    }

    SDL_UnlockTexture(tex);
}

static void update_plasma_texture(SDL_Texture* tex, int w, int h, float t,
                                  const PlasmaParams& p)
{
    void* pixels = nullptr;
    int pitch = 0;
    if (!SDL_LockTexture(tex, nullptr, &pixels, &pitch) != 0)
        return;

    auto* dst = static_cast<Uint8*>(pixels);

    float drift_x = p.drift_amp * std::sin(t * p.drift_speed_x);
    float drift_y = p.drift_amp * std::cos(t * p.drift_speed_y);
    float rot_sin = std::sin(t * p.rot_speed);
    float rot_cos = std::cos(t * p.rot_speed);

    for (int y = 0; y < h; ++y) {
        auto* row = reinterpret_cast<Uint32*>(dst + y * pitch);
        float fy = static_cast<float>(y) / static_cast<float>(h);
        
        for (int x = 0; x < w; ++x) {
            float fx = static_cast<float>(x) / static_cast<float>(w);

            // 1. HIGH-FREQ JITTER (The "Electric" serration)
            // Increased frequency for a sharper, more jagged "buzz"
            float jitter = std::sin(fx * 140.0f + t * 15.0f) * std::cos(fy * 240.0f - t * 10.0f) * 0.02f;
            
            float cx = (fx + jitter) - 0.5f;
            float cy = (fy + jitter) - 0.5f;

            // 2. COORDINATE TRANSFORM
            float rx = cx * rot_cos - cy * rot_sin + 0.5f + drift_x;
            float ry = cx * rot_sin + cy * rot_cos + 0.5f + drift_y;

            // 3. FULL-COLOR PLASMA MATH
            // We use standard sines here (not abs) to keep the full color range
            float v = 0.0f;
            v += std::sin(rx * p.scale_base_x + t);
            v += std::sin(ry * p.scale_base_y - t * 0.5f);
            v += std::sin((rx + ry) * (p.scale_base_x * 0.5f) + t);
            v += std::sin(std::sqrt(rx * rx + ry * ry) * 10.0f + t);
            v *= 0.25f; // Normalise to roughly [-1, 1]

            // 4. VIBRANT COLOR MAPPING
            // Use a higher multiplier (6.28 instead of 3.14) if you want the 
            // colors to cycle even faster/more intensely.
            float r_f = 0.5f + 0.5f * std::cos(6.28318f * (v + p.palette_phase_r));
            float g_f = 0.5f + 0.5f * std::cos(6.28318f * (v + p.palette_phase_g));
            float b_f = 0.5f + 0.5f * std::cos(6.28318f * (v + p.palette_phase_b));

            // 5. FINAL PIXEL (No Darkening, Full Opaque)
            Uint8 R = static_cast<Uint8>(r_f * 255.0f);
            Uint8 G = static_cast<Uint8>(g_f * 255.0f);
            Uint8 B = static_cast<Uint8>(b_f * 255.0f);

            row[x] = (0xFFu << 24) | (R << 16) | (G << 8) | B;
        }
    }

    SDL_UnlockTexture(tex);
}


//------------

ContentParser mParser;

// ---------------------------------------------------------------------------
int main(int argc, char** argv)
{
    // --- Parse CLI arguments ---
    // Usage: ./imtest [--record output.mp4] [text...]
    std::vector<std::string> cli_texts;
    std::string cli_record_path;
    int cli_record_max = -1;  // -1 = not specified on CLI
    bool  cli_no_nerds = false;
    bool cli_no_maximize = false;
    bool cli_plasma_tile = false;
    std::vector<std::unique_ptr<BDdisplay>>  mBdisplay;
    
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--record") == 0 && i + 1 < argc) {
            cli_record_path = argv[++i];
        } else if (std::strcmp(argv[i], "--record-max") == 0 && i + 1 < argc) {
            cli_record_max = std::atoi(argv[++i]);
            if (cli_record_max < 1) cli_record_max = 1;
        } else if (std::strcmp(argv[i], "--no-maximize") == 0) {
            cli_no_maximize = true;
        } else if (std::strcmp(argv[i], "--no-nerds") == 0) {
            cli_no_nerds = true;
        } else if (std::strcmp(argv[i], "--plasma-tiles") == 0) {
            cli_plasma_tile = true;       
        } else {
            cli_texts.push_back(argv[i]);
        }
    }
    
    for (const auto& t : cli_texts)
        std::printf("Overlay text: \"%s\"\n", t.c_str());
    if (!cli_record_path.empty())
        std::printf("Will record to: %s\n", cli_record_path.c_str());

    // --- SDL init ---
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::printf("SDL_Init error: %s\n", SDL_GetError());
        return 1;
    }

    float scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());

    int win_w = static_cast<int>(1024 * scale);
    int win_h = static_cast<int>(768 * scale);

    cur_rel = (float)win_w / (float)win_h;

    SDL_WindowFlags win_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (!cli_no_maximize)
        win_flags |= SDL_WINDOW_MAXIMIZED;

    window = SDL_CreateWindow(
        "SDL/ImGui Greeting Card",
        win_w, win_h,
        win_flags
    );
    if (!window) {
        std::printf("SDL_CreateWindow error: %s\n", SDL_GetError());
        return 1;
    }

    renderer = SDL_CreateRenderer(window, nullptr);
    SDL_SetRenderVSync(renderer, 1);
    if (!renderer) {
        std::printf("SDL_CreateRenderer error: %s\n", SDL_GetError());
        return 1;
    }

    // --- Plasma texture (streaming, updated every frame) ---
    // Use a reduced resolution for performance — will stretch to fill window
    int plasma_w = win_w / 4;
    int plasma_h = win_h / 4;
    SDL_Texture* plasma_tex = SDL_CreateTexture(
        renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
        plasma_w, plasma_h
    );

    // --- Pre-render a texture for each CLI text argument ---
    std::vector<TextEntry> cli_entries;
    for (const auto& t : cli_texts) {

        mParser.processAndPrint(t);
        auto pcsout = mParser.parse(t);

        auto newBD = std::make_unique<BDdisplay>();
        
        for(auto& pd : pcsout){
            newBD->add(pd);
        }
        mBdisplay.push_back(std::move(newBD));
        

        /*
        std::string timgstr = t;
        std::string imgfile;

        if(std::string::npos != timgstr.find("[image:")){ 

        auto spos = timgstr.find(timgstr.begin(),timgstr.end(), "[image:"); 
        
            


            std::size_t posb = timgstr.find(":"); 
            std::size_t pose = timgstr.find("]"); 

            imgfile = timgstr.substr(posb+1,(pose-posb)-1);
            std::cout << "Image:" << imgfile << std::endl;

            TextEntry e;
            e.label = t;
            e.tex = create_png_texture(renderer, imgfile.c_str(), &e.w, &e.h);
            if (e.tex)
                e.bNoColor = true;
                 cli_entries.push_back(std::move(e));
            
        } else {

            TextEntry e;
            e.label = t;
            e.tex = create_text_texture(renderer, t.c_str(), &e.w, &e.h);
            if (e.tex)
                 cli_entries.push_back(std::move(e));
    }*/
    }

    // Seed RNG and create one bouncer per CLI text
    std::srand(static_cast<unsigned>(SDL_GetPerformanceCounter()));
    std::vector<Bouncer> bouncers;
    for (const auto& e : cli_entries)
        bouncers.push_back(make_bouncer(win_w, win_h, e.tex, e.w, e.h, e.bNoColor));

    // Custom bouncer text — checkbox + input field state
    bool  use_custom_text = false;
    char  custom_text_buf[256] = "";

    // Keep track of all created textures so we can clean them up
    std::vector<SDL_Texture*> extra_textures;

    // Randomise plasma palette & X/Y properties for this run
    plasma_params = randomise_plasma();
    
    int prev_win_h = win_h;
    int prev_win_w = win_w;

    // --- ImGui init ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(scale);
    style.FontScaleDpi = scale;

    // Make ImGui windows slightly transparent so background shows through
    style.Colors[ImGuiCol_WindowBg].w = 0.80f;

    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    // --- State ---
    bool  running = true;
    
    float time_acc = 0.0f;
    bool  roll_palette = false;
    float roll_palette_speed = 0.5f;  // how fast the palette phases rotate

    // Recording state
    Recorder recorder;
    char record_path_buf[256] = "output.mp4";
    float record_time = 0.0f;  // elapsed recording time in seconds
    float record_frame_accum = 0.0f;  // accumulator for fixed-rate frame capture
    bool  record_max_enabled = true;   // whether max-length auto-stop is active
    int   record_max_seconds = 59;     // max recording length in seconds
    bool  record_gui = true;
    
    // Apply CLI overrides for recording max
    if (cli_plasma_tile == true) {
        plasma_render_tiles = true;
    }


    // Apply CLI overrides for recording max
    if (cli_no_nerds == true) {
        record_gui = false;
    }

    // Apply CLI overrides for recording max
    if (cli_record_max > 0) {
        record_max_seconds = cli_record_max;
        record_max_enabled = true;
    }

    // Start recording immediately if --record was passed
    if (!cli_record_path.empty()) {
        std::snprintf(record_path_buf, sizeof(record_path_buf), "%s", cli_record_path.c_str());
        int out_w = 0, out_h = 0;
        SDL_GetRenderOutputSize(renderer, &out_w, &out_h);
        recorder_start(recorder, out_w, out_h, cli_record_path.c_str());
    }

    Uint64 last_ticks = SDL_GetPerformanceCounter();
    Uint64 freq       = SDL_GetPerformanceFrequency();

    // --- Main loop ---
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            ImGui_ImplSDL3_ProcessEvent(&ev);
            if (ev.type == SDL_EVENT_QUIT)
                running = false;
            if (ev.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                ev.window.windowID == SDL_GetWindowID(window))
                running = false;
        }

        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
            SDL_Delay(10);
            continue;
        }

        // Delta time
        Uint64 now = SDL_GetPerformanceCounter();
        float dt = static_cast<float>(now - last_ticks) / static_cast<float>(freq);
        last_ticks = now;
        time_acc += dt * 1.5f;  // speed multiplier for the plasma

        // Handle resize — recreate plasma texture when window size changes
        
        bool bWinChange = false;

        SDL_GetWindowSize(window, &cur_w, &cur_h);
        if(cur_w % 16){
            cur_w = (((int)cur_w)/16)*16;
            bWinChange = true;
        }
        if(cur_h % 16){
            cur_h = (((int)cur_h)/16)*16;
            bWinChange = true;
        }

        if(bWinChange){
            SDL_SetWindowSize(window, cur_w, cur_h);
        }
        
        cur_rel = (float)cur_w / (float)cur_h;
        if (cur_w != prev_win_w || cur_h != prev_win_h) {
            // Recreate plasma at new reduced size
            if (plasma_tex) SDL_DestroyTexture(plasma_tex);
            plasma_w = cur_w / 4;
            plasma_h = cur_h / 4;
            if (plasma_w < 1) plasma_w = 1;
            if (plasma_h < 1) plasma_h = 1;
            plasma_tex = SDL_CreateTexture(
                renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                plasma_w, plasma_h
            );

            // Clamp all bouncers so they stay inside the new window
       /*
            for (auto& b : bouncers) {
                float max_x = static_cast<float>(cur_w - b.tw);
                float max_y = static_cast<float>(cur_h - b.th);
                if (max_x < 0) max_x = 0;
                if (max_y < 0) max_y = 0;
                if (b.x > max_x) b.x = max_x;
                if (b.y > max_y) b.y = max_y;
            }
    */
            prev_win_w = cur_w;
            prev_win_h = cur_h;
        }

        // --- Bounce all text instances (DVD screensaver style) ---
        {/*
            for (auto& b : bouncers) {
                b.x += b.vx * dt;
                b.y += b.vy * dt;

                float right_edge  = static_cast<float>(cur_w - b.tw);
                float bottom_edge = static_cast<float>(cur_h - b.th);
                if (right_edge  < 0) right_edge  = 0;
                if (bottom_edge < 0) bottom_edge = 0;

                if (b.x <= 0.0f)         { b.x = 0.0f;         b.vx = -b.vx; }
                if (b.x >= right_edge)    { b.x = right_edge;   b.vx = -b.vx; }
                if (b.y <= 0.0f)          { b.y = 0.0f;         b.vy = -b.vy; }
                if (b.y >= bottom_edge)   { b.y = bottom_edge;  b.vy = -b.vy; }
            } */
        }

        // Roll palette: smoothly rotate the colour phase offsets each frame
        if (roll_palette) {
            float step = roll_palette_speed * dt;
            plasma_params.palette_phase_r = std::fmod(plasma_params.palette_phase_r + step,        2.0f);
            plasma_params.palette_phase_g = std::fmod(plasma_params.palette_phase_g + step * 0.7f, 2.0f);
            plasma_params.palette_phase_b = std::fmod(plasma_params.palette_phase_b + step * 1.3f, 2.0f);
        }

        // Update plasma pixels
        if (plasma_tex)
            update_plasma_texture(plasma_tex, plasma_w, plasma_h, time_acc, plasma_params);

        // New frame
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // --- Main Menu Bar ---
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Bouncers")) {
                ImGui::Text("Bouncer Texts (%d):", static_cast<int>(mBdisplay.size()));
                {
                    int del_text_idx = -1;
                    for (int ti = 0; ti < static_cast<int>(mBdisplay.size()); ++ti) {
                        ImGui::PushID(ti);
                        if (ImGui::SmallButton("X")) del_text_idx = ti;
                        ImGui::SameLine();
                        ImGui::BulletText("\"%s\"", mBdisplay[ti]->getInput().c_str());
                        ImGui::PopID();
                    }
                    if (del_text_idx >= 0) {
                        // Remove all bouncers that use this texture
                        /*
                        SDL_Texture* dead_tex = cli_entries[static_cast<size_t>(del_text_idx)].tex;
                        bouncers.erase(
                            std::remove_if(bouncers.begin(), bouncers.end(),
                                [dead_tex](const Bouncer& b) { return b.tex == dead_tex; }),
                            bouncers.end());
                        // Destroy the texture and remove the entry
                        if (dead_tex) SDL_DestroyTexture(dead_tex);
                        cli_entries.erase(cli_entries.begin() + del_text_idx);
                    */
                        mBdisplay.erase(mBdisplay.begin() + del_text_idx);
                    }
                }
                ImGui::Separator();
                ImGui::Checkbox("Custom Text", &use_custom_text);
                if (use_custom_text) {
                    ImGui::SetNextItemWidth(200.0f);
                    ImGui::InputText("##custom", custom_text_buf, sizeof(custom_text_buf));
                }
                if (ImGui::MenuItem("Add Bouncer")) {
                    SDL_Texture* spawn_tex = nullptr;
                    int spawn_w = 0, spawn_h = 0;
                    bool cNoColor = false;
                    if (use_custom_text && custom_text_buf[0] != '\0') {

                        auto pcsout = mParser.parse(custom_text_buf);

                        auto newBD = std::make_unique<BDdisplay>();
        
                        for(auto& pd : pcsout){
                            newBD->add(pd);
                        }
                        mBdisplay.push_back(std::move(newBD));

                        /* int cw = 0, ch = 0;

                        SDL_Texture* ct = create_text_texture(renderer, custom_text_buf, &cw, &ch);
                        std::string timgstr = custom_text_buf;
                        std::string imgfile;

                        if(std::string::npos != timgstr.find("[image:")){ 

                            std::size_t posb = timgstr.find(":"); 
                            std::size_t pose = timgstr.find("]"); 

                            imgfile = timgstr.substr(posb+1,(pose-posb)-1);
                            std::cout << "Image:" << imgfile << std::endl;

                            TextEntry ne;
                            ne.label = imgfile;
                            ne.tex = create_png_texture(renderer, imgfile.c_str(), &ne.w, &ne.h);
                            if (ne.tex)
                            ne.label = custom_text_buf;
                            cNoColor = true;

                            cli_entries.push_back(std::move(ne));
                            spawn_tex = ne.tex;
                            spawn_w = ne.w;
                            spawn_h = ne.h;
                        }   else if (ct) {
                                TextEntry ne;
                                ne.label = custom_text_buf;
                                ne.tex = ct;
                                ne.w = cw;
                                ne.h = ch;
                                cli_entries.push_back(std::move(ne));
                                spawn_tex = ct;
                                spawn_w = cw;
                                spawn_h = ch;
                        
                        } else if (!cli_entries.empty()) {
                            const auto& e = cli_entries[static_cast<size_t>(std::rand()) % cli_entries.size()];
                            spawn_tex = e.tex;
                            spawn_w = e.w;
                            spawn_h = e.h;
                        }
                        if (spawn_tex)
                            bouncers.push_back(make_bouncer(cur_w, cur_h, spawn_tex, spawn_w, spawn_h, cNoColor));
                            */
                    }
                }
                //ImGui::Text("Count: %d", static_cast<int>(bouncers.size()));
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Plasma")) {
                if (ImGui::MenuItem("Randomise Palette"))
                    randomise_plasma_palette(plasma_params);
                if (ImGui::MenuItem("Randomise X/Y"))
                    randomise_plasma_xy(plasma_params);
                ImGui::Separator();
                ImGui::Checkbox("Tile Effect", &plasma_render_tiles);
                if(plasma_render_tiles){
                    ImGui::SliderFloat("Tile Size", &plasma_params.tile_count, 10, 100);
                }
                ImGui::Separator();
                ImGui::Checkbox("Roll Palette", &roll_palette);
                if (roll_palette)
                    ImGui::SliderFloat("Roll Speed", &roll_palette_speed, 0.05f, 3.0f);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Record")) {
                bool is_recording = (myNvec != NULL);
                if (!is_recording) {
                    ImGui::SetNextItemWidth(200.0f);
                    ImGui::InputText("File", record_path_buf, sizeof(record_path_buf));
                    if (ImGui::MenuItem("Start Recording")) {
                        int out_w = 0, out_h = 0;
                        SDL_GetRenderOutputSize(renderer, &out_w, &out_h);
                        recorder_start(recorder, out_w, out_h, record_path_buf);
                        record_time = 0.0f;
                        record_frame_accum = 0.0f;
                    }
                } else {
                    int mins = static_cast<int>(record_time) / 60;
                    int secs = static_cast<int>(record_time) % 60;
                    ImGui::Text("REC  %02d:%02d  (%d frames)", mins, secs, recorder.frame_count);
                    ImGui::Text("File: %s", recorder.output_path.c_str());
                    ImGui::Text("Size: %dx%d @ %d fps", recorder.width, recorder.height, recorder.fps);
                    if (record_max_enabled) {
                        int remaining = record_max_seconds - static_cast<int>(record_time);
                        if (remaining < 0) remaining = 0;
                        ImGui::Text("Auto-stop in: %ds", remaining);
                    }
                    if (ImGui::MenuItem("Stop Recording"))
                        recorder_stop(recorder);
                }
                ImGui::Checkbox("Record GUI", &record_gui);

                ImGui::Separator();
                ImGui::Checkbox("Max Length", &record_max_enabled);
                if (record_max_enabled) {
                    ImGui::SetNextItemWidth(200.0f);
                    ImGui::SliderInt("Seconds", &record_max_seconds, 1, 300);
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (myNvec != NULL) {
                record_time += dt;
                // Auto-stop recording when max length reached
                if (record_max_enabled && record_time >= static_cast<float>(record_max_seconds))
                    recorder_stop(recorder);
            }
            if (myNvec != NULL) {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "REC");
                ImGui::SameLine();
            }
            ImGui::Text("%.1f FPS", io.Framerate);
            ImGui::EndMainMenuBar();
        }

        // Render
        ImGui::Render();
        SDL_SetRenderScale(renderer, io.DisplayFramebufferScale.x,
                                     io.DisplayFramebufferScale.y);

        // 1) Draw animated plasma background (stretches from reduced res)
        if (plasma_tex) {
            SDL_RenderTexture(renderer, plasma_tex, nullptr, nullptr);
        } else {
            SDL_SetRenderDrawColorFloat(renderer, 0.10f, 0.08f, 0.15f, 1.0f);
            SDL_RenderClear(renderer);
        }

        // 2) Draw all bouncing text instances (each with its own texture & colour)
        
        for(auto& mbd : mBdisplay){
            mbd->update(dt,cur_w, cur_h);
            mbd->draw(renderer);
        }
        

        if (!record_gui){
            recorder_feed_frame(recorder, renderer);
        
            ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);


        } else {


            // 3) ImGui on top of everything
            ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);

            // 4) Capture frame for recording (must happen before Present)
            recorder_feed_frame(recorder, renderer);

       }

        SDL_RenderPresent(renderer);
    }

    // --- Cleanup ---
    recorder_stop(recorder);
    for (auto* et : extra_textures) SDL_DestroyTexture(et);
    for (auto& e : cli_entries) { if (e.tex) SDL_DestroyTexture(e.tex); }
    if (plasma_tex) SDL_DestroyTexture(plasma_tex);
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    return 0;
}
