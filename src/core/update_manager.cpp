#include "core/update_manager.h"
#include "core/toolkit_core.h"
#include "core/logging.h"
#include "templates/template_manager.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/zip_reader.hpp>
#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/main_loop.hpp>
#include <godot_cpp/classes/http_client.hpp>
#include <godot_cpp/classes/timer.hpp>
#include <godot_cpp/classes/tls_options.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace toolkit {

UpdateManager *UpdateManager::singleton = nullptr;

UpdateManager *UpdateManager::get_singleton() {
    return singleton;
}

UpdateManager::UpdateManager() {
    if (singleton == nullptr) {
        singleton = this;
    }

    // Don't create HTTP nodes in constructor
    // They will be created later in initialize()
    version_checker = nullptr;
    downloader = nullptr;
    http_client = nullptr;
    polling_timer = nullptr;

    // Initialize tracking array
    active_http_nodes = Array();

    // Initialize progress tracking variables
    download_total_bytes = 0;
    download_received_bytes = 0;
    is_downloading_with_progress = false;
}

UpdateManager::~UpdateManager() {
    // CRITICAL FIX: Actively clean up HTTPRequest and Timer nodes before engine shutdown
    // This prevents potential crashes during editor termination
    cleanup_http_nodes();

    // The http_client, however, is not a Node and was manually allocated, so it should be deleted.
    if (http_client) {
        memdelete(http_client);
        http_client = nullptr;
    }

    if (singleton == this) {
        singleton = nullptr;
    }
}

void UpdateManager::_bind_methods() {
    using namespace godot;

    ClassDB::bind_method(D_METHOD("initialize"), &UpdateManager::initialize);
    ClassDB::bind_method(D_METHOD("check_for_updates", "local_version"), &UpdateManager::check_for_updates);
    ClassDB::bind_method(D_METHOD("download_update"), &UpdateManager::download_update);
    ClassDB::bind_method(D_METHOD("cancel_download"), &UpdateManager::cancel_download);
    ClassDB::bind_method(D_METHOD("perform_update"), &UpdateManager::perform_update);
    ClassDB::bind_method(D_METHOD("restart_editor_for_update"), &UpdateManager::restart_editor_for_update);
    ClassDB::bind_method(D_METHOD("get_local_version"), &UpdateManager::get_local_version);
    ClassDB::bind_method(D_METHOD("get_remote_version_info"), &UpdateManager::get_remote_version_info);
    ClassDB::bind_method(D_METHOD("get_current_state"), &UpdateManager::get_current_state);
   
    ClassDB::bind_method(D_METHOD("_on_version_check_completed"), &UpdateManager::_on_version_check_completed);
    ClassDB::bind_method(D_METHOD("_on_download_completed"), &UpdateManager::_on_download_completed);
    ClassDB::bind_method(D_METHOD("update_polling"), &UpdateManager::update_polling);
   
    ADD_SIGNAL(MethodInfo("update_available", PropertyInfo(Variant::DICTIONARY, "version_info")));
    ADD_SIGNAL(MethodInfo("download_finished", PropertyInfo(Variant::BOOL, "success")));
    ADD_SIGNAL(MethodInfo("error", PropertyInfo(Variant::STRING, "message")));
    ADD_SIGNAL(MethodInfo("download_progress_changed", PropertyInfo(Variant::INT, "bytes_received"), PropertyInfo(Variant::INT, "total_bytes")));
    ADD_SIGNAL(MethodInfo("update_state_changed", PropertyInfo(Variant::INT, "new_state")));
   
    BIND_ENUM_CONSTANT(STATE_IDLE);
    BIND_ENUM_CONSTANT(STATE_CHECKING);
    BIND_ENUM_CONSTANT(STATE_UPDATE_AVAILABLE);
    BIND_ENUM_CONSTANT(STATE_DOWNLOADING);
    BIND_ENUM_CONSTANT(STATE_DOWNLOADED);
    BIND_ENUM_CONSTANT(STATE_ERROR);
   }

