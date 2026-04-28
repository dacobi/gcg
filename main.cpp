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


extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/time.h> // Location of av_usleep
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <alsa/asoundlib.h>

}

const int MIXER_SAMPLE_RATE = 48000;

// --- 1. Audio Mixer Class ---
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
    int a_frame_size = 0;

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
                std::vector<int16_t> mixed = shared_mixer->consume(a_frame_size);
                if (!mixed.empty()) {
                    busy = true;
                    processAudio(mixed.data(), mixed.size() / 2);
                }
            }

            if (!busy) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }

    void processAudio(const int16_t* data, int samples) {
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
        av_opt_set(v_enc->priv_data, "preset", "p4", 0);
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
        sws_ctx = sws_getContext(w, h, AV_PIX_FMT_RGBA, w, h, v_enc->pix_fmt, SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
        swr_alloc_set_opts2(&swr_ctx, &a_enc->ch_layout, a_enc->sample_fmt, sample_rate,
                            &a_enc->ch_layout, AV_SAMPLE_FMT_S16, sample_rate, 0, nullptr);
        swr_init(swr_ctx);

        avio_open(&out_ctx->pb, path.c_str(), AVIO_FLAG_WRITE);
        avformat_write_header(out_ctx, nullptr);

        // Pre-allocate some buffers
        for (int i = 0; i < 5; ++i) {
            RawFrame* rf = new RawFrame();
            rf->data.resize(w * h * 4);
            buffer_pool.push_back(rf);
        }

        worker_thread = std::thread(&NvencEncoder::workerFunc, this);
    }

    void pushVideoFrame(const uint8_t* rgba_data) {
        auto now = std::chrono::steady_clock::now();

        if (!recording_started) {
            start_time = now;
            recording_started = true;
        }

        // Calculate PTS based on actual elapsed wall-time
        auto elapsed = std::chrono::duration<double>(now - start_time).count();
        int64_t pts = static_cast<int64_t>(elapsed * target_fps);

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

        std::memcpy(raw->data.data(), rgba_data, width * height * 4);
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

class NvdecDecode {
private:
    AVFormatContext* fmt_ctx = nullptr;
    AVCodecContext* v_ctx = nullptr;
    AVCodecContext* a_ctx = nullptr;
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
    
    const size_t MAX_DECODED_QUEUE = 16;
    const size_t MAX_PACKET_QUEUE = 128;

    std::atomic<bool> quit{false};
    std::atomic<bool> seek_req{false};
    std::atomic<double> audio_clock{0.0}; 

    double get_pts_seconds(AVFrame* f, int stream_idx) {
        int64_t pts = f->best_effort_timestamp;
        if (pts == AV_NOPTS_VALUE) pts = f->pts;
        if (pts == AV_NOPTS_VALUE) return audio_clock.load(); 
        return pts * av_q2d(fmt_ctx->streams[stream_idx]->time_base);
    }

    void demuxWorker() {
        AVPacket* pkt = av_packet_alloc();
        while (!quit) {
            size_t v_q, a_q;
            { std::lock_guard<std::mutex> l(video_pkt_mtx); v_q = video_pkt_queue.size(); }
            { std::lock_guard<std::mutex> l(audio_mtx); a_q = audio_pkt_queue.size(); }

            if (v_q > MAX_PACKET_QUEUE || a_q > MAX_PACKET_QUEUE) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                if (quit) break;
                continue;
            }

            if (av_read_frame(fmt_ctx, pkt) < 0) {
                if (quit) break;
                av_seek_frame(fmt_ctx, video_stream_idx, 0, AVSEEK_FLAG_BACKWARD);
                seek_req.store(true);
                continue;
            }
            // ... (rest of the demuxWorker logic)
            if (pkt->stream_index == video_stream_idx) {
                AVPacket* v_pkt = av_packet_clone(pkt);
                std::lock_guard<std::mutex> l(video_pkt_mtx);
                video_pkt_queue.push(v_pkt);
            } else if (pkt->stream_index == audio_stream_idx) {
                AVPacket* a_pkt = av_packet_clone(pkt);
                std::lock_guard<std::mutex> l(audio_mtx);
                audio_pkt_queue.push(a_pkt);
            }
            av_packet_unref(pkt);
        }
        av_packet_free(&pkt);
    }

    double get_current_audio_time() {
        if (!myMix) return audio_clock.load();
        return myMix->getSourcePTS(this);
    }

    void audioWorker() {
        while (!quit) {
            if (seek_req.load()) {
                avcodec_flush_buffers(a_ctx);
                audio_clock.store(0.0);
                std::lock_guard<std::mutex> l(audio_mtx);
                while(!audio_pkt_queue.empty()) {
                    AVPacket* p = audio_pkt_queue.front();
                    av_packet_free(&p);
                    audio_pkt_queue.pop();
                }
            }

            AVPacket* pkt = nullptr;
            {
                std::lock_guard<std::mutex> lock(audio_mtx);
                if (!audio_pkt_queue.empty()) {
                    pkt = audio_pkt_queue.front();
                    audio_pkt_queue.pop();
                }
            }

            if (!pkt) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            if (avcodec_send_packet(a_ctx, pkt) == 0) {
                while (avcodec_receive_frame(a_ctx, audio_frame) == 0 && !quit) {
                    double pts = get_pts_seconds(audio_frame, audio_stream_idx);
                    
                    int out_samples = swr_convert(swr_ctx, &audio_out_buf, 4096,
                                                  (const uint8_t**)audio_frame->data, 
                                                  audio_frame->nb_samples);
                    
                    if (out_samples > 0) {
                        if (myMix) {
                            myMix->addAudio(this, (int16_t*)audio_out_buf, out_samples, pts);
                            audio_clock.store(pts + (double)out_samples / MIXER_SAMPLE_RATE);
                        }
                    }
                }
            }
            av_packet_free(&pkt);
        }
    }

    void videoWorker() {
        AVFrame* raw_frame = av_frame_alloc();
        while (!quit) {
            if (seek_req.load()) {
                avcodec_flush_buffers(v_ctx);
                {
                    std::lock_guard<std::mutex> l(video_pkt_mtx);
                    while(!video_pkt_queue.empty()) {
                        AVPacket* p = video_pkt_queue.front();
                        av_packet_free(&p);
                        video_pkt_queue.pop();
                    }
                }
                {
                    std::lock_guard<std::mutex> l(texture_mtx);
                    while(!decoded_queue.empty()) {
                        AVFrame* f = decoded_queue.front().frame_rgba;
                        { std::lock_guard<std::mutex> pl(pool_mtx); frame_pool.push_back(f); }
                        decoded_queue.pop();
                    }
                }
                // wait for audio worker to also see seek_req
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                seek_req.store(false); 
            }

            size_t q_size;
            { std::lock_guard<std::mutex> l(texture_mtx); q_size = decoded_queue.size(); }
            if (q_size >= MAX_DECODED_QUEUE) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            }

            AVPacket* pkt = nullptr;
            {
                std::lock_guard<std::mutex> l(video_pkt_mtx);
                if (!video_pkt_queue.empty()) {
                    pkt = video_pkt_queue.front();
                    video_pkt_queue.pop();
                }
            }

            if (!pkt) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            if (avcodec_send_packet(v_ctx, pkt) == 0) {
                while (avcodec_receive_frame(v_ctx, raw_frame) == 0 && !quit) {
                    AVFrame* rgba_f = nullptr;
                    {
                        std::lock_guard<std::mutex> l(pool_mtx);
                        if (!frame_pool.empty()) {
                            rgba_f = frame_pool.back();
                            frame_pool.pop_back();
                        }
                    }
                    if (!rgba_f) {
                        rgba_f = av_frame_alloc();
                        rgba_f->format = AV_PIX_FMT_RGBA;
                        rgba_f->width = width; rgba_f->height = height;
                        av_frame_get_buffer(rgba_f, 32);
                    }

                    if (!sws_ctx) {
                        sws_ctx = sws_getContext(width, height, (AVPixelFormat)raw_frame->format,
                                                 width, height, AV_PIX_FMT_RGBA,
                                                 SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
                    }
                    sws_scale(sws_ctx, raw_frame->data, raw_frame->linesize, 0, height,
                              rgba_f->data, rgba_f->linesize);

                    DecodedFrame df;
                    df.frame_rgba = rgba_f;
                    df.pts = get_pts_seconds(raw_frame, video_stream_idx);

                    {
                        std::lock_guard<std::mutex> l(texture_mtx);
                        decoded_queue.push(df);
                    }
                }
            }
            av_packet_free(&pkt);
        }
        av_frame_free(&raw_frame);
    }

public:
    NvdecDecode(const std::string& path) {
        avformat_open_input(&fmt_ctx, path.c_str(), nullptr, nullptr);
        avformat_find_stream_info(fmt_ctx, nullptr);

        const AVCodec *vcodec = nullptr, *acodec = nullptr;
        for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
            AVCodecParameters* p = fmt_ctx->streams[i]->codecpar;
            if (p->codec_type == AVMEDIA_TYPE_VIDEO && video_stream_idx == -1) {
                video_stream_idx = i;
                if (p->codec_id == AV_CODEC_ID_H264) vcodec = avcodec_find_decoder_by_name("h264_cuvid");
                else if (p->codec_id == AV_CODEC_ID_HEVC) vcodec = avcodec_find_decoder_by_name("hevc_cuvid");
                if (!vcodec) vcodec = avcodec_find_decoder(p->codec_id);
            } else if (p->codec_type == AVMEDIA_TYPE_AUDIO && audio_stream_idx == -1) {
                audio_stream_idx = i;
                acodec = avcodec_find_decoder(p->codec_id);
            }
        }

        v_ctx = avcodec_alloc_context3(vcodec);
        avcodec_parameters_to_context(v_ctx, fmt_ctx->streams[video_stream_idx]->codecpar);
        v_ctx->thread_count = 0;
        v_ctx->flags |= AV_CODEC_FLAG_LOW_DELAY;
        avcodec_open2(v_ctx, vcodec, nullptr);
        width = v_ctx->width; height = v_ctx->height;

        a_ctx = avcodec_alloc_context3(acodec);
        avcodec_parameters_to_context(a_ctx, fmt_ctx->streams[audio_stream_idx]->codecpar);
        avcodec_open2(a_ctx, acodec, nullptr);

        AVChannelLayout out_ch; av_channel_layout_default(&out_ch, 2);
        swr_alloc_set_opts2(&swr_ctx, &out_ch, AV_SAMPLE_FMT_S16, MIXER_SAMPLE_RATE,
                            &a_ctx->ch_layout, a_ctx->sample_fmt, a_ctx->sample_rate, 0, nullptr);
        swr_init(swr_ctx);
        audio_out_buf = (uint8_t*)av_malloc(av_samples_get_buffer_size(nullptr, 2, 4096, AV_SAMPLE_FMT_S16, 0));
        audio_frame = av_frame_alloc();

        demux_thread = std::thread(&NvdecDecode::demuxWorker, this);
        video_thread = std::thread(&NvdecDecode::videoWorker, this);
        audio_thread = std::thread(&NvdecDecode::audioWorker, this);
    }

    ~NvdecDecode() {
        quit = true;
        if (demux_thread.joinable()) demux_thread.join();
        if (video_thread.joinable()) video_thread.join();
        if (audio_thread.joinable()) audio_thread.join();

        if (myMix) myMix->removeSource(this);

        auto clear_q = [](std::queue<AVPacket*>& q) {
            while(!q.empty()) { av_packet_free(&q.front()); q.pop(); }
        };
        clear_q(audio_pkt_queue);
        clear_q(video_pkt_queue);

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
        avformat_close_input(&fmt_ctx);
    }

    void updateTexture(SDL_Texture* tex) {
        AVFrame* best_frame = nullptr;
        std::vector<AVFrame*> old_frames;
        {
            std::lock_guard<std::mutex> lock(texture_mtx);
            if (decoded_queue.empty()) return;
            double target_pts = get_current_audio_time();
            while (decoded_queue.size() > 2 && decoded_queue.front().pts < target_pts - 0.5) {
                old_frames.push_back(decoded_queue.front().frame_rgba);
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
            SDL_UpdateTexture(tex, nullptr, best_frame->data[0], best_frame->linesize[0]);
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
SDL_Renderer* renderer;

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


    SDL_SetWindowResizable(window, false);

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
    myNvec->pushVideoFrame(pixels);
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
                b.decoder->updateTexture(b.tex);
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
                    newB.decoder->updateTexture(tex); // load first frame
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


    
CLPlasmaParams plasma_params;

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
    p.darken_r          = rand_range(0.25f, 0.60f);
    p.darken_g          = rand_range(0.25f, 0.60f);
    p.darken_b          = rand_range(0.25f, 0.60f);

    p.tile_count        = rand_range(10.0f, 100.0f);

    return p;
}

// Re-randomise only the palette (colour) fields
static void randomise_plasma_palette(CLPlasmaParams& p) {
    p.palette_phase_r = rand_range(0.0f, 1.0f);
    p.palette_phase_g = rand_range(0.0f, 1.0f);
    p.palette_phase_b = rand_range(0.0f, 1.0f);
    p.darken_r        = rand_range(0.25f, 0.60f);
    p.darken_g        = rand_range(0.25f, 0.60f);
    p.darken_b        = rand_range(0.25f, 0.60f);
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

ContentParser mParser;

PlasmaOpenCL* myPlasma = nullptr;
bool bUsePlasma = true;

// ---------------------------------------------------------------------------
int main(int argc, char** argv)
{
    myMix = new AudioMixer(MIXER_SAMPLE_RATE);
    // --- Parse CLI arguments ---
    // Usage: ./imtest [--record output.mp4] [text...]
    std::vector<std::string> cli_texts;
    std::string cli_record_path;
    std::string cli_bg_path;
    int cli_record_max = -1;  // -1 = not specified on CLI
    bool  cli_no_nerds = false;
    bool cli_no_maximize = false;
    bool cli_plasma_tile = false;
    std::vector<std::unique_ptr<BDdisplay>>  mBdisplay;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--record") == 0 && i + 1 < argc) {
            cli_record_path = argv[++i];
        } else if (std::strcmp(argv[i], "--bg") == 0 && i + 1 < argc) {
            cli_bg_path = argv[++i];
            bUsePlasma = false;
        } else if (std::strcmp(argv[i], "--record-max") == 0 && i + 1 < argc) {            cli_record_max = std::atoi(argv[++i]);
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
    SDL_Texture* plasma_tex = nullptr;
    if (bUsePlasma) {
        plasma_tex = SDL_CreateTexture(
            renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
            plasma_w, plasma_h
        );
    }

    // --- Custom Background layer ---
    std::unique_ptr<NvdecDecode> bg_video;
    SDL_Texture* bg_tex = nullptr;
    if (!cli_bg_path.empty()) {
        std::string ext = cli_bg_path;
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        bool is_video = (ext.find(".mp4") != std::string::npos || 
                         ext.find(".mkv") != std::string::npos || 
                         ext.find(".mov") != std::string::npos ||
                         ext.find(".avi") != std::string::npos);
        
        if (is_video) {
            try {
                bg_video = std::make_unique<NvdecDecode>(cli_bg_path);
                bg_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, 
                                           bg_video->getWidth(), bg_video->getHeight());
                if (bg_tex) {
                    std::printf("BG: Loaded video %s (%dx%d)\n", cli_bg_path.c_str(), bg_video->getWidth(), bg_video->getHeight());
                }
            } catch (const std::exception& e) {
                std::printf("BG Video error: %s\n", e.what());
            }
        } else {
            int bw, bh;
            bg_tex = create_png_texture(renderer, cli_bg_path.c_str(), &bw, &bh);
            if (bg_tex) {
                std::printf("BG: Loaded image %s (%dx%d)\n", cli_bg_path.c_str(), bw, bh);
            }
        }
    }

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

    if (bUsePlasma) {
        myPlasma = new PlasmaOpenCL(plasma_w, plasma_h);
        myPlasma->init();
        CLPlasmaParams p = randomise_plasma();
        myPlasma->setParams(p);
        myPlasma->start();
    }


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
            if(bUsePlasma){
                if (plasma_tex) SDL_DestroyTexture(plasma_tex);
                plasma_w = cur_w / 4;
                plasma_h = cur_h / 4;
                if (plasma_w < 1) plasma_w = 1;
                if (plasma_h < 1) plasma_h = 1;
                plasma_tex = SDL_CreateTexture(
                    renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                    plasma_w, plasma_h
                );
                myPlasma->resize(plasma_w, plasma_h);
            }
            prev_win_w = cur_w;
            prev_win_h = cur_h;
        }

        // Roll palette: smoothly rotate the colour phase offsets each frame
        if (roll_palette) {
            float step = roll_palette_speed * dt;
            plasma_params.palette_phase_r = std::fmod(plasma_params.palette_phase_r + step,        2.0f);
            plasma_params.palette_phase_g = std::fmod(plasma_params.palette_phase_g + step * 0.7f, 2.0f);
            plasma_params.palette_phase_b = std::fmod(plasma_params.palette_phase_b + step * 1.3f, 2.0f);
        }

        // Update plasma pixels
        // 1) Update plasma background if active
        if (bUsePlasma) {
            //update_plasma_texture(plasma_tex, plasma_w, plasma_h, time_acc, plasma_params);
            myPlasma->updateTexture(plasma_tex);
        }
        
    
        
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

        if (bg_video && bg_tex) {
            bg_video->updateTexture(bg_tex);
        }

        // 1) Draw background (custom or plasma)
        if (bg_tex) {
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            SDL_RenderTexture(renderer, bg_tex, nullptr, nullptr);
        } else if (plasma_tex) {
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
    if(bUsePlasma) myPlasma->stop();
    if (bg_tex) SDL_DestroyTexture(bg_tex);
    for (auto* et : extra_textures) SDL_DestroyTexture(et);
    for (auto& e : cli_entries) { if (e.tex) SDL_DestroyTexture(e.tex); }
    if (plasma_tex) SDL_DestroyTexture(plasma_tex);

    // Explicitly destroy all decoders before the mixer
    mBdisplay.clear();
    bg_video.reset();

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    if (myMix) {
        delete myMix;
        myMix = nullptr;
    }

    return 0;
}
