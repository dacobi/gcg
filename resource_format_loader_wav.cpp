#include "resource_format_loader_wav.h"
#include "scene/resources/audio_stream_wav.h"

Ref<Resource> ResourceFormatLoaderWAV::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
    Dictionary options;
    Ref<AudioStreamWAV> stream = AudioStreamWAV::load_from_file(p_path, options);
    
    if (stream.is_null()) {
        if (r_error) {
            *r_error = ERR_CANT_OPEN;
        }
        return Ref<Resource>();
    }

    if (r_error) {
        *r_error = OK;
    }

    stream->set_path(p_path);
    return stream;
}

void ResourceFormatLoaderWAV::get_recognized_extensions(List<String> *p_extensions) const {
    p_extensions->push_back("wav");
}

bool ResourceFormatLoaderWAV::handles_type(const String &p_type) const {
    return p_type == "AudioStream" || p_type == "AudioStreamWAV";
}

String ResourceFormatLoaderWAV::get_resource_type(const String &p_path) const {
    if (p_path.get_extension().to_lower() == "wav") {
        return "AudioStreamWAV";
    }
    return "";
}