void UpdateManager::initialize() {
    using namespace godot;

    TOOLKIT_LOG("UpdateManager: Starting initialization...");

    // Check if already initialized
    if (version_checker && version_checker->get_parent()) {
        TOOLKIT_LOG("UpdateManager: Already initialized, skipping");
        return;
    }

    // Create HTTP nodes if they don't exist
    if (!version_checker) {
        version_checker = memnew(HTTPRequest);
        version_checker->set_name("UpdateManager_VersionChecker");
    }
    if (!downloader) {
        downloader = memnew(HTTPRequest);
        downloader->set_name("UpdateManager_Downloader");
    }
    if (!http_client) {
        http_client = memnew(HTTPClient);
    }
    if (!polling_timer) {
        polling_timer = memnew(Timer);
        polling_timer->set_name("UpdateManager_PollingTimer");
        polling_timer->set_wait_time(0.1); // Poll 10 times per second
        polling_timer->set_autostart(false);
    }

    // Get a safe parent node for HTTPRequest nodes
    Node* parent_node = nullptr;

    // Try to get editor interface only when running in editor context.
    if (Engine::get_singleton()->is_editor_hint()) {
        EditorInterface* editor = EditorInterface::get_singleton();
        if (editor) {
            Node* main_screen = editor->get_editor_main_screen();
            if (main_screen) {
                parent_node = main_screen;
                TOOLKIT_LOG("UpdateManager: Using editor main screen as parent");
            }
        }
    }

    // If editor interface failed, try engine singleton
    if (!parent_node && Engine::get_singleton()) {
        // Try to find the main loop
        MainLoop* main_loop = Engine::get_singleton()->get_main_loop();
        Node* main_loop_node = Object::cast_to<Node>(main_loop);
        if (main_loop_node) {
            parent_node = main_loop_node;
            TOOLKIT_LOG("UpdateManager: Using main loop as parent");
        }
    }

    if (parent_node) {
        // Add nodes to scene tree and connect signals
        if (!version_checker->get_parent()) {
            parent_node->add_child(version_checker);
            version_checker->connect("request_completed", callable_mp(this, &UpdateManager::_on_version_check_completed));
            track_http_node(version_checker);
        }

        if (!downloader->get_parent()) {
            parent_node->add_child(downloader);
            downloader->connect("request_completed", callable_mp(this, &UpdateManager::_on_download_completed));
            track_http_node(downloader);
        }

        if (!polling_timer->get_parent()) {
            parent_node->add_child(polling_timer);
            polling_timer->connect("timeout", callable_mp(this, &UpdateManager::update_polling));
            track_http_node(polling_timer);
        }

        TOOLKIT_LOG("UpdateManager: HTTPRequest nodes and Timer initialized successfully");
    } else {
        TOOLKIT_LOG("UpdateManager: No suitable parent found, deferring initialization");
        // Defer initialization and try again
        call_deferred("initialize");
    }
   }
   
   void UpdateManager::check_for_updates(const godot::String &p_local_version) {
    using namespace godot;

    if (current_state == STATE_CHECKING || current_state == STATE_DOWNLOADING) {
    	TOOLKIT_LOG("UpdateManager: Already checking/downloading, skipping");
    	return;
    }

    if (!version_checker || !version_checker->get_parent()) {
    	TOOLKIT_LOG("UpdateManager: Not properly initialized, attempting initialization");
    	initialize();
    	if (!version_checker || !version_checker->get_parent()) {
    		set_state(STATE_ERROR);
    		emit_signal("error", "UpdateManager initialization failed.");
    		return;
    	}
    }

    if (!is_properly_configured()) {
    	set_state(STATE_ERROR);
    	emit_signal("error", "UpdateManager not properly configured - release distribution is missing.");
    	return;
    }

    local_version = p_local_version;
    String update_url = resolve_distribution_asset_url("latest.json");

    TOOLKIT_LOG("UpdateManager: Checking for updates from: ", update_url);
    TOOLKIT_LOG("UpdateManager: Current version: ", local_version);

    // Configure HTTPRequest for better reliability
    version_checker->set_timeout(30.0); // 30 second timeout
    version_checker->set_download_chunk_size(4096);

    set_state(STATE_CHECKING);
    Error err = version_checker->request(update_url);

    if (err != OK) {
    	set_state(STATE_ERROR);
    	String error_msg = "Failed to start version check request. Error code: " + String::num_int64(err);
    	TOOLKIT_LOG("UpdateManager: ", error_msg);
    	emit_signal("error", error_msg);
    } else {
    	TOOLKIT_LOG("UpdateManager: Version check request started successfully");
    }
   }

