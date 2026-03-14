#include "editor/settings_panel.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/v_box_container.hpp>

using namespace godot;

namespace toolkit {
namespace editor {

SettingsPanel::SettingsPanel() {
	set_name("Settings");

	set_anchors_preset(Control::PRESET_FULL_RECT);
	add_theme_constant_override("margin_left", 4);
	add_theme_constant_override("margin_right", 4);
	add_theme_constant_override("margin_top", 4);
	add_theme_constant_override("margin_bottom", 4);
}

SettingsPanel::~SettingsPanel() {
}

void SettingsPanel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("refresh_distribution_info"), &SettingsPanel::refresh_distribution_info);
	ClassDB::bind_method(D_METHOD("_on_distribution_provider_selected", "index"), &SettingsPanel::_on_distribution_provider_selected);
	ClassDB::bind_method(D_METHOD("_on_save_distribution_config_pressed"), &SettingsPanel::_on_save_distribution_config_pressed);
	ClassDB::bind_method(D_METHOD("_on_refresh_versions_pressed"), &SettingsPanel::_on_refresh_versions_pressed);
	ClassDB::bind_method(D_METHOD("_on_versions_loaded"), &SettingsPanel::_on_versions_loaded);
}

void SettingsPanel::_ready() {
	create_interface();

	if (Engine::get_singleton()->has_singleton("TemplateManager")) {
		Object *template_manager = Engine::get_singleton()->get_singleton("TemplateManager");
		if (template_manager && !template_manager->is_connected("versions_loaded", callable_mp(this, &SettingsPanel::_on_versions_loaded))) {
			template_manager->connect("versions_loaded", callable_mp(this, &SettingsPanel::_on_versions_loaded));
		}
	}

	call_deferred("refresh_distribution_info");
}

void SettingsPanel::create_interface() {
	content_control = memnew(Control);
	content_control->set_anchors_preset(Control::PRESET_FULL_RECT);
	add_child(content_control);

	main_vbox = memnew(VBoxContainer);
	main_vbox->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	main_vbox->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	content_control->add_child(main_vbox);

	plugin_update_label = memnew(Label);
	plugin_update_label->set_text(String::utf8("插件更新方式：请通过 Godot Asset Library 管理"));
	plugin_update_label->set_custom_minimum_size(Vector2(0, 30));
	main_vbox->add_child(plugin_update_label);

	distribution_provider_row = memnew(HBoxContainer);
	distribution_provider_row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	main_vbox->add_child(distribution_provider_row);

	distribution_provider_title_label = memnew(Label);
	distribution_provider_title_label->set_text(String::utf8("模板分发源"));
	distribution_provider_title_label->set_custom_minimum_size(Vector2(72, 30));
	distribution_provider_row->add_child(distribution_provider_title_label);

	distribution_provider_selector = memnew(OptionButton);
	distribution_provider_selector->set_custom_minimum_size(Vector2(120, 0));
	distribution_provider_selector->add_item("GitHub");
	distribution_provider_selector->add_item("Gitee");
	distribution_provider_selector->connect("item_selected", callable_mp(this, &SettingsPanel::_on_distribution_provider_selected));
	distribution_provider_row->add_child(distribution_provider_selector);

	Control *distribution_provider_spacer = memnew(Control);
	distribution_provider_spacer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	distribution_provider_row->add_child(distribution_provider_spacer);

	save_config_button = memnew(Button);
	save_config_button->set_text(String::utf8("保存并刷新"));
	save_config_button->connect("pressed", callable_mp(this, &SettingsPanel::_on_save_distribution_config_pressed));
	distribution_provider_row->add_child(save_config_button);

	refresh_versions_button = memnew(Button);
	refresh_versions_button->set_text(String::utf8("刷新远端索引"));
	refresh_versions_button->connect("pressed", callable_mp(this, &SettingsPanel::_on_refresh_versions_pressed));
	distribution_provider_row->add_child(refresh_versions_button);

	Control *distribution_repo_gap = memnew(Control);
	distribution_repo_gap->set_custom_minimum_size(Vector2(0, 5));
	main_vbox->add_child(distribution_repo_gap);

	release_config_row = memnew(HBoxContainer);
	release_config_row->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	main_vbox->add_child(release_config_row);

	owner_label = memnew(Label);
	owner_label->set_text("Owner");
	owner_label->set_custom_minimum_size(Vector2(48, 30));
	release_config_row->add_child(owner_label);

	owner_input = memnew(LineEdit);
	owner_input->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	owner_input->set_custom_minimum_size(Vector2(120, 0));
	owner_input->set_stretch_ratio(1.2f);
	owner_input->set_placeholder("citizenll");
	release_config_row->add_child(owner_input);

	repo_label = memnew(Label);
	repo_label->set_text("Repo");
	repo_label->set_custom_minimum_size(Vector2(44, 30));
	release_config_row->add_child(repo_label);

	repo_input = memnew(LineEdit);
	repo_input->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	repo_input->set_custom_minimum_size(Vector2(160, 0));
	repo_input->set_stretch_ratio(1.6f);
	repo_input->set_placeholder("toolkit-addons");
	release_config_row->add_child(repo_input);

	tag_label = memnew(Label);
	tag_label->set_text("Tag");
	tag_label->set_custom_minimum_size(Vector2(36, 30));
	release_config_row->add_child(tag_label);

	tag_input = memnew(LineEdit);
	tag_input->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	tag_input->set_custom_minimum_size(Vector2(120, 0));
	tag_input->set_stretch_ratio(1.0f);
	tag_input->set_placeholder("latest / 4.5.1");
	release_config_row->add_child(tag_input);

	action_status_label = memnew(Label);
	action_status_label->set_text(String::utf8("配置已就绪"));
	action_status_label->set_custom_minimum_size(Vector2(0, 30));
	main_vbox->add_child(action_status_label);

	Control *spacer = memnew(Control);
	spacer->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	main_vbox->add_child(spacer);
}

