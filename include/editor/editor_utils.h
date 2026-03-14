#pragma once

#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace toolkit {
namespace editor {

godot::Ref<godot::Texture2D> load_embedded_icon(const godot::String& resource_path);

} // namespace editor
} // namespace toolkit