void UpdateManager::_on_version_check_completed(int p_result, int p_response_code, const godot::PackedStringArray &p_headers, const godot::PackedByteArray &p_body) {
    using namespace godot;

    TOOLKIT_LOG("UpdateManager: Version check completed. Result: ", p_result, ", Response code: ", p_response_code);

    // Check for HTTP errors
    if (p_result != HTTPRequest::RESULT_SUCCESS) {
    	set_state(STATE_ERROR);
    	String error_msg = "Version check failed. HTTP result: " + String::num(p_result);
    	TOOLKIT_LOG("UpdateManager: ", error_msg);
    	emit_signal("error", error_msg);
    	return;
    }

    if (p_response_code != 200) {
    	set_state(STATE_ERROR);
    	String error_msg = "Version check failed with HTTP status: " + String::num(p_response_code);
    	TOOLKIT_LOG("UpdateManager: ", error_msg);
    	emit_signal("error", error_msg);
    	return;
    }

    // Parse response body
    String response_text = p_body.get_string_from_utf8();
    TOOLKIT_LOG("UpdateManager: Received response: ", response_text.substr(0, 200), "...");

    Ref<JSON> json = memnew(JSON);
    Error err = json->parse(response_text);
    if (err != OK) {
    	set_state(STATE_ERROR);
    	String error_msg = "Failed to parse remote version JSON. Parse error: " + String::num(err);
    	TOOLKIT_LOG("UpdateManager: ", error_msg);
    	emit_signal("error", error_msg);
    	return;
    }

    Variant result = json->get_data();
    if (result.get_type() != Variant::DICTIONARY) {
    	set_state(STATE_ERROR);
    	emit_signal("error", "Remote version data is not a dictionary.");
    	return;
    }

    remote_version_info = result;
    String remote_v_str = remote_version_info.get("version", "0.0.0");

    TOOLKIT_LOG("UpdateManager: Remote version: ", remote_v_str, ", Local version: ", local_version);

    // Simple string comparison for version checking
    if (remote_v_str != local_version && !remote_v_str.is_empty()) {
    	set_state(STATE_UPDATE_AVAILABLE);
    	TOOLKIT_LOG("UpdateManager: Update available!");
    	emit_signal("update_available", remote_version_info);
    } else {
    	set_state(STATE_UP_TO_DATE);
    	TOOLKIT_LOG("UpdateManager: No update needed, already up to date.");
    }
   }

void UpdateManager::download_update() {
    using namespace godot;

    if (current_state != STATE_UPDATE_AVAILABLE) {
    	TOOLKIT_LOG("UpdateManager: Download called but no update available. Current state: ", current_state);
    	return;
    }

    if (!downloader || !downloader->get_parent()) {
    	TOOLKIT_LOG("UpdateManager: Downloader not initialized, attempting initialization");
    	initialize();
    	if (!downloader || !downloader->get_parent()) {
    		set_state(STATE_ERROR);
    		emit_signal("error", "UpdateManager initialization failed for download.");
    		return;
    	}
    }

    String platform = OS::get_singleton()->get_name().to_lower();
    String arch = Engine::get_singleton()->get_architecture_name();
    String platform_key;

    if (platform == "macos") {
        platform_key = "macos-universal";
    } else {
        platform_key = platform + "-" + arch;
    }

    TOOLKIT_LOG("UpdateManager: Looking for platform key: ", platform_key);

    Dictionary platforms = remote_version_info.get("platforms", Dictionary());
    if (!platforms.has(platform_key)) {
    	set_state(STATE_ERROR);
    	String error_msg = "No update package found for platform: " + platform_key;
    	TOOLKIT_LOG("UpdateManager: ", error_msg);
    	emit_signal("error", error_msg);
    	return;
    }

    Dictionary platform_data = platforms[platform_key];
    String asset_name = resolve_platform_asset_name(platform_data);
    if (asset_name.is_empty()) {
    	set_state(STATE_ERROR);
    	String error_msg = "Update asset is missing for platform: " + platform_key;
    	TOOLKIT_LOG("UpdateManager: ", error_msg);
    	emit_signal("error", error_msg);
    	return;
    }

    String full_url = resolve_distribution_asset_url(asset_name);
    if (full_url.is_empty()) {
        set_state(STATE_ERROR);
        String error_msg = "Failed to build update URL for asset: " + asset_name;
        TOOLKIT_LOG("UpdateManager: ", error_msg);
        emit_signal("error", error_msg);
        return;
    }
    String download_path = "user://toolkit_update.zip";

    TOOLKIT_LOG("UpdateManager: Starting download from: ", full_url);
    TOOLKIT_LOG("UpdateManager: Download destination: ", download_path);

    set_state(STATE_DOWNLOADING);

    // Configure HTTPRequest for download
    downloader->set_download_file(download_path);
    downloader->set_timeout(60.0); // 60-second timeout for downloads
    downloader->set_download_chunk_size(16384); // 16KB chunk size

    Error err = downloader->request(full_url);

    if (err != OK) {
        set_state(STATE_ERROR);
        String error_msg = "Failed to start download request. Error code: " + String::num_int64(err);
        TOOLKIT_LOG("UpdateManager: ", error_msg);
        emit_signal("error", error_msg);
    } else {
        TOOLKIT_LOG("UpdateManager: Download request started successfully");
    }
}

