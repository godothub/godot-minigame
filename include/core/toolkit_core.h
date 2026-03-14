#pragma once

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include "templates/template_manager.h"

namespace toolkit {
namespace core {

class ToolkitCore : public godot::RefCounted {
    GDCLASS(ToolkitCore, godot::RefCounted);

private:
    static ToolkitCore* singleton;

protected:
    static void _bind_methods();

public:
    ToolkitCore();
    ~ToolkitCore();

    // Singleton
    static ToolkitCore* get_singleton();
    // Initialization and shutdown are now handled by the GDExtension entry points
    // static void initialize();
    // static void shutdown();

    // Template Management API
    void initialize_template_system();
    godot::Array get_available_templates() const;
    godot::String get_editor_template_status() const;
    bool has_template_updates() const;
    godot::Array get_missing_templates() const;
    godot::Error download_template(const godot::String& filename);

    // Old method for compatibility, might be removed later
    bool extract_template(const godot::String& template_name, const godot::String& output_dir);
};

} // namespace core
} // namespace toolkit
