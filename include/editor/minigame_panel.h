#pragma once

#include <godot_cpp/classes/margin_container.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace toolkit {
namespace editor {

// 小游戏面板 - 包含导出小游戏模板功能
class MinigamePanel : public godot::MarginContainer {
    GDCLASS(MinigamePanel, godot::MarginContainer);

private:
    godot::Control* content_control = nullptr;    // 中间Control节点
    godot::VBoxContainer* layout = nullptr;
    godot::Button* export_button = nullptr;
    godot::Button* test_remote_button = nullptr;  // 测试远端下载按钮
    godot::Label* status_label = nullptr;

protected:
    static void _bind_methods();

public:
    MinigamePanel();
    ~MinigamePanel();

    virtual void _ready() override;

    // UI setup
    void create_interface();
    void setup_connections();

    // Event handlers
    void on_export_button_pressed();
    void on_test_remote_button_pressed();  // 测试远端下载处理
    void on_template_check_completed();
    void _on_template_download_finished(const godot::String& filename, bool success);
    void _on_template_download_progress(const godot::String& filename, double progress);

    // Utility functions
    void set_status(const godot::String& message);
    void update_button_state();
    void check_template_availability();
    void handle_template_download_if_needed(const godot::String& template_path);

private:
    void download_and_extract_template(const godot::String& filename);
    void extract_template_and_finish(const godot::String& filename, bool from_embedded);
};

} // namespace editor
} // namespace toolkit