void UpdateManager::_on_download_completed(int p_result, int p_response_code, const godot::PackedStringArray &p_headers, const godot::PackedByteArray &p_body) {
    using namespace godot;

    TOOLKIT_LOG("UpdateManager: Download completed. Result: ", p_result, ", Response code: ", p_response_code);

    if (p_result == HTTPRequest::RESULT_SUCCESS && p_response_code == 200) {
    	// Verify the downloaded file exists
    	String download_path = "user://toolkit_update.zip";
    	if (FileAccess::file_exists(download_path)) {
    		Ref<FileAccess> file = FileAccess::open(download_path, FileAccess::READ);
    		if (file.is_valid()) {
    			int64_t file_size = file->get_length();
    			file->close();
    			TOOLKIT_LOG("UpdateManager: Download successful. File size: ", file_size, " bytes");
    			set_state(STATE_DOWNLOADED);
    			emit_signal("download_finished", true);
    		} else {
    			set_state(STATE_ERROR);
    			emit_signal("download_finished", false);
    			emit_signal("error", "Downloaded file could not be opened.");
    		}
    	} else {
    		set_state(STATE_ERROR);
    		emit_signal("download_finished", false);
    		emit_signal("error", "Downloaded file not found.");
    	}
    } else {
    	set_state(STATE_ERROR);
    	emit_signal("download_finished", false);
    	String error_msg = "Download failed. HTTP result: " + String::num(p_result) + ", Status code: " + String::num(p_response_code);
    	TOOLKIT_LOG("UpdateManager: ", error_msg);
    	emit_signal("error", error_msg);
    }
   }

bool UpdateManager::is_properly_configured() const {
    return !resolve_distribution_asset_url("latest.json").is_empty();
}

String UpdateManager::resolve_distribution_asset_url(const String &asset_name) const {
    if (asset_name.strip_edges().is_empty()) {
        return "";
    }

    templates::TemplateManager *template_manager = templates::TemplateManager::get_singleton();
    if (!template_manager) {
        return "";
    }

    return template_manager->get_distribution_asset_url(asset_name);
}

String UpdateManager::resolve_platform_asset_name(const Dictionary &platform_data) const {
    String asset = platform_data.get("asset", "");
    if (!asset.strip_edges().is_empty()) {
        return asset.get_file();
    }

    // Backward compatibility for older latest.json format.
    String legacy_url = platform_data.get("url", "");
    if (legacy_url.strip_edges().is_empty()) {
        return "";
    }
    return legacy_url.get_file();
}

} // namespace toolkit

// _on_download_progress method removed - HTTPRequest doesn't support progress tracking in Godot 4.x

void toolkit::UpdateManager::set_state(UpdateState p_state) {
	if (current_state != p_state) {
		current_state = p_state;
		emit_signal("update_state_changed", current_state);
	}
}

void toolkit::UpdateManager::cancel_download() {
	if (current_state == STATE_DOWNLOADING && downloader) {
		downloader->cancel_request();
		set_state(STATE_UPDATE_AVAILABLE); // Revert to previous state
	}
}

