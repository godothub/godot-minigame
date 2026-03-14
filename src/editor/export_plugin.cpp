#include "editor/export_plugin.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include "core/logging.h"

using namespace godot;

namespace toolkit {
namespace editor {

void ToolkitExportPlugin::_bind_methods() {
}

ToolkitExportPlugin::ToolkitExportPlugin() {
}

ToolkitExportPlugin::~ToolkitExportPlugin() {
    // Ensure platform is removed before destruction
    if (wechat_platform.is_valid()) {
        // Platform should already be removed in _exit_tree
        wechat_platform.unref();
    }
}

void ToolkitExportPlugin::_enter_tree() {
    TOOLKIT_LOG("ToolkitExportPlugin: _enter_tree");
    
    // Always create a fresh platform instance when entering tree
    if (!wechat_platform.is_valid()) {
        wechat_platform.instantiate();
    }
    add_export_platform(wechat_platform);
    
    TOOLKIT_LOG("ToolkitExportPlugin: Export platform added");
}

void ToolkitExportPlugin::_exit_tree() {
    TOOLKIT_LOG("ToolkitExportPlugin: _exit_tree");
    
    // Remove the WeChat export platform
    if (wechat_platform.is_valid()) {
        remove_export_platform(wechat_platform);
        wechat_platform.unref();  // Release the reference so new instance is created on re-enter
        TOOLKIT_LOG("ToolkitExportPlugin: Export platform removed");
    }
}

} // namespace editor
} // namespace toolkit