void SettingsPanel::refresh_distribution_info() {
	String provider = "github";
	Dictionary config;

	if (Engine::get_singleton()->has_singleton("TemplateManager")) {
		Object *template_manager = Engine::get_singleton()->get_singleton("TemplateManager");
		if (template_manager) {
			if (template_manager->has_method("get_distribution_provider")) {
				provider = template_manager->call("get_distribution_provider");
			}

			if (template_manager->has_method("get_current_release_config")) {
				config = template_manager->call("get_current_release_config");
			}
		}
	}

	if (distribution_provider_selector) {
		distribution_provider_selector->select(provider == "gitee" ? 1 : 0);
	}
	if (owner_input) {
		owner_input->set_text(String(config.get("owner", "")));
	}
	if (repo_input) {
		repo_input->set_text(String(config.get("repo", "")));
	}
	if (tag_input) {
		tag_input->set_text(String(config.get("release_tag", "latest")));
	}
}

void SettingsPanel::_on_distribution_provider_selected(int index) {
	String provider = index == 1 ? String("gitee") : String("github");

	if (Engine::get_singleton()->has_singleton("TemplateManager")) {
		Object *template_manager = Engine::get_singleton()->get_singleton("TemplateManager");
		if (template_manager && template_manager->has_method("set_distribution_provider")) {
			template_manager->call("set_distribution_provider", provider);
		}
	}

	if (action_status_label) {
		action_status_label->set_text(String::utf8("已切换源，正在刷新索引..."));
	}
	refresh_distribution_info();
}

void SettingsPanel::_on_save_distribution_config_pressed() {
	if (!owner_input || !repo_input || !tag_input) {
		return;
	}

	String owner = owner_input->get_text().strip_edges();
	String repo = repo_input->get_text().strip_edges();
	String release_tag = tag_input->get_text().strip_edges();

	if (owner.is_empty() || repo.is_empty()) {
		if (action_status_label) {
			action_status_label->set_text(String::utf8("owner 和 repo 不能为空"));
		}
		return;
	}
	if (release_tag.is_empty()) {
		release_tag = "latest";
		tag_input->set_text(release_tag);
	}

	if (Engine::get_singleton()->has_singleton("TemplateManager")) {
		Object *template_manager = Engine::get_singleton()->get_singleton("TemplateManager");
		if (template_manager && template_manager->has_method("set_current_release_config")) {
			template_manager->call("set_current_release_config", owner, repo, release_tag);
		}
	}

	if (action_status_label) {
		action_status_label->set_text(String::utf8("配置已保存，正在刷新索引..."));
	}
	refresh_distribution_info();
}

void SettingsPanel::_on_refresh_versions_pressed() {
	int error_code = ERR_UNCONFIGURED;

	if (Engine::get_singleton()->has_singleton("TemplateManager")) {
		Object *template_manager = Engine::get_singleton()->get_singleton("TemplateManager");
		if (template_manager && template_manager->has_method("refresh_versions")) {
			Variant result = template_manager->call("refresh_versions");
			if (result.get_type() == Variant::INT) {
				error_code = int(result);
			}
		}
	}

	if (action_status_label) {
		if (error_code == OK) {
			action_status_label->set_text(String::utf8("已发起远端索引刷新"));
		} else {
			action_status_label->set_text(String::utf8("刷新启动失败，错误码: ") + String::num_int64(error_code));
		}
	}
	refresh_distribution_info();
}

void SettingsPanel::_on_versions_loaded() {
	if (action_status_label) {
		action_status_label->set_text(String::utf8("远端索引刷新完成"));
	}
	refresh_distribution_info();
}

} // namespace editor
} // namespace toolkit
