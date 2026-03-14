#pragma once

#include <godot_cpp/classes/margin_container.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/h_box_container.hpp>
#include <godot_cpp/classes/line_edit.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/option_button.hpp>
#include <godot_cpp/core/class_db.hpp>

namespace toolkit {
namespace editor {

class SettingsPanel : public godot::MarginContainer {
	GDCLASS(SettingsPanel, godot::MarginContainer);

private:
	godot::Control *content_control = nullptr;
	godot::VBoxContainer *main_vbox = nullptr;
	godot::Label *plugin_update_label = nullptr;
	godot::HBoxContainer *distribution_provider_row = nullptr;
	godot::Label *distribution_provider_title_label = nullptr;
	godot::OptionButton *distribution_provider_selector = nullptr;
	godot::HBoxContainer *release_config_row = nullptr;
	godot::Label *owner_label = nullptr;
	godot::LineEdit *owner_input = nullptr;
	godot::Label *repo_label = nullptr;
	godot::LineEdit *repo_input = nullptr;
	godot::Label *tag_label = nullptr;
	godot::LineEdit *tag_input = nullptr;
	godot::Button *save_config_button = nullptr;
	godot::Button *refresh_versions_button = nullptr;
	godot::Label *action_status_label = nullptr;

	void refresh_distribution_info();
	void _on_distribution_provider_selected(int index);
	void _on_save_distribution_config_pressed();
	void _on_refresh_versions_pressed();
	void _on_versions_loaded();

protected:
	static void _bind_methods();

public:
	SettingsPanel();
	~SettingsPanel();

	virtual void _ready() override;

	void create_interface();
};

} // namespace editor
} // namespace toolkit
