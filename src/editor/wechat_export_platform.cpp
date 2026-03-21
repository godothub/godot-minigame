#include "editor/wechat_export_platform.h"
#include "editor/editor_utils.h"
#include "templates/template_manager.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/progress_bar.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/panel_container.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/window.hpp>
#include "core/logging.h"

using namespace godot;

namespace toolkit {
namespace editor {

void WeChatExportPlatform::_bind_methods() {
}

static constexpr int WECHAT_EXPORT_LOGO_SIZE = 32;

static String _describe_export_error(Error p_error) {
    switch (p_error) {
        case OK:
            return "ok";
        case ERR_BUSY:
            return "busy";
        case ERR_FILE_NOT_FOUND:
            return "file_not_found";
        case ERR_FILE_CANT_WRITE:
            return "file_cant_write";
        case ERR_CANT_CONNECT:
            return "network_connect_failed";
        case ERR_TIMEOUT:
            return "network_timeout";
        case ERR_UNCONFIGURED:
            return "unconfigured";
        case ERR_INVALID_PARAMETER:
            return "invalid_parameter";
        default:
            return "unknown_error";
    }
}

static Ref<Texture2D> _normalize_export_logo_size(const Ref<Texture2D> &p_texture) {
    if (!p_texture.is_valid()) {
        return Ref<Texture2D>();
    }

    Ref<Image> image = p_texture->get_image();
    if (image.is_null()) {
        return p_texture;
    }

    if (image->get_width() == WECHAT_EXPORT_LOGO_SIZE && image->get_height() == WECHAT_EXPORT_LOGO_SIZE) {
        return p_texture;
    }

    image->resize(WECHAT_EXPORT_LOGO_SIZE, WECHAT_EXPORT_LOGO_SIZE, Image::INTERPOLATE_LANCZOS);
    return ImageTexture::create_from_image(image);
}

static Ref<Texture2D> _load_wechat_logo_fallback() {
    const char *logo_paths[] = {
        "res://resources/assets/mini_game_icon.png",
        "res://addons/godot-minigame/resources/assets/mini_game_icon.png",
        nullptr
    };

    for (int i = 0; logo_paths[i] != nullptr; i++) {
        Ref<Texture2D> tex = ResourceLoader::get_singleton()->load(String::utf8(logo_paths[i]));
        if (tex.is_valid()) {
            UtilityFunctions::print("[GodotMinigame][WeChatExportPlatform] fallback logo loaded from ", String::utf8(logo_paths[i]));
            return tex;
        }
    }

    // Final fallback: create a placeholder so export platform entry is never icon-less.
    Ref<Image> image = Image::create_empty(48, 48, false, Image::FORMAT_RGBA8);
    if (image.is_valid()) {
        image->fill(Color(0.16, 0.67, 0.35, 1.0));
        UtilityFunctions::print("[GodotMinigame][WeChatExportPlatform] using generated placeholder logo");
        return ImageTexture::create_from_image(image);
    }

    return Ref<Texture2D>();
}

static Node *_resolve_export_overlay_parent(EditorInterface *editor) {
    if (!editor) {
        return nullptr;
    }

    Control *base_control = editor->get_base_control();
    if (!base_control) {
        return nullptr;
    }

    SceneTree *tree = base_control->get_tree();
    if (!tree) {
        return base_control;
    }

    Node *root = tree->get_root();
    if (!root) {
        return base_control;
    }

    // Prefer ProjectExportDialog so overlay is visible above the export UI.
    Node *export_dialog = root->find_child("*ProjectExportDialog*", true, false);
    if (export_dialog) {
        return export_dialog;
    }

    return base_control;
}

void WeChatExportPlatform::_show_download_progress_dialog(const String &p_filename) {
    _hide_download_progress_dialog();

    if (!Engine::get_singleton()->is_editor_hint()) {
        return;
    }

    EditorInterface *editor = EditorInterface::get_singleton();
    if (!editor) {
        return;
    }
    Node *parent = _resolve_export_overlay_parent(editor);
    if (!parent) {
        return;
    }

    active_download_filename = p_filename;
    export_progress_value = 0.0;

    download_overlay = memnew(Control);
    download_overlay->set_name("GodotMinigameDownloadOverlay");
    download_overlay->set_anchors_preset(Control::PRESET_FULL_RECT);
    download_overlay->set_mouse_filter(Control::MOUSE_FILTER_IGNORE);
    parent->add_child(download_overlay);
    parent->move_child(download_overlay, parent->get_child_count() - 1);

    download_panel = memnew(PanelContainer);
    download_panel->set_name("GodotMinigameDownloadPanel");
    download_panel->set_anchors_preset(Control::PRESET_CENTER);
    download_panel->set_offset(SIDE_LEFT, -230.0);
    download_panel->set_offset(SIDE_TOP, -70.0);
    download_panel->set_offset(SIDE_RIGHT, 230.0);
    download_panel->set_offset(SIDE_BOTTOM, 70.0);
    download_panel->set_mouse_filter(Control::MOUSE_FILTER_STOP);
    download_overlay->add_child(download_panel);

    VBoxContainer *layout = memnew(VBoxContainer);
    layout->set_anchors_preset(Control::PRESET_FULL_RECT);
    layout->set_offset(SIDE_LEFT, 16.0);
    layout->set_offset(SIDE_TOP, 14.0);
    layout->set_offset(SIDE_RIGHT, -16.0);
    layout->set_offset(SIDE_BOTTOM, -14.0);
    layout->add_theme_constant_override("separation", 10);

    download_status_label = memnew(Label);
    download_status_label->set_text(String::utf8("准备导出..."));
    layout->add_child(download_status_label);

    download_progress_bar = memnew(ProgressBar);
    download_progress_bar->set_min(0.0);
    download_progress_bar->set_max(100.0);
    download_progress_bar->set_value(0.0);
    download_progress_bar->set_show_percentage(true);
    download_progress_bar->set_custom_minimum_size(Vector2(420, 24));
    layout->add_child(download_progress_bar);

    download_panel->add_child(layout);

    templates::TemplateManager *tm = templates::TemplateManager::get_singleton();
    if (tm) {
        Callable progress_cb = callable_mp(this, &WeChatExportPlatform::_on_template_download_progress);
        Callable finish_cb = callable_mp(this, &WeChatExportPlatform::_on_template_download_finished);
        if (!tm->is_connected("template_download_progress", progress_cb)) {
            tm->connect("template_download_progress", progress_cb);
        }
        if (!tm->is_connected("template_download_finished", finish_cb)) {
            tm->connect("template_download_finished", finish_cb);
        }
    }

    RenderingServer *rs = RenderingServer::get_singleton();
    if (rs) {
        rs->force_draw(false, 0.0);
    }
}

void WeChatExportPlatform::_hide_download_progress_dialog() {
    templates::TemplateManager *tm = templates::TemplateManager::get_singleton();
    if (tm) {
        Callable progress_cb = callable_mp(this, &WeChatExportPlatform::_on_template_download_progress);
        Callable finish_cb = callable_mp(this, &WeChatExportPlatform::_on_template_download_finished);
        if (tm->is_connected("template_download_progress", progress_cb)) {
            tm->disconnect("template_download_progress", progress_cb);
        }
        if (tm->is_connected("template_download_finished", finish_cb)) {
            tm->disconnect("template_download_finished", finish_cb);
        }
    }

    if (download_overlay) {
        download_overlay->queue_free();
        download_overlay = nullptr;
    }
    download_panel = nullptr;
    download_status_label = nullptr;
    download_progress_bar = nullptr;
    active_download_filename = "";
    export_progress_value = 0.0;

    RenderingServer *rs = RenderingServer::get_singleton();
    if (rs) {
        rs->force_draw(false, 0.0);
    }
}

void WeChatExportPlatform::_set_export_progress(double p_progress, const String &p_text) {
    double clamped = p_progress;
    if (clamped < 0.0) {
        clamped = 0.0;
    }
    if (clamped > 100.0) {
        clamped = 100.0;
    }

    export_progress_value = clamped;

    if (download_progress_bar) {
        download_progress_bar->set_value(clamped);
    }
    if (download_status_label && !p_text.is_empty()) {
        download_status_label->set_text(p_text);
    }

    RenderingServer *rs = RenderingServer::get_singleton();
    if (rs) {
        rs->force_draw(false, 0.0);
    }
}

void WeChatExportPlatform::_simulate_export_progress(double p_from, double p_to, int p_duration_ms, const String &p_text) {
    if (!download_overlay) {
        return;
    }

    int steps = p_duration_ms / 40;
    if (steps < 1) {
        steps = 1;
    }

    for (int i = 1; i <= steps; i++) {
        double t = double(i) / double(steps);
        double value = p_from + (p_to - p_from) * t;
        _set_export_progress(value, p_text);
        OS::get_singleton()->delay_msec(40);
    }
}

void WeChatExportPlatform::_on_template_download_progress(const String &p_filename, double p_progress) {
    if (!download_overlay || p_filename != active_download_filename) {
        return;
    }

    double clamped = p_progress;
    if (clamped < 0.0) {
        clamped = 0.0;
    }
    if (clamped > 1.0) {
        clamped = 1.0;
    }

    int percent = int(clamped * 100.0 + 0.5);
    double mapped_progress = 18.0 + clamped * 64.0;
    _set_export_progress(mapped_progress, String::utf8("正在下载模板: ") + p_filename + " (" + String::num_int64(percent) + "%)");
}

void WeChatExportPlatform::_on_template_download_finished(const String &p_filename, bool p_success) {
    if (!download_overlay || p_filename != active_download_filename) {
        return;
    }

    if (p_success) {
        _set_export_progress(82.0, String::utf8("模板下载完成，正在继续导出..."));
    } else {
        _set_export_progress(export_progress_value, String::utf8("模板下载失败"));
    }
}

PackedStringArray WeChatExportPlatform::_get_preset_features(const Ref<EditorExportPreset> &p_preset) const {
    PackedStringArray features;
    features.append("web");
    features.append("wechat");
    features.append("wasm");
    return features;
}

TypedArray<Dictionary> WeChatExportPlatform::_get_export_options() const {
    TypedArray<Dictionary> options;

    // 微信小游戏基本信息
    Dictionary appid;
    appid["name"] = String::utf8("微信小游戏/游戏_AppID");
    appid["type"] = Variant::STRING;
    appid["default_value"] = "wxf40904ea6120ad08";
    options.append(appid);

    Dictionary project_name;
    project_name["name"] = String::utf8("微信小游戏/小游戏项目名");
    project_name["type"] = Variant::STRING;
    project_name["default_value"] = "GodotMiniGame";
    options.append(project_name);

    Dictionary orientation;
    orientation["name"] = String::utf8("微信小游戏/游戏方向");
    orientation["type"] = Variant::STRING;
    orientation["hint"] = PROPERTY_HINT_ENUM;
    orientation["hint_string"] = "portrait,landscape";
    orientation["default_value"] = "portrait";
    options.append(orientation);

    // 资源信息
    Dictionary cover_image;
    cover_image["name"] = String::utf8("资源信息/启动封面背景图");
    cover_image["type"] = Variant::STRING;
    cover_image["hint"] = PROPERTY_HINT_FILE;
    cover_image["hint_string"] = "*.png,*.jpg,*.jpeg";
    cover_image["default_value"] = "";
    options.append(cover_image);

    Dictionary logo_image;
    logo_image["name"] = String::utf8("资源信息/启动封面logo");
    logo_image["type"] = Variant::STRING;
    logo_image["hint"] = PROPERTY_HINT_FILE;
    logo_image["hint_string"] = "*.png,*.jpg,*.jpeg";
    logo_image["default_value"] = "";
    options.append(logo_image);

    return options;
}

String WeChatExportPlatform::_get_name() const {
    return String::utf8("小游戏");
}

String WeChatExportPlatform::_get_os_name() const {
    return "WeChatMiniGame";
}

Ref<Texture2D> WeChatExportPlatform::_get_logo() const {
    static bool printed = false;
    if (!printed) {
        UtilityFunctions::print("[GodotMinigame][WeChatExportPlatform] _get_logo called, logo valid=", logo.is_valid());
        printed = true;
    }
    return logo;
}

Error WeChatExportPlatform::_export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags) {
    TOOLKIT_LOG("WeChatExportPlatform: Starting project export to ", p_path);
    
    String export_dir = p_path;
    if (!p_path.get_extension().is_empty()) {
        export_dir = p_path.get_base_dir();
    }
    
    Ref<DirAccess> da = DirAccess::open("res://");
    if (da.is_valid()) {
        da->make_dir_recursive(export_dir);
    }

    _show_download_progress_dialog("__export__");
    _set_export_progress(2.0, String::utf8("准备导出..."));
    _simulate_export_progress(2.0, 12.0, 220, String::utf8("正在检查导出环境..."));

    // 1. 设置模板
    Error err = _setup_wechat_template(p_preset, export_dir);
    if (err != OK) {
        _set_export_progress(100.0, String::utf8("导出失败"));
        OS::get_singleton()->delay_msec(180);
        _hide_download_progress_dialog();
        return err;
    }

    if (export_progress_value < 70.0) {
        _simulate_export_progress(export_progress_value, 70.0, 260, String::utf8("模板就绪，准备打包..."));
    }

    // 2. 导出资源包到 engine/demo-pck.bin
    String engine_dir = export_dir.path_join("engine");
    if (da.is_valid() && !da->dir_exists(engine_dir)) {
        da->make_dir_recursive(engine_dir);
    }

    String res_path = engine_dir.path_join("demo-pck.bin");
    TOOLKIT_LOG("WeChatExportPlatform: Exporting main pack to ", res_path);
    _simulate_export_progress(export_progress_value, 92.0, 280, String::utf8("正在打包资源..."));
    save_pack(p_preset, p_debug, res_path);
    _set_export_progress(100.0, String::utf8("导出完成"));
    OS::get_singleton()->delay_msec(200);
    _hide_download_progress_dialog();

    TOOLKIT_LOG("WeChatExportPlatform: Project export completed.");
    return OK;
}

Error WeChatExportPlatform::_export_pack(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags) {
    // 拦截“导出 PCK/ZIP”按钮
    String target_path = p_path;
    if (p_path.get_extension() == "pck") {
        target_path = p_path.get_basename() + ".bin";
    }
    
    TOOLKIT_LOG("WeChatExportPlatform: Intercepted resource export to: ", target_path);
    
    String project_dir = target_path.get_base_dir();
    if (FileAccess::file_exists(project_dir.path_join("game.json"))) {
        _modify_json_configs(p_preset, project_dir);
        _copy_export_images(p_preset, project_dir);
    }

    save_pack(p_preset, p_debug, target_path);
    return OK;
}

Error WeChatExportPlatform::_export_zip(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags) {
    TOOLKIT_LOG("WeChatExportPlatform: Exporting as ZIP to: ", p_path);
    save_zip(p_preset, p_debug, p_path);
    return OK;
}

Error WeChatExportPlatform::_setup_wechat_template(const Ref<EditorExportPreset> &p_preset, const String &p_path) {
    String game_json_path = p_path.path_join("game.json");
    
    if (!FileAccess::file_exists(game_json_path)) {
        TOOLKIT_LOG("WeChatExportPlatform: Setting up template at ", p_path);
        
        if (!Engine::get_singleton()->has_singleton("TemplateManager")) {
            UtilityFunctions::push_warning("TemplateManager missing.");
            return ERR_UNCONFIGURED;
        }

        templates::TemplateManager* tm = templates::TemplateManager::get_singleton();
        Error init_err = tm->initialize_template_system();
        if (init_err != OK) {
            String msg = "Template system init failed: " + _describe_export_error(init_err) + " (" + String::num_int64(init_err) + ")";
            UtilityFunctions::push_warning(msg);
            TOOLKIT_LOG("WeChatExportPlatform: ", msg);
            return init_err;
        }

        Error remote_versions_err = tm->load_versions_from_remote_sync();
        if (remote_versions_err != OK) {
            TOOLKIT_LOG("WeChatExportPlatform: remote versions refresh failed, falling back to cached versions. error=", remote_versions_err);
        }

        String best_template_ref = tm->get_best_available_template_for_editor();
        
        if (best_template_ref.is_empty()) {
            UtilityFunctions::push_warning("No template available for current editor version.");
            return ERR_FILE_NOT_FOUND;
        }

        if (best_template_ref.begins_with("remote://")) {
            String filename = best_template_ref.replace("remote://", "");
            TOOLKIT_LOG("WeChatExportPlatform: template not found locally, downloading ", filename);

            active_download_filename = filename;
            _set_export_progress(16.0, String::utf8("模板缺失，准备下载..."));
            Error download_err = tm->download_template_sync(filename, "");
            active_download_filename = "__export__";

            if (download_err != OK) {
                String msg = "Template download failed: " + _describe_export_error(download_err) + " (" + String::num_int64(download_err) + ")";
                UtilityFunctions::push_warning(msg);
                TOOLKIT_LOG("WeChatExportPlatform: ", msg);
                return download_err;
            }

            best_template_ref = tm->get_template_path(filename);
            if (best_template_ref.is_empty() || best_template_ref.begins_with("remote://")) {
                UtilityFunctions::push_warning("Template download finished but local template file is still missing.");
                return ERR_FILE_NOT_FOUND;
            }
        }

        TOOLKIT_LOG("WeChatExportPlatform: extracting template from ", best_template_ref);
        _simulate_export_progress(export_progress_value, 88.0, 220, String::utf8("正在解压模板..."));
        Error extract_err = tm->extract_template(best_template_ref, p_path);
        if (extract_err != OK) {
            String msg = "Template extraction failed: " + _describe_export_error(extract_err) + " (" + String::num_int64(extract_err) + ")";
            UtilityFunctions::push_warning(msg);
            TOOLKIT_LOG("WeChatExportPlatform: ", msg);
            return extract_err;
        }

        _modify_json_configs(p_preset, p_path);
        _copy_export_images(p_preset, p_path);
    }
    return OK;
}

void WeChatExportPlatform::_modify_json_configs(const Ref<EditorExportPreset> &p_preset, const String &p_path) {
    String project_config_path = p_path.path_join("project.config.json");
    if (FileAccess::file_exists(project_config_path)) {
        Ref<FileAccess> f = FileAccess::open(project_config_path, FileAccess::READ);
        if (f.is_valid()) {
            String content = f->get_as_text();
            f->close();

            Variant json_var = JSON::parse_string(content);
            if (json_var.get_type() == Variant::DICTIONARY) {
                Dictionary dict = json_var;
                dict["appid"] = p_preset->get(String::utf8("微信小游戏/游戏_AppID"));
                dict["projectname"] = p_preset->get(String::utf8("微信小游戏/小游戏项目名"));

                String new_content = JSON::stringify(dict, "    ");
                f = FileAccess::open(project_config_path, FileAccess::WRITE);
                if (f.is_valid()) {
                    f->store_string(new_content);
                    f->close();
                }
            }
        }
    }

    String game_json_path = p_path.path_join("game.json");
    if (FileAccess::file_exists(game_json_path)) {
        Ref<FileAccess> f = FileAccess::open(game_json_path, FileAccess::READ);
        if (f.is_valid()) {
            String content = f->get_as_text();
            f->close();

            Variant json_var = JSON::parse_string(content);
            if (json_var.get_type() == Variant::DICTIONARY) {
                Dictionary dict = json_var;
                dict["deviceOrientation"] = p_preset->get(String::utf8("微信小游戏/游戏方向"));

                String new_content = JSON::stringify(dict, "    ");
                f = FileAccess::open(game_json_path, FileAccess::WRITE);
                if (f.is_valid()) {
                    f->store_string(new_content);
                    f->close();
                }
            }
        }
    }
}

void WeChatExportPlatform::_copy_export_images(const Ref<EditorExportPreset> &p_preset, const String &p_path) {
    String images_dir = p_path.path_join("images");
    Ref<DirAccess> da = DirAccess::open("res://");
    if (da.is_valid() && !da->dir_exists(images_dir)) {
        da->make_dir_recursive(images_dir);
    }

    String cover_src = p_preset->get(String::utf8("资源信息/启动封面背景图"));
    if (!cover_src.is_empty() && da.is_valid()) {
        String cover_dst = images_dir.path_join("background.jpg");
        da->copy(cover_src, cover_dst);
    }

    String logo_src = p_preset->get(String::utf8("资源信息/启动封面logo"));
    if (!logo_src.is_empty() && da.is_valid()) {
        String logo_dst = images_dir.path_join("logo.png");
        da->copy(logo_src, logo_dst);
    }
}

PackedStringArray WeChatExportPlatform::_get_platform_features() const {
    PackedStringArray features;
    features.append("wechat");
    return features;
}

PackedStringArray WeChatExportPlatform::_get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const {
    PackedStringArray extensions;
    extensions.append("bin");
    return extensions;
}

bool WeChatExportPlatform::_has_valid_export_configuration(const Ref<EditorExportPreset> &p_preset, bool p_debug) const {
    return true;
}

bool WeChatExportPlatform::_has_valid_project_configuration(const Ref<EditorExportPreset> &p_preset) const {
    return true;
}

String WeChatExportPlatform::_get_export_option_warning(const Ref<EditorExportPreset> &p_preset, const StringName &p_name) const {
    return "";
}

bool WeChatExportPlatform::_get_export_option_visibility(const Ref<EditorExportPreset> &p_preset, const String &p_option) const {
    return true;
}

WeChatExportPlatform::WeChatExportPlatform() {
#ifdef EMBED_RESOURCES
    UtilityFunctions::print("[GodotMinigame][WeChatExportPlatform] ctor, EMBED_RESOURCES=ON");
#else
    UtilityFunctions::print("[GodotMinigame][WeChatExportPlatform] ctor, EMBED_RESOURCES=OFF");
#endif
    logo = load_embedded_icon("resources/assets/mini_game_icon.png");
    UtilityFunctions::print("[GodotMinigame][WeChatExportPlatform] embedded logo valid=", logo.is_valid());
    if (!logo.is_valid()) {
        logo = _load_wechat_logo_fallback();
    }
    logo = _normalize_export_logo_size(logo);
    if (!logo.is_valid()) {
        UtilityFunctions::printerr("[GodotMinigame][WeChatExportPlatform] logo is still null after all fallbacks");
    }
}

WeChatExportPlatform::~WeChatExportPlatform() {
    _hide_download_progress_dialog();
}

} // namespace editor
} // namespace toolkit
