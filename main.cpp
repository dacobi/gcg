// SDL3 + Dear ImGui with animated plasma background and transparent text overlay
// Usage: ./gcg [--record output.mp4] [--lua script.lua] [--audio music.mp3] [--bg FILE|"[plasma:#]"|"[fractal:#]"] [--record-max N] [--maximize] [text...]
//   --record FILE     start recording frames to FILE on launch
//   --lua FILE        run Lua script on launch
//   --audio FILE      play audio file on loop
//   --bg FILE         use image or video as background
//   --bg "[plasma:#]" use specific plasma index (#) as background
//   --bg "[fractal:#]" use specific fractal index (#) as background
//   --record-max N    max recording length in seconds (default 59)
//   --maximize        start the window maximized
//   --geekd           show tech info / status line and record GUI
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3_image/SDL_image.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <algorithm>
#include <vector>
#include <deque>
#include <map>
#include <iostream>
#include <tuple>
#include <regex>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <chrono>
#include "clplasma.h"
#include "clmandelbrot.h"
#include "luascripting.h"
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlgpu3.h"
#include "renderer.h"
#include "input_manager.h"

#include "shplasma.h"

#include "godot_manager.h"
#include "godot_renderer.h"

#include "usd_manager.h"
#include "usd_hydra_renderer.h"
#include "object3d.h"
#include "core/variant/variant.h"
#include <pxr/base/gf/rotation.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdGeom/camera.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/time.h> // Location of av_usleep
#include <libavutil/imgutils.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <alsa/asoundlib.h>
}

#include "randhelp.h"

const int MIXER_SAMPLE_RATE = 48000;

// --- 1. Audio Mixer Class ---

static void renderUSDTree(UsdPrim prim, USDHydraRenderer* renderer) {
    if (!prim) return;

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    if (prim.GetChildren().empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    char label[256];
    std::snprintf(label, sizeof(label), "%s [%s]", prim.GetName().GetText(), prim.GetTypeName().GetText());

    if (renderer->getActiveCamera() == prim.GetPath()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
    }

    bool open = ImGui::TreeNodeEx(label, flags);
    
    if (renderer->getActiveCamera() == prim.GetPath()) {
        ImGui::PopStyleColor();
    }
    
    if (ImGui::IsItemClicked() && prim.IsA<UsdGeomCamera>()) {
        renderer->setActiveCamera(prim.GetPath());
        renderer->freeCamera = false;
    }

    if (open && !prim.GetChildren().empty()) {
        for (UsdPrim child : prim.GetChildren()) {
            renderUSDTree(child, renderer);
        }
        ImGui::TreePop();
    }
}

class AudioMixer {

private:
    struct SourceState {
        std::vector<float> buffer;
        int64_t total_pushed = 0; // Total samples (frames * channels)
        double start_pts = -1.0;
    };

    std::mutex mtx;
    std::map<void*, SourceState> sources;
    std::deque<int16_t> encoder_queue;

    snd_pcm_t* alsa_handle = nullptr;
    std::thread playback_thread;
    std::atomic<bool> quit{false};
    int sample_rate;
    int64_t total_written_to_alsa = 0; // In samples (frames * channels)

    struct DelaySnap {
        double delay;
        std::chrono::steady_clock::time_point timestamp;
    };
    std::mutex snap_mtx;
    DelaySnap last_snap;

    void setupALSA(int channels, int rate) {
        if (snd_pcm_open(&alsa_handle, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) return;
        // 500ms buffer for stability
        snd_pcm_set_params(alsa_handle, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED,
                           channels, rate, 1, 500000); 
        snd_pcm_nonblock(alsa_handle, 1);
        snd_pcm_prepare(alsa_handle);
    }

    void updateDelaySnap() {
        snd_pcm_sframes_t delay_frames = 0;
        double d = 0;
        if (alsa_handle && snd_pcm_delay(alsa_handle, &delay_frames) == 0) {
            d = (double)delay_frames / sample_rate;
        }
        std::lock_guard<std::mutex> lock(snap_mtx);
        last_snap = {d, std::chrono::steady_clock::now()};
    }

    void playbackWorker() {
        const int max_frames = 512;
        std::vector<float> mix_buf(max_frames * 2);
        std::vector<int16_t> out_buf(max_frames * 2);

        while (!quit) {
            int frames_to_write = 0;
            {
                std::lock_guard<std::mutex> lock(mtx);
                int max_avail = 0;
                for (auto& p : sources) max_avail = std::max(max_avail, (int)p.second.buffer.size());

                if (max_avail > 0) {
                    int samples = std::min(max_avail, (int)mix_buf.size());
                    if (samples % 2 != 0) samples--;
                    if (samples > 0) {
                        std::fill(mix_buf.begin(), mix_buf.begin() + samples, 0.0f);
                        for (auto& p : sources) {
                            auto& src = p.second;
                            int to_copy = std::min((int)src.buffer.size(), samples);
                            for (int i = 0; i < to_copy; ++i) mix_buf[i] += src.buffer[i];
                            src.buffer.erase(src.buffer.begin(), src.buffer.begin() + to_copy);
                        }
                        for (int i = 0; i < samples; ++i) {
                            float s = std::max(-1.0f, std::min(1.0f, mix_buf[i]));
                            out_buf[i] = static_cast<int16_t>(s * 32767.0f);
                            encoder_queue.push_back(out_buf[i]);
                        }
                        if (encoder_queue.size() > (size_t)sample_rate * 2 * 5)
                            encoder_queue.erase(encoder_queue.begin(), encoder_queue.begin() + (encoder_queue.size() - (size_t)sample_rate * 2 * 5));
                        frames_to_write = samples / 2;
                    }
                }
            }

            if (frames_to_write == 0) {
                updateDelaySnap();
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            }

            int written = 0;
            while (written < frames_to_write && !quit && alsa_handle) {
                snd_pcm_sframes_t ret = snd_pcm_writei(alsa_handle, out_buf.data() + written * 2, frames_to_write - written);
                if (ret == -EAGAIN) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); continue; }
                if (ret == -EPIPE) { snd_pcm_prepare(alsa_handle); continue; }
                if (ret < 0) break;
                written += ret;
                total_written_to_alsa += ret * 2;
            }
            updateDelaySnap();
        }
    }

public:
    AudioMixer(int rate) : sample_rate(rate) {
        last_snap = {0.0, std::chrono::steady_clock::now()};
        setupALSA(2, rate);
        playback_thread = std::thread(&AudioMixer::playbackWorker, this);
    }

    ~AudioMixer() {
        quit = true;
        if (alsa_handle) snd_pcm_drop(alsa_handle);
        if (playback_thread.joinable()) playback_thread.join();
        if (alsa_handle) snd_pcm_close(alsa_handle);
    }

    void addAudio(void* source, const int16_t* data, int nb_samples, double pts) {
        std::lock_guard<std::mutex> lock(mtx);
        auto& src = sources[source];
        if (src.start_pts < 0 || std::abs(pts - (src.start_pts + (double)src.total_pushed / (sample_rate * 2))) > 0.5) {
            src.start_pts = pts;
            src.total_pushed = 0;
            src.buffer.clear();
        }
        for (int i = 0; i < nb_samples * 2; ++i) src.buffer.push_back(data[i] / 32768.0f);
        src.total_pushed += nb_samples * 2;
    }

    void removeSource(void* source) {
        std::lock_guard<std::mutex> lock(mtx);
        sources.erase(source);
    }

    std::vector<int16_t> consume(int nb_samples) {
        std::lock_guard<std::mutex> lock(mtx);
        int total = nb_samples * 2;
        int available = std::min((int)encoder_queue.size(), total);
        if (available == 0) return {};
        std::vector<int16_t> out(available);
        for (int i = 0; i < available; ++i) { out[i] = encoder_queue.front(); encoder_queue.pop_front(); }
        return out;
    }

    double getSourcePTS(void* source) {
        std::lock_guard<std::mutex> lock(mtx);
        if (sources.find(source) == sources.end()) return 0.0;
        auto& src = sources[source];
        if (src.start_pts < 0) return 0.0;

        DelaySnap snap;
        {
            std::lock_guard<std::mutex> slock(snap_mtx);
            snap = last_snap;
        }
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - snap.timestamp).count();
        double alsa_delay = std::max(0.0, snap.delay - elapsed);
        double buffer_delay = (double)(src.buffer.size() / 2) / sample_rate;

        double total_delay = alsa_delay + buffer_delay;
        return src.start_pts + (double)(src.total_pushed / 2) / sample_rate - total_delay;
    }
};// ---------------------------------------------------------------------------
// Audio Decoder — decode MPEG audio files using FFmpeg and feed to AudioMixer
// ---------------------------------------------------------------------------
struct AudioDecoder {
    AVFormatContext* fmt_ctx = nullptr;
    AVCodecContext*  dec_ctx = nullptr;
    int              audio_stream_idx = -1;
    AVFrame*         frame = nullptr;
    AVPacket*        pkt = nullptr;
    SwrContext*      swr_ctx = nullptr;
    AudioMixer*      mixer = nullptr;
    std::atomic<bool> quit{false};
    std::thread      decode_thread;
    std::string      path;

    AudioDecoder(const std::string& p, AudioMixer* m) : mixer(m), path(p) {
        if (avformat_open_input(&fmt_ctx, path.c_str(), nullptr, nullptr) < 0)
            throw std::runtime_error("Could not open audio file");
        if (avformat_find_stream_info(fmt_ctx, nullptr) < 0)
            throw std::runtime_error("Could not find stream info");

        for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
            if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                audio_stream_idx = i;
                break;
            }
        }
        if (audio_stream_idx == -1) throw std::runtime_error("No audio stream");

        const AVCodec* codec = avcodec_find_decoder(fmt_ctx->streams[audio_stream_idx]->codecpar->codec_id);
        dec_ctx = avcodec_alloc_context3(codec);
        avcodec_parameters_to_context(dec_ctx, fmt_ctx->streams[audio_stream_idx]->codecpar);
        if (avcodec_open2(dec_ctx, codec, nullptr) < 0)
            throw std::runtime_error("Could not open decoder");

        frame = av_frame_alloc();
        pkt = av_packet_alloc();

        swr_ctx = swr_alloc();
        av_opt_set_chlayout(swr_ctx, "in_chlayout", &dec_ctx->ch_layout, 0);
        av_opt_set_int(swr_ctx, "in_sample_rate", dec_ctx->sample_rate, 0);
        av_opt_set_int(swr_ctx, "in_sample_fmt", dec_ctx->sample_fmt, 0);
        
        AVChannelLayout out_ch_layout;
        av_channel_layout_default(&out_ch_layout, 2);
        av_opt_set_chlayout(swr_ctx, "out_chlayout", &out_ch_layout, 0);
        av_opt_set_int(swr_ctx, "out_sample_rate", MIXER_SAMPLE_RATE, 0);
        av_opt_set_int(swr_ctx, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
        swr_init(swr_ctx);

        decode_thread = std::thread(&AudioDecoder::decodeLoop, this);
    }

    ~AudioDecoder() {
        quit = true;
        if (decode_thread.joinable()) decode_thread.join();
        if (swr_ctx) swr_free(&swr_ctx);
        av_frame_free(&frame);
        av_packet_free(&pkt);
        avcodec_free_context(&dec_ctx);
        avformat_close_input(&fmt_ctx);
    }

    void decodeLoop() {
        int64_t total_out = 0;
        while (!quit) {
            if (av_read_frame(fmt_ctx, pkt) < 0) {
                av_seek_frame(fmt_ctx, audio_stream_idx, 0, AVSEEK_FLAG_BACKWARD);
                avcodec_flush_buffers(dec_ctx);
                continue;
            }

            if (pkt->stream_index == audio_stream_idx) {
                if (avcodec_send_packet(dec_ctx, pkt) == 0) {
                    while (avcodec_receive_frame(dec_ctx, frame) == 0) {
                        int out_samples = (int)av_rescale_rnd(swr_get_delay(swr_ctx, dec_ctx->sample_rate) + frame->nb_samples, MIXER_SAMPLE_RATE, dec_ctx->sample_rate, AV_ROUND_UP);
                        std::vector<int16_t> out_buf(out_samples * 2);
                        uint8_t* out_data[1] = {(uint8_t*)out_buf.data()};
                        int converted = swr_convert(swr_ctx, out_data, out_samples, (const uint8_t**)frame->data, frame->nb_samples);
                        
                        if (mixer && converted > 0) {
                            double pts = (double)total_out / (MIXER_SAMPLE_RATE * 1.0);
                            mixer->addAudio(this, out_buf.data(), converted, pts);
                            total_out += converted;
                        }
                        
                        // Simple throttle to avoid overfilling mixer buffer (keep ~1 sec)
                        while (!quit && mixer && mixer->getSourcePTS(this) > 1.0) {
                             std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        }
                    }
                }
            }
            av_packet_unref(pkt);
        }
    }
};// --- 2. Encoder Class ---
class NvencEncoder {
private:
    struct RawFrame {
        std::vector<uint8_t> data;
        int64_t pts;
    };

    AVFormatContext* out_ctx = nullptr;
    AVCodecContext *v_enc = nullptr, *a_enc = nullptr;
    AVStream *v_stream = nullptr, *a_stream = nullptr;
    SwsContext* sws_ctx = nullptr;
    SwrContext* swr_ctx = nullptr;

    AudioMixer* shared_mixer;

    std::thread worker_thread;
    std::atomic<bool> quit{false};
    std::mutex v_queue_mtx;
    std::queue<RawFrame*> video_queue;
    std::vector<RawFrame*> buffer_pool;
    std::mutex pool_mtx;

    int width, height;
    double target_fps;
    int64_t a_pts = 0;
    int64_t last_v_pts = -1;
    int a_frame_size = 0;
    std::vector<int16_t> audio_remainder_buf;

    // Time-based Sync State
    std::chrono::steady_clock::time_point start_time;
    std::atomic<bool> recording_started{false};
    const size_t MAX_QUEUE_SIZE = 15; 

    void workerFunc() {
        while (!quit || !video_queue.empty()) {
            bool busy = false;

            // 1. Process Video Frames
            RawFrame* raw = nullptr;
            {
                std::lock_guard<std::mutex> lock(v_queue_mtx);
                if (!video_queue.empty()) {
                    raw = video_queue.front();
                    video_queue.pop();
                    busy = true;
                }
            }

            if (raw) {
                AVFrame* v_frame = av_frame_alloc();
                v_frame->format = v_enc->pix_fmt;
                v_frame->width = width;
                v_frame->height = height;
                v_frame->pts = raw->pts;
                av_frame_get_buffer(v_frame, 0);

                const uint8_t* src[] = { raw->data.data() };
                int src_stride[] = { 4 * width };
                sws_scale(sws_ctx, src, src_stride, 0, height, v_frame->data, v_frame->linesize);

                encodeAndMux(v_frame, v_enc, v_stream);
                av_frame_free(&v_frame);

                // Return to pool
                {
                    std::lock_guard<std::mutex> lock(pool_mtx);
                    buffer_pool.push_back(raw);
                }
            }

            // 2. Process Audio (Only if recording has officially started)
            if (recording_started) {
                // Request a decent chunk of audio to avoid frequent wakeups, but stay responsive
                std::vector<int16_t> mixed = shared_mixer->consume(a_frame_size * 2);
                if (!mixed.empty()) {
                    busy = true;
                    audio_remainder_buf.insert(audio_remainder_buf.end(), mixed.begin(), mixed.end());
                    while ((int)audio_remainder_buf.size() >= a_frame_size * 2) {
                        processAudio(audio_remainder_buf.data(), a_frame_size);
                        audio_remainder_buf.erase(audio_remainder_buf.begin(), audio_remainder_buf.begin() + a_frame_size * 2);
                    }
                }
            }

            if (!busy) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        // Flush remaining audio on exit
        if (!audio_remainder_buf.empty()) {
            int samples = audio_remainder_buf.size() / 2;
            processAudio(audio_remainder_buf.data(), samples);
            audio_remainder_buf.clear();
        }
    }

    void processAudio(const int16_t* data, int samples) {
        if (samples <= 0) return;
        AVFrame* f = av_frame_alloc();
        f->format = a_enc->sample_fmt;
        f->nb_samples = samples;
        av_channel_layout_copy(&f->ch_layout, &a_enc->ch_layout);
        av_frame_get_buffer(f, 0);

        const uint8_t* src[] = { (const uint8_t*)data };
        swr_convert(swr_ctx, f->data, samples, src, samples);
        
        f->pts = a_pts;
        a_pts += samples;
        encodeAndMux(f, a_enc, a_stream);
        av_frame_free(&f);
    }

    void encodeAndMux(AVFrame* frame, AVCodecContext* enc, AVStream* st) {
        if (avcodec_send_frame(enc, frame) < 0) return;
        AVPacket* pkt = av_packet_alloc();
        while (avcodec_receive_packet(enc, pkt) == 0) {
            av_packet_rescale_ts(pkt, enc->time_base, st->time_base);
            pkt->stream_index = st->index;
            av_interleaved_write_frame(out_ctx, pkt);
            av_packet_unref(pkt);
        }
        av_packet_free(&pkt);
    }

public:
    NvencEncoder(int w, int h, int fps, int sample_rate, AudioMixer* mixer, const std::string& path) 
        : width(w), height(h), target_fps((double)fps), shared_mixer(mixer) {
        
        avformat_alloc_output_context2(&out_ctx, nullptr, nullptr, path.c_str());

        // Video: H264_NVENC
        const AVCodec* v_codec = avcodec_find_encoder_by_name("h264_nvenc");
        v_stream = avformat_new_stream(out_ctx, v_codec);
        v_enc = avcodec_alloc_context3(v_codec);
        v_enc->width = w; v_enc->height = h;
        v_enc->pix_fmt = AV_PIX_FMT_NV12;
        v_enc->time_base = {1, (int)target_fps};
        v_enc->framerate = {(int)target_fps, 1};
        v_enc->bit_rate = 20000000; // 20 Mbps
        av_opt_set(v_enc->priv_data, "preset", "p7", 0);
        av_opt_set(v_enc->priv_data, "tune", "hq", 0);
        v_enc->gop_size = 30; // Add GOP for stability
        if (out_ctx->oformat->flags & AVFMT_GLOBALHEADER) v_enc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        avcodec_open2(v_enc, v_codec, nullptr);
        avcodec_parameters_from_context(v_stream->codecpar, v_enc);

        // Audio: AAC
        const AVCodec* a_codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
        a_stream = avformat_new_stream(out_ctx, a_codec);
        a_enc = avcodec_alloc_context3(a_codec);
        a_enc->sample_fmt = AV_SAMPLE_FMT_FLTP;
        a_enc->sample_rate = sample_rate;
        av_channel_layout_default(&a_enc->ch_layout, 2);
        a_enc->time_base = {1, sample_rate};
        avcodec_open2(a_enc, a_codec, nullptr);
        avcodec_parameters_from_context(a_stream->codecpar, a_enc);
        a_frame_size = a_enc->frame_size;

        // Converters
        sws_ctx = sws_getContext(w, h, AV_PIX_FMT_RGBA, w, h, v_enc->pix_fmt, SWS_BICUBIC, nullptr, nullptr, nullptr);
        swr_alloc_set_opts2(&swr_ctx, &a_enc->ch_layout, a_enc->sample_fmt, sample_rate,
                            &a_enc->ch_layout, AV_SAMPLE_FMT_S16, sample_rate, 0, nullptr);
        swr_init(swr_ctx);

        avio_open(&out_ctx->pb, path.c_str(), AVIO_FLAG_WRITE);
        if (avformat_write_header(out_ctx, nullptr) < 0) {
            throw std::runtime_error("Could not write header to: " + path);
        }

        // Pre-allocate some buffers
        for (int i = 0; i < 5; ++i) {
            RawFrame* rf = new RawFrame();
            rf->data.resize(w * h * 4);
            buffer_pool.push_back(rf);
        }

        worker_thread = std::thread(&NvencEncoder::workerFunc, this);
    }

    void pushVideoFrame(const uint8_t* rgba_data, int pitch) {
        auto now = std::chrono::steady_clock::now();

        if (!recording_started) {
            start_time = now;
            recording_started = true;
            last_v_pts = -1;
        }

        // Calculate PTS based on actual elapsed wall-time
        auto elapsed = std::chrono::duration<double>(now - start_time).count();
        int64_t pts = static_cast<int64_t>(elapsed * target_fps);

        if (pts <= last_v_pts) return;
        last_v_pts = pts;

        {
            std::lock_guard<std::mutex> lock(v_queue_mtx);
            // Drop frame if main thread is severely out-pacing the encoder
            if (video_queue.size() > MAX_QUEUE_SIZE) return;
        }

        RawFrame* raw = nullptr;
        {
            std::lock_guard<std::mutex> lock(pool_mtx);
            if (!buffer_pool.empty()) {
                raw = buffer_pool.back();
                buffer_pool.pop_back();
            }
        }

        if (!raw) {
            raw = new RawFrame();
            raw->data.resize(width * height * 4);
        }

        // Safe copy with pitch handling
        if (pitch == width * 4) {
            std::memcpy(raw->data.data(), rgba_data, width * height * 4);
        } else {
            for (int y = 0; y < height; ++y) {
                std::memcpy(raw->data.data() + y * width * 4, rgba_data + y * pitch, width * 4);
            }
        }
        
        raw->pts = pts;

        {
            std::lock_guard<std::mutex> lock(v_queue_mtx);
            video_queue.push(raw);
        }
    }

    ~NvencEncoder() {
        quit = true;
        if (worker_thread.joinable()) worker_thread.join();
        encodeAndMux(nullptr, v_enc, v_stream);
        encodeAndMux(nullptr, a_enc, a_stream);
        av_write_trailer(out_ctx);
        sws_freeContext(sws_ctx); swr_free(&swr_ctx);
        avcodec_free_context(&v_enc); avcodec_free_context(&a_enc);
        avio_closep(&out_ctx->pb); avformat_free_context(out_ctx);

        while (!video_queue.empty()) {
            delete video_queue.front();
            video_queue.pop();
        }
        for (auto b : buffer_pool) delete b;
    }
};

AudioMixer* myMix = NULL;

class MediaDecoder {
private:
    AVFormatContext* fmt_ctx = nullptr;
    AVCodecContext* v_ctx = nullptr;
    AVCodecContext* a_ctx = nullptr;
    AVBufferRef*     hw_device_ctx = nullptr;
    enum AVPixelFormat hw_pix_fmt = AV_PIX_FMT_NONE;
    SwsContext* sws_ctx = nullptr;
    SwrContext* swr_ctx = nullptr;

