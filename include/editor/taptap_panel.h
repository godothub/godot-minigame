#pragma once

#include <godot_cpp/classes/margin_container.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace toolkit {
namespace editor {

// TapTap面板 - 预留扩展功能
class TapTapPanel : public godot::MarginContainer {
    GDCLASS(TapTapPanel, godot::MarginContainer);

private:
    godot::Control* content_control = nullptr;    // 中间Control节点
    godot::Label* placeholder_label = nullptr;

protected:
    static void _bind_methods();

public:
    TapTapPanel();
    ~TapTapPanel();

    virtual void _ready() override;

    // UI setup
    void create_interface();
};

} // namespace editor
} // namespace toolkit