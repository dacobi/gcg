#include "resource_format_loader_png.h"
#include "core/io/image.h"
#include "scene/resources/image_texture.h"

Ref<Resource> ResourceFormatLoaderPNG::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
    Ref<Image> image = Image::load_from_file(p_path);
    
    if (image.is_null()) {
        if (r_error) {
            *r_error = ERR_CANT_OPEN;
        }
        return Ref<Resource>();
    }

    Ref<ImageTexture> texture = ImageTexture::create_from_image(image);
    if (texture.is_null()) {
        if (r_error) {
            *r_error = ERR_CANT_CREATE;
        }
        return Ref<Resource>();
    }

    if (r_error) {
        *r_error = OK;
    }

    texture->set_path(p_path);
    return texture;
}

void ResourceFormatLoaderPNG::get_recognized_extensions(List<String> *p_extensions) const {
    p_extensions->push_back("png");
}

bool ResourceFormatLoaderPNG::handles_type(const String &p_type) const {
    return p_type == "Texture2D" || p_type == "ImageTexture" || p_type == "Texture";
}

String ResourceFormatLoaderPNG::get_resource_type(const String &p_path) const {
    if (p_path.get_extension().to_lower() == "png") {
        return "ImageTexture";
    }
    return "";
}