bool toolkit::UpdateManager::perform_update() {
	using namespace godot;

	const String TEMP_DOWNLOAD_FILE = "user://toolkit_update.zip";
	if (!FileAccess::file_exists(TEMP_DOWNLOAD_FILE)) {
		UtilityFunctions::push_warning("Update file not found. Nothing to do.");
		return false;
	}

	TOOLKIT_LOG("--- Starting C++ update process ---");

	// 1. Unzip the package
	Ref<ZIPReader> zip_reader = memnew(ZIPReader);
	Error zip_err = zip_reader->open(TEMP_DOWNLOAD_FILE);
	if (zip_err != OK) {
		UtilityFunctions::push_error("Failed to open downloaded zip package.");
		return false;
	}

	PackedStringArray files = zip_reader->get_files();
	String addon_path = ProjectSettings::get_singleton()->globalize_path("res://addons/toolkit-addons/");

	for (int i = 0; i < files.size(); i++) {
		String file_path = files[i];
		String dest_path = addon_path.path_join(file_path);

		if (file_path.ends_with("/")) { // It's a directory
			DirAccess::make_dir_recursive_absolute(dest_path);
		} else { // It's a file
			DirAccess::make_dir_recursive_absolute(dest_path.get_base_dir());
			PackedByteArray data = zip_reader->read_file(file_path);
			Ref<FileAccess> file = FileAccess::open(dest_path, FileAccess::WRITE);
			if (file.is_valid()) {
				file->store_buffer(data);
				file->close();
			} else {
				UtilityFunctions::push_error("Failed to write file: " + dest_path);
				zip_reader->close();
				return false;
			}
		}
	}
	zip_reader->close();
	TOOLKIT_LOG("Unzipped all files successfully.");

	// 2. Update plugin.cfg version
	if (remote_version_info.has("version")) {
		String new_version = remote_version_info.get("version", "0.0.0");
		Ref<ConfigFile> config_file = memnew(ConfigFile);
		String config_path = "res://addons/toolkit-addons/plugin.cfg";
		if (config_file->load(config_path) == OK) {
			config_file->set_value("plugin", "version", new_version);
			config_file->save(config_path);
			TOOLKIT_LOG("Updated plugin.cfg to version: " + new_version);
		} else {
			UtilityFunctions::push_warning("Failed to load plugin.cfg to update version.");
		}
	}

	// 3. Clean up temporary file
	Ref<DirAccess> dir = DirAccess::open("user://");
	if (dir.is_valid()) {
		dir->remove(TEMP_DOWNLOAD_FILE.get_file());
	}

	TOOLKIT_LOG("--- C++ update process finished ---");
	return true;
}

void toolkit::UpdateManager::restart_editor_for_update() {
	if (perform_update()) {
		TOOLKIT_LOG("Update applied. Restarting editor...");
		if (!godot::Engine::get_singleton()->is_editor_hint()) {
			godot::UtilityFunctions::push_warning("Restart editor is only available in editor mode.");
			return;
		}
		godot::EditorInterface *editor = godot::EditorInterface::get_singleton();
		if (editor) {
			editor->restart_editor(true);
		} else {
			godot::UtilityFunctions::push_warning("EditorInterface is unavailable, restart skipped.");
		}
	} else {
		godot::UtilityFunctions::push_error("Failed to apply update. Restart aborted.");
	}
}

toolkit::UpdateManager::UpdateState toolkit::UpdateManager::get_current_state() const {
	return current_state;
}

godot::String toolkit::UpdateManager::get_local_version() const {
	return local_version;
}

godot::Dictionary toolkit::UpdateManager::get_remote_version_info() const {
	return remote_version_info;
}

// The HTTPClient-based download methods are no longer needed and can be removed.
// The update_polling method is also no longer needed for downloads.
void toolkit::UpdateManager::update_polling() {
    // This method can be kept for future polling needs, but is not used for downloads anymore.
}

// HTTP nodes lifecycle management implementation
void toolkit::UpdateManager::track_http_node(godot::Node* node) {
    if (node && !active_http_nodes.has(node)) {
        active_http_nodes.append(node);
    }
}

void toolkit::UpdateManager::cleanup_http_nodes() {
    using namespace godot;

    TOOLKIT_LOG("UpdateManager: Cleaning up ", active_http_nodes.size(), " HTTP nodes");

    for (int i = 0; i < active_http_nodes.size(); i++) {
        Object* obj = active_http_nodes[i];
        Node* node = Object::cast_to<Node>(obj);

        if (node && node->get_parent()) {
            TOOLKIT_LOG("UpdateManager: Removing HTTP node: ", node->get_name());

            // Disconnect all signals to prevent callbacks during destruction
            if (node->has_method("disconnect_all_signals")) {
                node->call("disconnect_all_signals");
            }

            // Remove from parent and queue for deletion
            node->get_parent()->remove_child(node);
            node->queue_free();
        }
    }

    active_http_nodes.clear();

    // Clear direct references
    version_checker = nullptr;
    downloader = nullptr;
    polling_timer = nullptr;

    TOOLKIT_LOG("UpdateManager: HTTP nodes cleanup completed");
}
