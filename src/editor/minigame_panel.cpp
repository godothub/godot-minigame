#include "editor/minigame_panel.h"
#include "templates/template_manager.h"
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/theme.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include "editor/editor_utils.h"
#include "core/logging.h"


using namespace godot;

namespace toolkit {
namespace editor {

MinigamePanel::MinigamePanel() {
    set_name("Minigame");

    // 设置为填充整个矩形，并设置边距
    set_anchors_preset(Control::PRESET_FULL_RECT);
    add_theme_constant_override("margin_left", 4);
    add_theme_constant_override("margin_right", 4);
    add_theme_constant_override("margin_top", 4);
    add_theme_constant_override("margin_bottom", 4);
}

MinigamePanel::~MinigamePanel() {
    // Cleanup handled by Godot
}

void MinigamePanel::_bind_methods() {
    ClassDB::bind_method(D_METHOD("create_interface"), &MinigamePanel::create_interface);
    ClassDB::bind_method(D_METHOD("on_export_button_pressed"), &MinigamePanel::on_export_button_pressed);
    ClassDB::bind_method(D_METHOD("on_test_remote_button_pressed"), &MinigamePanel::on_test_remote_button_pressed);
    ClassDB::bind_method(D_METHOD("on_template_check_completed"), &MinigamePanel::on_template_check_completed);
    ClassDB::bind_method(D_METHOD("set_status", "message"), &MinigamePanel::set_status);
    ClassDB::bind_method(D_METHOD("check_template_availability"), &MinigamePanel::check_template_availability);
    ClassDB::bind_method(D_METHOD("_on_template_download_finished", "filename", "success"), &MinigamePanel::_on_template_download_finished);
    ClassDB::bind_method(D_METHOD("_on_template_download_progress", "filename", "progress"), &MinigamePanel::_on_template_download_progress);
}

void MinigamePanel::_ready() {
    TOOLKIT_LOG("MinigamePanel: Ready");
    create_interface();

    // Check template availability after interface is created
    call_deferred("check_template_availability");
}

void MinigamePanel::create_interface() {
    // Create intermediate Control node (MarginContainer -> Control -> Content)
    content_control = memnew(Control);
    content_control->set_anchors_preset(Control::PRESET_FULL_RECT);
    add_child(content_control);

    // Create main layout and add it to the Control
    layout = memnew(VBoxContainer);
    layout->set_h_size_flags(Control::SIZE_EXPAND_FILL);
    layout->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    content_control->add_child(layout);

    // Create export button
    export_button = memnew(Button);
    export_button->set_text(String::utf8("导出小游戏模板"));
    export_button->set_custom_minimum_size(Vector2(200, 40));

    // Set icon for the button
#ifdef EMBED_RESOURCES
    // Try to load embedded PNG icon
    Ref<Texture2D> icon = toolkit::editor::load_embedded_icon("resources/assets/mini_game_icon.png");
    if (icon.is_valid()) {
        export_button->set_button_icon(icon);
    } else {
        // Try SVG fallback (though it won't work directly)
        TOOLKIT_LOG("PNG icon not found, consider converting SVG to PNG");

        // Fallback to editor theme icon
        EditorInterface* editor = EditorInterface::get_singleton();
        if (editor) {
            Ref<Theme> editor_theme = editor->get_editor_main_screen()->get_theme();
            if (editor_theme.is_valid()) {
                Ref<Texture2D> fallback_icon = editor_theme->get_icon("FileDialog", "EditorIcons");
                if (fallback_icon.is_valid()) {
                    export_button->set_button_icon(fallback_icon);
                }
            }
        }
    }
#else
    // Use editor theme icon when not using embedded resources
    EditorInterface* editor = EditorInterface::get_singleton();
    if (editor) {
        Ref<Theme> editor_theme = editor->get_editor_main_screen()->get_theme();
        if (editor_theme.is_valid()) {
            Ref<Texture2D> icon = editor_theme->get_icon("FileDialog", "EditorIcons");
            if (icon.is_valid()) {
                export_button->set_button_icon(icon);
            }
        }
    }
#endif

    layout->add_child(export_button);

    // Create test remote download button
    test_remote_button = memnew(Button);
    test_remote_button->set_text(String::utf8("测试远端下载"));
    test_remote_button->set_custom_minimum_size(Vector2(200, 40));
    test_remote_button->set_modulate(Color(0.8, 0.9, 1.0, 1.0)); // Light blue tint to distinguish from main button
    layout->add_child(test_remote_button);

    // Create status label
    status_label = memnew(Label);
    status_label->set_text(String::utf8("准备就绪"));
    status_label->set_custom_minimum_size(Vector2(0, 30));
    layout->add_child(status_label);

    setup_connections();
}

void MinigamePanel::setup_connections() {
    if (export_button) {
        export_button->connect("pressed", Callable(this, "on_export_button_pressed"));
    }
    if (test_remote_button) {
        test_remote_button->connect("pressed", Callable(this, "on_test_remote_button_pressed"));
    }

    if (Engine::get_singleton()->has_singleton("TemplateManager")) {
        Object* template_manager = Engine::get_singleton()->get_singleton("TemplateManager");
        if (template_manager) {
            template_manager->connect("template_download_finished", callable_mp(this, &MinigamePanel::_on_template_download_finished));
            if (template_manager->has_signal("template_download_progress")) {
                template_manager->connect("template_download_progress", callable_mp(this, &MinigamePanel::_on_template_download_progress));
            }
        }
    }
}

void MinigamePanel::on_export_button_pressed() {
    set_status(String::utf8("检查模板状态..."));

    // 检查 TemplateManager 单例是否可用
    if (Engine::get_singleton()->has_singleton("TemplateManager")) {
        Object* template_manager = Engine::get_singleton()->get_singleton("TemplateManager");
        if (template_manager) {
            // 获取当前编辑器最佳模板
            String best_template = template_manager->call("get_best_available_template_for_editor");

            if (best_template.is_empty()) {
                set_status(String::utf8("未找到兼容的模板"));
                TOOLKIT_LOG("No compatible template found for current editor version");
                return;
            }

            handle_template_download_if_needed(best_template);
        } else {
            set_status(String::utf8("模板管理器不可用"));
            TOOLKIT_LOG("TemplateManager singleton not available");
        }
    } else {
        set_status(String::utf8("模板系统未加载"));
        TOOLKIT_LOG("TemplateManager singleton not available");
    }
}

void MinigamePanel::set_status(const String& message) {
    if (status_label) {
        status_label->set_text(message);
        TOOLKIT_LOG("MinigamePanel status: " + message);
    }
}

void MinigamePanel::update_button_state() {
    if (!export_button) {
        return;
    }

    // Check if template system is available
    bool has_template_manager = Engine::get_singleton()->has_singleton("TemplateManager");

    if (!has_template_manager) {
        export_button->set_disabled(true);
        set_status(String::utf8("等待模板系统加载..."));
        return;
    }

    // Check if compatible template is available
    Object* template_manager = Engine::get_singleton()->get_singleton("TemplateManager");
    if (template_manager) {
        String editor_status = template_manager->call("check_editor_template_status");

        if (editor_status == "exact_match_available" || editor_status == "compatible_available") {
            export_button->set_disabled(false);
            export_button->set_text(String::utf8("导出小游戏模板"));
            set_status(String::utf8("模板就绪，可以导出"));
        } else if (editor_status == "exact_match_missing" || editor_status == "compatible_missing") {
            export_button->set_disabled(false);
            export_button->set_text(String::utf8("下载并导出模板"));
            set_status(String::utf8("需要下载模板"));
        } else {
            export_button->set_disabled(true);
            export_button->set_text(String::utf8("导出小游戏模板"));
            set_status(String::utf8("未找到兼容的模板版本"));
        }
    } else {
        export_button->set_disabled(true);
        set_status(String::utf8("模板管理器不可用"));
    }
}

void MinigamePanel::check_template_availability() {
    TOOLKIT_LOG("MinigamePanel: Checking template availability...");

    // Update button state based on current template availability
    update_button_state();

    // Initialize template system if not already done
    if (Engine::get_singleton()->has_singleton("TemplateManager")) {
        Object* template_manager = Engine::get_singleton()->get_singleton("TemplateManager");
        if (template_manager && template_manager->has_method("initialize_template_system")) {
            template_manager->call("initialize_template_system");
            // Update button state again after initialization
            call_deferred("on_template_check_completed");
        }
    }
}

void MinigamePanel::on_template_check_completed() {
    TOOLKIT_LOG("MinigamePanel: Template check completed, updating UI");
    update_button_state();
}

void MinigamePanel::handle_template_download_if_needed(const String& template_path) {
    TOOLKIT_LOG("MinigamePanel: Processing template path: '", template_path, "'");

    if (template_path.is_empty()) {
        set_status(String::utf8("模板路径为空"));
        TOOLKIT_LOG("Error: Template path is empty");
        return;
    }

    if (template_path.begins_with("embedded://")) {
        // Template is embedded, proceed with extraction
        String filename = template_path.replace("embedded://", "");
        TOOLKIT_LOG("Using embedded template: ", filename);
        extract_template_and_finish(filename, true);
    } else if (template_path.begins_with("remote://")) {
        // Template needs to be downloaded
        String filename = template_path.replace("remote://", "");
        TOOLKIT_LOG("Downloading remote template: ", filename);
        download_and_extract_template(filename);
    } else if (template_path.begins_with("/") || template_path.begins_with("C:")) {
        // Template is in cache (full file path), proceed with extraction
        String filename = template_path.get_file();
        TOOLKIT_LOG("Using cached template: ", filename, " from path: ", template_path);
        extract_template_and_finish(filename, false);
    } else {
        // Unknown template path format
        set_status(String::utf8("未知的模板路径格式"));
        TOOLKIT_LOG("Error: Unknown template path format: ", template_path);
    }
}

void MinigamePanel::download_and_extract_template(const String& filename) {
    set_status(String::utf8("正在下载模板: ") + filename);

    if (Engine::get_singleton()->has_singleton("TemplateManager")) {
        Object* template_manager = Engine::get_singleton()->get_singleton("TemplateManager");
        if (template_manager && template_manager->has_method("download_template")) {
            // Start download, the actual extraction will be handled by the signal callback
            Variant ret = template_manager->call("download_template", filename, "");
            if (ret.get_type() == Variant::INT) {
                int error_code = int(ret);
                if (error_code != OK) {
                    set_status(String::utf8("模板下载启动失败，错误码: ") + String::num_int64(error_code));
                }
            }
        }
    }
}

void MinigamePanel::extract_template_and_finish(const String& filename, bool from_embedded) {
    set_status(String::utf8("正在解压模板..."));

    // Check for GodotToolkit for extraction functionality
    if (Engine::get_singleton()->has_singleton("GodotToolkit")) {
        Object* toolkit_core = Engine::get_singleton()->get_singleton("GodotToolkit");
        if (toolkit_core && toolkit_core->has_method("extract_template")) {
            Variant result = toolkit_core->call("extract_template", filename, "minigame");

            if (result.get_type() == Variant::BOOL && bool(result)) {
                set_status(String::utf8("模板导出成功！"));
                TOOLKIT_LOG("Minigame template exported successfully: ", filename);
            } else {
                set_status(String::utf8("模板解压失败"));
                TOOLKIT_LOG("Failed to extract template: ", filename);
            }
        } else {
            set_status(String::utf8("解压功能不可用"));
            TOOLKIT_LOG("GodotToolkit extract_template method not available");
        }
    } else {
        set_status(String::utf8("核心系统不可用"));
        TOOLKIT_LOG("GodotToolkit singleton not available");
    }
}

void MinigamePanel::on_test_remote_button_pressed() {
    set_status(String::utf8("正在测试远端下载..."));
    TOOLKIT_LOG("MinigamePanel: Testing remote download workflow");

    if (Engine::get_singleton()->has_singleton("TemplateManager")) {
        Object* template_manager = Engine::get_singleton()->get_singleton("TemplateManager");
        if (template_manager) {
            // For testing, let's simulate downloading a template that doesn't exist locally
            // We'll use a fake filename to force remote download
            String test_filename = "minigame4.3.0.3.tpz"; // Use a different version to test remote workflow

            TOOLKIT_LOG("MinigamePanel: Testing download of: ", test_filename);

            // First check if it's in cache, if so, remove it for testing
            String cache_path = template_manager->call("get_download_cache_path", test_filename);
            TOOLKIT_LOG("MinigamePanel: Cache path would be: ", cache_path);

            // Start download process using sync method for testing
            // Use the async download method. The extraction will be triggered by the signal.
            template_manager->call("download_template", test_filename, "");
        } else {
            set_status(String::utf8("模板管理器不可用"));
        }
    } else {
        set_status(String::utf8("模板系统未加载"));
    }
}

void MinigamePanel::_on_template_download_finished(const String& filename, bool success) {
    if (success) {
        set_status(String::utf8("模板下载完成，开始解压..."));
        // Determine if it was the test button that triggered this
        if (filename == "minigame4.3.0.3.tpz") { // A bit of a hack, but works for this test case
             if (Engine::get_singleton()->has_singleton("GodotToolkit")) {
                Object* toolkit_core = Engine::get_singleton()->get_singleton("GodotToolkit");
                if (toolkit_core && toolkit_core->has_method("extract_template")) {
                    Variant extract_result = toolkit_core->call("extract_template", filename, "test_remote_minigame");
                    if (extract_result.get_type() == Variant::BOOL && bool(extract_result)) {
                        set_status(String::utf8("远端测试成功！模板已解压到 test_remote_minigame/"));
                        TOOLKIT_LOG("MinigamePanel: Remote download test completed successfully");
                    } else {
                        set_status(String::utf8("远端测试失败：解压错误"));
                        TOOLKIT_LOG("MinigamePanel: Remote test failed during extraction");
                    }
                }
            }
        } else {
            extract_template_and_finish(filename, false);
        }
    } else {
        set_status(String::utf8("模板下载失败: ") + filename);
        TOOLKIT_LOG("Template download failed for: ", filename);
    }
}

void MinigamePanel::_on_template_download_progress(const String& filename, double progress) {
    (void)filename;
    double clamped = progress;
    if (clamped < 0.0) {
        clamped = 0.0;
    }
    if (clamped > 1.0) {
        clamped = 1.0;
    }
    int percent = int(clamped * 100.0 + 0.5);
    set_status(String::utf8("模板下载中: ") + String::num_int64(percent) + "%");
}

} // namespace editor
} // namespace toolkit
