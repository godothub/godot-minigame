#include "editor/taptap_panel.h"
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include "core/logging.h"

using namespace godot;

namespace toolkit {
namespace editor {

TapTapPanel::TapTapPanel() {
    set_name("TapTap");

    // 设置为填充整个矩形，并设置边距
    set_anchors_preset(Control::PRESET_FULL_RECT);
    add_theme_constant_override("margin_left", 4);
    add_theme_constant_override("margin_right", 4);
    add_theme_constant_override("margin_top", 4);
    add_theme_constant_override("margin_bottom", 4);
}

TapTapPanel::~TapTapPanel() {
    // Cleanup handled by Godot
}

void TapTapPanel::_bind_methods() {
    ClassDB::bind_method(D_METHOD("create_interface"), &TapTapPanel::create_interface);
}

void TapTapPanel::_ready() {
    TOOLKIT_LOG("TapTapPanel: Ready");
    create_interface();
}

void TapTapPanel::create_interface() {
    // Create intermediate Control node (MarginContainer -> Control -> Content)
    content_control = memnew(Control);
    content_control->set_anchors_preset(Control::PRESET_FULL_RECT);
    add_child(content_control);

    // Create main layout and add it to the Control
    VBoxContainer* layout = memnew(VBoxContainer);
    layout->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    layout->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    content_control->add_child(layout);

    // Create placeholder label
    placeholder_label = memnew(Label);
    placeholder_label->set_text(String::utf8("TapTap 功能正在开发中..."));
    placeholder_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
    placeholder_label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
    placeholder_label->set_custom_minimum_size(Vector2(200, 100));

    layout->add_child(placeholder_label);
}

} // namespace editor
} // namespace toolkit