    AVFrame* audio_frame = nullptr;
    uint8_t* audio_out_buf = nullptr;

    int video_stream_idx = -1;
    int audio_stream_idx = -1;
    int width = 0, height = 0;

    std::thread demux_thread;
    std::thread video_thread;
    std::thread audio_thread;
    
    std::mutex audio_mtx;
    std::mutex video_pkt_mtx;
    std::mutex texture_mtx;
    std::mutex pool_mtx;

    std::queue<AVPacket*> audio_pkt_queue;
    std::queue<AVPacket*> video_pkt_queue;
    
    struct DecodedFrame {
        AVFrame* frame_rgba;
        double pts;
    };
    std::queue<DecodedFrame> decoded_queue;
    std::vector<AVFrame*> frame_pool;
    
    const size_t MAX_DECODED_QUEUE = 32;
    const size_t MAX_PACKET_QUEUE = 1024;

    std::atomic<bool> quit{false};
    std::atomic<bool> seek_req{false};
    std::atomic<double> audio_clock{0.0}; 
    std::chrono::steady_clock::time_point start_t = std::chrono::steady_clock::now();
    bool bTransparent = false;
    bool bNoAudio = false;

    static enum AVPixelFormat get_hw_format(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts) {
        MediaDecoder* self = (MediaDecoder*)ctx->opaque;
        for (const enum AVPixelFormat *p = pix_fmts; *p != -1; p++) {
            if (*p == self->hw_pix_fmt) return *p;
        }
        return AV_PIX_FMT_NONE;
    }

    int init_hw_decoder(AVCodecContext *ctx, enum AVHWDeviceType type) {
        int err = av_hwdevice_ctx_create(&hw_device_ctx, type, NULL, NULL, 0);
        if (err < 0) return err;
        ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
        return 0;
    }

    std::atomic<double> first_pts{-1.0};

    double get_pts_seconds(AVFrame* f, int stream_idx) {
        if (stream_idx < 0 || !fmt_ctx || stream_idx >= (int)fmt_ctx->nb_streams) return 0.0;
        int64_t pts = f->best_effort_timestamp;
        if (pts == AV_NOPTS_VALUE) pts = f->pts;
        if (pts == AV_NOPTS_VALUE) pts = f->pkt_dts;
        if (pts == AV_NOPTS_VALUE) return 0.0; 
        
        double sec = pts * av_q2d(fmt_ctx->streams[stream_idx]->time_base);
        if (first_pts.load() < 0) {
            first_pts.store(sec);
            std::printf("MediaDecoder: First frame detected (stream %d, type %s) at PTS %.2f\n", 
                        stream_idx, (stream_idx == video_stream_idx ? "VIDEO" : "AUDIO"), sec);
        }
        return std::max(0.0, sec - first_pts.load());
    }

    std::atomic<bool> v_seek_ack{false};
    std::atomic<bool> a_seek_ack{false};

    void demuxWorker() {
        AVPacket* pkt = av_packet_alloc();
        int v_pkt_count = 0, a_pkt_count = 0;
        bool eof_reached = false;

        while (!quit) {
            size_t v_q, a_q;
            { std::lock_guard<std::mutex> l(video_pkt_mtx); v_q = video_pkt_queue.size(); }
            { std::lock_guard<std::mutex> l(audio_mtx); a_q = audio_pkt_queue.size(); }

            if (eof_reached) {
                if (v_q == 0 && a_q == 0) {
                    v_pkt_count = 0; a_pkt_count = 0; eof_reached = false;
                    seek_req.store(true);
                    v_seek_ack.store(false); a_seek_ack.store(false);
                    int seek_idx = (video_stream_idx != -1) ? video_stream_idx : audio_stream_idx;
                    if (seek_idx != -1) av_seek_frame(fmt_ctx, seek_idx, 0, AVSEEK_FLAG_BACKWARD);
                    while (!quit && ((video_stream_idx != -1 && !v_seek_ack.load()) || 
                                     (audio_stream_idx != -1 && !a_seek_ack.load()))) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                    seek_req.store(false);
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
                continue;
            }

            if (v_q > MAX_PACKET_QUEUE || a_q > MAX_PACKET_QUEUE) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                continue;
            }

            int ret = av_read_frame(fmt_ctx, pkt);
            if (ret < 0) {
                if (ret == AVERROR_EOF) eof_reached = true;
                else {
                    char errbuf[256]; av_strerror(ret, errbuf, sizeof(errbuf));
                    std::printf("MediaDecoder: av_read_frame error: %s\n", errbuf);
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                continue;
            }

            if (pkt->stream_index == video_stream_idx) {
                v_pkt_count++;
                AVPacket* v_pkt = av_packet_clone(pkt);
                std::lock_guard<std::mutex> l(video_pkt_mtx);
                video_pkt_queue.push(v_pkt);
            } else if (audio_stream_idx != -1 && pkt->stream_index == audio_stream_idx) {
                a_pkt_count++;
                AVPacket* a_pkt = av_packet_clone(pkt);
                std::lock_guard<std::mutex> l(audio_mtx);
                audio_pkt_queue.push(a_pkt);
            }
            av_packet_unref(pkt);
        }
        av_packet_free(&pkt);
    }

    double get_current_audio_time() {
        auto now = std::chrono::steady_clock::now();
        double wall_time = std::chrono::duration<double>(now - start_t).count();
        if (audio_stream_idx == -1 || !myMix) return wall_time;
        double pts = myMix->getSourcePTS(this);
        if (pts <= 0.0 || std::abs(pts - wall_time) > 1.0) return wall_time;
        return pts;
    }

    void audioWorker() {
        while (!quit) {
            if (seek_req.load() && !a_seek_ack.load()) {
                if (a_ctx) avcodec_flush_buffers(a_ctx);
                audio_clock.store(0.0);
                {
                    std::lock_guard<std::mutex> l(audio_mtx);
                    while(!audio_pkt_queue.empty()) { av_packet_free(&audio_pkt_queue.front()); audio_pkt_queue.pop(); }
                }
                a_seek_ack.store(true);
            }

            AVPacket* pkt = nullptr;
            {
                std::lock_guard<std::mutex> lock(audio_mtx);
                if (!audio_pkt_queue.empty()) pkt = audio_pkt_queue.front();
            }
            if (!pkt) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); continue; }

            int ret = avcodec_send_packet(a_ctx, pkt);
            if (ret == 0) {
                { std::lock_guard<std::mutex> lock(audio_mtx); audio_pkt_queue.pop(); }
            } else if (ret != AVERROR(EAGAIN)) {
                { std::lock_guard<std::mutex> lock(audio_mtx); audio_pkt_queue.pop(); }
                av_packet_free(&pkt); continue;
            }

            while (avcodec_receive_frame(a_ctx, audio_frame) == 0 && !quit) {
                double pts = get_pts_seconds(audio_frame, audio_stream_idx);
                int out_samples = swr_convert(swr_ctx, &audio_out_buf, 4096, (const uint8_t**)audio_frame->data, audio_frame->nb_samples);
                if (out_samples > 0 && myMix) {
                    myMix->addAudio(this, (int16_t*)audio_out_buf, out_samples, pts);
                    audio_clock.store(pts + (double)out_samples / MIXER_SAMPLE_RATE);
                }
                av_frame_unref(audio_frame);
            }
            if (ret == 0) av_packet_free(&pkt); else std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    void videoWorker() {
        AVFrame* raw_frame = av_frame_alloc();
        AVFrame* sw_frame = av_frame_alloc();
        enum AVPixelFormat last_fmt = AV_PIX_FMT_NONE;
        int frames_decoded = 0;
        int packets_sent = 0;

        while (!quit) {
            if (seek_req.load() && !v_seek_ack.load()) {
                if (v_ctx) avcodec_flush_buffers(v_ctx);
                {
                    std::lock_guard<std::mutex> l(video_pkt_mtx);
                    while(!video_pkt_queue.empty()) { av_packet_free(&video_pkt_queue.front()); video_pkt_queue.pop(); }
                }
                {
                    std::lock_guard<std::mutex> l(texture_mtx);
                    while(!decoded_queue.empty()) { AVFrame* f = decoded_queue.front().frame_rgba; { std::lock_guard<std::mutex> pl(pool_mtx); frame_pool.push_back(f); } decoded_queue.pop(); }
                }
                start_t = std::chrono::steady_clock::now();
                first_pts.store(-1.0);
                frames_decoded = 0; packets_sent = 0;
                v_seek_ack.store(true);
            }

            size_t q_size;
            { std::lock_guard<std::mutex> l(texture_mtx); q_size = decoded_queue.size(); }
            if (q_size >= MAX_DECODED_QUEUE) { std::this_thread::sleep_for(std::chrono::milliseconds(5)); continue; }

            AVPacket* pkt = nullptr;
            { std::lock_guard<std::mutex> l(video_pkt_mtx); if (!video_pkt_queue.empty()) pkt = video_pkt_queue.front(); }
            if (!pkt) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); continue; }

            int ret = avcodec_send_packet(v_ctx, pkt);
            if (ret == 0) {
                { std::lock_guard<std::mutex> l(video_pkt_mtx); video_pkt_queue.pop(); }
                packets_sent++;
            } else if (ret != AVERROR(EAGAIN)) {
                { std::lock_guard<std::mutex> l(video_pkt_mtx); video_pkt_queue.pop(); }
                av_packet_free(&pkt); continue;
            }

            // Safety: If HW decoder is eating packets but not producing frames, fall back to SW
            if (hw_device_ctx && frames_decoded == 0 && packets_sent > 100) {
                std::printf("MediaDecoder: HW decoder stalling (VDPAU/CUDA), forcing software fallback...\n");
                avcodec_flush_buffers(v_ctx);
                av_buffer_unref(&hw_device_ctx);
                v_ctx->hw_device_ctx = nullptr;
                v_ctx->get_format = nullptr;
                const AVCodec* sw_vcodec = avcodec_find_decoder(fmt_ctx->streams[video_stream_idx]->codecpar->codec_id);
                avcodec_open2(v_ctx, sw_vcodec, nullptr);
            }

            while (avcodec_receive_frame(v_ctx, raw_frame) == 0 && !quit) {
                frames_decoded++;
                AVFrame* frame_to_use = raw_frame;
                if (raw_frame->format == hw_pix_fmt && hw_device_ctx) {
                    av_frame_unref(sw_frame);
                    if (av_hwframe_transfer_data(sw_frame, raw_frame, 0) != 0) {
                        av_frame_unref(raw_frame);
                        continue;
                    }
                    av_frame_copy_props(sw_frame, raw_frame);
                    frame_to_use = sw_frame;
                }

                AVFrame* rgba_f = nullptr;
                { std::lock_guard<std::mutex> l(pool_mtx); if (!frame_pool.empty()) { rgba_f = frame_pool.back(); frame_pool.pop_back(); } }
                if (!rgba_f) {
                    rgba_f = av_frame_alloc(); rgba_f->format = AV_PIX_FMT_RGBA;
                    rgba_f->width = width; rgba_f->height = height; av_frame_get_buffer(rgba_f, 32);
                }

                if (!sws_ctx || last_fmt != frame_to_use->format) {
                    if (sws_ctx) sws_freeContext(sws_ctx);
                    sws_ctx = sws_getContext(width, height, (AVPixelFormat)frame_to_use->format, width, height, AV_PIX_FMT_RGBA, SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
                    last_fmt = (AVPixelFormat)frame_to_use->format;
                }
                sws_scale(sws_ctx, frame_to_use->data, frame_to_use->linesize, 0, height, rgba_f->data, rgba_f->linesize);

                DecodedFrame df; df.frame_rgba = rgba_f; df.pts = get_pts_seconds(frame_to_use, video_stream_idx);
                { std::lock_guard<std::mutex> l(texture_mtx); decoded_queue.push(df); }
                av_frame_unref(raw_frame);
            }
            if (ret == 0) av_packet_free(&pkt); else std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        av_frame_free(&raw_frame); av_frame_free(&sw_frame);
    }

public:
    MediaDecoder(const std::string& path, bool transparent = false, bool no_audio = false) 
        : bTransparent(transparent), bNoAudio(no_audio) {
        if (avformat_open_input(&fmt_ctx, path.c_str(), nullptr, nullptr) < 0) {
            throw std::runtime_error("Could not open input file: " + path);
        }
        if (avformat_find_stream_info(fmt_ctx, nullptr) < 0) {
            throw std::runtime_error("Could not find stream information: " + path);
        }

        const AVCodec *vcodec = nullptr, *acodec = nullptr;
        for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
            AVCodecParameters* p = fmt_ctx->streams[i]->codecpar;
            if (p->codec_type == AVMEDIA_TYPE_VIDEO && video_stream_idx == -1) {
                video_stream_idx = i;
                vcodec = avcodec_find_decoder(p->codec_id);
            } else if (!bNoAudio && p->codec_type == AVMEDIA_TYPE_AUDIO && audio_stream_idx == -1) {
                audio_stream_idx = i;
                acodec = avcodec_find_decoder(p->codec_id);
            }
        }

        if (video_stream_idx != -1 && vcodec) {
            v_ctx = avcodec_alloc_context3(vcodec);
            if (v_ctx) {
                avcodec_parameters_to_context(v_ctx, fmt_ctx->streams[video_stream_idx]->codecpar);
                v_ctx->opaque = this;
                v_ctx->get_format = get_hw_format;

                static const enum AVHWDeviceType hw_types[] = {
                    AV_HWDEVICE_TYPE_VDPAU,
                    AV_HWDEVICE_TYPE_VAAPI,
                    AV_HWDEVICE_TYPE_CUDA,
                    AV_HWDEVICE_TYPE_NONE
                };

                for (int i = 0; hw_types[i] != AV_HWDEVICE_TYPE_NONE; i++) {
                    for (int j = 0;; j++) {
                        const AVCodecHWConfig *config = avcodec_get_hw_config(vcodec, j);
                        if (!config) break;
                        if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
                            config->device_type == hw_types[i]) {
                            hw_pix_fmt = config->pix_fmt;
                            if (init_hw_decoder(v_ctx, hw_types[i]) == 0) {
                                std::printf("MediaDecoder: Using HW acceleration: %s\n", av_hwdevice_get_type_name(hw_types[i]));
                                break;
                            }
                        }
                    }
                    if (hw_device_ctx) break;
                }

                if (!hw_device_ctx) {
                    std::printf("MediaDecoder: No HW acceleration found, using software decoding.\n");
                }

                v_ctx->thread_count = 0; 
                if (avcodec_open2(v_ctx, vcodec, nullptr) < 0) {
                    if (hw_device_ctx) {
                         std::printf("MediaDecoder: HW open failed, falling back to software.\n");
                         av_buffer_unref(&hw_device_ctx);
                         v_ctx->hw_device_ctx = nullptr;
                         v_ctx->get_format = nullptr;
                         avcodec_open2(v_ctx, vcodec, nullptr);
                    }
                }
                if (v_ctx->priv_data && vcodec->id == AV_CODEC_ID_H264) {
                    av_opt_set(v_ctx->priv_data, "reref_frames", "1", 0);
                }
                width = v_ctx->width; height = v_ctx->height;
            }
        }

        if (audio_stream_idx != -1 && acodec) {
            a_ctx = avcodec_alloc_context3(acodec);
            if (a_ctx) {
                avcodec_parameters_to_context(a_ctx, fmt_ctx->streams[audio_stream_idx]->codecpar);
                a_ctx->thread_count = 0; // Audio is fine with auto
                if (avcodec_open2(a_ctx, acodec, nullptr) >= 0) {
                    AVChannelLayout out_ch; av_channel_layout_default(&out_ch, 2);
                    swr_alloc_set_opts2(&swr_ctx, &out_ch, AV_SAMPLE_FMT_S16, MIXER_SAMPLE_RATE,
                                        &a_ctx->ch_layout, a_ctx->sample_fmt, a_ctx->sample_rate, 0, nullptr);
                    swr_init(swr_ctx);
                    audio_out_buf = (uint8_t*)av_malloc(av_samples_get_buffer_size(nullptr, 2, 4096, AV_SAMPLE_FMT_S16, 0));
                    audio_frame = av_frame_alloc();
                } else {
                    avcodec_free_context(&a_ctx);
                    audio_stream_idx = -1;
                }
            }
        }

        if (!v_ctx) throw std::runtime_error("Could not initialize video decoder for: " + path);

        start_t = std::chrono::steady_clock::now();
        demux_thread = std::thread(&MediaDecoder::demuxWorker, this);
        video_thread = std::thread(&MediaDecoder::videoWorker, this);
        if (a_ctx) audio_thread = std::thread(&MediaDecoder::audioWorker, this);
    }

