#include "register_types.h"
#include "core/logging.h"

#include <gdextension_interface.h>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/classes/engine.hpp>

// Include our classes
#include "core/toolkit_core.h"
#include "editor/toolkit_dock.h"
#include "editor/minigame_panel.h"
#include "editor/taptap_panel.h"
#include "editor/settings_panel.h"
#include "editor/wechat_export_platform.h"
#include "yaml/yaml.h"
#include "templates/template_manager.h"
#include "filesystem/user_data_helper.h"

using namespace godot;

// Using namespace declarations to reduce verbosity
using namespace toolkit::core;
using namespace toolkit::editor;
using namespace toolkit::templates;

// Pointers to our singletons
static toolkit::templates::TemplateManager* template_manager_singleton = nullptr;
static toolkit::core::GodotMinigameCore* godot_minigame_core_singleton = nullptr;

void initialize_gdextension_types(ModuleInitializationLevel p_level)
{
	switch (p_level) {
		case MODULE_INITIALIZATION_LEVEL_CORE: {
			break;
		}
		case MODULE_INITIALIZATION_LEVEL_SCENE: {
			// Editor-only plugin: avoid registering editor-facing singletons in game runtime.
			if (!Engine::get_singleton()->is_editor_hint()) {
				break;
			}

			// Register core utility classes that can be used anywhere
			ClassDB::register_class<YAML>();
			ClassDB::register_class<TemplateManager>();

			// Initialize and register TemplateManager singleton
			template_manager_singleton = memnew(TemplateManager);
			Engine::get_singleton()->register_singleton("TemplateManager", template_manager_singleton);
			break;
		}
		case MODULE_INITIALIZATION_LEVEL_EDITOR: {
			// Editor-only plugin: skip editor initialization in game runtime.
			if (!Engine::get_singleton()->is_editor_hint()) {
				break;
			}

			// Register editor-specific classes
			ClassDB::register_class<GodotMinigameCore>();
			ClassDB::register_class<GodotMinigameDock>();
			ClassDB::register_class<MinigamePanel>();
			ClassDB::register_class<TapTapPanel>();
			ClassDB::register_class<SettingsPanel>();
			ClassDB::register_class<WeChatExportPlatform>();

			// Initialize and register the main plugin singleton
			godot_minigame_core_singleton = memnew(GodotMinigameCore);
			Engine::get_singleton()->register_singleton("GodotMinigame", godot_minigame_core_singleton);

			TOOLKIT_LOG("Godot Minigame: Plugin initialized for Godot 4.4");
			break;
		}
		default:
			break;
	}
}

void uninitialize_gdextension_types(ModuleInitializationLevel p_level) {
	switch (p_level) {
		case MODULE_INITIALIZATION_LEVEL_EDITOR: {
			if (!Engine::get_singleton()->is_editor_hint()) {
				break;
			}

			if (Engine::get_singleton()->has_singleton("GodotMinigame")) {
				Engine::get_singleton()->unregister_singleton("GodotMinigame");
			}
			if (godot_minigame_core_singleton) {
				godot_minigame_core_singleton = nullptr;
			}

			TOOLKIT_LOG("Godot Minigame: Plugin terminated");
			break;
		}
		case MODULE_INITIALIZATION_LEVEL_SCENE: {
			if (!Engine::get_singleton()->is_editor_hint()) {
				break;
			}

			// TemplateManager is deleted last since other components depend on it
			if (Engine::get_singleton()->has_singleton("TemplateManager")) {
				Engine::get_singleton()->unregister_singleton("TemplateManager");
			}
			if (template_manager_singleton) {
				template_manager_singleton = nullptr;
			}
			break;
		}
		default:
			break;
	}
}

extern "C"
{
	// Initialization
	GDExtensionBool GDE_EXPORT godot_minigame_library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization)
	{
		GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);
		init_obj.register_initializer(initialize_gdextension_types);
		init_obj.register_terminator(uninitialize_gdextension_types);
		init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_CORE);

		return init_obj.init();
	}
}
