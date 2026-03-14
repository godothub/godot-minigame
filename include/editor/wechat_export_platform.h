#pragma once

#include <godot_cpp/classes/editor_export_platform_extension.hpp>
#include <godot_cpp/classes/editor_export_preset.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace godot {
class Control;
class Label;
class ProgressBar;
class PanelContainer;
}

namespace toolkit {
namespace editor {

class WeChatExportPlatform : public godot::EditorExportPlatformExtension {
	GDCLASS(WeChatExportPlatform, godot::EditorExportPlatformExtension);

private:
    godot::Ref<godot::Texture2D> logo;
    godot::Control *download_overlay = nullptr;
    godot::PanelContainer *download_panel = nullptr;
    godot::Label *download_status_label = nullptr;
    godot::ProgressBar *download_progress_bar = nullptr;
    godot::String active_download_filename;
    double export_progress_value = 0.0;

protected:
	static void _bind_methods();

public:
    // Virtual methods from EditorExportPlatformExtension
	virtual godot::PackedStringArray _get_preset_features(const godot::Ref<godot::EditorExportPreset> &p_preset) const override;
	virtual godot::TypedArray<godot::Dictionary> _get_export_options() const override;
	virtual godot::String _get_name() const override;
	virtual godot::String _get_os_name() const override;
	virtual godot::Ref<godot::Texture2D> _get_logo() const override;
	
    virtual godot::Error _export_project(const godot::Ref<godot::EditorExportPreset> &p_preset, bool p_debug, const godot::String &p_path, godot::BitField<godot::EditorExportPlatform::DebugFlags> p_flags) override;
	virtual godot::Error _export_pack(const godot::Ref<godot::EditorExportPreset> &p_preset, bool p_debug, const godot::String &p_path, godot::BitField<godot::EditorExportPlatform::DebugFlags> p_flags) override;
    virtual godot::Error _export_zip(const godot::Ref<godot::EditorExportPreset> &p_preset, bool p_debug, const godot::String &p_path, godot::BitField<godot::EditorExportPlatform::DebugFlags> p_flags) override;

    virtual godot::PackedStringArray _get_platform_features() const override;
	virtual godot::PackedStringArray _get_binary_extensions(const godot::Ref<godot::EditorExportPreset> &p_preset) const override;
    virtual bool _has_valid_export_configuration(const godot::Ref<godot::EditorExportPreset> &p_preset, bool p_debug) const override;
    virtual bool _has_valid_project_configuration(const godot::Ref<godot::EditorExportPreset> &p_preset) const override;
    virtual godot::String _get_export_option_warning(const godot::Ref<godot::EditorExportPreset> &p_preset, const godot::StringName &p_name) const override;
    virtual bool _get_export_option_visibility(const godot::Ref<godot::EditorExportPreset> &p_preset, const godot::String &p_option) const override;

    // Internal helpers
    godot::Error _setup_wechat_template(const godot::Ref<godot::EditorExportPreset> &p_preset, const godot::String &p_path);
    void _modify_json_configs(const godot::Ref<godot::EditorExportPreset> &p_preset, const godot::String &p_path);
    void _copy_export_images(const godot::Ref<godot::EditorExportPreset> &p_preset, const godot::String &p_path);
    void _show_download_progress_dialog(const godot::String &p_filename);
    void _hide_download_progress_dialog();
    void _set_export_progress(double p_progress, const godot::String &p_text = "");
    void _simulate_export_progress(double p_from, double p_to, int p_duration_ms, const godot::String &p_text = "");
    void _on_template_download_progress(const godot::String &p_filename, double p_progress);
    void _on_template_download_finished(const godot::String &p_filename, bool p_success);

	WeChatExportPlatform();
	~WeChatExportPlatform();
};

} // namespace editor
} // namespace toolkit
