#include "editor/editor_utils.h"
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#ifdef EMBED_RESOURCES
#include "resources/embedded_resources.h"

namespace toolkit {
namespace editor {

godot::Ref<godot::Texture2D> load_embedded_icon(const godot::String& resource_path) {
    for (int i = 0; toolkit::resources::embedded_resources[i].path != nullptr; i++) {
        const auto& resource = toolkit::resources::embedded_resources[i];
        if (godot::String(resource.path) == resource_path) {
            godot::Ref<godot::Image> image = memnew(godot::Image);
            godot::PackedByteArray image_data;
            image_data.resize(resource.size);
            memcpy(image_data.ptrw(), resource.data, resource.size);

            godot::Error err = godot::ERR_CANT_OPEN;
            if (resource_path.ends_with(".png")) {
                err = image->load_png_from_buffer(image_data);
            } else if (resource_path.ends_with(".svg")) {
                float editor_scale = 1.0f;
                if (godot::EditorInterface::get_singleton()) {
                    editor_scale = godot::EditorInterface::get_singleton()->get_editor_scale();
                }
                err = image->load_svg_from_buffer(image_data, editor_scale);
            } else if (resource_path.ends_with(".jpg") || resource_path.ends_with(".jpeg")) {
                err = image->load_jpg_from_buffer(image_data);
            } else if (resource_path.ends_with(".webp")) {
                err = image->load_webp_from_buffer(image_data);
            }

            if (err == godot::OK && image.is_valid()) {
                return godot::ImageTexture::create_from_image(image);
            }
            break;
        }
    }
    return godot::Ref<godot::Texture2D>();
}

} // namespace editor
} // namespace toolkit
#else
namespace toolkit {
namespace editor {
godot::Ref<godot::Texture2D> load_embedded_icon(const godot::String& resource_path) {
    return godot::Ref<godot::Texture2D>();
}
} // namespace editor
} // namespace toolkit
#endif