    ~MediaDecoder() {
        quit = true;
        if (demux_thread.joinable()) demux_thread.join();
        if (video_thread.joinable()) video_thread.join();
        if (audio_thread.joinable()) audio_thread.join();

        if (myMix) myMix->removeSource(this);

        auto clear_q = [](std::queue<AVPacket*>& q, std::mutex& mtx) {
            std::lock_guard<std::mutex> l(mtx);
            while(!q.empty()) { av_packet_free(&q.front()); q.pop(); }
        };
        clear_q(audio_pkt_queue, audio_mtx);
        clear_q(video_pkt_queue, video_pkt_mtx);

        {
            std::lock_guard<std::mutex> lock(texture_mtx);
            while(!decoded_queue.empty()){
                AVFrame* f = decoded_queue.front().frame_rgba;
                av_frame_free(&f);
                decoded_queue.pop();
            }
        }
        for(auto f : frame_pool) av_frame_free(&f);

        if (sws_ctx) sws_freeContext(sws_ctx);
        if (swr_ctx) swr_free(&swr_ctx);
        av_free(audio_out_buf);
        av_frame_free(&audio_frame);
        avcodec_free_context(&v_ctx);
        avcodec_free_context(&a_ctx);
        if (hw_device_ctx) av_buffer_unref(&hw_device_ctx);
        avformat_close_input(&fmt_ctx);
    }

    void updateTexture(Renderer* renderer, SDL_GPUTexture* tex) {
        AVFrame* best_frame = nullptr;
        std::vector<AVFrame*> old_frames;
        {
            std::lock_guard<std::mutex> lock(texture_mtx);
            if (decoded_queue.empty()) return;
            double target_pts = get_current_audio_time();

            while (decoded_queue.size() > 2 && decoded_queue.front().pts < target_pts - 0.5) {                old_frames.push_back(decoded_queue.front().frame_rgba);
                decoded_queue.pop();
            }
            while (!decoded_queue.empty()) {
                if (decoded_queue.front().pts <= target_pts + 0.04) {
                    if (best_frame) old_frames.push_back(best_frame);
                    best_frame = decoded_queue.front().frame_rgba;
                    decoded_queue.pop();
                } else break;
            }
        }
        if (best_frame) {
            renderer->updateTexture(tex, width, height, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, best_frame->data[0], best_frame->linesize[0]);
            old_frames.push_back(best_frame);
        }
        if (!old_frames.empty()) {
            std::lock_guard<std::mutex> plock(pool_mtx);
            for (auto f : old_frames) frame_pool.push_back(f);
        }
    }

    int getWidth() const { return width; }
    int getHeight() const { return height; }
};

bool  plasma_render_tiles = false;
float cur_rel;
int cur_w, cur_h;
SDL_Window* window;
Renderer* g_renderer;

// ---------------------------------------------------------------------------
// FFmpeg recording — pipe raw RGBA frames to ffmpeg, produce mp4
// ---------------------------------------------------------------------------
struct Recorder {
    FILE*  pipe       = nullptr;
    int    width      = 0;
    int    height     = 0;
    int    fps        = 30;
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

    
    if (!myMix) myMix = new AudioMixer(MIXER_SAMPLE_RATE);
    myNvec = new NvencEncoder(w, h, fps, MIXER_SAMPLE_RATE, myMix ,std::string(path));
 
    std::printf("Recording started: %s (%dx%d @ %d fps)\n", path, w, h, fps);
    return true;
}

