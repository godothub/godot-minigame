#pragma once

#include <godot_cpp/classes/editor_plugin.hpp>
#include <godot_cpp/core/class_db.hpp>
#include "editor/wechat_export_platform.h"

namespace toolkit {
namespace editor {

class ToolkitExportPlugin : public godot::EditorPlugin {
    GDCLASS(ToolkitExportPlugin, godot::EditorPlugin);

private:
    godot::Ref<WeChatExportPlatform> wechat_platform;

protected:
    static void _bind_methods();

public:
    ToolkitExportPlugin();
    ~ToolkitExportPlugin();

    virtual void _enter_tree() override;
    virtual void _exit_tree() override;
};

} // namespace editor
} // namespace toolkit
