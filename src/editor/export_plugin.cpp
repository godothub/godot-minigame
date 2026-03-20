#include "editor/export_plugin.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include "core/logging.h"

using namespace godot;

namespace toolkit {
namespace editor {

void GodotMinigameExportPlugin::_bind_methods() {
}

GodotMinigameExportPlugin::GodotMinigameExportPlugin() {
}

GodotMinigameExportPlugin::~GodotMinigameExportPlugin() {
    // Ensure platform is removed before destruction
    if (wechat_platform.is_valid()) {
        // Platform should already be removed in _exit_tree
        wechat_platform.unref();
    }
}

void GodotMinigameExportPlugin::_enter_tree() {
    TOOLKIT_LOG("GodotMinigameExportPlugin: _enter_tree");
    
    // Always create a fresh platform instance when entering tree
    if (!wechat_platform.is_valid()) {
        wechat_platform.instantiate();
    }
    add_export_platform(wechat_platform);
    
    TOOLKIT_LOG("GodotMinigameExportPlugin: Export platform added");
}

void GodotMinigameExportPlugin::_exit_tree() {
    TOOLKIT_LOG("GodotMinigameExportPlugin: _exit_tree");
    
    // Remove the WeChat export platform
    if (wechat_platform.is_valid()) {
        remove_export_platform(wechat_platform);
        wechat_platform.unref();  // Release the reference so new instance is created on re-enter
        TOOLKIT_LOG("GodotMinigameExportPlugin: Export platform removed");
    }
}

} // namespace editor
} // namespace toolkit