static void recorder_feed_frame(Recorder& rec, Renderer* renderer) {
    if (myNvec == NULL) return;

    // Read back the rendered frame
    SDL_Surface* surf = renderer->readPixels();
    if (!surf) {
        std::printf("readPixels failed\n");
        return;
    }

    // Convert to RGBA32 if needed
    SDL_Surface* rgba = SDL_ConvertSurface(surf, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(surf);
    if (!rgba) {
        std::printf("Surface conversion failed: %s\n", SDL_GetError());
        return;
    }

    SDL_Surface* final_surf = rgba;
    bool need_free_final = false;
    if (rgba->w != rec.width || rgba->h != rec.height) {
        SDL_Surface* scaled = SDL_CreateSurface(rec.width, rec.height, SDL_PIXELFORMAT_RGBA32);
        if (scaled) {
            SDL_BlitSurfaceScaled(rgba, nullptr, scaled, nullptr, SDL_SCALEMODE_LINEAR);
            final_surf = scaled;
            need_free_final = true;
        }
    }
    
    // Write raw pixels to ffmpeg pipe
    SDL_LockSurface(final_surf);
    myNvec->pushVideoFrame(static_cast<const Uint8*>(final_surf->pixels), final_surf->pitch);
    SDL_UnlockSurface(final_surf);

    if (need_free_final)
        SDL_DestroySurface(final_surf);
    
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
    SDL_GPUTexture* tex;
    int           w, h;
    bool           bNoColor = false;
};

// ---------------------------------------------------------------------------
// A single bouncing text instance
// ---------------------------------------------------------------------------

struct ParsedSegment {
    int posx, posy, velox, veloy;
    bool bIsStatic;
    int r, g, b; 
    std::string content;
    int bIsFile; // 0: None, 1: PNG, 2: Video, 3: Plasma, 4: Fractal, 5: Tvid, 6: USD, 7: Transparent USD
    std::string fullInput;
    int over_w = 0, over_h = 0;
    int line_breaks = 0;
    bool start_new_group = false;
    std::string stencil_path;
    int ttl_ms = -1;
    float phys_vx = 0, phys_vy = 0, phys_sx = 0, phys_sy = 0, mass = 1.0f, bouncy = 1.0f;
    bool hasPhys = false;
    bool noAudio = false;
    int layer = 1;

    bool hasHover = false;
    Uint8 hover_r=255, hover_g=255, hover_b=255;
    int hover_w=0;
    bool hasClicked = false;
    std::string clicked_lua;
};

class ContentParser {
public:
    static std::vector<ParsedSegment> parse(const std::string& input) {
        std::vector<ParsedSegment> results;

        // Initial State
        int px = 0, py = 0, vx = 0, vy = 0;
        int cr = 255, cg = 255, cb = 255;
        int ow = 0, oh = 0;
        int ttl = -1;
        int line_breaks = 0;
        float p_vx = 0, p_vy = 0, p_sx = 0, p_sy = 0, p_mass = 1.0f, p_bouncy = 1.0f;
        bool hasPhys = false;
        bool bIsStatic = false;
        bool next_is_new_group = true; // First segment always starts a group
        std::string stencil_path = "";
        int layer = 1;

        bool hasHover = false;
        Uint8 hr=255, hg=255, hb=255;
        int hw=0;
        bool hasClicked = false;
        std::string clua = "";

        // Scan string for tags
        std::string body = input;
        std::regex tagRegex(R"(\[(image|video|tvid|plasma|fractal|rgb|rect|lf|pos|stencil|ttl|phys|layer|usd|tusd|tscn|ttscn|hover|clicked)(?::\s*([^\]]*))?\])", std::regex::icase);
        auto tags_begin = std::sregex_iterator(body.begin(), body.end(), tagRegex);
        auto tags_end = std::sregex_iterator();

        size_t lastPos = 0;
        for (std::sregex_iterator i = tags_begin; i != tags_end; ++i) {
            std::smatch match = *i;
            size_t matchPos = match.position();

            // Text segment before a tag
            if (matchPos > lastPos) {
                results.push_back({px, py, vx, vy, bIsStatic, cr, cg, cb, body.substr(lastPos, matchPos - lastPos), 0, input, 0, 0, line_breaks, next_is_new_group, stencil_path, ttl, p_vx, p_vy, p_sx, p_sy, p_mass, p_bouncy, hasPhys, false, layer, hasHover, hr, hg, hb, hw, hasClicked, clua});
                line_breaks = 0; next_is_new_group = false;
                stencil_path = "";
            }

            std::string tagType = match.str(1);
            std::transform(tagType.begin(), tagType.end(), tagType.begin(), ::tolower);
            std::string tagContent = match.str(2);

            if (tagType == "pos") {
                std::vector<std::string> tokens = tokenize(tagContent);
                if (tokens.size() >= 4) {
                    px = std::stoi(tokens[0]); py = std::stoi(tokens[1]);
                    vx = std::stoi(tokens[2]); vy = std::stoi(tokens[3]);
                    bIsStatic = false;
                } else if (tokens.size() >= 2) {
                    px = std::stoi(tokens[0]); py = std::stoi(tokens[1]);
                    vx = 0; vy = 0;
                    bIsStatic = true;
                }
                next_is_new_group = true;
            } else if (tagType == "rgb") {
                std::vector<std::string> rgbTokens = tokenize(tagContent);
                if (rgbTokens.size() >= 3) {
                    cr = std::stoi(rgbTokens[0]);
                    cg = std::stoi(rgbTokens[1]);
                    cb = std::stoi(rgbTokens[2]);
                }
            } else if (tagType == "stencil") {
                stencil_path = tagContent;
            } else if (tagType == "ttl") {
                try { ttl = std::stoi(tagContent); } catch(...) { ttl = -1; }
            } else if (tagType == "layer") {
                try { layer = std::stoi(tagContent); } catch(...) { layer = 1; }
            } else if (tagType == "phys") {
                std::vector<std::string> tokens = tokenize(tagContent);
                if (tokens.size() >= 6) {
                    try {
                        p_vx = std::stof(tokens[0]); p_vy = std::stof(tokens[1]);
                        p_sx = std::stof(tokens[2]); p_sy = std::stof(tokens[3]);
                        p_mass = std::stof(tokens[4]); p_bouncy = std::stof(tokens[5]);
                        hasPhys = true;
                        vx = (int)p_vx; vy = (int)p_vy;
                        px = (int)p_sx; py = (int)p_sy;
                    } catch(...) {}
                } else if (tokens.size() >= 4) {
                    try {
                        p_vx = std::stof(tokens[0]); p_vy = std::stof(tokens[1]);
                        p_mass = std::stof(tokens[2]); p_bouncy = std::stof(tokens[3]);
                        hasPhys = true;
                        vx = (int)p_vx; vy = (int)p_vy;
                    } catch(...) {}
                }
            } else if (tagType == "rect") {

                std::vector<std::string> rectTokens = tokenize(tagContent);
                if (rectTokens.size() >= 2) {
                    ow = std::stoi(rectTokens[0]);
                    oh = std::stoi(rectTokens[1]);
                }
            } else if (tagType == "lf") {
                line_breaks++;
            } else if (tagType == "image") {
                results.push_back({px, py, vx, vy, bIsStatic, cr, cg, cb, tagContent, 1, input, ow, oh, line_breaks, next_is_new_group, stencil_path, ttl, p_vx, p_vy, p_sx, p_sy, p_mass, p_bouncy, hasPhys, false, layer, hasHover, hr, hg, hb, hw, hasClicked, clua});
                ow = 0; oh = 0; line_breaks = 0; next_is_new_group = false; stencil_path = ""; ttl = -1;
            } else if (tagType == "video") {
                std::vector<std::string> tokens = tokenize(tagContent);
                std::string path = tagContent;
                bool noAudio = false;
                if (tokens.size() >= 2) {
                    path = tokens[0];
                    noAudio = (tokens[1] == "1");
                }
                results.push_back({px, py, vx, vy, bIsStatic, cr, cg, cb, path, 2, input, ow, oh, line_breaks, next_is_new_group, stencil_path, ttl, p_vx, p_vy, p_sx, p_sy, p_mass, p_bouncy, hasPhys, noAudio, layer, hasHover, hr, hg, hb, hw, hasClicked, clua});
                ow = 0; oh = 0; line_breaks = 0; next_is_new_group = false; stencil_path = ""; ttl = -1;
            } else if (tagType == "tvid") {
                std::vector<std::string> tokens = tokenize(tagContent);
                std::string path = tagContent;
                bool noAudio = false;
                if (tokens.size() >= 2) {
                    path = tokens[0];
                    noAudio = (tokens[1] == "1");
                }
                results.push_back({px, py, vx, vy, bIsStatic, cr, cg, cb, path, 5, input, ow, oh, line_breaks, next_is_new_group, stencil_path, ttl, p_vx, p_vy, p_sx, p_sy, p_mass, p_bouncy, hasPhys, noAudio, layer, hasHover, hr, hg, hb, hw, hasClicked, clua});
                ow = 0; oh = 0; line_breaks = 0; next_is_new_group = false; stencil_path = ""; ttl = -1;
            } else if (tagType == "plasma") {
                results.push_back({px, py, vx, vy, bIsStatic, cr, cg, cb, tagContent, 3, input, ow, oh, line_breaks, next_is_new_group, stencil_path, ttl, p_vx, p_vy, p_sx, p_sy, p_mass, p_bouncy, hasPhys, false, layer, hasHover, hr, hg, hb, hw, hasClicked, clua});
                ow = 0; oh = 0; line_breaks = 0; next_is_new_group = false; stencil_path = ""; ttl = -1;
            } else if (tagType == "fractal") {
                results.push_back({px, py, vx, vy, bIsStatic, cr, cg, cb, tagContent, 4, input, ow, oh, line_breaks, next_is_new_group, stencil_path, ttl, p_vx, p_vy, p_sx, p_sy, p_mass, p_bouncy, hasPhys, false, layer, hasHover, hr, hg, hb, hw, hasClicked, clua});
                ow = 0; oh = 0; line_breaks = 0; next_is_new_group = false; stencil_path = ""; ttl = -1;
            } else if (tagType == "usd") {
                results.push_back({px, py, vx, vy, bIsStatic, cr, cg, cb, tagContent, 6, input, ow, oh, line_breaks, next_is_new_group, stencil_path, ttl, p_vx, p_vy, p_sx, p_sy, p_mass, p_bouncy, hasPhys, false, layer, hasHover, hr, hg, hb, hw, hasClicked, clua});
                ow = 0; oh = 0; line_breaks = 0; next_is_new_group = false; stencil_path = ""; ttl = -1;
            } else if (tagType == "tusd") {
                results.push_back({px, py, vx, vy, bIsStatic, cr, cg, cb, tagContent, 7, input, ow, oh, line_breaks, next_is_new_group, stencil_path, ttl, p_vx, p_vy, p_sx, p_sy, p_mass, p_bouncy, hasPhys, false, layer, hasHover, hr, hg, hb, hw, hasClicked, clua});
                ow = 0; oh = 0; line_breaks = 0; next_is_new_group = false; stencil_path = ""; ttl = -1;
            } else if (tagType == "tscn") {
                results.push_back({px, py, vx, vy, bIsStatic, cr, cg, cb, tagContent, 8, input, ow, oh, line_breaks, next_is_new_group, stencil_path, ttl, p_vx, p_vy, p_sx, p_sy, p_mass, p_bouncy, hasPhys, false, layer, hasHover, hr, hg, hb, hw, hasClicked, clua});
                ow = 0; oh = 0; line_breaks = 0; next_is_new_group = false; stencil_path = ""; ttl = -1;
            } else if (tagType == "ttscn") {
                results.push_back({px, py, vx, vy, bIsStatic, cr, cg, cb, tagContent, 9, input, ow, oh, line_breaks, next_is_new_group, stencil_path, ttl, p_vx, p_vy, p_sx, p_sy, p_mass, p_bouncy, hasPhys, false, layer, hasHover, hr, hg, hb, hw, hasClicked, clua});
                ow = 0; oh = 0; line_breaks = 0; next_is_new_group = false; stencil_path = ""; ttl = -1;
            } else if (tagType == "hover") {
                std::vector<std::string> tokens = tokenize(tagContent);
                if (tokens.size() >= 4) {
                    hr = std::stoi(tokens[0]);
                    hg = std::stoi(tokens[1]);
                    hb = std::stoi(tokens[2]);
                    hw = std::stoi(tokens[3]);
                    hasHover = true;
                }
            } else if (tagType == "clicked") {
                clua = tagContent;
                hasClicked = true;
            }

            lastPos = matchPos + match.length();
        }

        // 3. Final trailing text
        if (lastPos < body.length()) {
            results.push_back({px, py, vx, vy, bIsStatic, cr, cg, cb, body.substr(lastPos), 0, input, 0, 0, line_breaks, next_is_new_group, stencil_path, ttl, p_vx, p_vy, p_sx, p_sy, p_mass, p_bouncy, hasPhys, false, layer, hasHover, hr, hg, hb, hw, hasClicked, clua});
        }

        return results;
    }

void processAndPrint(const std::string& input) {
    auto segments = ContentParser::parse(input);
    std::cout << "\nInput: " << input << "\n";
    for (const auto& s : segments) {
        std::printf("  Pos:(%d,%d) Velo:(%d,%d) Static:%s RGB:(%d,%d,%d) Rect:(%d,%d) | Type:%d | Content: \"%s\" Muted: %s\n",
                    s.posx, s.posy, s.velox, s.veloy,
                    s.bIsStatic ? "Y" : "N",
                    s.r, s.g, s.b,
                    s.over_w, s.over_h,
                    s.bIsFile, s.content.c_str(),
                    s.noAudio ? "Y" : "N");
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




struct AppState;

struct Bouncer {
    float x, y;
    float vx, vy;
    Uint8 r, g, b;   // random tint colour
    SDL_GPUTexture* tex; // which text texture to use (not owned — shared)
    SDL_GPUTexture* stencil_tex = nullptr;
    int tw, th;       // dimensions of that texture()
    MediaDecoder* decoder = nullptr;
    PlasmaShader* plasma = nullptr;
    MandelbrotOpenCL* mandel = nullptr;
    USDHydraRenderer* usd_renderer = nullptr;
    GodotRenderer* godot_renderer = nullptr;
    float ttl_remaining_ms = -1.0f;
    int layer = 1;
    bool bTransparent = false;

    bool hasHover = false;
    Uint8 hover_r=255, hover_g=255, hover_b=255;
    int hover_w=0;
    bool hasClicked = false;
    std::string clicked_lua;
};

static ContentParser mParser;
static PlasmaShader* myPlasma = nullptr;
static MandelbrotOpenCL* myMandel = nullptr;
static bool bUsePlasma = true;
static bool bUseMandel = false;
static bool bUseUSD = false;
static bool bUseGodot = false;

static SDL_GPUTexture* create_png_texture(Renderer* renderer,
                                        const char* text,
                                        int* out_w, int* out_h,
                                        bool isStencil = false)
{
    SDL_Surface* text_surf = IMG_Load(text);
    if (!text_surf) {
        std::printf("IMG_Load error: %s\n", SDL_GetError());
        return nullptr;
    }

    SDL_Surface* rgba_surf = SDL_ConvertSurface(text_surf, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(text_surf);
    if (!rgba_surf) return nullptr;

    if (isStencil) {
        Uint32* pixels = (Uint32*)rgba_surf->pixels;
        const SDL_PixelFormatDetails* details = SDL_GetPixelFormatDetails(rgba_surf->format);
        for (int i = 0; i < rgba_surf->w * rgba_surf->h; ++i) {
            Uint8 r, g, b, a;
            SDL_GetRGBA(pixels[i], details, NULL, &r, &g, &b, &a);
            // Combine original alpha with grayscale brightness
            Uint8 brightness = (Uint8)(0.299f * r + 0.587f * g + 0.114f * b);
            Uint8 final_alpha = (Uint8)((float)a * (float)brightness / 255.0f);
            pixels[i] = SDL_MapRGBA(details, NULL, 255, 255, 255, final_alpha);
        }
    }

    *out_w = rgba_surf->w;
    *out_h = rgba_surf->h;

    SDL_GPUTexture* texture = renderer->createAndUploadTexture(*out_w, *out_h, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, rgba_surf->pixels, rgba_surf->pitch);

    SDL_DestroySurface(rgba_surf);
    
    return texture;
}

static SDL_GPUTexture* create_text_texture(Renderer* renderer,
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

    SDL_Color fg = {255, 255, 255, 200};
    SDL_Surface* text_surf = TTF_RenderText_Blended(font, text, 0, fg);
    if (!text_surf) {
        std::printf("TTF_RenderText_Blended error: %s\n", SDL_GetError());
        TTF_CloseFont(font);
        return nullptr;
    }

    SDL_Surface* rgba_surf = SDL_ConvertSurface(text_surf, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(text_surf);
    TTF_CloseFont(font);

    if (!rgba_surf) return nullptr;

    *out_w = rgba_surf->w;
    *out_h = rgba_surf->h;

    SDL_GPUTexture* texture = renderer->createAndUploadTexture(*out_w, *out_h, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, rgba_surf->pixels, rgba_surf->pitch);

    SDL_DestroySurface(rgba_surf);
    return texture;
}

struct PropertyWatcher {
    std::string node_name;
    std::string property_name;
    Variant target_value;
    std::string callback_file;
    bool fired = false;
};

struct AppState {
    std::vector<std::string> cli_texts;
    std::string cli_record_path;
    std::string cli_bg_path;
    std::string cli_lua_path;
    std::string cli_audio_path;
    int cli_bg_plasma_idx = -1;
    int cli_bg_fractal_idx = -1;
    std::string cli_bg_usd_path;
    std::string cli_bg_tscn_path;
    int cli_record_max = -1;
    bool cli_geekd = false;
    bool cli_maximize = false;
    bool cli_plasma_tile = false;
    int cli_win_w = 0;
    int cli_win_h = 0;

    PlasmaShader* selected_plasma = nullptr;
    MandelbrotOpenCL* selected_mandel = nullptr;
    USDHydraRenderer* selected_usd = nullptr;
    GodotRenderer* selected_godot = nullptr;
    float usd_tree_height = 300.0f;

    std::vector<std::unique_ptr<class BDdisplay>> mBdisplay;

    LuaScripting* scriptSystem = nullptr;

    SDL_GPUTexture* plasma_tex = nullptr;
    SDL_GPUTexture* mandel_tex = nullptr;
    USDHydraRenderer* bg_usd = nullptr;
    GodotRenderer* bg_godot = nullptr;
    std::unique_ptr<MediaDecoder> bg_video;
    std::unique_ptr<AudioDecoder> loop_audio;
    SDL_GPUTexture* bg_tex = nullptr;
    SDL_GPUTexture* scratch_tex = nullptr;
    SDL_BlendMode SDL_BLENDMODE_STENCIL;

    std::vector<TextEntry> cli_entries;
    std::vector<Bouncer> bouncers;

    bool use_custom_text = false;
    char custom_text_buf[256];
    std::vector<SDL_GPUTexture*> extra_textures;

    int plasma_w, plasma_h;
    int prev_win_w, prev_win_h;

    class USDManager* usdManager = nullptr;
    std::vector<class Object3D*> usdObjects;

    GodotManager* godot_manager = nullptr;

    bool show_imgui = true;

    float time_acc = 0.0f;
    bool roll_palette = false;
    float roll_palette_speed = 0.5f;
    bool roll_mandel_palette = false;
    float roll_mandel_palette_speed = 0.5f;

    Recorder recorder;
    char record_path_buf[256];
    float record_time = 0.0f;
    float record_frame_accum = 0.0f;
    bool record_max_enabled = true;
    int record_max_seconds = 59;
    bool record_gui = false;

    std::vector<PropertyWatcher> watchers;
    std::string pending_lua_path;

    Uint64 last_ticks;
    Uint64 freq;
    Uint64 last_time;
    double frequency;


    struct LuaCommand {
        enum Type { ADD_BOUNCER, DEL_BOUNCER, SET_BG, SELECT_PLASMA, SELECT_FRACTAL, SELECT_USD, SELECT_GODOT, SET_PLASMA_PARAM, SET_FRACTAL_PARAM, SET_USD_PARAM, RANDOMIZE_PLASMA_PALETTE, RANDOMIZE_PLASMA_XY, RANDOMIZE_FRACTAL_PALETTE, SET_AUDIO, START_RECORD, STOP_RECORD, SET_RECORD_MAX, QUIT_APP, IMGUI_HIDE, IMGUI_SHOW, CLEAR_AND_RUN,
                    GODOT_SELECT_ROOT, GODOT_SELECT_NODE, GODOT_SEARCH_NODE, GODOT_GET_NODE_TYPE, GODOT_GET_NAME, GODOT_GET_CHILD_COUNT, GODOT_PRINT_HIERARCHY, GODOT_RENAME_NODE, GODOT_SET_CAMERA, GODOT_GET_POS, GODOT_SET_POS, GODOT_MOVE_X, GODOT_MOVE_Y, GODOT_MOVE_Z,
                    GODOT_MOVE_AND_COLLIDE, GODOT_GET_OVERLAPPING_AREAS, GODOT_CREATE_NODE, GODOT_LOAD_NODE, GODOT_DELETE_NODE,
                    GODOT_ATTACH_SCRIPT, GODOT_SET_PROPERTY, GODOT_GET_PROPERTY, WATCH_PROPERTY };
        Type type;
        std::string syntax;
        int index;
        double value;
        float fargs[3];
        std::shared_ptr<LuaSyncData> sync;
    };
    std::queue<LuaCommand> lua_commands;
    std::mutex lua_mutex;

    int event_burst_cooldown = 0;

    AppState() {
        std::memset(record_path_buf, 0, sizeof(record_path_buf));
        std::strcpy(record_path_buf, "output.mp4");
        std::memset(custom_text_buf, 0, sizeof(custom_text_buf));
    }
};

class BDdisplay {
public:
    std::vector<Bouncer> bouncers;
private:
    SDL_FRect boundingBox = {0.0f, 0.0f, 0.0f, 0.0f};   
    float groupVx = 20.0f; // Default horizontal speed
    float groupVy = 20.0f; // Default vertical speed
    float mass = 1.0f;
    float bounciness = 1.0f;
    bool hasPhysics = false;

    float initialStartX = 0.0f;
    float currentLineY = 0.0f;
    float maxLineHeight = 0.0f;

    std::string cli_input;

    bool bAmNotMoving = false;

public:
    ~BDdisplay() {
        for (auto& b : bouncers) {
            if (b.decoder) delete b.decoder;
            if (b.plasma) delete b.plasma;
            if (b.mandel) delete b.mandel;
            if (b.usd_renderer) delete b.usd_renderer;
            if (b.godot_renderer) delete b.godot_renderer;
            if (b.tex) SDL_ReleaseGPUTexture(g_renderer->getDevice(), b.tex);
            if (b.stencil_tex) SDL_ReleaseGPUTexture(g_renderer->getDevice(), b.stencil_tex);
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


    void update(float deltaTime, int windowW, int windowH, struct AppState* state) {
        if (bouncers.empty()) return;

        float dt_ms = deltaTime * 1000.0f;
        for (auto it = bouncers.begin(); it != bouncers.end(); ) {
            if (it->ttl_remaining_ms > 0) {
                it->ttl_remaining_ms -= dt_ms;
                if (it->ttl_remaining_ms <= 0) {
                    // Cleanup resources
                    if (it->plasma == state->selected_plasma) state->selected_plasma = myPlasma;
                    if (it->mandel == state->selected_mandel) state->selected_mandel = myMandel;
                    if (it->usd_renderer == state->selected_usd) state->selected_usd = nullptr;
                    if (it->godot_renderer == state->selected_godot) state->selected_godot = nullptr;

                    if (it->decoder) delete it->decoder;
                    if (it->plasma) delete it->plasma;
                    if (it->mandel) delete it->mandel;
                    if (it->usd_renderer) delete it->usd_renderer;
                    if (it->godot_renderer) delete it->godot_renderer;
                    if (it->tex) SDL_ReleaseGPUTexture(g_renderer->getDevice(), it->tex);
                    if (it->stencil_tex) SDL_ReleaseGPUTexture(g_renderer->getDevice(), it->stencil_tex);
                    it = bouncers.erase(it);
                    continue;
                }
            }
            ++it;
        }

        if (bouncers.empty()) return;

        // Update video and plasma frames
        for (auto& b : bouncers) {
            if (b.decoder && b.tex) {
                b.decoder->updateTexture(g_renderer, b.tex);
            }
            if (b.plasma && b.tex) {
                b.plasma->updateTexture(g_renderer, b.tex);
            }
            if (b.mandel && b.tex) {
                b.mandel->updateTexture(g_renderer, b.tex);
            }
            if (b.usd_renderer && b.tex) {
                std::vector<uint8_t> pixels(b.tw * b.th * 4);
                b.usd_renderer->render(pixels.data());
                g_renderer->updateTexture(b.tex, b.tw, b.th, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, pixels.data(), b.tw * 4);
            }
            if (b.godot_renderer && b.tex) {
                std::vector<uint8_t> pixels(b.tw * b.th * 4);
                b.godot_renderer->render(pixels.data());
                g_renderer->updateTexture(b.tex, b.tw, b.th, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, pixels.data(), b.tw * 4);
            }
        }

        if(bAmNotMoving) return;

        if (hasPhysics) {
            float gravity = 500.0f; // px/s^2
            float drag = 0.1f;    // base drag coefficient
            
            // Deceleration due to drag is inverse to mass: a = F/m
            float effective_drag = drag / (mass > 0.01f ? mass : 0.01f);
            
            groupVy += gravity * deltaTime;
            groupVx -= groupVx * effective_drag * deltaTime;
            groupVy -= groupVy * effective_drag * deltaTime;
        }

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
            groupVx = std::abs(groupVx) * bounciness; // Force positive
            float offset = -boundingBox.x;
            for (auto& b : bouncers) b.x += offset;
        } else if (boundingBox.x + boundingBox.w > windowW) {
            groupVx = -std::abs(groupVx) * bounciness; // Force negative
            float offset = (boundingBox.x + boundingBox.w) - windowW;
            for (auto& b : bouncers) b.x -= offset;
        }

        // Check Top/Bottom
        if (boundingBox.y < 0) {
            groupVy = std::abs(groupVy) * bounciness; // Force positive
            float offset = -boundingBox.y;
            for (auto& b : bouncers) b.y += offset;
        } else if (boundingBox.y + boundingBox.h > windowH) {
            groupVy = -std::abs(groupVy) * bounciness; // Force negative
            float offset = (boundingBox.y + boundingBox.h) - windowH;
            for (auto& b : bouncers) b.y -= offset;
        }
        
        // Final sync of bounding box after correction
        recalculateBoundingBox();
    }


    bool add(ParsedSegment pd) {
        SDL_GPUTexture* tex = NULL;
        Bouncer newB;
        newB.layer = pd.layer;

        if (pd.stencil_path != "") {
            int sw, sh;
            newB.stencil_tex = create_png_texture(g_renderer, pd.stencil_path.c_str(), &sw, &sh, true);
        }

        if (pd.bIsFile == 1) { // PNG
            tex = create_png_texture(g_renderer, pd.content.c_str(), &newB.tw, &newB.th);
        } else if (pd.bIsFile == 2 || pd.bIsFile == 5) { // Video or Tvid
            try {
                newB.bTransparent = (pd.bIsFile == 5);
                newB.decoder = new MediaDecoder(pd.content, newB.bTransparent, pd.noAudio);
                newB.tw = newB.decoder->getWidth();
                newB.th = newB.decoder->getHeight();
                // Create streaming texture for video (RGBA is preferred for SDL_UpdateTexture)
                tex = g_renderer->createTexture(newB.tw, newB.th, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
                if (tex) {
                    newB.decoder->updateTexture(g_renderer, tex); // load first frame
                }
            } catch (const std::exception& e) {
                std::printf("Video load error: %s\n", e.what());
                return false;
            }
        } else if (pd.bIsFile == 3) { // Plasma
            int p_idx = pd.content.empty() ? -1 : std::stoi(pd.content);
            newB.tw = (pd.over_w > 0) ? pd.over_w : 256;
            newB.th = (pd.over_h > 0) ? pd.over_h : 256;
            newB.plasma = new PlasmaShader(newB.tw, newB.th);
            if (newB.plasma->init(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, p_idx)) {
                tex = g_renderer->createTexture(newB.tw, newB.th, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET);
            } else {
                delete newB.plasma;
                newB.plasma = nullptr;
                return false;
            }
        } else if (pd.bIsFile == 4) { // Fractal
            int f_idx = pd.content.empty() ? -1 : std::stoi(pd.content);
            newB.tw = (pd.over_w > 0) ? pd.over_w : 256;
            newB.th = (pd.over_h > 0) ? pd.over_h : 256;
            newB.mandel = new MandelbrotOpenCL(newB.tw, newB.th);
            if (newB.mandel->init(f_idx)) {
                newB.mandel->start();
                tex = g_renderer->createTexture(newB.tw, newB.th, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM);
            } else {
                delete newB.mandel;
                newB.mandel = nullptr;
                return false;
            }
        } else if (pd.bIsFile == 6 || pd.bIsFile == 7) { // USD or Transparent USD
            newB.tw = (pd.over_w > 0) ? pd.over_w : 512;
            newB.th = (pd.over_h > 0) ? pd.over_h : 512;
            newB.usd_renderer = new USDHydraRenderer(newB.tw, newB.th);
            if (pd.bIsFile == 7) {
                newB.usd_renderer->backgroundTransparency = true;
            }
            if (newB.usd_renderer->init(pd.content)) {
                tex = g_renderer->createTexture(newB.tw, newB.th, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, SDL_GPU_TEXTUREUSAGE_SAMPLER);
            } else {
                delete newB.usd_renderer;
                newB.usd_renderer = nullptr;
                return false;
            }
        } else if (pd.bIsFile == 8 || pd.bIsFile == 9) { // Godot tscn or ttscn
            newB.tw = (pd.over_w > 0) ? pd.over_w : 512;
            newB.th = (pd.over_h > 0) ? pd.over_h : 512;
            bool transparent = (pd.bIsFile == 9);
            newB.godot_renderer = new GodotRenderer(newB.tw, newB.th, transparent);
            if (newB.godot_renderer->init(pd.content)) {
                tex = g_renderer->createTexture(newB.tw, newB.th, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, SDL_GPU_TEXTUREUSAGE_SAMPLER);
            } else {
                delete newB.godot_renderer;
                newB.godot_renderer = nullptr;
                return false;
            }
        } else { // Text (pd.bIsFile == 0)
            tex = create_text_texture(g_renderer, pd.content.c_str(), &newB.tw, &newB.th);
        }

        if(tex == NULL) return false;

        // Apply overrides if provided
        if (pd.over_w > 0) newB.tw = pd.over_w;
        if (pd.over_h > 0) newB.th = pd.over_h;

        bAmNotMoving = pd.bIsStatic;

        if (pd.hasPhys) {
            groupVx = pd.phys_vx;
            groupVy = pd.phys_vy;
            mass = pd.mass;
            bounciness = pd.bouncy;
            hasPhysics = true;
        }

        newB.vx = pd.velox;
        newB.vy = pd.veloy;
        newB.r = pd.r;
        newB.g = pd.g;
        newB.b = pd.b;
        newB.tex = tex;
        newB.ttl_remaining_ms = (float)pd.ttl_ms;

        newB.hasHover = pd.hasHover;
        newB.hover_r = pd.hover_r;
        newB.hover_g = pd.hover_g;
        newB.hover_b = pd.hover_b;
        newB.hover_w = pd.hover_w;
        newB.hasClicked = pd.hasClicked;
        newB.clicked_lua = pd.clicked_lua;

        if (bouncers.empty()) {            // Initial spawn point
            newB.x = pd.posx;
            newB.y = pd.posy;
            initialStartX = pd.posx;
            currentLineY = pd.posy;
            maxLineHeight = (float)newB.th;
            groupVx = pd.velox;
            groupVy = pd.veloy;
            cli_input = pd.fullInput;
        } else {
            if (pd.line_breaks > 0) {
                // Reset X and move Y down
                newB.x = initialStartX;
                currentLineY += maxLineHeight * pd.line_breaks;
                newB.y = currentLineY;
                maxLineHeight = (float)newB.th; // Reset line height for new line
            } else {
                // Continue on current line
                newB.x = boundingBox.x + boundingBox.w;
                // Align to the middle of the current line's tallest element (approx)
                newB.y = currentLineY + (maxLineHeight / 2.0f) - (newB.th / 2.0f);
                maxLineHeight = std::max(maxLineHeight, (float)newB.th);
            }
        }

        bouncers.push_back(newB);
        
        recalculateBoundingBox();
        return true;
    }

    std::string getInput(){
        return cli_input;
    }


    void draw(Renderer* renderer, int target_layer) {
        int mx, my;
        InputManager::getInstance().lua_getMousePos(mx, my);

        for (auto& b : bouncers) {
            if (b.layer != target_layer) continue;
            SDL_FRect dst = { b.x, b.y, static_cast<float>(b.tw), static_cast<float>(b.th) };

            renderer->drawBouncer(b.tex, dst, b.r, b.g, b.b, 255, b.stencil_tex, b.bTransparent);

            if (b.hasHover) {
                if (mx >= b.x && mx <= b.x + b.tw && my >= b.y && my <= b.y + b.th) {
                    float w = (float)b.hover_w;
                    // Draw 4 rectangles for the frame
                    renderer->drawRect({b.x - w, b.y - w, (float)b.tw + 2*w, w}, b.hover_r, b.hover_g, b.hover_b, 255); // Top
                    renderer->drawRect({b.x - w, b.y + b.th, (float)b.tw + 2*w, w}, b.hover_r, b.hover_g, b.hover_b, 255); // Bottom
                    renderer->drawRect({b.x - w, b.y, w, (float)b.th}, b.hover_r, b.hover_g, b.hover_b, 255); // Left
                    renderer->drawRect({b.x + b.tw, b.y, w, (float)b.th}, b.hover_r, b.hover_g, b.hover_b, 255); // Right
                }
            }
        }
    }
    const SDL_FRect& getBounds() const {
        return boundingBox;
    }
};



// Spawn a new bouncer with random position & velocity
static Bouncer make_bouncer(int win_w, int win_h, SDL_GPUTexture* tex, int tw, int th, bool bNoColor=false) {
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


    
CLPlasmaParams plasma_params;
CLMandelbrotParams mandel_params;

static void print_help() {
    std::printf("SDL3 + Dear ImGui Animated Backgrounds and Text Overlay\n");
    std::printf("Usage: ./gcg [options] [text...]\n\n");
    std::printf("CLI Options:\n");
    std::printf("  --record FILE         start recording frames to FILE on launch\n");
    std::printf("  --lua FILE            run Lua script on launch\n");
    std::printf("  --audio FILE          play audio file on loop\n");
    std::printf("  --bg FILE             use image or video as background\n");
    std::printf("  --bg \"[plasma:#]\"     use specific plasma index (#) as background\n");
    std::printf("  --bg \"[fractal:#]\"    use specific fractal index (#) as background\n");
    std::printf("  --bg \"[tscn:FILE]\"     use Godot scene as background\n");
    std::printf("  --record-max N        max recording length in seconds (default 59)\n");
    std::printf("  --maximize            start the window maximized\n");
    std::printf("  --geekd               show tech info / status line and record GUI\n");
    std::printf("  --w N                 set window width (forces non-maximized)\n");
    std::printf("  --h N                 set window height (forces non-maximized)\n");
    std::printf("  --plasma-tiles        render plasma in a tiled grid (for stress testing)\n");
    std::printf("  --help                show this help message\n\n");
    
    std::printf("Lua Scripting Functions:\n");
    std::printf("  addBouncer(syntax)         Adds a bouncer group (e.g. \"[plasma:1] Hello\")\n");
    std::printf("  delBouncer(index)          Removes a bouncer group by index\n");
    std::printf("  setBG(path_or_tag)         Sets background to file, [plasma:#], [fractal:#], or [tscn:FILE]\n");
    std::printf("  selectPlasma(index)        Selects plasma instance (-1=BG, 0+=bouncer)\n");
    std::printf("  selectFractal(index)       Selects fractal instance (-1=BG, 0+=bouncer)\n");
    std::printf("  selectUSD(index)           Selects USD instance (0+=bouncer)\n");
    std::printf("  selectGodot(index)         Selects Godot instance (-1=BG, 0+=bouncer)\n");
    std::printf("  setPlasmaParam(name, val)  Sets parameter on selected plasma\n");
    std::printf("  setFractalParam(name, val) Sets parameter on selected fractal\n");
    std::printf("  setUSDParam(name, val)     Sets parameter on selected USD\n");
    std::printf("  randomizePlasmaPalette()   Randomizes selected plasma colors\n");
    std::printf("  randomizePlasmaXY()        Randomizes selected plasma motion/scale\n");
    std::printf("  randomizeFractalPalette()  Randomizes selected fractal colors\n");
    std::printf("  setAudio(path)             Sets and loops background audio file\n");
    std::printf("  startRecord(path)          Starts video recording to path\n");
    std::printf("  stopRecord(wait)           Stops recording (wait=1 to wait for max-time)\n");
    std::printf("  setRecordMax(seconds)      Sets auto-stop duration for recording\n");
    std::printf("  delay(ms)                  Pauses script for ms milliseconds\n");
    std::printf("  appQuit()                  Closes the application\n");
    std::printf("  luaClearAndRun(file)       Deletes all bouncers and runs Lua script\n");
    std::printf("  imGuiHide()                Hides the ImGui overlay\n");
    std::printf("  imGuiShow()                Shows the ImGui overlay\n\n");

    std::printf("Input Framework Functions:\n");
    std::printf("  ioKBClicked(key)           Returns true if key (\"SDLK_...\") was clicked\n");
    std::printf("  ioKBDown(key)              Returns true if key is held down\n");
    std::printf("  ioKBUp(key)                Returns true if key was released\n");
    std::printf("  ioMousePos()               Returns x, y mouse coordinates\n");
    std::printf("  ioMouseMoved()             Returns true if mouse moved since last call\n");
    std::printf("  ioMouseGetMotion()         Returns rx, ry relative movement\n");
    std::printf("  ioMouseBTNClicked(btn)     Returns true if mouse button (1-3) clicked\n");
    std::printf("  ioMouseBTNDown(btn)        Returns true if mouse button is down\n");
    std::printf("  ioMouseBTNUp(btn)          Returns true if mouse button was released\n\n");

    std::printf("Godot Manipulation Functions:\n");
    std::printf("  godotSelectRoot()          Selects scene root\n");
    std::printf("  godotSelectNode(name)      Selects direct child node (returns success)\n");
    std::printf("  godotSearchNode(name)      Searches tree for node (returns success)\n");
    std::printf("  godotGetNodeType()         Returns type string of selected node\n");
    std::printf("  godotSetCamera()           Sets selected node as active camera\n");
    std::printf("  godotGetPos()              Returns x, y, z of selected node\n");
    std::printf("  godotSetPos(x, y, z)       Sets position of selected node\n");
    std::printf("  godotMoveX(v), godotMoveY, godotMoveZ  Relative movement\n");
    std::printf("  godotMoveAndCollide(x, y, z)  Move PhysicsBody and return collision\n");
    std::printf("  godotGetOverlappingAreas()  Returns list of overlapping Area3D names\n");
    std::printf("  godotCreateNode(name)      Creates Node3D as child of current node\n");
    std::printf("  godotLoadNode(path)        Instances scene as child of current node\n");
    std::printf("  godotDeleteNode()          Deletes selected node and selects parent\n");
    std::printf("  godotAttachScript(path)    Attaches GDScript to selected node\n");
    std::printf("  godotSetProperty(name, v)  Sets property/variable on selected node\n");
    std::printf("  godotGetProperty(name)     Gets property/variable from selected node\n");
    std::printf("  godotWatchProperty(node, prop, target, file) Runs file when prop == target\n\n");

    std::printf("Supported Plasma Parameters (for setPlasmaParam):\n");
    std::printf("  drift_amp, drift_speed_x, drift_speed_y, rot_speed,\n");
    std::printf("  scale_base_x, scale_base_y, scale_mod_amp, scale_mod_speed_x,\n");
    std::printf("  scale_mod_speed_y, warp_base, warp_amp, warp_speed, swirl,\n");
    std::printf("  darken_r, darken_g, darken_b, tile_count, roll_palette, roll_speed\n\n");

    std::printf("Supported Fractal Parameters (for setFractalParam):\n");
    std::printf("  x_offset, y_offset, zoom, max_iterations, color_speed,\n");
    std::printf("  palette_phase_r, palette_phase_g, palette_phase_b,\n");
    std::printf("  transparency (bands), roll_palette, roll_speed\n\n");

    std::printf("Supported USD Parameters (for setUSDParam):\n");
    std::printf("  rot_x, rot_y, rot_z, dist, camera (-1=Free Cam)\n\n");

    std::printf("Overlay Tag Syntax:\n");
    std::printf("  [pos:x,y,vx,vy]            Position and velocity\n");
    std::printf("  [rect:w,h]                 Texture dimensions\n");
    std::printf("  [rgb:r,g,b]                Tint color (0-255)\n");
    std::printf("  [image:file]               Render image file (e.g. .png, .jpg)\n");
    std::printf("  [video:file,no_audio]      Render video file (optional no_audio=1)\n");
    std::printf("  [tvid:file,no_audio]       Render video with transparency (optional no_audio=1)\n");
    std::printf("  [plasma:idx]               Render plasma #\n");
    std::printf("  [fractal:idx]              Render fractal #\n");
    std::printf("  [usd:file.usd]             Render USD file using Hydra\n");
    std::printf("  [tusd:file.usd]            Render USD file with transparent background\n");
    std::printf("  [stencil:file.png]         Apply alpha mask\n");
    std::printf("  [ttl:ms]                   Self-destruct timer\n");
    std::printf("  [phys:vx,vy,sx,sy,m,b]     Advanced physics and spawn point\n");
    std::printf("  [layer:#]                  Rendering layer (0=Foreground, 1=Middle, 2=Background)\n");
    std::printf("  [hover:r,g,b,w]            Draw RGB frame of width W when hovered\n");
    std::printf("  [clicked:file.lua]         Run Lua script when clicked\n");
}

static void randomise_mandel_palette(CLMandelbrotParams& p) {
    p.palette_phase_r = rand_range(0.0f, 1.0f);
    p.palette_phase_g = rand_range(0.0f, 1.0f);
    p.palette_phase_b = rand_range(0.0f, 1.0f);
}

static CLPlasmaParams randomise_plasma() {
    CLPlasmaParams p;
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
    p.darken_r          = 1.0f; // rand_range(0.25f, 0.60f);
    p.darken_g          = 1.0f; // rand_range(0.25f, 0.60f);
    p.darken_b          = 1.0f; // rand_range(0.25f, 0.60f);

    p.tile_count        = rand_range(10.0f, 100.0f);

    return p;
}

// Re-randomise only the palette (colour) fields
static void randomise_plasma_palette(CLPlasmaParams& p) {
    p.palette_phase_r = rand_range(0.0f, 1.0f);
    p.palette_phase_g = rand_range(0.0f, 1.0f);
    p.palette_phase_b = rand_range(0.0f, 1.0f);
    p.darken_r        = 1.0f; // rand_range(0.25f, 0.60f);
    p.darken_g        = 1.0f; // rand_range(0.25f, 0.60f);
    p.darken_b        = 1.0f; // rand_range(0.25f, 0.60f);
}

// Re-randomise only the X/Y spatial / animation fields
static void randomise_plasma_xy(CLPlasmaParams& p) {
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


//------------
SDL_AppResult SDL_AppInit(void **appstate, int argc, char **argv)
{
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--help") == 0) {
            print_help();
            return SDL_APP_SUCCESS;
        }
    }

    AppState* state = new AppState();
    *appstate = state;

    state->godot_manager = new GodotManager();
    const char* godot_args[] = {"gcg", "--offscreen", "--rendering-driver", "vulkan"};
    state->godot_manager->init(4, (char**)godot_args);

    myMix = new AudioMixer(MIXER_SAMPLE_RATE);

    // --- Parse CLI arguments ---
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--record") == 0 && i + 1 < argc) {
            state->cli_record_path = argv[++i];
        } else if (std::strcmp(argv[i], "--lua") == 0 && i + 1 < argc) {
            state->cli_lua_path = argv[++i];
        } else if (std::strcmp(argv[i], "--audio") == 0 && i + 1 < argc) {
            state->cli_audio_path = argv[++i];
        } else if (std::strcmp(argv[i], "--bg") == 0 && i + 1 < argc) {
            std::string arg = argv[++i];
            if (arg == "mandelbrot") {
                bUseMandel = true;
                bUsePlasma = false;
            } else if (arg.size() > 8 && arg.substr(0, 8) == "[plasma:" && arg.back() == ']') {
                std::string idx_str = arg.substr(8, arg.size() - 9);
                try {
                    state->cli_bg_plasma_idx = std::stoi(idx_str);
                    bUsePlasma = true;
                    bUseMandel = false;
                    bUseGodot = false;
                } catch (...) {
                    state->cli_bg_path = arg;
                    bUsePlasma = false;
                    bUseMandel = false;
                    bUseGodot = false;
                }
            } else if (arg.size() > 9 && arg.substr(0, 9) == "[fractal:" && arg.back() == ']') {
                std::string idx_str = arg.substr(9, arg.size() - 10);
                try {
                    state->cli_bg_fractal_idx = std::stoi(idx_str);
                    bUseMandel = true;
                    bUsePlasma = false;
                    bUseUSD = false;
                    bUseGodot = false;
                } catch (...) {
                    state->cli_bg_path = arg;
                    bUsePlasma = false;
                    bUseMandel = false;
                    bUseUSD = false;
                    bUseGodot = false;
                }
            } else if (arg.size() > 5 && arg.substr(0, 5) == "[usd:" && arg.back() == ']') {
                state->cli_bg_usd_path = arg.substr(5, arg.size() - 6);
                bUseUSD = true;
                bUsePlasma = false;
                bUseMandel = false;
                bUseGodot = false;
            } else if (arg.size() > 6 && arg.substr(0, 6) == "[tscn:" && arg.back() == ']') {
                state->cli_bg_tscn_path = arg.substr(6, arg.size() - 7);
                bUseGodot = true;
                bUsePlasma = false;
                bUseMandel = false;
                bUseUSD = false;
            } else {
                state->cli_bg_path = arg;
                bUsePlasma = false;
                bUseMandel = false;
                bUseUSD = false;
                bUseGodot = false;
            }
        } else if (std::strcmp(argv[i], "--record-max") == 0 && i + 1 < argc) {
            state->cli_record_max = std::atoi(argv[++i]);
            if (state->cli_record_max < 1) state->cli_record_max = 1;
        } else if (std::strcmp(argv[i], "--maximize") == 0) {
            state->cli_maximize = true;
        } else if (std::strcmp(argv[i], "--w") == 0 && i + 1 < argc) {
            state->cli_win_w = std::atoi(argv[++i]);
            state->cli_maximize = false;
        } else if (std::strcmp(argv[i], "--h") == 0 && i + 1 < argc) {
            state->cli_win_h = std::atoi(argv[++i]);
            state->cli_maximize = false;
        } else if (std::strcmp(argv[i], "--geekd") == 0) {
            state->cli_geekd = true;
            state->record_gui = true;
        } else if (std::strcmp(argv[i], "--plasma-tiles") == 0) {
            state->cli_plasma_tile = true;       
        } else {
            state->cli_texts.push_back(argv[i]);
        }
    }
    
    for (const auto& t : state->cli_texts)
        std::printf("Overlay text: \"%s\"\n", t.c_str());
    if (!state->cli_record_path.empty())
        std::printf("Will record to: %s\n", state->cli_record_path.c_str());

    // --- SDL init ---
    
    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "1");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::printf("SDL_Init error: %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    float scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());

    cur_w = (state->cli_win_w > 0) ? state->cli_win_w : static_cast<int>(1024 * scale);
    cur_h = (state->cli_win_h > 0) ? state->cli_win_h : static_cast<int>(768 * scale);
    cur_rel = (float)cur_w / (float)cur_h;

    SDL_WindowFlags win_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (state->cli_maximize)
        win_flags |= SDL_WINDOW_MAXIMIZED;

    window = SDL_CreateWindow(
        "SDL/ImGui Greeting Card",
        cur_w, cur_h,
        win_flags
    );
    if (!window) {
        std::printf("SDL_CreateWindow error: %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    g_renderer = new Renderer();
    if (!g_renderer->init(window)) {
        std::printf("Failed to init Renderer\n");
        return SDL_APP_FAILURE;
    }
    std::printf("Active Renderer: SDL3 GPU API\n");

    // --- Plasma texture ---
    state->plasma_w = cur_w / 2;
    state->plasma_h = cur_h / 2;
    SDL_Log("Texture dimensions: %d x %d", state->plasma_w, state->plasma_h);
    if (bUsePlasma) {
        state->plasma_tex = g_renderer->createTexture(state->plasma_w, state->plasma_h, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM, SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET);
    }
    if (bUseMandel) {
        SDL_Log("Creating Mandelbrot texture...");
        state->mandel_tex = g_renderer->createTexture(cur_w, cur_h, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM);
    }
    if (bUseUSD) {
        SDL_Log("Creating USD Background...");
        state->bg_usd = new USDHydraRenderer(cur_w, cur_h);
        if (state->bg_usd->init(state->cli_bg_usd_path)) {
            state->selected_usd = state->bg_usd;
        } else {
            SDL_Log("USD background initialization failed!");
            delete state->bg_usd;
            state->bg_usd = nullptr;
            bUseUSD = false;
        }
        state->bg_tex = g_renderer->createTexture(cur_w, cur_h, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
    }
    if (bUseGodot) {
        SDL_Log("Creating Godot Background...");
        state->bg_godot = new GodotRenderer(cur_w, cur_h);
        if (state->bg_godot->init(state->cli_bg_tscn_path)) {
            state->selected_godot = state->bg_godot;
        } else {
            SDL_Log("Godot background initialization failed!");
            delete state->bg_godot;
            state->bg_godot = nullptr;
            bUseGodot = false;
        }
        state->bg_tex = g_renderer->createTexture(cur_w, cur_h, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
    }

    // --- Custom Background layer ---
    if (!state->cli_bg_path.empty()) {
        std::string ext = state->cli_bg_path;
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        bool is_video = (ext.find(".mp4") != std::string::npos || 
                         ext.find(".mkv") != std::string::npos || 
                         ext.find(".mov") != std::string::npos ||
                         ext.find(".avi") != std::string::npos);
        
        if (is_video) {
            try {
                state->bg_video = std::make_unique<MediaDecoder>(state->cli_bg_path);
                state->bg_tex = g_renderer->createTexture(
                    state->bg_video->getWidth(), state->bg_video->getHeight(), SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);            } catch (const std::exception& e) {
                std::printf("BG Video error: %s\n", e.what());
            }
        } else {
            int bw, bh;
            state->bg_tex = create_png_texture(g_renderer, state->cli_bg_path.c_str(), &bw, &bh);
        }
    }

    // --- Pre-render CLI text ---
    for (const auto& t : state->cli_texts) {
        mParser.processAndPrint(t);
        auto segments = mParser.parse(t);
        
        BDdisplay* currentGroup = nullptr;
        for (auto& seg : segments) {
            if (seg.start_new_group || currentGroup == nullptr) {
                auto newBD = std::make_unique<BDdisplay>();
                currentGroup = newBD.get();
                state->mBdisplay.push_back(std::move(newBD));
            }
            currentGroup->add(seg);
        }
    }

    std::srand(static_cast<unsigned>(SDL_GetPerformanceCounter()));
    plasma_params = randomise_plasma();
    state->prev_win_w = cur_w;
    state->prev_win_h = cur_h;

    // --- ImGui init ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(scale);
    style.FontScaleDpi = scale;
    style.Colors[ImGuiCol_WindowBg].w = 0.80f;

    ImGui_ImplSDL3_InitForOther(window);
    
    ImGui_ImplSDLGPU3_InitInfo init_info = {};
    init_info.Device = g_renderer->getDevice();
    init_info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(g_renderer->getDevice(), window);
    init_info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
    ImGui_ImplSDLGPU3_Init(&init_info);

    if (state->cli_plasma_tile) plasma_render_tiles = true;
    if (state->cli_record_max > 0) {
        state->record_max_seconds = state->cli_record_max;
        state->record_max_enabled = true;
    }

    if (!state->cli_record_path.empty()) {
        std::snprintf(state->record_path_buf, sizeof(state->record_path_buf), "%s", state->cli_record_path.c_str());
        int out_w = 0, out_h = 0;
        SDL_GetWindowSize(window, &out_w, &out_h);
        recorder_start(state->recorder, out_w, out_h, state->cli_record_path.c_str());
    }

    state->SDL_BLENDMODE_STENCIL = SDL_ComposeCustomBlendMode(SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_SRC_COLOR, SDL_BLENDOPERATION_ADD, SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_SRC_ALPHA, SDL_BLENDOPERATION_ADD);
    // scratch_tex removed

    state->last_ticks = SDL_GetPerformanceCounter();
    state->freq       = SDL_GetPerformanceFrequency();

    state->last_time = SDL_GetPerformanceCounter();
    state->frequency = (double)SDL_GetPerformanceFrequency();

    if (bUsePlasma) {
        myPlasma = new PlasmaShader(state->plasma_w, state->plasma_h);
        myPlasma->init(SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM, state->cli_bg_plasma_idx);
        myPlasma->setArgs(plasma_params);
        state->selected_plasma = myPlasma;
    }
    if (bUseMandel) {
        myMandel = new MandelbrotOpenCL(cur_w, cur_h);
        if (myMandel->init(state->cli_bg_fractal_idx)) {
            myMandel->setArgs(mandel_params);
            myMandel->start();
            state->selected_mandel = myMandel;
        } else {
            SDL_Log("Mandelbrot initialization failed!");
        }
    }

    if (!state->cli_audio_path.empty()) {
        try {
            state->loop_audio = std::make_unique<AudioDecoder>(state->cli_audio_path, myMix);
        } catch (const std::exception& e) {
            SDL_Log("Audio error: %s", e.what());
        }
    }

    if (!state->cli_lua_path.empty()) {
        state->scriptSystem = new LuaScripting(
            [state](const std::string& syntax) {
                std::lock_guard<std::mutex> lock(state->lua_mutex);
                state->lua_commands.push({AppState::LuaCommand::ADD_BOUNCER, syntax, 0});
            },
            [state](int index) {
                std::lock_guard<std::mutex> lock(state->lua_mutex);
                state->lua_commands.push({AppState::LuaCommand::DEL_BOUNCER, "", index});
            },
            [state](const std::string& bg) {
                std::lock_guard<std::mutex> lock(state->lua_mutex);
                state->lua_commands.push({AppState::LuaCommand::SET_BG, bg, 0, 0.0});
            },
            [state](bool isPlasma, int index, std::shared_ptr<LuaSyncData> sync_data) {
                std::lock_guard<std::mutex> lock(state->lua_mutex);
                state->lua_commands.push({isPlasma ? AppState::LuaCommand::SELECT_PLASMA : AppState::LuaCommand::SELECT_FRACTAL, "", index, 0.0, {0,0,0}, sync_data});
            },
            [state](bool isPlasma, const std::string& name, double value) {
                std::lock_guard<std::mutex> lock(state->lua_mutex);
                state->lua_commands.push({isPlasma ? AppState::LuaCommand::SET_PLASMA_PARAM : AppState::LuaCommand::SET_FRACTAL_PARAM, name, 0, value});
            },
            [state](bool isPlasma, bool isXY) {
                std::lock_guard<std::mutex> lock(state->lua_mutex);
                AppState::LuaCommand::Type t;
                if (isPlasma) t = isXY ? AppState::LuaCommand::RANDOMIZE_PLASMA_XY : AppState::LuaCommand::RANDOMIZE_PLASMA_PALETTE;
                else t = AppState::LuaCommand::RANDOMIZE_FRACTAL_PALETTE;
                state->lua_commands.push({t, "", 0, 0.0});
            },
            [state](const std::string& path) {
                std::lock_guard<std::mutex> lock(state->lua_mutex);
                state->lua_commands.push({AppState::LuaCommand::SET_AUDIO, path, 0, 0.0});
            },
            [state](int type, const std::string& path, int val) {
                std::lock_guard<std::mutex> lock(state->lua_mutex);
                AppState::LuaCommand::Type t;
                if (type == 0) t = AppState::LuaCommand::START_RECORD;
                else if (type == 1) t = AppState::LuaCommand::STOP_RECORD;
                else t = AppState::LuaCommand::SET_RECORD_MAX;
                state->lua_commands.push({t, path, 0, (double)val});
            },
            []() {
                return (myNvec != nullptr);
            },
            [state](int index, std::shared_ptr<LuaSyncData> sync_data) {
               std::lock_guard<std::mutex> lock(state->lua_mutex);
               state->lua_commands.push({AppState::LuaCommand::SELECT_USD, "", index, 0.0, {0,0,0}, sync_data});
            },
            [state](int index, std::shared_ptr<LuaSyncData> sync_data) {
               std::lock_guard<std::mutex> lock(state->lua_mutex);
               state->lua_commands.push({AppState::LuaCommand::SELECT_GODOT, "", index, 0.0, {0,0,0}, sync_data});
            },
            [state](const std::string& name, double value) {
                std::lock_guard<std::mutex> lock(state->lua_mutex);
                state->lua_commands.push({AppState::LuaCommand::SET_USD_PARAM, name, 0, value});
            },
            [state](LuaScripting::GodotCmd gcmd, const std::string& str_arg, float f_args[3], std::shared_ptr<LuaSyncData> sync_data) {
                std::lock_guard<std::mutex> lock(state->lua_mutex);
                AppState::LuaCommand cmd;
                cmd.syntax = str_arg;
                if (f_args) { cmd.fargs[0] = f_args[0]; cmd.fargs[1] = f_args[1]; cmd.fargs[2] = f_args[2]; }
                cmd.sync = sync_data;
                switch (gcmd) {
                    case LuaScripting::GCMD_SELECT_ROOT: cmd.type = AppState::LuaCommand::GODOT_SELECT_ROOT; break;
                    case LuaScripting::GCMD_SELECT_NODE: cmd.type = AppState::LuaCommand::GODOT_SELECT_NODE; break;
                    case LuaScripting::GCMD_SEARCH_NODE: cmd.type = AppState::LuaCommand::GODOT_SEARCH_NODE; break;
                    case LuaScripting::GCMD_GET_NODE_TYPE: cmd.type = AppState::LuaCommand::GODOT_GET_NODE_TYPE; break;
                    case LuaScripting::GCMD_GET_NAME: cmd.type = AppState::LuaCommand::GODOT_GET_NAME; break;
                    case LuaScripting::GCMD_GET_CHILD_COUNT: cmd.type = AppState::LuaCommand::GODOT_GET_CHILD_COUNT; break;
                    case LuaScripting::GCMD_PRINT_HIERARCHY: cmd.type = AppState::LuaCommand::GODOT_PRINT_HIERARCHY; break;
                    case LuaScripting::GCMD_RENAME_NODE: cmd.type = AppState::LuaCommand::GODOT_RENAME_NODE; break;
                    case LuaScripting::GCMD_SET_CAMERA: cmd.type = AppState::LuaCommand::GODOT_SET_CAMERA; break;
                    case LuaScripting::GCMD_GET_POS: cmd.type = AppState::LuaCommand::GODOT_GET_POS; break;
                    case LuaScripting::GCMD_SET_POS: cmd.type = AppState::LuaCommand::GODOT_SET_POS; break;
                    case LuaScripting::GCMD_MOVE_X: cmd.type = AppState::LuaCommand::GODOT_MOVE_X; break;
                    case LuaScripting::GCMD_MOVE_Y: cmd.type = AppState::LuaCommand::GODOT_MOVE_Y; break;
                    case LuaScripting::GCMD_MOVE_Z: cmd.type = AppState::LuaCommand::GODOT_MOVE_Z; break;
                    case LuaScripting::GCMD_MOVE_AND_COLLIDE: cmd.type = AppState::LuaCommand::GODOT_MOVE_AND_COLLIDE; break;
                    case LuaScripting::GCMD_GET_OVERLAPPING_AREAS: cmd.type = AppState::LuaCommand::GODOT_GET_OVERLAPPING_AREAS; break;
                    case LuaScripting::GCMD_CREATE_NODE: cmd.type = AppState::LuaCommand::GODOT_CREATE_NODE; break;
                    case LuaScripting::GCMD_LOAD_NODE: cmd.type = AppState::LuaCommand::GODOT_LOAD_NODE; break;
                    case LuaScripting::GCMD_DELETE_NODE: cmd.type = AppState::LuaCommand::GODOT_DELETE_NODE; break;
                    case LuaScripting::GCMD_ATTACH_SCRIPT: cmd.type = AppState::LuaCommand::GODOT_ATTACH_SCRIPT; break;
                    case LuaScripting::GCMD_SET_PROPERTY: cmd.type = AppState::LuaCommand::GODOT_SET_PROPERTY; break;
                    case LuaScripting::GCMD_GET_PROPERTY: cmd.type = AppState::LuaCommand::GODOT_GET_PROPERTY; break;
                    case LuaScripting::GCMD_WATCH_PROPERTY: cmd.type = AppState::LuaCommand::WATCH_PROPERTY; break;
                    default: return;
                }
                state->lua_commands.push(cmd);
            },
            [state](std::shared_ptr<LuaSyncData> sync_data) {
                std::lock_guard<std::mutex> lock(state->lua_mutex);
                state->lua_commands.push({AppState::LuaCommand::QUIT_APP, "", 0, 0.0, {0,0,0}, sync_data});
            },
            [state](bool visible) {
                std::lock_guard<std::mutex> lock(state->lua_mutex);
                state->lua_commands.push({visible ? AppState::LuaCommand::IMGUI_SHOW : AppState::LuaCommand::IMGUI_HIDE, "", 0, 0.0});
            },
            [state](const std::string& filename, std::shared_ptr<LuaSyncData> sync_data) {
                std::lock_guard<std::mutex> lock(state->lua_mutex);
                state->lua_commands.push({AppState::LuaCommand::CLEAR_AND_RUN, filename, 0, 0.0, {0,0,0}, sync_data});
            }
        );
        state->scriptSystem->runScript(state->cli_lua_path);
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *ev)
{
    AppState* state = (AppState*)appstate;
    state->event_burst_cooldown = 10;

    ImGui_ImplSDL3_ProcessEvent(ev);
    InputManager::getInstance().processEvent(ev);

    ImGuiIO& io = ImGui::GetIO();
    bool captured = false;
    if (io.WantCaptureKeyboard && (ev->type >= SDL_EVENT_KEY_DOWN && ev->type <= SDL_EVENT_KEY_UP)) captured = true;
    if (io.WantCaptureMouse && (ev->type >= SDL_EVENT_MOUSE_MOTION && ev->type <= SDL_EVENT_MOUSE_BUTTON_UP)) captured = true;

    if (!captured) {
        if (ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN && ev->button.button == SDL_BUTTON_LEFT) {
            float mx = ev->button.x;
            float my = ev->button.y;
            for (auto& bd : state->mBdisplay) {
                for (auto& b : bd->bouncers) {
                    if (b.hasClicked) {
                        if (mx >= b.x && mx <= b.x + b.tw && my >= b.y && my <= b.y + b.th) {
                            state->scriptSystem->runOneShotScript(b.clicked_lua);
                        }
                    }
                }
            }
        }
    }

    if (ev->type == SDL_EVENT_QUIT) return SDL_APP_SUCCESS;    if (ev->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && ev->window.windowID == SDL_GetWindowID(window))
        return SDL_APP_SUCCESS;
    
    if (ev->type == SDL_EVENT_WINDOW_RESIZED) {
        cur_w = ev->window.data1;
        cur_h = ev->window.data2;
        cur_rel = (float)cur_w / (float)cur_h;

        if (cur_w != state->prev_win_w || cur_h != state->prev_win_h) {
            if (bUsePlasma && state->plasma_tex) {
                SDL_ReleaseGPUTexture(g_renderer->getDevice(), state->plasma_tex);
                state->plasma_w = cur_w / 8;
                state->plasma_h = cur_h / 8;
                if (state->plasma_w < 1) state->plasma_w = 1;
                if (state->plasma_h < 1) state->plasma_h = 1;
                state->plasma_tex = g_renderer->createTexture(state->plasma_w, state->plasma_h, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM, SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET);
                myPlasma->resize(state->plasma_w, state->plasma_h);
                myPlasma->setArgs(plasma_params);
            }
            if (bUseUSD && state->bg_usd) {
                state->bg_usd->resize(cur_w, cur_h);
                if (state->bg_tex) SDL_ReleaseGPUTexture(g_renderer->getDevice(), state->bg_tex);
                state->bg_tex = g_renderer->createTexture(cur_w, cur_h, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
            }
            if (bUseGodot && state->bg_godot) {
                state->bg_godot->resize(cur_w, cur_h);
                if (state->bg_tex) SDL_ReleaseGPUTexture(g_renderer->getDevice(), state->bg_tex);
                state->bg_tex = g_renderer->createTexture(cur_w, cur_h, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
            }
            state->prev_win_w = cur_w;
            state->prev_win_h = cur_h;
        }

    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    InputManager::getInstance().beginFrame();
    AppState* state = (AppState*)appstate;
    if (state->godot_manager) {
        state->godot_manager->iteration();
    }

    // Process Lua Commands
    {
        std::lock_guard<std::mutex> lock(state->lua_mutex);
        while (!state->lua_commands.empty()) {
            auto cmd = state->lua_commands.front();
            state->lua_commands.pop();
            if (cmd.type == AppState::LuaCommand::ADD_BOUNCER) {
                auto segments = mParser.parse(cmd.syntax);
                BDdisplay* currentGroup = nullptr;
                for (auto& seg : segments) {
                    if (seg.start_new_group || currentGroup == nullptr) {
                        auto newBD = std::make_unique<BDdisplay>();
                        currentGroup = newBD.get();
                        state->mBdisplay.push_back(std::move(newBD));
                    }
                    currentGroup->add(seg);
                }
            } else if (cmd.type == AppState::LuaCommand::DEL_BOUNCER) {
                if (cmd.index >= 0 && cmd.index < (int)state->mBdisplay.size()) {
                    for (auto& b : state->mBdisplay[cmd.index]->bouncers) {
                        if (state->selected_plasma == b.plasma) state->selected_plasma = myPlasma;
                        if (state->selected_mandel == b.mandel) state->selected_mandel = myMandel;
                        if (state->selected_usd == b.usd_renderer) state->selected_usd = nullptr;
                        if (state->selected_godot == b.godot_renderer) state->selected_godot = nullptr;
                    }
                    state->mBdisplay.erase(state->mBdisplay.begin() + cmd.index);
                }
            } else if (cmd.type == AppState::LuaCommand::SET_BG) {
                std::string arg = cmd.syntax;

                // Clear EVERYTHING first to ensure a clean state
                if (state->bg_tex) { SDL_ReleaseGPUTexture(g_renderer->getDevice(), state->bg_tex); state->bg_tex = nullptr; }
                state->bg_video.reset();
                if (state->plasma_tex) { SDL_ReleaseGPUTexture(g_renderer->getDevice(), state->plasma_tex); state->plasma_tex = nullptr; }
                if (state->mandel_tex) { SDL_ReleaseGPUTexture(g_renderer->getDevice(), state->mandel_tex); state->mandel_tex = nullptr; }
                if (state->bg_usd) {
                    if (state->selected_usd == state->bg_usd) state->selected_usd = nullptr;
                    delete state->bg_usd; state->bg_usd = nullptr; bUseUSD = false;
                }
                if (state->bg_godot) {
                    if (state->selected_godot == state->bg_godot) state->selected_godot = nullptr;
                    delete state->bg_godot; state->bg_godot = nullptr; bUseGodot = false;
                }
                if (myPlasma) { 
                    if (state->selected_plasma == myPlasma) state->selected_plasma = nullptr;
                    delete myPlasma; myPlasma = nullptr; bUsePlasma = false; 
                }
                if (myMandel) { 
                    if (state->selected_mandel == myMandel) state->selected_mandel = nullptr;
                    delete myMandel; myMandel = nullptr; bUseMandel = false; 
                }
                state->cli_bg_path = "";
                state->cli_bg_usd_path = "";
                state->cli_bg_tscn_path = "";

                if (arg.size() > 8 && arg.substr(0, 8) == "[plasma:" && arg.back() == ']') {
                    std::string idx_str = arg.substr(8, arg.size() - 9);
                    try {
                        state->cli_bg_plasma_idx = std::stoi(idx_str);
                        bUsePlasma = true;
                    } catch (...) {}
                } else if (arg.size() > 9 && arg.substr(0, 9) == "[fractal:" && arg.back() == ']') {
                    std::string idx_str = arg.substr(9, arg.size() - 10);
                    try {
                        state->cli_bg_fractal_idx = std::stoi(idx_str);
                        bUseMandel = true;
                    } catch (...) {}
                } else if (arg.size() > 5 && arg.substr(0, 5) == "[usd:" && arg.back() == ']') {
                    state->cli_bg_usd_path = arg.substr(5, arg.size() - 6);
                    bUseUSD = true;
                } else if (arg.size() > 6 && arg.substr(0, 6) == "[tscn:" && arg.back() == ']') {
                    state->cli_bg_tscn_path = arg.substr(6, arg.size() - 7);
                    bUseGodot = true;
                } else {
                    state->cli_bg_path = arg;
                }

                if (bUsePlasma) {
                    myPlasma = new PlasmaShader(state->plasma_w, state->plasma_h);
                    myPlasma->init(SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM, state->cli_bg_plasma_idx);
                    myPlasma->setArgs(plasma_params);
                    state->selected_plasma = myPlasma;
                    state->plasma_tex = g_renderer->createTexture(state->plasma_w, state->plasma_h, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM, SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET);
                }
                if (bUseMandel) {
                    myMandel = new MandelbrotOpenCL(cur_w, cur_h);
                    if (myMandel->init(state->cli_bg_fractal_idx)) {
                        myMandel->setArgs(mandel_params);
                        myMandel->start();
                        state->selected_mandel = myMandel;
                    }
                    state->mandel_tex = g_renderer->createTexture(cur_w, cur_h, SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM);
                }
                if (bUseUSD) {
                    state->bg_usd = new USDHydraRenderer(cur_w, cur_h);
                    if (state->bg_usd->init(state->cli_bg_usd_path)) {
                        state->selected_usd = state->bg_usd;
                    } else {
                        delete state->bg_usd; state->bg_usd = nullptr; bUseUSD = false;
                    }
                    state->bg_tex = g_renderer->createTexture(cur_w, cur_h, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
                }
                if (bUseGodot) {
                    state->bg_godot = new GodotRenderer(cur_w, cur_h);
                    if (state->bg_godot->init(state->cli_bg_tscn_path)) {
                        state->selected_godot = state->bg_godot;
                    } else {
                        delete state->bg_godot; state->bg_godot = nullptr; bUseGodot = false;
                    }
                    state->bg_tex = g_renderer->createTexture(cur_w, cur_h, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
                }
                if (!state->cli_bg_path.empty()) {
                    std::string ext = state->cli_bg_path;
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    bool is_video = (ext.find(".mp4") != std::string::npos ||
                                     ext.find(".mkv") != std::string::npos ||
                                     ext.find(".mov") != std::string::npos ||
                                     ext.find(".avi") != std::string::npos);
                    if (is_video) {
                        try {
                            state->bg_video = std::make_unique<MediaDecoder>(state->cli_bg_path);
                            state->bg_tex = g_renderer->createTexture(
                                state->bg_video->getWidth(), state->bg_video->getHeight(), SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);                        } catch (const std::exception& e) {
                            std::printf("BG Video error (Lua): %s\n", e.what());
                        }
                    } else {
                        int bw, bh;
                        state->bg_tex = create_png_texture(g_renderer, state->cli_bg_path.c_str(), &bw, &bh);
                    }
                }
            } else if (cmd.type == AppState::LuaCommand::SELECT_PLASMA) {
                if (cmd.index == -1) {
                    state->selected_plasma = myPlasma;
                } else {
                    int count = 0;
                    for (auto& bd : state->mBdisplay) {
                        for (auto& b : bd->bouncers) {
                            if (b.plasma) {
                                if (count == cmd.index) { state->selected_plasma = b.plasma; goto found_p; }
                                count++;
                            }
                        }
                    }
                    found_p:;
                }
                if (cmd.sync) {
                    std::lock_guard<std::mutex> lock(cmd.sync->mtx);
                    cmd.sync->done = true;
                    cmd.sync->cv.notify_one();
                }
            } else if (cmd.type == AppState::LuaCommand::SELECT_FRACTAL) {
                if (cmd.index == -1) {
                    state->selected_mandel = myMandel;
                } else {
                    int count = 0;
                    for (auto& bd : state->mBdisplay) {
                        for (auto& b : bd->bouncers) {
                            if (b.mandel) {
                                if (count == cmd.index) { state->selected_mandel = b.mandel; goto found_f; }
                                count++;
                            }
                        }
                    }
                    found_f:;
                }
                if (cmd.sync) {
                    std::lock_guard<std::mutex> lock(cmd.sync->mtx);
                    cmd.sync->done = true;
                    cmd.sync->cv.notify_one();
                }
            } else if (cmd.type == AppState::LuaCommand::SELECT_USD) {
                if (cmd.index == -1) {
                    state->selected_usd = state->bg_usd;
                } else {
                    int count = 0;
                    for (auto& bd : state->mBdisplay) {
                        for (auto& b : bd->bouncers) {
                            if (b.usd_renderer) {
                                if (count == cmd.index) { state->selected_usd = b.usd_renderer; goto found_usd; }
                                count++;
                            }
                        }
                    }
                }
                found_usd:;
                if (cmd.sync) {
                    std::lock_guard<std::mutex> lock(cmd.sync->mtx);
                    cmd.sync->done = true;
                    cmd.sync->cv.notify_one();
                }
            } else if (cmd.type == AppState::LuaCommand::SELECT_GODOT) {
                if (cmd.index == -1) {
                    state->selected_godot = state->bg_godot;
                } else {
                    int count = 0;
                    for (auto& bd : state->mBdisplay) {
                        for (auto& b : bd->bouncers) {
                            if (b.godot_renderer) {
                                if (count == cmd.index) { state->selected_godot = b.godot_renderer; goto found_godot; }
                                count++;
                            }
                        }
                    }
                }
                found_godot:;
                if (cmd.sync) {
                    std::lock_guard<std::mutex> lock(cmd.sync->mtx);
                    cmd.sync->done = true;
                    cmd.sync->cv.notify_one();
                }
            } else if (cmd.type >= AppState::LuaCommand::GODOT_SELECT_ROOT && cmd.type <= AppState::LuaCommand::WATCH_PROPERTY) {
                if (state->selected_godot) {
                    switch (cmd.type) {
                        case AppState::LuaCommand::GODOT_SELECT_ROOT: state->selected_godot->selectRoot(); break;
                        case AppState::LuaCommand::GODOT_SELECT_NODE: if (cmd.sync) cmd.sync->b_res = state->selected_godot->selectNode(cmd.syntax); break;
                        case AppState::LuaCommand::GODOT_SEARCH_NODE: if (cmd.sync) cmd.sync->b_res = state->selected_godot->searchNode(cmd.syntax); break;
                        case AppState::LuaCommand::GODOT_GET_NODE_TYPE: if (cmd.sync) cmd.sync->s_res = state->selected_godot->getNodeType(); break;
                        case AppState::LuaCommand::GODOT_GET_NAME: if (cmd.sync) cmd.sync->s_res = state->selected_godot->getName(); break;
                        case AppState::LuaCommand::GODOT_GET_CHILD_COUNT: if (cmd.sync) { cmd.sync->b_res = true; cmd.sync->d_res = (double)state->selected_godot->getChildCount(); } break;
                        case AppState::LuaCommand::GODOT_PRINT_HIERARCHY: state->selected_godot->printHierarchy(); break;
                        case AppState::LuaCommand::GODOT_RENAME_NODE: state->selected_godot->renameNode(cmd.syntax); break;
                        case AppState::LuaCommand::GODOT_SET_CAMERA: if (cmd.sync) cmd.sync->b_res = state->selected_godot->setCamera(); break;
                        case AppState::LuaCommand::GODOT_GET_POS: if (cmd.sync) cmd.sync->b_res = state->selected_godot->getPos(cmd.sync->f_res[0], cmd.sync->f_res[1], cmd.sync->f_res[2]); break;
                        case AppState::LuaCommand::GODOT_SET_POS: state->selected_godot->setPos(cmd.fargs[0], cmd.fargs[1], cmd.fargs[2]); break;
                        case AppState::LuaCommand::GODOT_MOVE_X: state->selected_godot->move(cmd.fargs[0], 0, 0); break;
                        case AppState::LuaCommand::GODOT_MOVE_Y: state->selected_godot->move(0, cmd.fargs[1], 0); break;
                        case AppState::LuaCommand::GODOT_MOVE_Z: state->selected_godot->move(0, 0, cmd.fargs[2]); break;
                        case AppState::LuaCommand::GODOT_MOVE_AND_COLLIDE: if (cmd.sync) cmd.sync->b_res = state->selected_godot->moveAndCollide(cmd.fargs[0], cmd.fargs[1], cmd.fargs[2]); break;
                        case AppState::LuaCommand::GODOT_GET_OVERLAPPING_AREAS: if (cmd.sync) cmd.sync->vs_res = state->selected_godot->getOverlappingAreas(); break;
                        case AppState::LuaCommand::GODOT_CREATE_NODE: if (cmd.sync) cmd.sync->b_res = state->selected_godot->createNode(cmd.syntax); break;
                        case AppState::LuaCommand::GODOT_LOAD_NODE: 
                            if (cmd.sync) {
                                cmd.sync->b_res = state->selected_godot->loadNode(cmd.syntax, cmd.fargs[0], cmd.fargs[1], cmd.fargs[2], cmd.sync->b_res);
                            }
                            break;
                        case AppState::LuaCommand::GODOT_DELETE_NODE: state->selected_godot->deleteNode(); break;
                        case AppState::LuaCommand::GODOT_ATTACH_SCRIPT: if (cmd.sync) cmd.sync->b_res = state->selected_godot->attachScript(cmd.syntax); break;
                        case AppState::LuaCommand::GODOT_SET_PROPERTY: 
                            if (cmd.fargs[1] == 1.0f) {
                                size_t sep = cmd.syntax.find('|');
                                if (sep != std::string::npos) {
                                    std::string prop = cmd.syntax.substr(0, sep);
                                    std::string val = cmd.syntax.substr(sep + 1);
                                    state->selected_godot->setProperty(prop, Variant(val.c_str()));
                                }
                            } else {
                                state->selected_godot->setProperty(cmd.syntax, Variant(cmd.fargs[0]));
                            }
                            break;
                        case AppState::LuaCommand::GODOT_GET_PROPERTY:
                            if (cmd.sync) {
                                Variant v = state->selected_godot->getProperty(cmd.syntax);
                                if (v.get_type() == Variant::INT || v.get_type() == Variant::FLOAT) {
                                    cmd.sync->b_res = true;
                                    cmd.sync->d_res = (double)v;
                                } else {
                                    cmd.sync->b_res = false;
                                    cmd.sync->s_res = String(v).utf8().get_data();
                                }
                            }
                            break;
                        case AppState::LuaCommand::WATCH_PROPERTY:
                            {
                                // node|prop|file|val
                                std::string s = cmd.syntax;
                                size_t p1 = s.find('|');
                                size_t p2 = s.find('|', p1+1);
                                size_t p3 = s.find('|', p2+1);
                                if (p1 != std::string::npos && p2 != std::string::npos && p3 != std::string::npos) {
                                    PropertyWatcher w;
                                    w.node_name = s.substr(0, p1);
                                    w.property_name = s.substr(p1+1, p2-p1-1);
                                    w.callback_file = s.substr(p2+1, p3-p2-1);
                                    std::string val_str = s.substr(p3+1);
                                    
                                    if (cmd.fargs[1] == 0.0f) w.target_value = Variant(cmd.fargs[0]);
                                    else if (cmd.fargs[1] == 1.0f) w.target_value = Variant(val_str.c_str());
                                    else if (cmd.fargs[1] == 2.0f) w.target_value = Variant(cmd.fargs[0] > 0.5f);
                                    
                                    state->watchers.push_back(w);
                                }
                            }
                            break;
                    }
                }
                if (cmd.sync) {
                    std::lock_guard<std::mutex> lock(cmd.sync->mtx);
                    cmd.sync->done = true;
                    cmd.sync->cv.notify_one();
                }
            } else if (cmd.type == AppState::LuaCommand::QUIT_APP) {
                if (cmd.sync) {
                    std::lock_guard<std::mutex> lock(cmd.sync->mtx);
                    cmd.sync->done = true;
                    cmd.sync->cv.notify_one();
                }
                return SDL_APP_SUCCESS;
            } else if (cmd.type == AppState::LuaCommand::IMGUI_HIDE) {
                state->show_imgui = false;
            } else if (cmd.type == AppState::LuaCommand::IMGUI_SHOW) {
                state->show_imgui = true;
            } else if (cmd.type == AppState::LuaCommand::CLEAR_AND_RUN) {
                state->pending_lua_path = cmd.syntax;
                if (cmd.sync) {
                    std::lock_guard<std::mutex> lock(cmd.sync->mtx);
                    cmd.sync->done = true;
                    cmd.sync->cv.notify_one();
                }
            } else if (cmd.type == AppState::LuaCommand::SET_USD_PARAM) {
                if (state->selected_usd) {
                    if (cmd.syntax == "rot_x") state->selected_usd->sceneRotation[0] = (float)cmd.value;
                    else if (cmd.syntax == "rot_y") state->selected_usd->sceneRotation[1] = (float)cmd.value;
                    else if (cmd.syntax == "rot_z") state->selected_usd->sceneRotation[2] = (float)cmd.value;
                    else if (cmd.syntax == "dist") state->selected_usd->cameraDistance = (float)cmd.value;
                    else if (cmd.syntax == "camera") state->selected_usd->setCameraByIndex((int)cmd.value);
                }
            } else if (cmd.type == AppState::LuaCommand::SET_PLASMA_PARAM) {
                if (state->selected_plasma) {
                    CLPlasmaParams p = state->selected_plasma->getArgs();
                    if (cmd.syntax == "drift_amp") p.drift_amp = (float)cmd.value;
                    else if (cmd.syntax == "drift_speed_x") p.drift_speed_x = (float)cmd.value;
                    else if (cmd.syntax == "drift_speed_y") p.drift_speed_y = (float)cmd.value;
                    else if (cmd.syntax == "rot_speed") p.rot_speed = (float)cmd.value;
                    else if (cmd.syntax == "scale_base_x") p.scale_base_x = (float)cmd.value;
                    else if (cmd.syntax == "scale_base_y") p.scale_base_y = (float)cmd.value;
                    else if (cmd.syntax == "palette_phase_r") p.palette_phase_r = (float)cmd.value;
                    else if (cmd.syntax == "palette_phase_g") p.palette_phase_g = (float)cmd.value;
                    else if (cmd.syntax == "palette_phase_b") p.palette_phase_b = (float)cmd.value;
                    else if (cmd.syntax == "scale_mod_amp") p.scale_mod_amp = (float)cmd.value;
                    else if (cmd.syntax == "scale_mod_speed_x") p.scale_mod_speed_x = (float)cmd.value;
                    else if (cmd.syntax == "scale_mod_speed_y") p.scale_mod_speed_y = (float)cmd.value;
                    else if (cmd.syntax == "warp_base") p.warp_base = (float)cmd.value;
                    else if (cmd.syntax == "warp_amp") p.warp_amp = (float)cmd.value;
                    else if (cmd.syntax == "warp_speed") p.warp_speed = (float)cmd.value;
                    else if (cmd.syntax == "swirl") p.swirl_dist_mul = (float)cmd.value;
                    else if (cmd.syntax == "darken_r") p.darken_r = (float)cmd.value;
                    else if (cmd.syntax == "darken_g") p.darken_g = (float)cmd.value;
                    else if (cmd.syntax == "darken_b") p.darken_b = (float)cmd.value;
                    else if (cmd.syntax == "tile_count") p.tile_count = (float)cmd.value;
                    else if (cmd.syntax == "roll_palette") state->roll_palette = (cmd.value > 0.5);
                    else if (cmd.syntax == "roll_speed") state->roll_palette_speed = (float)cmd.value;
                    state->selected_plasma->setArgs(p);
                    if (state->selected_plasma == myPlasma) plasma_params = p;
                }
            } else if (cmd.type == AppState::LuaCommand::SET_FRACTAL_PARAM) {
                if (state->selected_mandel) {
                    CLMandelbrotParams p = state->selected_mandel->getArgs();
                    if (cmd.syntax == "x_offset") p.x_offset = cmd.value;
                    else if (cmd.syntax == "y_offset") p.y_offset = cmd.value;
                    else if (cmd.syntax == "zoom") p.zoom = cmd.value;
                    else if (cmd.syntax == "max_iterations") p.max_iterations = (int)cmd.value;
                    else if (cmd.syntax == "palette_phase_r") p.palette_phase_r = (float)cmd.value;
                    else if (cmd.syntax == "palette_phase_g") p.palette_phase_g = (float)cmd.value;
                    else if (cmd.syntax == "palette_phase_b") p.palette_phase_b = (float)cmd.value;
                    else if (cmd.syntax == "color_speed") p.color_speed = (float)cmd.value;
                    else if (cmd.syntax == "transparency") p.transparency = (float)cmd.value;
                    else if (cmd.syntax == "roll_palette") state->roll_mandel_palette = (cmd.value > 0.5);
                    else if (cmd.syntax == "roll_speed") state->roll_mandel_palette_speed = (float)cmd.value;
                    state->selected_mandel->setArgs(p);
                    if (state->selected_mandel == myMandel) mandel_params = p;
                }
            } else if (cmd.type == AppState::LuaCommand::RANDOMIZE_PLASMA_PALETTE) {
                if (state->selected_plasma) {
                    CLPlasmaParams p = state->selected_plasma->getArgs();
                    randomise_plasma_palette(p);
                    state->selected_plasma->setArgs(p);
                    if (state->selected_plasma == myPlasma) plasma_params = p;
                }
            } else if (cmd.type == AppState::LuaCommand::RANDOMIZE_PLASMA_XY) {
                if (state->selected_plasma) {
                    CLPlasmaParams p = state->selected_plasma->getArgs();
                    randomise_plasma_xy(p);
                    state->selected_plasma->setArgs(p);
                    if (state->selected_plasma == myPlasma) plasma_params = p;
                }
            } else if (cmd.type == AppState::LuaCommand::RANDOMIZE_FRACTAL_PALETTE) {
                if (state->selected_mandel) {
                    CLMandelbrotParams p = state->selected_mandel->getArgs();
                    randomise_mandel_palette(p);
                    state->selected_mandel->setArgs(p);
                    if (state->selected_mandel == myMandel) mandel_params = p;
                }
            } else if (cmd.type == AppState::LuaCommand::SET_AUDIO) {
                state->cli_audio_path = cmd.syntax;
                state->loop_audio.reset();
                if (!state->cli_audio_path.empty()) {
                    try {
                        state->loop_audio = std::make_unique<AudioDecoder>(state->cli_audio_path, myMix);
                    } catch (const std::exception& e) {
                        SDL_Log("Audio error (Lua): %s", e.what());
                    }
                }
            } else if (cmd.type == AppState::LuaCommand::START_RECORD) {
                if (myNvec == NULL) {
                    std::snprintf(state->record_path_buf, sizeof(state->record_path_buf), "%s", cmd.syntax.c_str());
                    int out_w = 0, out_h = 0;
                    SDL_GetWindowSize(window, &out_w, &out_h);
                    recorder_start(state->recorder, out_w, out_h, state->record_path_buf);
                    state->record_time = 0.0f;
                }
            } else if (cmd.type == AppState::LuaCommand::STOP_RECORD) {
                if (myNvec != NULL) {
                    recorder_stop(state->recorder);
                }
            } else if (cmd.type == AppState::LuaCommand::SET_RECORD_MAX) {
                state->record_max_seconds = (int)cmd.value;
                state->record_max_enabled = (state->record_max_seconds > 0);
            }
        }
    }

    ImGuiIO& io = ImGui::GetIO();

    Uint64 current_time = SDL_GetPerformanceCounter();

    Uint64 now = SDL_GetPerformanceCounter();

    double delta_time = (double)(current_time - state->last_time) / state->frequency;
    state->last_time = current_time;

    float dt = static_cast<float>(now - state->last_ticks) / static_cast<float>(state->freq);
    state->last_ticks = now;
    state->time_acc += dt * 1.5f;

    //if (state->event_burst_cooldown > 0) state->event_burst_cooldown--;

    if (state->roll_palette && state->selected_plasma) {
        float step = state->roll_palette_speed * dt;
        CLPlasmaParams p = state->selected_plasma->getArgs();
        p.palette_phase_r = std::fmod(p.palette_phase_r + step, 2.0f);
        p.palette_phase_g = std::fmod(p.palette_phase_g + step * 0.7f, 2.0f);
        p.palette_phase_b = std::fmod(p.palette_phase_b + step * 1.3f, 2.0f);
        if (!state->cli_plasma_tile) p.tile_count = 0.0f;
        state->selected_plasma->setArgs(p);
        if (state->selected_plasma == myPlasma) plasma_params = p;
    }

    if (state->roll_mandel_palette && state->selected_mandel) {
        float step = state->roll_mandel_palette_speed * dt;
        CLMandelbrotParams p = state->selected_mandel->getArgs();
        p.palette_phase_r = std::fmod(p.palette_phase_r + step, 1.0f);
        p.palette_phase_g = std::fmod(p.palette_phase_g + step * 0.7f, 1.0f);
        p.palette_phase_b = std::fmod(p.palette_phase_b + step * 1.3f, 1.0f);
        state->selected_mandel->setArgs(p);
        if (state->selected_mandel == myMandel) mandel_params = p;
    }

    if (state->show_imgui) {
        ImGui_ImplSDLGPU3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Bouncers")) {
            ImGui::Text("Bouncer Texts (%d):", static_cast<int>(state->mBdisplay.size()));
            int del_text_idx = -1;
            for (int ti = 0; ti < static_cast<int>(state->mBdisplay.size()); ++ti) {
                ImGui::PushID(ti);
                if (ImGui::SmallButton("X")) del_text_idx = ti;
                ImGui::SameLine();
                ImGui::BulletText("\"%s\"", state->mBdisplay[ti]->getInput().c_str());
                ImGui::PopID();
            }
            if (del_text_idx >= 0) {
                for (auto& b : state->mBdisplay[del_text_idx]->bouncers) {
                    if (state->selected_plasma == b.plasma) state->selected_plasma = myPlasma;
                    if (state->selected_mandel == b.mandel) state->selected_mandel = myMandel;
                    if (state->selected_usd == b.usd_renderer) state->selected_usd = nullptr;
                    if (state->selected_godot == b.godot_renderer) state->selected_godot = nullptr;
                }
                state->mBdisplay.erase(state->mBdisplay.begin() + del_text_idx);
            }
            
            ImGui::Separator();
            ImGui::Checkbox("Custom Text", &state->use_custom_text);
            if (state->use_custom_text) {
                ImGui::SetNextItemWidth(200.0f);
                ImGui::InputText("##custom", state->custom_text_buf, sizeof(state->custom_text_buf));
            }
            if (ImGui::MenuItem("Add Bouncer")) {
                if (state->use_custom_text && state->custom_text_buf[0] != '\0') {
                    auto segments = mParser.parse(state->custom_text_buf);
                    BDdisplay* currentGroup = nullptr;
                    for (auto& seg : segments) {
                        if (seg.start_new_group || currentGroup == nullptr) {
                            auto newBD = std::make_unique<BDdisplay>();
                            currentGroup = newBD.get();
                            state->mBdisplay.push_back(std::move(newBD));
                        }
                        currentGroup->add(seg);
                    }
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Plasma")) {
            if (ImGui::BeginMenu("Select Plasma")) {
                if (myPlasma) {
                    if (ImGui::MenuItem("Background", NULL, state->selected_plasma == myPlasma))
                        state->selected_plasma = myPlasma;
                }
                for (size_t i = 0; i < state->mBdisplay.size(); ++i) {
                    for (size_t j = 0; j < state->mBdisplay[i]->bouncers.size(); ++j) {
                        PlasmaShader* p = state->mBdisplay[i]->bouncers[j].plasma;
                        if (p) {
                            char label[64];
                            std::snprintf(label, sizeof(label), "Bouncer %zu:%zu", i, j);
                            if (ImGui::MenuItem(label, NULL, state->selected_plasma == p))
                                state->selected_plasma = p;
                        }
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();

            if (state->selected_plasma) {
                CLPlasmaParams p = state->selected_plasma->getArgs();
                bool changed = false;

                if (ImGui::MenuItem("Randomise Palette")) { randomise_plasma_palette(p); changed = true; }
                if (ImGui::MenuItem("Randomise X/Y")) { randomise_plasma_xy(p); changed = true; }
                
                ImGui::Separator();
                changed |= ImGui::SliderFloat("Drift Amp", &p.drift_amp, 0.0f, 5.0f);
                changed |= ImGui::SliderFloat("Drift Speed X", &p.drift_speed_x, 0.0f, 2.0f);
                changed |= ImGui::SliderFloat("Drift Speed Y", &p.drift_speed_y, 0.0f, 2.0f);
                changed |= ImGui::SliderFloat("Rot Speed", &p.rot_speed, -1.0f, 1.0f);
                changed |= ImGui::SliderFloat("Scale X", &p.scale_base_x, 0.1f, 30.0f);
                changed |= ImGui::SliderFloat("Scale Y", &p.scale_base_y, 0.1f, 30.0f);
                changed |= ImGui::SliderFloat("Zoom", &p.zoom, 0.1f, 10.0f);
                
                ImGui::Separator();
                changed |= ImGui::SliderFloat("Phase R", &p.palette_phase_r, 0.0f, 1.0f);
                changed |= ImGui::SliderFloat("Phase G", &p.palette_phase_g, 0.0f, 1.0f);
                changed |= ImGui::SliderFloat("Phase B", &p.palette_phase_b, 0.0f, 1.0f);

                ImGui::Separator();
                changed |= ImGui::SliderFloat("Scale Mod Amp", &p.scale_mod_amp, 0.0f, 5.0f);
                changed |= ImGui::SliderFloat("Scale Mod Speed X", &p.scale_mod_speed_x, 0.0f, 2.0f);
                changed |= ImGui::SliderFloat("Scale Mod Speed Y", &p.scale_mod_speed_y, 0.0f, 2.0f);
                changed |= ImGui::SliderFloat("Warp Base", &p.warp_base, 0.0f, 1.0f);
                changed |= ImGui::SliderFloat("Warp Amp", &p.warp_amp, 0.0f, 1.0f);
                changed |= ImGui::SliderFloat("Warp Speed", &p.warp_speed, 0.0f, 2.0f);
                changed |= ImGui::SliderFloat("Swirl", &p.swirl_dist_mul, 0.0f, 15.0f);
                changed |= ImGui::SliderFloat("Smooth Noise", &p.noise_smooth, 0.0f, 2.0f);
                changed |= ImGui::SliderFloat("Rough Noise", &p.noise_rough, 0.0f, 1.0f);

                ImGui::Separator();
                changed |= ImGui::SliderFloat("Darken R", &p.darken_r, 0.0f, 2.0f);
                changed |= ImGui::SliderFloat("Darken G", &p.darken_g, 0.0f, 2.0f);
                changed |= ImGui::SliderFloat("Darken B", &p.darken_b, 0.0f, 2.0f);

                ImGui::Separator();
                bool tiling = (p.tile_count > 0.0f);
                if (ImGui::Checkbox("Tiling", &tiling)) {
                    if (tiling) p.tile_count = 20.0f;
                    else p.tile_count = 0.0f;
                    changed = true;
                }
                if (tiling) {
                    changed |= ImGui::SliderFloat("Tile Count", &p.tile_count, 1.0f, 100.0f);
                }

                if (changed) {
                    state->selected_plasma->setArgs(p);
                    if (state->selected_plasma == myPlasma) plasma_params = p;
                }
            }

            ImGui::Separator();
            ImGui::Checkbox("Roll Palette", &state->roll_palette);
            if (state->roll_palette) ImGui::SliderFloat("Roll Speed", &state->roll_palette_speed, 0.05f, 3.0f);

            if (state->selected_plasma) {
                ImGui::Separator();
                int current_idx = state->selected_plasma->iPlasmaIDX;
                if (ImGui::SliderInt("Plasma Type", &current_idx, 0, 13)) {
                    CLPlasmaParams current_p = state->selected_plasma->getArgs();
                    state->selected_plasma->init(state->selected_plasma->getTargetFormat(), current_idx);
                    state->selected_plasma->setArgs(current_p);
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Fractal")) {
            if (ImGui::BeginMenu("Select Fractal")) {
                if (myMandel) {
                    if (ImGui::MenuItem("Background", NULL, state->selected_mandel == myMandel))
                        state->selected_mandel = myMandel;
                }
                for (size_t i = 0; i < state->mBdisplay.size(); ++i) {
                    for (size_t j = 0; j < state->mBdisplay[i]->bouncers.size(); ++j) {
                        MandelbrotOpenCL* m = state->mBdisplay[i]->bouncers[j].mandel;
                        if (m) {
                            char label[64];
                            std::snprintf(label, sizeof(label), "Bouncer %zu:%zu", i, j);
                            if (ImGui::MenuItem(label, NULL, state->selected_mandel == m))
                                state->selected_mandel = m;
                        }
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();

            if (state->selected_mandel) {
                CLMandelbrotParams p = state->selected_mandel->getArgs();
                bool changed = false;

                if (ImGui::MenuItem("Randomise Palette")) { randomise_mandel_palette(p); changed = true; }
                ImGui::Separator();

                changed |= ImGui::InputDouble("X Offset", &p.x_offset, 0.001, 0.01, "%.10f");
                changed |= ImGui::InputDouble("Y Offset", &p.y_offset, 0.001, 0.01, "%.10f");
                changed |= ImGui::InputDouble("Zoom", &p.zoom, 0.1, 1.0, "%.10f");
                changed |= ImGui::SliderInt("Max Iterations", &p.max_iterations, 16, 2048);
                
                ImGui::Separator();
                changed |= ImGui::SliderFloat("Phase R", &p.palette_phase_r, 0.0f, 1.0f);
                changed |= ImGui::SliderFloat("Phase G", &p.palette_phase_g, 0.0f, 1.0f);
                changed |= ImGui::SliderFloat("Phase B", &p.palette_phase_b, 0.0f, 1.0f);
                changed |= ImGui::SliderFloat("Color Speed", &p.color_speed, 0.0f, 10.0f);

                changed |= ImGui::SliderFloat("Trans. Bands", &p.transparency, 0.0f, 50.0f);

                if (changed) {
                    state->selected_mandel->setArgs(p);
                    if (state->selected_mandel == myMandel) mandel_params = p;
                }

                ImGui::Separator();
                ImGui::Checkbox("Roll Palette", &state->roll_mandel_palette);
                if (state->roll_mandel_palette) ImGui::SliderFloat("Roll Speed", &state->roll_mandel_palette_speed, 0.05f, 3.0f);

                ImGui::Separator();
                ImGui::Text("Fractal Mode");
                for (int i = 0; i < 6; i++) {
                    char label[32];
                    std::snprintf(label, sizeof(label), "F%d", i);
                    if (ImGui::RadioButton(label, state->selected_mandel->iFractalIDX == i)) {
                        state->selected_mandel->stop();
                        CLMandelbrotParams current_p = state->selected_mandel->getArgs();
                        state->selected_mandel->init(i);
                        state->selected_mandel->setArgs(current_p);
                        state->selected_mandel->start();
                    }
                    if ((i + 1) % 4 != 0) ImGui::SameLine();
                }
            } else {
                ImGui::Text("No fractal selected.");
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("USD")) {
            if (ImGui::BeginMenu("Select USD Bouncer")) {
                if (state->bg_usd) {
                    if (ImGui::MenuItem("Background", NULL, state->selected_usd == state->bg_usd))
                        state->selected_usd = state->bg_usd;
                }
                for (size_t i = 0; i < state->mBdisplay.size(); ++i) {
                    for (size_t j = 0; j < state->mBdisplay[i]->bouncers.size(); ++j) {
                        USDHydraRenderer* u = state->mBdisplay[i]->bouncers[j].usd_renderer;
                        if (u) {
                            char label[64];
                            std::snprintf(label, sizeof(label), "Bouncer %zu:%zu", i, j);
                            if (ImGui::MenuItem(label, NULL, state->selected_usd == u))
                                state->selected_usd = u;
                        }
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();

            if (state->selected_usd) {
                if (ImGui::Checkbox("Free Camera", &state->selected_usd->freeCamera)) {
                    if (state->selected_usd->freeCamera) {
                        state->selected_usd->setActiveCamera(SdfPath());
                    }
                }
                
                ImGui::BeginDisabled(!state->selected_usd->freeCamera);
                ImGui::SliderFloat("Rot X", &state->selected_usd->sceneRotation[0], 0.0f, 360.0f);
                ImGui::SliderFloat("Rot Y", &state->selected_usd->sceneRotation[1], 0.0f, 360.0f);
                ImGui::SliderFloat("Rot Z", &state->selected_usd->sceneRotation[2], 0.0f, 360.0f);
                ImGui::SliderFloat("Distance", &state->selected_usd->cameraDistance, 0.1f, 100.0f);
                ImGui::EndDisabled();

                ImGui::Separator();

                UsdStageRefPtr stage = state->selected_usd->getStage();
                if (stage) {
                    ImGui::Text("USD Hierarchy:");
                    renderUSDTree(stage->GetPseudoRoot(), state->selected_usd);
                } else {
                    ImGui::Text("No stage loaded.");
                }
            } else {
                ImGui::Text("No USD bouncer selected.");
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Godot")) {
            if (ImGui::BeginMenu("Select Godot Bouncer")) {
                if (state->bg_godot) {
                    if (ImGui::MenuItem("Background", NULL, state->selected_godot == state->bg_godot))
                        state->selected_godot = state->bg_godot;
                }
                for (size_t i = 0; i < state->mBdisplay.size(); ++i) {
                    for (size_t j = 0; j < state->mBdisplay[i]->bouncers.size(); ++j) {
                        GodotRenderer* g = state->mBdisplay[i]->bouncers[j].godot_renderer;
                        if (g) {
                            char label[64];
                            std::snprintf(label, sizeof(label), "Bouncer %zu:%zu", i, j);
                            if (ImGui::MenuItem(label, NULL, state->selected_godot == g))
                                state->selected_godot = g;
                        }
                    }
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();

            if (state->selected_godot) {
                ImGui::Text("Godot Hierarchy:");
                state->selected_godot->renderTree();
            } else {
                ImGui::Text("No Godot bouncer selected.");
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Record")) {
            bool is_recording = (myNvec != NULL);
            if (!is_recording) {
                ImGui::SetNextItemWidth(200.0f);
                ImGui::InputText("File", state->record_path_buf, sizeof(state->record_path_buf));
                if (ImGui::MenuItem("Start Recording")) {
                    SDL_GetWindowSize(window, &cur_w, &cur_h);
                    cur_w = ((int)cur_w / 16)*16; cur_h = ((int)cur_h / 16)*16;
                    SDL_SetWindowSize(window, cur_w, cur_h);
                    cur_rel = (float)cur_w / (float)cur_h;
                    SDL_SetWindowResizable(window, false);
                    int out_w = 0, out_h = 0;
                    SDL_GetWindowSize(window, &out_w, &out_h);
                    recorder_start(state->recorder, out_w, out_h, state->record_path_buf);
                    state->record_time = 0.0f;
                }
            } else {
                int mins = static_cast<int>(state->record_time) / 60;
                int secs = static_cast<int>(state->record_time) % 60;
                ImGui::Text("REC  %02d:%02d  (%d frames)", mins, secs, state->recorder.frame_count);
                if (state->record_max_enabled) {
                    int remaining = state->record_max_seconds - static_cast<int>(state->record_time);
                    ImGui::Text("Auto-stop in: %ds", std::max(0, remaining));
                }
                if (ImGui::MenuItem("Stop Recording")) recorder_stop(state->recorder);
            }
            ImGui::Checkbox("Record GUI", &state->record_gui);
            ImGui::Separator();
            ImGui::Checkbox("Max Length", &state->record_max_enabled);
            if (state->record_max_enabled) {
                ImGui::SetNextItemWidth(200.0f);
                ImGui::SliderInt("Seconds", &state->record_max_seconds, 1, 300);
            }
            ImGui::EndMenu();
        }
        if (myNvec != NULL) {
            state->record_time += dt;
            if (state->record_max_enabled && state->record_time >= static_cast<float>(state->record_max_seconds))
                recorder_stop(state->recorder);
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "REC");
            ImGui::SameLine();
        }
        ImGui::Text("%.1f FPS",  1.0f / delta_time);//(io.Framerate);
        ImGui::EndMainMenuBar();
    }
    }

    if (state->bg_video && state->bg_tex) state->bg_video->updateTexture(g_renderer, state->bg_tex);

    // --- Property Watchers Polling ---
    for (auto& w : state->watchers) {
        if (w.fired) continue;
        
        GodotRenderer* target_renderer = nullptr;
        if (state->bg_godot) {
            Node* saved = state->bg_godot->getCurrentNode();
            state->bg_godot->selectRoot();
            if (state->bg_godot->searchNode(w.node_name)) {
                target_renderer = state->bg_godot;
                Variant val = target_renderer->getProperty(w.property_name);
                if (val.get_type() != Variant::NIL) {
                    if (val == w.target_value) {
                        w.fired = true;
                        state->scriptSystem->runOneShotScript(w.callback_file);
                    }
                }
            }
            state->bg_godot->setCurrentNode(saved);
        }
        if (w.fired) goto next_watcher;

        for (auto& bd : state->mBdisplay) {
            for (auto& b : bd->bouncers) {
                if (b.godot_renderer) {
                    Node* saved = b.godot_renderer->getCurrentNode();
                    b.godot_renderer->selectRoot();
                    if (b.godot_renderer->searchNode(w.node_name)) {
                        Variant val = b.godot_renderer->getProperty(w.property_name);
                        if (val.get_type() != Variant::NIL) {
                            if (val == w.target_value) {
                                w.fired = true;
                                state->scriptSystem->runOneShotScript(w.callback_file);
                            }
                        }
                    }
                    b.godot_renderer->setCurrentNode(saved);
                    if (w.fired) goto next_watcher;
                }
            }
        }
        next_watcher:;
    }
    // Prune fired watchers
    state->watchers.erase(std::remove_if(state->watchers.begin(), state->watchers.end(), [](const PropertyWatcher& w){ return w.fired; }), state->watchers.end());

    if (!state->pending_lua_path.empty()) {
        std::printf("Performing deferred transition to: %s\n", state->pending_lua_path.c_str());
        
        state->scriptSystem->stop();
        
        // Reset selections
        state->selected_plasma = myPlasma;
        state->selected_mandel = myMandel;
        state->selected_usd = nullptr;
        state->selected_godot = nullptr;
        
        state->mBdisplay.clear();
        state->watchers.clear();
        
        state->scriptSystem->runScript(state->pending_lua_path);
        state->pending_lua_path = "";
    }

    for (auto it = state->mBdisplay.begin(); it != state->mBdisplay.end(); ) {
        (*it)->update(dt, cur_w, cur_h, state);
        if ((*it)->bouncers.empty()) {
            // No need to check bouncers here since they are already empty,
            // but wait, if they were emptied inside update(), their plasma/mandel pointers
            // are already gone. Let's just do a global check if selected_plasma is dangling.
            // Wait, we can't easily tell if it's dangling here.
            // Let's modify BDdisplay::update to return deleted bouncers or just
            // do the check before erase. Actually, BDdisplay::update erases them one by one.
            it = state->mBdisplay.erase(it);
        } else {
            ++it;
        }
    }

    if (bUsePlasma) {
        myPlasma->updateTexture(g_renderer, state->plasma_tex);
    }
    if (bUseMandel) {
        myMandel->updateTexture(g_renderer, state->mandel_tex);
    }
    if (bUseUSD && state->bg_usd && state->bg_tex) {
        std::vector<uint8_t> pixels(cur_w * cur_h * 4);
        state->bg_usd->render(pixels.data());
        g_renderer->updateTexture(state->bg_tex, cur_w, cur_h, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, pixels.data(), cur_w * 4);
    }
    if (bUseGodot && state->bg_godot && state->bg_tex) {
        std::vector<uint8_t> pixels(cur_w * cur_h * 4);
        state->bg_godot->render(pixels.data());
        g_renderer->updateTexture(state->bg_tex, cur_w, cur_h, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, pixels.data(), cur_w * 4);
    }

    g_renderer->beginFrame();

    if (state->show_imgui) {
        ImGui::Render();
        ImGui_ImplSDLGPU3_PrepareDrawData(ImGui::GetDrawData(), g_renderer->getCommandBuffer());
    }

    g_renderer->beginRenderPass();
    
    if (state->bg_tex) {
        g_renderer->drawBackground(state->bg_tex);
    } else if (state->plasma_tex) {
        g_renderer->drawBackground(state->plasma_tex);
    } else if (state->mandel_tex) {
        g_renderer->drawBackground(state->mandel_tex);
    }

    for (int l = 2; l >= 0; --l) {
        for (auto& bd : state->mBdisplay) {
            bd->draw(g_renderer, l);
        }
    }

    if (state->show_imgui && state->record_gui) {
        ImGui_ImplSDLGPU3_RenderDrawData(ImGui::GetDrawData(), g_renderer->getCommandBuffer(), g_renderer->getRenderPass());
    }

    g_renderer->endRenderPass();

    g_renderer->blitToSwapchain();

    if (state->show_imgui && !state->record_gui) {
        g_renderer->beginSwapchainRenderPass();
        if (g_renderer->getRenderPass()) {
            ImGui_ImplSDLGPU3_RenderDrawData(ImGui::GetDrawData(), g_renderer->getCommandBuffer(), g_renderer->getRenderPass());
            g_renderer->endRenderPass();
        }
    }

    if (myNvec != NULL) {
        recorder_feed_frame(state->recorder, g_renderer);
    }

    g_renderer->endFrame();

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    AppState* state = (AppState*)appstate;
    if (state) {
        if (state->scriptSystem) {
            state->scriptSystem->stop();
            delete state->scriptSystem;
            state->scriptSystem = nullptr;
        }
        recorder_stop(state->recorder);
        if (bUsePlasma && myPlasma) { /* myPlasma->stop(); */ }
        if (bUseMandel && myMandel) myMandel->stop();
        if (state->bg_usd) delete state->bg_usd;
        if (state->bg_godot) delete state->bg_godot;
        if (state->bg_tex) SDL_ReleaseGPUTexture(g_renderer->getDevice(), state->bg_tex);
        for (auto* et : state->extra_textures) SDL_ReleaseGPUTexture(g_renderer->getDevice(), et);
        for (auto& e : state->cli_entries) { if (e.tex) SDL_ReleaseGPUTexture(g_renderer->getDevice(), e.tex); }
        if (state->plasma_tex) SDL_ReleaseGPUTexture(g_renderer->getDevice(), state->plasma_tex);
        if (state->mandel_tex) SDL_ReleaseGPUTexture(g_renderer->getDevice(), state->mandel_tex);
        if (state->scratch_tex) SDL_ReleaseGPUTexture(g_renderer->getDevice(), state->scratch_tex);

        state->mBdisplay.clear();
        state->bg_video.reset();
        state->loop_audio.reset(); // Destroy audio decoder before mixer

        if (state->godot_manager) {
            delete state->godot_manager;
            state->godot_manager = nullptr;
        }

        for (auto obj : state->usdObjects) {
            obj->destroy(g_renderer->getDevice());
            delete obj;
        }
        state->usdObjects.clear();
        if (state->usdManager) {
            delete state->usdManager;
            state->usdManager = nullptr;
        }

        ImGui_ImplSDLGPU3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        
        if (myMix) {
            delete myMix;
            myMix = nullptr;
        }
        if (myPlasma) { delete myPlasma; myPlasma = nullptr; }
        if (myMandel) { delete myMandel; myMandel = nullptr; }

        if (g_renderer) {
            g_renderer->shutdown();
            delete g_renderer;
            g_renderer = nullptr;
        }

        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();

        delete state;
    }
}


