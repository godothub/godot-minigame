#include "core/toolkit_core.h"
#include "resources/resource_manager.h"
#include "filesystem/filesystem_interface.h"
#include "templates/template_manager.h" // Include the TemplateManager header
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/zip_reader.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/core/class_db.hpp>
#include "core/logging.h"

using namespace godot;
// Using namespace declarations to reduce verbosity
using namespace toolkit::resources;
using namespace toolkit::filesystem;
using namespace toolkit::templates;

namespace toolkit {
namespace core {

// Static member definitions
ToolkitCore* ToolkitCore::singleton = nullptr;

ToolkitCore* ToolkitCore::get_singleton() {
    return singleton;
}

// initialize() and shutdown() are removed as they are now handled by register_types.cpp

ToolkitCore::ToolkitCore() {
}

ToolkitCore::~ToolkitCore() {
}

// --- Template Management API ---

void ToolkitCore::initialize_template_system() {
    // Safe singleton access with engine registry check
    if (!Engine::get_singleton()->has_singleton("TemplateManager")) {
        TOOLKIT_LOG("ToolkitCore: TemplateManager singleton not available");
        return;
    }

    TemplateManager* tm = TemplateManager::get_singleton();
    if (tm) {
        tm->initialize_template_system();
    }
}

Array ToolkitCore::get_available_templates() const {
    if (!Engine::get_singleton()->has_singleton("TemplateManager")) {
        return Array();
    }

    TemplateManager* tm = TemplateManager::get_singleton();
    if (tm) {
        return tm->get_available_versions();
    }
    return Array();
}

String ToolkitCore::get_editor_template_status() const {
    if (!Engine::get_singleton()->has_singleton("TemplateManager")) {
        return "not_initialized";
    }

    TemplateManager* tm = TemplateManager::get_singleton();
    if (tm) {
        return tm->check_editor_template_status();
    }
    return "not_initialized";
}

bool ToolkitCore::has_template_updates() const {
    if (!Engine::get_singleton()->has_singleton("TemplateManager")) {
        return false;
    }

    TemplateManager* tm = TemplateManager::get_singleton();
    if (tm) {
        return tm->has_template_updates_available();
    }
    return false;
}

Array ToolkitCore::get_missing_templates() const {
    if (!Engine::get_singleton()->has_singleton("TemplateManager")) {
        return Array();
    }

    TemplateManager* tm = TemplateManager::get_singleton();
    if (tm) {
        return tm->get_missing_templates();
    }
    return Array();
}

Error ToolkitCore::download_template(const String& filename) {
    if (!Engine::get_singleton()->has_singleton("TemplateManager")) {
        return ERR_UNCONFIGURED;
    }

    TemplateManager* tm = TemplateManager::get_singleton();
    if (tm) {
        return tm->download_template(filename);
    }
    return ERR_UNCONFIGURED;
}


bool ToolkitCore::extract_template(const String& template_name, const String& output_dir) {
    TOOLKIT_LOG("ToolkitCore: Extracting template '", template_name, "' to '", output_dir, "'");

    // CRITICAL FIX: Safe singleton access with engine registry check
    if (!Engine::get_singleton()->has_singleton("TemplateManager")) {
        TOOLKIT_LOG_RICH("[color=red]ToolkitCore: TemplateManager singleton not available in engine registry[/color]");
        return false;
    }

    TemplateManager* tm = TemplateManager::get_singleton();
    if (tm) {
        String template_path = tm->get_template_path(template_name);
        TOOLKIT_LOG("ToolkitCore: Template path: '", template_path, "'");

        if (template_path.is_empty()) {
            TOOLKIT_LOG_RICH("[color=red]ToolkitCore: Template path is empty for '", template_name, "'[/color]");
            return false;
        }

        // Use the template manager's extract_template method which handles both embedded and file paths
        Error result = tm->extract_template(template_path, output_dir);
        bool success = (result == OK);

        TOOLKIT_LOG("ToolkitCore: Extraction result: ", result, " (success: ", success, ")");
        return success;
    }

    TOOLKIT_LOG_RICH("[color=red]ToolkitCore: Template manager not available[/color]");
    return false;
}

void ToolkitCore::_bind_methods() {
    // Template Management
    ClassDB::bind_method(D_METHOD("initialize_template_system"), &ToolkitCore::initialize_template_system);
    ClassDB::bind_method(D_METHOD("get_available_templates"), &ToolkitCore::get_available_templates);
    ClassDB::bind_method(D_METHOD("get_editor_template_status"), &ToolkitCore::get_editor_template_status);
    ClassDB::bind_method(D_METHOD("has_template_updates"), &ToolkitCore::has_template_updates);
    ClassDB::bind_method(D_METHOD("get_missing_templates"), &ToolkitCore::get_missing_templates);
    ClassDB::bind_method(D_METHOD("download_template", "filename"), &ToolkitCore::download_template);
    ClassDB::bind_method(D_METHOD("extract_template", "template_name", "output_dir"), &ToolkitCore::extract_template);
}

} // namespace core
} // namespace toolkit
