#include "templates/template_manager.h"
#include "yaml/yaml.h"
#include "network/download_manager.h"
#include "filesystem/user_data_path.h"
#include "core/logging.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/http_request.hpp>
#include <godot_cpp/classes/http_client.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/timer.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/zip_reader.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_settings.hpp>
#include <godot_cpp/classes/main_loop.hpp>
#include <godot_cpp/classes/tls_options.hpp>
#include <godot_cpp/classes/v_box_container.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#ifdef EMBED_RESOURCES
#include "resources/embedded_resources.h"
#endif

using namespace godot;
// Reduce namespace verbosity
using namespace toolkit::filesystem;

namespace toolkit {
namespace templates {

static bool _parse_http_url(const String &url, String &host, int &port, String &path, bool &use_tls);

namespace {
constexpr const char *TOOLKIT_EDITOR_METADATA_SECTION = "toolkit_addons";
constexpr const char *TOOLKIT_EDITOR_METADATA_DISTRIBUTION_PROVIDER = "distribution_provider";
constexpr const char *TOOLKIT_EDITOR_METADATA_GITHUB_OWNER = "github_owner";
constexpr const char *TOOLKIT_EDITOR_METADATA_GITHUB_REPO = "github_repo";
constexpr const char *TOOLKIT_EDITOR_METADATA_GITHUB_RELEASE_TAG = "github_release_tag";
constexpr const char *TOOLKIT_EDITOR_METADATA_GITEE_OWNER = "gitee_owner";
constexpr const char *TOOLKIT_EDITOR_METADATA_GITEE_REPO = "gitee_repo";
constexpr const char *TOOLKIT_EDITOR_METADATA_GITEE_RELEASE_TAG = "gitee_release_tag";

String _sanitize_cache_component(const String &value) {
    String sanitized = value.strip_edges().to_lower();
    const char *invalid_chars[] = {"/", "\\", ":", "*", "?", "\"", "<", ">", "|", " ", nullptr};
    for (int i = 0; invalid_chars[i] != nullptr; i++) {
        sanitized = sanitized.replace(invalid_chars[i], "_");
    }

    while (sanitized.find("__") != -1) {
        sanitized = sanitized.replace("__", "_");
    }

    sanitized = sanitized.strip_edges();
    if (sanitized.is_empty()) {
        sanitized = "default";
    }

    return sanitized;
}

bool _is_redirect_response_code(int response_code) {
    return response_code == HTTPClient::RESPONSE_MOVED_PERMANENTLY ||
            response_code == HTTPClient::RESPONSE_FOUND ||
            response_code == HTTPClient::RESPONSE_SEE_OTHER ||
            response_code == HTTPClient::RESPONSE_TEMPORARY_REDIRECT ||
            response_code == HTTPClient::RESPONSE_PERMANENT_REDIRECT;
}

String _get_header_value_case_insensitive(const Dictionary &headers, const String &header_name) {
    Array keys = headers.keys();
    String normalized_header_name = header_name.to_lower();
    for (int i = 0; i < keys.size(); i++) {
        String key = String(keys[i]);
        if (key.to_lower() == normalized_header_name) {
            return String(headers[key]).strip_edges();
        }
    }
    return "";
}

String _build_absolute_redirect_url(const String &base_url, const String &location) {
    String normalized_location = location.strip_edges();
    if (normalized_location.is_empty()) {
        return "";
    }
    if (normalized_location.begins_with("http://") || normalized_location.begins_with("https://")) {
        return normalized_location;
    }

    String host;
    String path;
    int port = 0;
    bool use_tls = false;
    if (!_parse_http_url(base_url, host, port, path, use_tls)) {
        return normalized_location;
    }

    String scheme = use_tls ? "https://" : "http://";
    String host_with_port = host;
    bool use_default_port = (use_tls && port == 443) || (!use_tls && port == 80);
    if (!use_default_port) {
        host_with_port += ":" + String::num_int64(port);
    }

    if (normalized_location.begins_with("/")) {
        return scheme + host_with_port + normalized_location;
    }

    String base_path = path.get_base_dir();
    if (base_path.is_empty() || base_path == ".") {
        base_path = "/";
    }
    return scheme + host_with_port + base_path.path_join(normalized_location);
}

Error _remove_directory_recursive_absolute(const String &dir_path) {
    if (!DirAccess::dir_exists_absolute(dir_path)) {
        return OK;
    }

    Ref<DirAccess> dir = DirAccess::open(dir_path);
    if (dir.is_null()) {
        return ERR_CANT_OPEN;
    }

    dir->list_dir_begin();
    while (true) {
        String entry = dir->get_next();
        if (entry.is_empty()) {
            break;
        }
        if (entry == "." || entry == "..") {
            continue;
        }

        String child_path = dir_path.path_join(entry);
        Error child_err = OK;
        if (dir->current_is_dir()) {
            child_err = _remove_directory_recursive_absolute(child_path);
        } else {
            child_err = DirAccess::remove_absolute(child_path);
        }

        if (child_err != OK) {
            dir->list_dir_end();
            return child_err;
        }
    }
    dir->list_dir_end();

    return DirAccess::remove_absolute(dir_path);
}

String _strip_wrapping_quotes(const String &value) {
    String stripped = value.strip_edges();
    if (stripped.length() >= 2) {
        bool double_quoted = stripped.begins_with("\"") && stripped.ends_with("\"");
        bool single_quoted = stripped.begins_with("'") && stripped.ends_with("'");
        if (double_quoted || single_quoted) {
            return stripped.substr(1, stripped.length() - 2);
        }
    }
    return stripped;
}

String _get_env_trimmed(const char *name) {
    return OS::get_singleton()->get_environment(String::utf8(name)).strip_edges();
}
}

TemplateManager* TemplateManager::singleton = nullptr;

static bool _parse_http_url(const String &url, String &host, int &port, String &path, bool &use_tls) {
    String rest;
    if (url.begins_with("https://")) {
        use_tls = true;
        port = 443;
        rest = url.substr(8);
    } else if (url.begins_with("http://")) {
        use_tls = false;
        port = 80;
        rest = url.substr(7);
    } else {
        return false;
    }

    int slash_pos = rest.find("/");
    String host_port = slash_pos >= 0 ? rest.substr(0, slash_pos) : rest;
    path = slash_pos >= 0 ? rest.substr(slash_pos) : "/";
    if (path.is_empty()) {
        path = "/";
    }

    int colon_pos = host_port.rfind(":");
    if (colon_pos > 0) {
        String parsed_host = host_port.substr(0, colon_pos);
        String parsed_port = host_port.substr(colon_pos + 1);
        if (!parsed_port.is_empty()) {
            int custom_port = parsed_port.to_int();
            if (custom_port > 0) {
                port = custom_port;
                host = parsed_host;
                return !host.is_empty();
            }
        }
    }

    host = host_port;
    return !host.is_empty();
}

static String _resolve_absolute_output_path(const String &output_path) {
    if (output_path.is_empty()) {
        return ProjectSettings::get_singleton()->globalize_path("res://");
    }
    if (output_path.is_absolute_path()) {
        return output_path;
    }
    if (output_path.begins_with("res://") || output_path.begins_with("user://")) {
        return ProjectSettings::get_singleton()->globalize_path(output_path);
    }
    String project_path = ProjectSettings::get_singleton()->globalize_path("res://");
    return project_path.path_join(output_path);
}

TemplateManager::TemplateManager() {
    if (singleton == nullptr) {
        singleton = this;
    }
    load_download_states();
}

TemplateManager::~TemplateManager() {
    if (singleton == this) {
        singleton = nullptr;
    }
}

void TemplateManager::_bind_methods() {
    ClassDB::bind_method(D_METHOD("load_versions_from_remote"), &TemplateManager::load_versions_from_remote);
    ClassDB::bind_method(D_METHOD("load_versions_from_remote_sync"), &TemplateManager::load_versions_from_remote_sync);
    ClassDB::bind_method(D_METHOD("load_versions_from_embedded"), &TemplateManager::load_versions_from_embedded);
    ClassDB::bind_method(D_METHOD("load_versions_from_local_cache"), &TemplateManager::load_versions_from_local_cache);
    ClassDB::bind_method(D_METHOD("get_available_versions"), &TemplateManager::get_available_versions);
    ClassDB::bind_method(D_METHOD("get_best_version_for_editor"), &TemplateManager::get_best_version_for_editor);
    ClassDB::bind_method(
            D_METHOD("resolve_template_filename_for_version", "target_version", "major_version"),
            &TemplateManager::resolve_template_filename_for_version,
            DEFVAL(""));
    ClassDB::bind_method(D_METHOD("download_template", "filename", "target_path"), &TemplateManager::download_template, DEFVAL(""));
    ClassDB::bind_method(D_METHOD("extract_template", "template_path", "output_path"), &TemplateManager::extract_template);
    ClassDB::bind_method(D_METHOD("get_current_godot_version"), &TemplateManager::get_current_godot_version);
    ClassDB::bind_method(D_METHOD("get_godot_major_version"), &TemplateManager::get_godot_major_version);

    // New methods for update detection
    ClassDB::bind_method(D_METHOD("initialize_template_system"), &TemplateManager::initialize_template_system);
    ClassDB::bind_method(D_METHOD("has_template_updates_available"), &TemplateManager::has_template_updates_available);
    ClassDB::bind_method(D_METHOD("get_missing_templates"), &TemplateManager::get_missing_templates);
    ClassDB::bind_method(D_METHOD("check_editor_template_status"), &TemplateManager::check_editor_template_status);

    // Individual template status
    ClassDB::bind_method(D_METHOD("is_template_embedded", "filename"), &TemplateManager::is_template_embedded);
    ClassDB::bind_method(D_METHOD("is_template_downloaded", "filename"), &TemplateManager::is_template_downloaded);
    ClassDB::bind_method(D_METHOD("get_template_path", "filename"), &TemplateManager::get_template_path);
    ClassDB::bind_method(D_METHOD("get_best_available_template_for_editor"), &TemplateManager::get_best_available_template_for_editor);
    ClassDB::bind_method(
            D_METHOD("get_best_available_template_for_version", "target_version", "major_version"),
            &TemplateManager::get_best_available_template_for_version,
            DEFVAL(""));
    ClassDB::bind_method(D_METHOD("get_nearest_compatible_version", "target_version", "major_version"), &TemplateManager::get_nearest_compatible_version);
    ClassDB::bind_method(D_METHOD("get_download_cache_path", "filename"), &TemplateManager::get_download_cache_path);
    ClassDB::bind_method(D_METHOD("download_template_sync", "filename", "target_path"), &TemplateManager::download_template_sync, DEFVAL(""));
    ClassDB::bind_method(D_METHOD("clear_cache"), &TemplateManager::clear_cache);
    ClassDB::bind_method(D_METHOD("purge_distribution_cache"), &TemplateManager::purge_distribution_cache);
    ClassDB::bind_method(D_METHOD("update_polling"), &TemplateManager::update_polling);
    ClassDB::bind_method(D_METHOD("set_distribution_provider", "provider"), &TemplateManager::set_distribution_provider);
    ClassDB::bind_method(D_METHOD("get_distribution_provider"), &TemplateManager::get_distribution_provider);
    ClassDB::bind_method(
            D_METHOD("set_current_release_config", "owner", "repo", "release_tag"),
            &TemplateManager::set_current_release_config,
            DEFVAL("latest"));
    ClassDB::bind_method(D_METHOD("get_current_release_config"), &TemplateManager::get_current_release_config);
    ClassDB::bind_method(
            D_METHOD("set_github_release_config", "owner", "repo", "release_tag"),
            &TemplateManager::set_github_release_config,
            DEFVAL("latest"));
    ClassDB::bind_method(D_METHOD("get_github_release_config"), &TemplateManager::get_github_release_config);
    ClassDB::bind_method(
            D_METHOD("set_gitee_release_config", "owner", "repo", "release_tag"),
            &TemplateManager::set_gitee_release_config,
            DEFVAL("latest"));
    ClassDB::bind_method(D_METHOD("get_gitee_release_config"), &TemplateManager::get_gitee_release_config);
    ClassDB::bind_method(D_METHOD("get_versions_remote_url"), &TemplateManager::get_versions_remote_url);
    ClassDB::bind_method(D_METHOD("get_update_manifest_url"), &TemplateManager::get_update_manifest_url);
    ClassDB::bind_method(D_METHOD("get_distribution_asset_url", "asset_name"), &TemplateManager::get_distribution_asset_url);
    ClassDB::bind_method(D_METHOD("refresh_versions"), &TemplateManager::refresh_versions);
    ClassDB::bind_method(D_METHOD("set_download_timeout", "timeout_seconds"), &TemplateManager::set_download_timeout);
    ClassDB::bind_method(D_METHOD("get_download_timeout"), &TemplateManager::get_download_timeout);

    ClassDB::bind_method(D_METHOD("_on_template_download_completed"), &TemplateManager::_on_template_download_completed);
    ClassDB::bind_method(D_METHOD("_on_versions_download_completed"), &TemplateManager::_on_versions_download_completed);

    ADD_SIGNAL(MethodInfo("versions_loaded"));
    ADD_SIGNAL(MethodInfo("template_download_finished", PropertyInfo(Variant::STRING, "filename"), PropertyInfo(Variant::BOOL, "success")));
    ADD_SIGNAL(MethodInfo("template_download_progress", PropertyInfo(Variant::STRING, "filename"), PropertyInfo(Variant::FLOAT, "progress")));
}

TemplateManager* TemplateManager::get_singleton() {
    return singleton;
}

Error TemplateManager::load_versions_from_remote() {
    if (!Engine::get_singleton()->is_editor_hint()) {
        TOOLKIT_LOG("TemplateManager: load_versions_from_remote skipped outside editor.");
        return ERR_UNCONFIGURED;
    }

    String versions_url = build_versions_url();
    if (versions_url.is_empty()) {
        TOOLKIT_LOG_RICH("[color=red]TemplateManager: Remote versions URL is empty, provider config is invalid.[/color]");
        return ERR_INVALID_PARAMETER;
    }
    TOOLKIT_LOG("TemplateManager: Downloading versions.yaml from remote: ", versions_url);

    HTTPRequest* http_request = memnew(HTTPRequest);
    http_request->set_name("TemplateManager_VersionsRequest");
    
    EditorInterface *editor = EditorInterface::get_singleton();
    Node* parent_node = editor ? editor->get_editor_main_screen() : nullptr;
    if (!parent_node) {
        TOOLKIT_LOG_RICH("[color=red]TemplateManager: Cannot get editor main screen to add HTTPRequest.[/color]");
        memdelete(http_request);
        return ERR_UNCONFIGURED;
    }
    parent_node->add_child(http_request);

    http_request->connect("request_completed", callable_mp(this, &TemplateManager::_on_versions_download_completed));

    Error err = http_request->request(versions_url);
    if (err != OK) {
        TOOLKIT_LOG_RICH("[color=red]TemplateManager: Failed to start versions.yaml download request.[/color]");
        http_request->queue_free();
        return err;
    }

    return OK;
}

Error TemplateManager::http_get_sync_follow_redirects(const String &url, PackedByteArray &r_body, int &r_response_code, Dictionary *r_response_headers) const {
    String current_url = url.strip_edges();
    if (current_url.is_empty()) {
        r_response_code = 0;
        return ERR_INVALID_PARAMETER;
    }

    constexpr int max_redirects = 5;
    for (int redirect_count = 0; redirect_count <= max_redirects; redirect_count++) {
        String host;
        String request_path;
        int port = -1;
        bool use_tls = false;
        if (!_parse_http_url(current_url, host, port, request_path, use_tls)) {
            r_response_code = 0;
            return ERR_INVALID_PARAMETER;
        }

        Ref<HTTPClient> client = memnew(HTTPClient);
        Ref<TLSOptions> tls_options;
        if (use_tls) {
            tls_options = TLSOptions::client();
        }

        Error connect_err = client->connect_to_host(host, port, tls_options);
        if (connect_err != OK) {
            r_response_code = 0;
            return connect_err;
        }

        uint64_t connect_deadline_ms = Time::get_singleton()->get_ticks_msec() + uint64_t(download_timeout) * 1000;
        while (true) {
            if (Time::get_singleton()->get_ticks_msec() > connect_deadline_ms) {
                client->close();
                r_response_code = 0;
                return ERR_TIMEOUT;
            }

            Error poll_err = client->poll();
            if (poll_err != OK) {
                client->close();
                r_response_code = 0;
                return poll_err;
            }

            HTTPClient::Status status = client->get_status();
            if (status == HTTPClient::STATUS_CONNECTED) {
                break;
            }
            if (status == HTTPClient::STATUS_CANT_CONNECT ||
                    status == HTTPClient::STATUS_CANT_RESOLVE ||
                    status == HTTPClient::STATUS_CONNECTION_ERROR ||
                    status == HTTPClient::STATUS_TLS_HANDSHAKE_ERROR) {
                client->close();
                r_response_code = 0;
                return ERR_CANT_CONNECT;
            }

            OS::get_singleton()->delay_msec(16);
        }

        PackedStringArray headers;
        headers.append("User-Agent: Toolkit-Addons/1.0");
        headers.append("Accept: */*");
        Error request_err = client->request(HTTPClient::METHOD_GET, request_path, headers);
        if (request_err != OK) {
            client->close();
            r_response_code = 0;
            return request_err;
        }

        uint64_t response_deadline_ms = Time::get_singleton()->get_ticks_msec() + uint64_t(download_timeout) * 1000;
        while (!client->has_response()) {
            if (Time::get_singleton()->get_ticks_msec() > response_deadline_ms) {
                client->close();
                r_response_code = 0;
                return ERR_TIMEOUT;
            }

            Error poll_err = client->poll();
            if (poll_err != OK) {
                client->close();
                r_response_code = 0;
                return poll_err;
            }

            OS::get_singleton()->delay_msec(16);
        }

        r_response_code = client->get_response_code();
        Dictionary response_headers = client->get_response_headers_as_dictionary();

        if (_is_redirect_response_code(r_response_code)) {
            String location = _get_header_value_case_insensitive(response_headers, "Location");
            client->close();
            if (location.is_empty()) {
                return ERR_CANT_CONNECT;
            }
            current_url = _build_absolute_redirect_url(current_url, location);
            continue;
        }

        if (r_response_code != HTTPClient::RESPONSE_OK) {
            client->close();
            if (r_response_code == HTTPClient::RESPONSE_NOT_FOUND) {
                return ERR_FILE_NOT_FOUND;
            }
            return ERR_CANT_CONNECT;
        }

        PackedByteArray response_body;
        int64_t total = client->get_response_body_length();
        int64_t downloaded = 0;
        uint64_t body_idle_deadline_ms = Time::get_singleton()->get_ticks_msec() + uint64_t(download_timeout) * 1000;

        while (true) {
            if (Time::get_singleton()->get_ticks_msec() > body_idle_deadline_ms) {
                client->close();
                return ERR_TIMEOUT;
            }

            Error poll_err = client->poll();
            if (poll_err != OK) {
                client->close();
                return poll_err;
            }

            PackedByteArray chunk = client->read_response_body_chunk();
            if (!chunk.is_empty()) {
                int old_size = response_body.size();
                response_body.resize(old_size + chunk.size());
                for (int i = 0; i < chunk.size(); i++) {
                    response_body.set(old_size + i, chunk[i]);
                }
                downloaded += chunk.size();
                body_idle_deadline_ms = Time::get_singleton()->get_ticks_msec() + uint64_t(download_timeout) * 1000;
            }

            HTTPClient::Status status = client->get_status();
            if (status == HTTPClient::STATUS_CONNECTED) {
                break;
            }
            if (status == HTTPClient::STATUS_CANT_CONNECT ||
                    status == HTTPClient::STATUS_CANT_RESOLVE ||
                    status == HTTPClient::STATUS_CONNECTION_ERROR ||
                    status == HTTPClient::STATUS_TLS_HANDSHAKE_ERROR) {
                client->close();
                return ERR_CANT_CONNECT;
            }

            if (chunk.is_empty()) {
                if (total > 0 && downloaded >= total) {
                    break;
                }
                OS::get_singleton()->delay_msec(8);
            }
        }

        client->close();
        r_body = response_body;
        if (r_response_headers != nullptr) {
            *r_response_headers = response_headers;
        }
        return OK;
    }

    r_response_code = 0;
    return ERR_CANT_CONNECT;
}

Error TemplateManager::load_versions_from_remote_sync() {
    String versions_url = build_versions_url();
    if (versions_url.is_empty()) {
        TOOLKIT_LOG_RICH("[color=red]TemplateManager: Remote versions URL is empty, provider config is invalid.[/color]");
        return ERR_INVALID_PARAMETER;
    }

    PackedByteArray response_body;
    int response_code = 0;
    Error request_err = ERR_CANT_CONNECT;
    for (int attempt = 0; attempt < 5; attempt++) {
        request_err = http_get_sync_follow_redirects(versions_url, response_body, response_code);
        if (request_err == OK || request_err == ERR_FILE_NOT_FOUND || request_err == ERR_INVALID_PARAMETER) {
            break;
        }
        OS::get_singleton()->delay_msec(250 * (attempt + 1));
    }
    if (request_err != OK) {
        return request_err;
    }

    String yaml_content = response_body.get_string_from_utf8();
    Error parse_err = parse_versions_yaml(yaml_content);
    if (parse_err != OK) {
        return parse_err;
    }

    return save_versions_to_local_cache();
}

Error TemplateManager::load_versions_from_local_cache() {
    String local_versions_path = get_local_versions_cache_path();

    if (!FileAccess::file_exists(local_versions_path)) {
        TOOLKIT_LOG("TemplateManager: No local versions cache found at ", local_versions_path);
        return ERR_FILE_NOT_FOUND;
    }

    Ref<FileAccess> file = FileAccess::open(local_versions_path, FileAccess::READ);
    if (file.is_null()) {
        TOOLKIT_LOG_RICH("[color=red]Error: Cannot read local versions cache[/color]");
        return ERR_FILE_CANT_READ;
    }

    String yaml_content = file->get_as_text();
    file->close();

    Error result = parse_versions_yaml(yaml_content);
    if (result == OK) {
        TOOLKIT_LOG("TemplateManager: Loaded versions from local cache");
    }
    return result;
}

Error TemplateManager::load_versions_from_embedded() {
#ifdef EMBED_RESOURCES
    // Try to find versions.yaml in embedded resources
    for (int i = 0; toolkit::resources::embedded_resources[i].path != nullptr; i++) {
        const auto& resource = toolkit::resources::embedded_resources[i];
        if (String(resource.path) == "resources/versions.yaml") {
            String yaml_content = String::utf8((const char*)resource.data, resource.size);
            return parse_versions_yaml(yaml_content);
        }
    }
#endif

    // Fallback: try to read from file system
    Ref<FileAccess> file = FileAccess::open("res://versions.yaml", FileAccess::READ);
    if (file.is_null()) {
        TOOLKIT_LOG_RICH("[color=yellow]Warning: versions.yaml not found, using fallback[/color]");
        // Create a minimal fallback version structure
        versions_cache["godot4"] = Dictionary();
        Dictionary godot4 = versions_cache["godot4"];
        godot4["4.4.0"] = "minigame4.4.0.1.tpz";
        godot4["4.3.0"] = "minigame4.3.0.3.tpz";
        versions_loaded = true;
        return OK;
    }

    String yaml_content = file->get_as_text();
    file->close();
    return parse_versions_yaml(yaml_content);
}

Error TemplateManager::parse_versions_yaml(const String& yaml_content) {
    TOOLKIT_LOG("TemplateManager: Parsing YAML content: ", yaml_content);

    Dictionary parsed;
    String current_major;
    PackedStringArray lines = yaml_content.split("\n", false);

    for (int i = 0; i < lines.size(); i++) {
        String raw_line = lines[i];
        String trimmed = raw_line.strip_edges();

        if (trimmed.is_empty() || trimmed.begins_with("#")) {
            continue;
        }

        if (!raw_line.begins_with(" ") && trimmed.ends_with(":")) {
            current_major = _strip_wrapping_quotes(trimmed.substr(0, trimmed.length() - 1));
            if (!parsed.has(current_major)) {
                parsed[current_major] = Dictionary();
            }
            continue;
        }

        if (current_major.is_empty()) {
            continue;
        }

        int separator_pos = trimmed.find(":");
        if (separator_pos <= 0) {
            continue;
        }

        String version_key = _strip_wrapping_quotes(trimmed.substr(0, separator_pos));
        String filename = _strip_wrapping_quotes(trimmed.substr(separator_pos + 1));
        if (version_key.is_empty() || filename.is_empty()) {
            continue;
        }

        Dictionary versions = parsed[current_major];
        versions[version_key] = filename;
        parsed[current_major] = versions;
    }

    if (parsed.is_empty()) {
        TOOLKIT_LOG_RICH("[color=red]Error: Invalid versions.yaml format[/color]");
        return ERR_PARSE_ERROR;
    }

    TOOLKIT_LOG("TemplateManager: Parsed YAML: ", parsed);

    versions_cache = parsed;
    available_versions.clear();

    // Build available versions list
    Dictionary dict = parsed;
    Array godot_majors = dict.keys();

    TOOLKIT_LOG("TemplateManager: Major versions found: ", godot_majors);

    for (int i = 0; i < godot_majors.size(); i++) {
        String major = godot_majors[i];
        Variant versions_variant = dict[major];
        TOOLKIT_LOG("TemplateManager: Processing major '", major, "', versions data: ", versions_variant, " (type: ", versions_variant.get_type(), ")");

        if (versions_variant.get_type() != Variant::DICTIONARY) {
            TOOLKIT_LOG("TemplateManager: Warning - versions data for '", major, "' is not a dictionary, skipping");
            continue;
        }

        Dictionary versions = versions_variant;
        Array version_keys = versions.keys();

        TOOLKIT_LOG("TemplateManager: Version keys for '", major, "': ", version_keys);

        for (int j = 0; j < version_keys.size(); j++) {
            Variant version_key = version_keys[j];
            String version = version_key;

            // Handle both numeric and string keys from YAML parsing
            Variant filename_variant;
            if (versions.has(version_key)) {
                filename_variant = versions[version_key];
            } else {
                // Try the string version if numeric key doesn't work
                filename_variant = versions[version];
            }

            String filename = filename_variant;

            TOOLKIT_LOG("TemplateManager: Processing version '", version, "' -> '", filename, "' (key: ", version_key, " type: ", version_key.get_type(), ", value: ", filename_variant, " type: ", filename_variant.get_type(), ")");

            if (filename.is_empty() || filename == "<null>") {
                TOOLKIT_LOG("TemplateManager: Skipping invalid filename for version ", version);
                continue;
            }

            TemplateVersion template_ver;
            template_ver.godot_major = major;
            template_ver.version = version;
            template_ver.filename = filename;
            template_ver.is_embedded = is_template_embedded(filename);

            Dictionary version_info;
            version_info["godot_major"] = major;
            version_info["version"] = version;
            version_info["filename"] = filename;
            version_info["is_embedded"] = template_ver.is_embedded;

            available_versions.append(version_info);
        }
    }

    versions_loaded = true;
    TOOLKIT_LOG("TemplateManager: Loaded ", available_versions.size(), " template versions");
    return OK;
}

Array TemplateManager::get_available_versions() const {
    return available_versions;
}

Dictionary TemplateManager::get_versions_data() const {
    return versions_cache;
}

String TemplateManager::get_best_version_for_editor() const {
    String current_version = get_current_godot_version();
    String major_version = get_godot_major_version();

    TOOLKIT_LOG("TemplateManager: Finding best version for editor ", current_version, " (major: ", major_version, ")");

    // First try exact match
    if (has_version(major_version, current_version)) {
        String exact_filename = get_template_filename(major_version, current_version);
        TOOLKIT_LOG("TemplateManager: Found exact match filename: '", exact_filename, "'");
        // Check if template is actually available (embedded, cached, or remote)
        if (!exact_filename.is_empty() && is_template_available_anywhere(exact_filename)) {
            TOOLKIT_LOG("TemplateManager: Exact match is available");
            return exact_filename;
        }
    }

    // Try nearest compatible version using just-close matching principle
    String nearest_filename = get_nearest_compatible_version(current_version, major_version);
    TOOLKIT_LOG("TemplateManager: Nearest compatible filename: '", nearest_filename, "'");
    if (!nearest_filename.is_empty() && is_template_available_anywhere(nearest_filename)) {
        TOOLKIT_LOG("TemplateManager: Nearest compatible is available");
        return nearest_filename;
    }

    // Fall back to latest version for this major version
    String latest_filename = get_latest_version_for_godot_major(major_version);
    TOOLKIT_LOG("TemplateManager: Latest version filename: '", latest_filename, "'");
    return latest_filename;
}

String TemplateManager::resolve_template_filename_for_version(const String &target_version, const String &major_version) const {
    String resolved_target_version = target_version.strip_edges();
    if (resolved_target_version.is_empty()) {
        resolved_target_version = get_current_godot_version();
    }

    String resolved_major_version = major_version.strip_edges();
    if (resolved_major_version.is_empty()) {
        PackedStringArray version_parts = resolved_target_version.split(".");
        if (!version_parts.is_empty() && !String(version_parts[0]).strip_edges().is_empty()) {
            resolved_major_version = "godot" + String(version_parts[0]).strip_edges();
        } else {
            resolved_major_version = get_godot_major_version();
        }
    }

    TOOLKIT_LOG("TemplateManager: Resolving template filename for version ", resolved_target_version, " (major: ", resolved_major_version, ")");

    if (has_version(resolved_major_version, resolved_target_version)) {
        String exact_filename = get_template_filename(resolved_major_version, resolved_target_version);
        if (!exact_filename.is_empty()) {
            return exact_filename;
        }
    }

    String nearest_filename = get_nearest_compatible_version(resolved_target_version, resolved_major_version);
    if (!nearest_filename.is_empty()) {
        return nearest_filename;
    }

    return get_latest_version_for_godot_major(resolved_major_version);
}

String TemplateManager::get_latest_version_for_godot_major(const String& major_version) const {
    if (!versions_cache.has(major_version)) {
        return "";
    }

    Dictionary versions = versions_cache[major_version];
    Array version_keys = versions.keys();

    if (version_keys.is_empty()) {
        return "";
    }

    // Take the last version key (assuming sorted)
    Variant latest_key = version_keys[version_keys.size() - 1];

    // Handle both numeric and string keys from YAML parsing
    Variant filename_variant;
    if (versions.has(latest_key)) {
        filename_variant = versions[latest_key];
    } else {
        String latest_version = latest_key;
        filename_variant = versions[latest_version];
    }

    String result = filename_variant;
    TOOLKIT_LOG("TemplateManager: Latest version for ", major_version, ": key=", latest_key, " -> filename=", result);
    return result;
}

bool TemplateManager::has_version(const String& godot_major, const String& version) const {
    if (!versions_cache.has(godot_major)) {
        return false;
    }

    Dictionary versions = versions_cache[godot_major];
    return versions.has(version);
}

String TemplateManager::get_template_filename(const String& godot_major, const String& version) const {
    if (!has_version(godot_major, version)) {
        return "";
    }

    Dictionary versions = versions_cache[godot_major];
    return versions[version];
}

bool TemplateManager::is_template_embedded(const String& filename) const {
    TOOLKIT_LOG("TemplateManager: Checking if template '", filename, "' is embedded");

#ifdef EMBED_RESOURCES
    String resource_path = "resources/templates/" + filename;
    TOOLKIT_LOG("TemplateManager: Looking for embedded resource: ", resource_path);

    // List all embedded resources for debugging
    TOOLKIT_LOG("TemplateManager: Available embedded resources:");
    for (int i = 0; toolkit::resources::embedded_resources[i].path != nullptr; i++) {
        const auto& resource = toolkit::resources::embedded_resources[i];
        TOOLKIT_LOG("  - ", String(resource.path));
        if (String(resource.path) == resource_path) {
            TOOLKIT_LOG("TemplateManager: Found embedded template: ", resource_path);
            return true;
        }
    }
    TOOLKIT_LOG("TemplateManager: Template not found in embedded resources");
#else
    TOOLKIT_LOG("TemplateManager: EMBED_RESOURCES not defined");
#endif
    return false;
}

bool TemplateManager::is_template_downloaded(const String& filename) const {
    String cache_path = get_download_cache_path(filename);
    return FileAccess::file_exists(cache_path);
}

String TemplateManager::get_template_path(const String& filename) const {
    // Priority: embedded -> cached -> remote

    // Check embedded first
    if (is_template_embedded(filename)) {
        return "embedded://" + filename;
    }

    // Check downloaded cache
    String cache_path = get_download_cache_path(filename);
    if (FileAccess::file_exists(cache_path)) {
        return cache_path;
    }

    // Check if available remotely (but don't download automatically)
    if (is_template_available_remotely(filename)) {
        return "remote://" + filename;  // Indicates needs download
    }

    return "";
}

Error TemplateManager::download_template(const String& filename, const String& target_path) {
    if (!Engine::get_singleton()->is_editor_hint()) {
        TOOLKIT_LOG("TemplateManager: download_template skipped outside editor.");
        update_download_state(filename, "failed", 0.0f);
        return ERR_UNCONFIGURED;
    }

    String download_url = build_download_url(filename);
    if (download_url.is_empty()) {
        TOOLKIT_LOG_RICH("[color=red]TemplateManager: Download URL is empty for template: ", filename, "[/color]");
        update_download_state(filename, "failed", 0.0f);
        return ERR_INVALID_PARAMETER;
    }
    String output_path = target_path.is_empty() ? get_download_cache_path(filename) : target_path;

    TOOLKIT_LOG("TemplateManager: Starting async download of '", filename, "' from '", download_url, "' to '", output_path, "'");

    update_download_state(filename, "downloading", 0.0f);

    HTTPRequest* http_request = memnew(HTTPRequest);
    http_request->set_name("TemplateManager_HTTPRequest");

    EditorInterface *editor = EditorInterface::get_singleton();
    Node* parent_node = editor ? editor->get_editor_main_screen() : nullptr;
    if (!parent_node) {
        TOOLKIT_LOG_RICH("[color=red]TemplateManager: Cannot get editor main screen to add HTTPRequest.[/color]");
        memdelete(http_request);
        update_download_state(filename, "failed", 0.0f);
        return ERR_UNCONFIGURED;
    }
    parent_node->add_child(http_request);

    // We will not use set_download_file to avoid race conditions.
    // Instead, we'll get the body in the completion signal and write it manually.

    // Ensure cache directory exists
    String cache_dir = output_path.get_base_dir();
    UserDataPath::create_directory_if_not_exists(cache_dir);

    // Connect completion signal
    http_request->connect("request_completed", callable_mp(this, &TemplateManager::_on_template_download_completed).bind(filename, output_path));

    // Make the HTTP request
    Error request_result = http_request->request(download_url);

    if (request_result != OK) {
        TOOLKIT_LOG_RICH("[color=red]TemplateManager: Failed to start HTTPRequest: ", request_result, "[/color]");
        parent_node->remove_child(http_request);
        memdelete(http_request);
        update_download_state(filename, "failed", 0.0f);
        return request_result;
    }

    TOOLKIT_LOG("TemplateManager: HTTPRequest started successfully, download is in progress...");
    return OK;
}

Error TemplateManager::download_template_sync(const String& filename, const String& target_path) {
    String download_url = build_download_url(filename);
    if (download_url.is_empty()) {
        UtilityFunctions::push_warning(String("Template download URL is empty for: ") + filename);
        update_download_state(filename, "failed", 0.0f);
        emit_signal("template_download_finished", filename, false);
        return ERR_INVALID_PARAMETER;
    }
    String output_path = target_path.is_empty() ? get_download_cache_path(filename) : target_path;
    if (output_path.is_empty()) {
        UtilityFunctions::push_warning("Template output path is empty.");
        update_download_state(filename, "failed", 0.0f);
        emit_signal("template_download_finished", filename, false);
        return ERR_FILE_CANT_WRITE;
    }

    if (FileAccess::file_exists(output_path)) {
        update_download_state(filename, "completed", 1.0f);
        return OK;
    }

    String cache_dir = output_path.get_base_dir();
    if (cache_dir.is_empty()) {
        UtilityFunctions::push_warning(String("Template cache directory is empty for output path: ") + output_path);
        update_download_state(filename, "failed", 0.0f);
        emit_signal("template_download_finished", filename, false);
        return ERR_FILE_CANT_WRITE;
    }
    if (!UserDataPath::create_directory_if_not_exists(cache_dir)) {
        TOOLKIT_LOG_RICH("[color=red]TemplateManager: Cannot create cache directory: ", cache_dir, "[/color]");
        UtilityFunctions::push_warning(String("Template cache directory create failed: ") + cache_dir);
        update_download_state(filename, "failed", 0.0f);
        emit_signal("template_download_finished", filename, false);
        return ERR_FILE_CANT_WRITE;
    }
    update_download_state(filename, "downloading", 0.0f);
    emit_signal("template_download_progress", filename, 0.0f);

    PackedByteArray response_body;
    int response_code = 0;
    Error request_err = ERR_CANT_CONNECT;
    for (int attempt = 0; attempt < 5; attempt++) {
        request_err = http_get_sync_follow_redirects(download_url, response_body, response_code);
        if (request_err == OK || request_err == ERR_FILE_NOT_FOUND || request_err == ERR_INVALID_PARAMETER) {
            break;
        }
        OS::get_singleton()->delay_msec(250 * (attempt + 1));
    }
    if (request_err != OK) {
        update_download_state(filename, "failed", 0.0f);
        emit_signal("template_download_finished", filename, false);
        return request_err;
    }

    Ref<FileAccess> out = FileAccess::open(output_path, FileAccess::WRITE);
    if (out.is_null()) {
        TOOLKIT_LOG_RICH("[color=red]TemplateManager: Cannot open output file for write: ", output_path, "[/color]");
        UtilityFunctions::push_warning(String("Template output write failed: ") + output_path);
        update_download_state(filename, "failed", 0.0f);
        emit_signal("template_download_finished", filename, false);
        return ERR_FILE_CANT_WRITE;
    }

    out->store_buffer(response_body);
    out->close();
    update_download_state(filename, "completed", 1.0f);
    emit_signal("template_download_progress", filename, 1.0f);
    emit_signal("template_download_finished", filename, true);
    return OK;
}

Error TemplateManager::download_template_async(const String& filename, const String& target_path) {
    // TODO: Implement async download
    return download_template(filename, target_path);
}

bool TemplateManager::is_downloading(const String& filename) const {
    if (!download_states.has(filename)) {
        return false;
    }

    Dictionary state = download_states[filename];
    return state.get("status", "") == "downloading";
}

float TemplateManager::get_download_progress(const String& filename) const {
    if (!download_states.has(filename)) {
        return 0.0f;
    }

    Dictionary state = download_states[filename];
    return state.get("progress", 0.0f);
}

String TemplateManager::get_current_godot_version() const {
    Dictionary version_info = Engine::get_singleton()->get_version_info();
    String major = version_info.get("major", 4);
    String minor = version_info.get("minor", 4);
    String patch = version_info.get("patch", 0);

    return major + "." + minor + "." + patch;
}

String TemplateManager::get_godot_major_version() const {
    Dictionary version_info = Engine::get_singleton()->get_version_info();
    int major = version_info.get("major", 4);
    return "godot" + String::num_int64(major);
}

String TemplateManager::format_version_string(const String& version) const {
    return "Godot " + version;
}

Error TemplateManager::extract_template(const String& template_path, const String& output_path) {
    TOOLKIT_LOG("TemplateManager: Extracting '", template_path, "' to '", output_path, "'");

    // Check if it's an embedded template
    if (template_path.begins_with("embedded://")) {
        String filename = template_path.replace("embedded://", "");
        TOOLKIT_LOG("TemplateManager: Extracting embedded template: ", filename);
        return extract_embedded_template(filename, output_path);
    }

    // For file-based templates, check if file exists and implement ZIP extraction
    if (!FileAccess::file_exists(template_path)) {
        TOOLKIT_LOG_RICH("[color=red]Template file not found: ", template_path, "[/color]");
        return ERR_FILE_NOT_FOUND;
    }

    Ref<ZIPReader> zip_reader = memnew(ZIPReader);
    Error open_result = zip_reader->open(template_path);

    if (open_result != OK) {
        TOOLKIT_LOG_RICH("[color=red]Failed to open ZIP file: ", template_path, " (error: ", open_result, ")[/color]");
        return open_result;
    }

    String absolute_output_path = _resolve_absolute_output_path(output_path);

    // Create output directory if it doesn't exist
    if (!DirAccess::dir_exists_absolute(absolute_output_path)) {
        Error mkdir_err = DirAccess::make_dir_recursive_absolute(absolute_output_path);
        if (mkdir_err != OK) {
            TOOLKIT_LOG_RICH("[color=red]Failed to create output directory: ", absolute_output_path, "[/color]");
            UtilityFunctions::push_warning(String("Template extract mkdir failed: ") + absolute_output_path);
            zip_reader->close();
            return mkdir_err;
        }
    }

    PackedStringArray files = zip_reader->get_files();
    TOOLKIT_LOG("TemplateManager: Found ", files.size(), " files in ZIP");

    for (int i = 0; i < files.size(); i++) {
        String file_path_in_zip = files[i];
        String full_dest_path = absolute_output_path.path_join(file_path_in_zip);

        if (file_path_in_zip.ends_with("/")) {
            Error mkdir_err = DirAccess::make_dir_recursive_absolute(full_dest_path);
            if (mkdir_err != OK) {
                TOOLKIT_LOG_RICH("[color=red]Failed to create directory in template extraction: ", full_dest_path, " (error: ", mkdir_err, ")[/color]");
                UtilityFunctions::push_warning(String("Template extract mkdir failed: ") + full_dest_path);
                zip_reader->close();
                return mkdir_err;
            }
        } else {
            Error mkdir_err = DirAccess::make_dir_recursive_absolute(full_dest_path.get_base_dir());
            if (mkdir_err != OK) {
                TOOLKIT_LOG_RICH("[color=red]Failed to create parent directory for file: ", full_dest_path, " (error: ", mkdir_err, ")[/color]");
                UtilityFunctions::push_warning(String("Template extract parent mkdir failed: ") + full_dest_path.get_base_dir());
                zip_reader->close();
                return mkdir_err;
            }
            PackedByteArray data = zip_reader->read_file(file_path_in_zip);
            Ref<FileAccess> file = FileAccess::open(full_dest_path, FileAccess::WRITE);
            if (file.is_valid()) {
                file->store_buffer(data);
                file->close();
            } else {
                TOOLKIT_LOG_RICH("[color=red]Failed to write file: ", full_dest_path, "[/color]");
                UtilityFunctions::push_warning(String("Template extract write failed: ") + full_dest_path);
                zip_reader->close();
                return ERR_FILE_CANT_WRITE;
            }
        }
    }

    zip_reader->close();
    TOOLKIT_LOG_RICH("[color=green]Successfully extracted template '", template_path, "' to '", absolute_output_path, "'[/color]");
    return OK;
}

Error TemplateManager::extract_embedded_template(const String& filename, const String& output_path) {
    TOOLKIT_LOG("TemplateManager: Extracting embedded template '", filename, "' to '", output_path, "'");

#ifdef EMBED_RESOURCES
    String resource_path = "resources/templates/" + filename;
    for (int i = 0; toolkit::resources::embedded_resources[i].path != nullptr; i++) {
        const auto& resource = toolkit::resources::embedded_resources[i];
        if (String(resource.path) == resource_path) {
            String absolute_output_path = _resolve_absolute_output_path(output_path);

            TOOLKIT_LOG("TemplateManager: Using absolute output path: ", absolute_output_path);

            String temp_zip_path = absolute_output_path.path_join(String("temp_") + filename);
            String extract_dir = absolute_output_path; // Extract directly to output path, no subfolder

            // Create the output directory if it doesn't exist
            if (!DirAccess::dir_exists_absolute(absolute_output_path)) {
                Error mkdir_result = DirAccess::make_dir_recursive_absolute(absolute_output_path);
                if (mkdir_result != OK) {
                    TOOLKIT_LOG_RICH("[color=red]Cannot create output directory: ", absolute_output_path, " (error: ", mkdir_result, ")[/color]");
                    UtilityFunctions::push_warning(String("Embedded template mkdir failed: ") + absolute_output_path);
                    return mkdir_result;
                }
                TOOLKIT_LOG("TemplateManager: Created directory: ", absolute_output_path);
            }

            // Write embedded ZIP to temp file
            Ref<FileAccess> temp_file = FileAccess::open(temp_zip_path, FileAccess::WRITE);
            if (temp_file.is_null()) {
                TOOLKIT_LOG_RICH("[color=red]Cannot create temp file: ", temp_zip_path, "[/color]");
                UtilityFunctions::push_warning(String("Embedded template temp write failed: ") + temp_zip_path);
                return ERR_FILE_CANT_WRITE;
            }

            PackedByteArray data;
            data.resize(resource.size);
            memcpy(data.ptrw(), resource.data, resource.size);
            temp_file->store_buffer(data);
            temp_file->close();

            // Extract the ZIP file using Godot's ZIPReader
            Ref<ZIPReader> zip_reader = memnew(ZIPReader);
            Error open_result = zip_reader->open(temp_zip_path);

            if (open_result == OK) {
                PackedStringArray files = zip_reader->get_files();
                TOOLKIT_LOG("TemplateManager: Found ", files.size(), " files in ZIP");

                for (int f = 0; f < files.size(); f++) {
                    String file_path = files[f];
                    String output_file_path = extract_dir + "/" + file_path;

                    TOOLKIT_LOG("Extracting: ", file_path, " -> ", output_file_path);

                    // Skip directory entries (they end with "/")
                    if (file_path.ends_with("/")) {
                        TOOLKIT_LOG("  Skipping directory entry: ", file_path);
                        continue;
                    }

                    // Create directory structure for this file
                    String file_dir = output_file_path.get_base_dir();
                    if (!DirAccess::dir_exists_absolute(file_dir)) {
                        Error mkdir_result = DirAccess::make_dir_recursive_absolute(file_dir);
                        if (mkdir_result != OK) {
                            TOOLKIT_LOG_RICH("[color=red]Failed to create directory: ", file_dir, " (error: ", mkdir_result, ")[/color]");
                            UtilityFunctions::push_warning(String("Embedded template mkdir failed: ") + file_dir);
                            zip_reader->close();
                            DirAccess::remove_absolute(temp_zip_path);
                            return mkdir_result;
                        }
                    }

                    // Read file data from ZIP
                    PackedByteArray file_data = zip_reader->read_file(file_path);
                    if (file_data.size() > 0) {
                        // Write file to disk
                        Ref<FileAccess> output_file = FileAccess::open(output_file_path, FileAccess::WRITE);
                        if (output_file.is_valid()) {
                            output_file->store_buffer(file_data);
                            output_file->close();
                            TOOLKIT_LOG("  Written: ", file_data.size(), " bytes");
                        } else {
                            TOOLKIT_LOG_RICH("[color=red]  Failed to write: ", output_file_path, "[/color]");
                            UtilityFunctions::push_warning(String("Embedded template write failed: ") + output_file_path);
                            zip_reader->close();
                            DirAccess::remove_absolute(temp_zip_path);
                            return ERR_FILE_CANT_WRITE;
                        }
                    } else if (!file_path.ends_with("/")) {
                        TOOLKIT_LOG("  Empty file: ", file_path);
                        // Create empty file
                        Ref<FileAccess> empty_file = FileAccess::open(output_file_path, FileAccess::WRITE);
                        if (empty_file.is_valid()) {
                            empty_file->close();
                        } else {
                            TOOLKIT_LOG_RICH("[color=red]  Failed to create empty file: ", output_file_path, "[/color]");
                            UtilityFunctions::push_warning(String("Embedded template empty file create failed: ") + output_file_path);
                            zip_reader->close();
                            DirAccess::remove_absolute(temp_zip_path);
                            return ERR_FILE_CANT_WRITE;
                        }
                    }
                }

                zip_reader->close();
                TOOLKIT_LOG_RICH("[color=green]Successfully extracted ", files.size(), " files from ZIP[/color]");
            } else {
                TOOLKIT_LOG_RICH("[color=red]Failed to open ZIP file: ", temp_zip_path, " (error: ", open_result, ")[/color]");
            }

            // Clean up temp file
            DirAccess::remove_absolute(temp_zip_path);

            TOOLKIT_LOG_RICH("[color=green]Successfully extracted embedded template '", filename, "' to '", extract_dir, "'[/color]");
            return OK;
        }
    }
    TOOLKIT_LOG_RICH("[color=red]Embedded template not found: ", resource_path, "[/color]");
#else
    TOOLKIT_LOG_RICH("[color=red]EMBED_RESOURCES not defined[/color]");
#endif
    return ERR_FILE_NOT_FOUND;
}

void TemplateManager::clear_cache() {
    versions_cache.clear();
    available_versions.clear();
    download_states.clear();
    versions_loaded = false;
}

Error TemplateManager::purge_distribution_cache() {
    String cache_root = get_distribution_cache_root_dir();
    clear_cache();
    return _remove_directory_recursive_absolute(cache_root);
}

Error TemplateManager::refresh_versions() {
    versions_cache.clear();
    available_versions.clear();
    versions_loaded = false;
    Error remote_result = load_versions_from_remote();
    if (remote_result != OK) {
        if (load_versions_from_local_cache() != OK) {
            return load_versions_from_embedded();
        }
    }
    return remote_result;
}

void TemplateManager::set_distribution_provider(const String& provider) {
    if (!apply_distribution_provider(provider, true, true)) {
        UtilityFunctions::push_warning(String("TemplateManager: Unknown distribution provider: ") + provider);
    }
}

String TemplateManager::get_distribution_provider() const {
    switch (distribution_provider) {
        case DistributionProvider::GITHUB_RELEASE:
            return "github";
        case DistributionProvider::GITEE_RELEASE:
            return "gitee";
        default:
            return "github";
    }
}

void TemplateManager::set_current_release_config(const String& owner, const String& repo, const String& release_tag) {
    switch (distribution_provider) {
        case DistributionProvider::GITHUB_RELEASE:
            set_github_release_config(owner, repo, release_tag);
            break;
        case DistributionProvider::GITEE_RELEASE:
            set_gitee_release_config(owner, repo, release_tag);
            break;
        default:
            break;
    }
}

Dictionary TemplateManager::get_current_release_config() const {
    switch (distribution_provider) {
        case DistributionProvider::GITHUB_RELEASE:
            return get_github_release_config();
        case DistributionProvider::GITEE_RELEASE:
            return get_gitee_release_config();
        default:
            return Dictionary();
    }
}

void TemplateManager::set_github_release_config(const String& owner, const String& repo, const String& release_tag) {
    String normalized_owner = owner.strip_edges();
    String normalized_repo = repo.strip_edges();
    String normalized_tag = release_tag.strip_edges();
    if (normalized_tag.is_empty()) {
        normalized_tag = "latest";
    }

    bool changed = github_repo_owner != normalized_owner ||
            github_repo_name != normalized_repo ||
            github_release_tag != normalized_tag;

    github_repo_owner = normalized_owner;
    github_repo_name = normalized_repo;
    github_release_tag = normalized_tag;
    persist_distribution_preferences();

    if (changed && distribution_provider == DistributionProvider::GITHUB_RELEASE) {
        reload_active_distribution_cache(true);
    }
}

Dictionary TemplateManager::get_github_release_config() const {
    Dictionary config;
    config["owner"] = github_repo_owner;
    config["repo"] = github_repo_name;
    config["release_tag"] = github_release_tag;
    return config;
}

void TemplateManager::set_gitee_release_config(const String& owner, const String& repo, const String& release_tag) {
    String normalized_owner = owner.strip_edges();
    String normalized_repo = repo.strip_edges();
    String normalized_tag = release_tag.strip_edges();
    if (normalized_tag.is_empty()) {
        normalized_tag = "latest";
    }

    bool changed = gitee_repo_owner != normalized_owner ||
            gitee_repo_name != normalized_repo ||
            gitee_release_tag != normalized_tag;

    gitee_repo_owner = normalized_owner;
    gitee_repo_name = normalized_repo;
    gitee_release_tag = normalized_tag;
    persist_distribution_preferences();

    if (changed && distribution_provider == DistributionProvider::GITEE_RELEASE) {
        reload_active_distribution_cache(true);
    }
}

Dictionary TemplateManager::get_gitee_release_config() const {
    Dictionary config;
    config["owner"] = gitee_repo_owner;
    config["repo"] = gitee_repo_name;
    config["release_tag"] = gitee_release_tag;
    return config;
}

String TemplateManager::get_versions_remote_url() const {
    return build_versions_url();
}

String TemplateManager::get_update_manifest_url() const {
    return get_distribution_asset_url("latest.json");
}

String TemplateManager::get_distribution_asset_url(const String& asset_name) const {
    return build_download_url(asset_name);
}

void TemplateManager::set_download_timeout(int timeout_seconds) {
    download_timeout = timeout_seconds;
}

int TemplateManager::get_download_timeout() const {
    return download_timeout;
}

String TemplateManager::build_versions_url() const {
    switch (distribution_provider) {
        case DistributionProvider::GITHUB_RELEASE:
            return build_release_download_url(
                    DistributionProvider::GITHUB_RELEASE,
                    github_repo_owner,
                    github_repo_name,
                    github_release_tag,
                    "versions.yaml");
        case DistributionProvider::GITEE_RELEASE:
            return build_release_download_url(
                    DistributionProvider::GITEE_RELEASE,
                    gitee_repo_owner,
                    gitee_repo_name,
                    gitee_release_tag,
                    "versions.yaml");
        default:
            return "";
    }
}

String TemplateManager::build_release_download_url(DistributionProvider provider, const String& owner, const String& repo, const String& release_tag, const String& filename) const {
    String normalized_owner = owner.strip_edges();
    String normalized_repo = repo.strip_edges();
    String normalized_filename = filename.strip_edges();
    String normalized_tag = release_tag.strip_edges();
    if (normalized_tag.is_empty()) {
        normalized_tag = "latest";
    }

    if (normalized_owner.is_empty() ||
            normalized_repo.is_empty() ||
            normalized_filename.is_empty()) {
        return "";
    }

    switch (provider) {
        case DistributionProvider::GITHUB_RELEASE:
            if (normalized_tag.to_lower() == "latest") {
                return "https://github.com/" + normalized_owner + "/" + normalized_repo + "/releases/latest/download/" + normalized_filename;
            }
            return "https://github.com/" + normalized_owner + "/" + normalized_repo + "/releases/download/" + normalized_tag + "/" + normalized_filename;
        case DistributionProvider::GITEE_RELEASE:
            return "https://gitee.com/" + normalized_owner + "/" + normalized_repo + "/releases/download/" + normalized_tag + "/" + normalized_filename;
        default:
            return "";
    }
}

String TemplateManager::build_download_url(const String& filename) const {
    switch (distribution_provider) {
        case DistributionProvider::GITHUB_RELEASE:
            return build_release_download_url(
                    DistributionProvider::GITHUB_RELEASE,
                    github_repo_owner,
                    github_repo_name,
                    github_release_tag,
                    filename);
        case DistributionProvider::GITEE_RELEASE:
            return build_release_download_url(
                    DistributionProvider::GITEE_RELEASE,
                    gitee_repo_owner,
                    gitee_repo_name,
                    gitee_release_tag,
                    filename);
        default:
            return "";
    }
}

String TemplateManager::get_download_cache_path(const String& filename) const {
    String templates_dir = get_distribution_cache_root_dir().path_join("templates");
    DirAccess::make_dir_recursive_absolute(templates_dir);
    return templates_dir.path_join(filename);
}

String TemplateManager::get_local_versions_cache_path() const {
    return get_distribution_cache_root_dir().path_join("versions.yaml");
}

String TemplateManager::get_download_state_cache_path() const {
    return get_distribution_cache_root_dir().path_join("download_states.json");
}

void TemplateManager::load_download_states() {
    String path = get_download_state_cache_path();
    if (!FileAccess::file_exists(path)) {
        return;
    }

    Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
    if (file.is_null()) {
        return;
    }

    String content = file->get_as_text();
    file->close();
    if (content.is_empty()) {
        return;
    }

    Variant parsed = JSON::parse_string(content);
    if (parsed.get_type() == Variant::DICTIONARY) {
        download_states = parsed;
    }
}

void TemplateManager::save_download_states() const {
    String path = get_download_state_cache_path();
    String dir = path.get_base_dir();
    UserDataPath::create_directory_if_not_exists(dir);

    Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
    if (file.is_null()) {
        return;
    }
    file->store_string(JSON::stringify(download_states, "  "));
    file->close();
}

void TemplateManager::update_download_state(const String& filename, const String& state, float progress) {
    Dictionary download_info;
    download_info["status"] = state;
    download_info["progress"] = progress;
    download_info["timestamp"] = Time::get_singleton()->get_unix_time_from_system();

    download_states[filename] = download_info;
    save_download_states();
}

// New helper methods for template availability checking
bool TemplateManager::is_template_available_remotely(const String& filename) const {
    // Check if template exists in versions configuration
    Array versions = get_available_versions();
    for (int i = 0; i < versions.size(); i++) {
        Dictionary version_info = versions[i];
        if (version_info.get("filename", "") == filename) {
            return true;
        }
    }
    return false;
}

bool TemplateManager::is_template_available_anywhere(const String& filename) const {
    return is_template_embedded(filename) ||
           is_template_downloaded(filename) ||
           is_template_available_remotely(filename);
}

String TemplateManager::get_best_available_template_for_editor() const {
    String current_version = get_current_godot_version();
    String major_version = get_godot_major_version();

    // Get the best template filename first
    String best_filename = get_best_version_for_editor();
    if (best_filename.is_empty()) {
        return "";
    }

    // Check availability with priority: embedded -> cached -> remote
    String template_path = get_template_path(best_filename);
    return template_path;
}

String TemplateManager::get_best_available_template_for_version(const String &target_version, const String &major_version) const {
    String best_filename = resolve_template_filename_for_version(target_version, major_version);
    if (best_filename.is_empty()) {
        return "";
    }
    return get_template_path(best_filename);
}

Error TemplateManager::save_versions_to_local_cache() {
    if (!versions_loaded || versions_cache.is_empty()) {
        return ERR_INVALID_DATA;
    }

    String local_cache_path = get_local_versions_cache_path();

    // Ensure directory exists
    String cache_dir = local_cache_path.get_base_dir();
    UserDataPath::create_directory_if_not_exists(cache_dir);

    // Convert Dictionary to simple YAML format manually (since YAML::stringify doesn't exist)
    String yaml_content = dictionary_to_simple_yaml(versions_cache);

    Ref<FileAccess> file = FileAccess::open(local_cache_path, FileAccess::WRITE);
    if (file.is_null()) {
        return ERR_FILE_CANT_WRITE;
    }

    file->store_string(yaml_content);
    file->close();

    TOOLKIT_LOG("TemplateManager: Saved versions to local cache: ", local_cache_path);
    return OK;
}

// Version comparison and nearest matching implementation
String TemplateManager::get_nearest_compatible_version(const String& target_version, const String& major_version) const {
    TOOLKIT_LOG("TemplateManager: Looking for nearest compatible version for ", target_version, " in ", major_version);

    if (!versions_cache.has(major_version)) {
        TOOLKIT_LOG("TemplateManager: Major version ", major_version, " not found in cache");
        return "";
    }

    Dictionary versions = versions_cache[major_version];
    Array version_keys = versions.keys();

    TOOLKIT_LOG("TemplateManager: Available versions: ", version_keys);

    if (version_keys.is_empty()) {
        return "";
    }

    String best_match = "";
    String best_filename = "";
    int best_diff = INT_MAX;

    for (int i = 0; i < version_keys.size(); i++) {
        Variant candidate_key = version_keys[i];
        String candidate_version = candidate_key;

        // Handle both numeric and string keys from YAML parsing
        Variant filename_variant;
        if (versions.has(candidate_key)) {
            filename_variant = versions[candidate_key];
        } else {
            filename_variant = versions[candidate_version];
        }
        String candidate_filename = filename_variant;

        TOOLKIT_LOG("TemplateManager: Checking candidate ", candidate_version, " -> ", candidate_filename, " (key: ", candidate_key, " type: ", candidate_key.get_type(), ")");

        if (candidate_filename.is_empty() || candidate_filename == "<null>") {
            TOOLKIT_LOG("TemplateManager: Skipping candidate with invalid filename");
            continue;
        }

        // Only consider versions that are <= target version (no future versions)
        int comparison = compare_version_numbers(candidate_version, target_version);
        TOOLKIT_LOG("TemplateManager: Version comparison ", candidate_version, " vs ", target_version, " = ", comparison);

        if (comparison > 0) {
            TOOLKIT_LOG("TemplateManager: Skipping newer version ", candidate_version);
            continue;  // Skip versions newer than target
        }

        // Calculate "distance" from target version (closer to 0 is better)
        int diff = abs(comparison);
        TOOLKIT_LOG("TemplateManager: Distance = ", diff, ", best_diff = ", best_diff);

        if (diff < best_diff) {
            best_diff = diff;
            best_match = candidate_version;
            best_filename = candidate_filename;
            TOOLKIT_LOG("TemplateManager: New best match: ", best_match, " -> ", best_filename);
        } else if (diff == best_diff) {
            // If distances are equal, prefer the newer version
            int version_comparison = compare_version_numbers(candidate_version, best_match);
            if (version_comparison > 0) {
                best_match = candidate_version;
                best_filename = candidate_filename;
                TOOLKIT_LOG("TemplateManager: Equal distance, preferring newer version: ", best_match, " -> ", best_filename);
            }
        }
    }

    TOOLKIT_LOG("TemplateManager: Final best match: '", best_match, "' -> '", best_filename, "'");
    return best_filename;
}

Array TemplateManager::get_compatible_versions_for_major(const String& major_version) const {
    Array compatible_versions;

    if (!versions_cache.has(major_version)) {
        return compatible_versions;
    }

    Dictionary versions = versions_cache[major_version];
    Array version_keys = versions.keys();

    for (int i = 0; i < version_keys.size(); i++) {
        String version = version_keys[i];
        String filename = versions[version];

        Dictionary version_info;
        version_info["version"] = version;
        version_info["filename"] = filename;
        version_info["major_version"] = major_version;
        version_info["is_embedded"] = is_template_embedded(filename);
        version_info["is_downloaded"] = is_template_downloaded(filename);
        version_info["is_available_remotely"] = is_template_available_remotely(filename);

        compatible_versions.append(version_info);
    }

    return compatible_versions;
}

int TemplateManager::compare_version_numbers(const String& version1, const String& version2) const {
    Array components1 = parse_version_components(version1);
    Array components2 = parse_version_components(version2);

    TOOLKIT_LOG("TemplateManager: Comparing '", version1, "' ", components1, " vs '", version2, "' ", components2);

    int max_length = Math::max(components1.size(), components2.size());

    for (int i = 0; i < max_length; i++) {
        int comp1 = (i < components1.size()) ? int(components1[i]) : 0;
        int comp2 = (i < components2.size()) ? int(components2[i]) : 0;

        if (comp1 < comp2) {
            TOOLKIT_LOG("TemplateManager: '", version1, "' < '", version2, "' (component ", i, ": ", comp1, " < ", comp2, ")");
            return -1;
        }
        if (comp1 > comp2) {
            TOOLKIT_LOG("TemplateManager: '", version1, "' > '", version2, "' (component ", i, ": ", comp1, " > ", comp2, ")");
            return 1;
        }
    }

    TOOLKIT_LOG("TemplateManager: '", version1, "' == '", version2, "' (equal)");
    return 0;  // Equal
}

Array TemplateManager::parse_version_components(const String& version) const {
    Array components;
    PackedStringArray parts = version.split(".");

    for (int i = 0; i < parts.size(); i++) {
        String part = parts[i];
        // Extract numeric part only (ignore suffixes like "beta", "rc", etc.)
        int numeric_part = part.to_int();
        components.append(numeric_part);
    }

    return components;
}

// Simple YAML serialization helper
String TemplateManager::dictionary_to_simple_yaml(const Dictionary& dict) const {
    String yaml_content = "";

    Array keys = dict.keys();
    for (int i = 0; i < keys.size(); i++) {
        Variant key_variant = keys[i];
        String key = String(key_variant);
        Variant value = dict[key_variant];

        yaml_content += key + ":\n";

        if (value.get_type() == Variant::DICTIONARY) {
            Dictionary sub_dict = value;
            Array sub_keys = sub_dict.keys();

            for (int j = 0; j < sub_keys.size(); j++) {
                Variant sub_key_variant = sub_keys[j];
                String sub_key = String(sub_key_variant);
                Variant sub_value_variant = sub_dict[sub_key_variant];
                String sub_value = String(sub_value_variant);
                yaml_content += "  " + sub_key + ": " + sub_value + "\n";
            }
        }
        yaml_content += "\n";
    }

    return yaml_content;
}

Error TemplateManager::initialize_template_system() {
    TOOLKIT_LOG("TemplateManager: Initializing template system...");
    load_distribution_preferences();

    // Step 1: Try to load from local cache first
    Error load_result = load_versions_from_local_cache();

    if (load_result != OK) {
        TOOLKIT_LOG("TemplateManager: Local cache not available, trying embedded versions");
        // Step 2: Fall back to embedded versions
        load_result = load_versions_from_embedded();

        if (load_result != OK) {
            TOOLKIT_LOG_RICH("[color=yellow]Warning: No version data available[/color]");
            return load_result;
        }
    }

    // Step 3: Check template status for current editor
    String template_status = check_editor_template_status();
    TOOLKIT_LOG("TemplateManager: Current editor template status: ", template_status);

    return OK;
}

bool TemplateManager::apply_distribution_provider(const String& provider, bool persist_selection, bool refresh_version_cache) {
    String normalized = provider.strip_edges().to_lower();
    DistributionProvider next_provider;
    if (normalized == "github" || normalized == "github_release" || normalized == "github-release") {
        next_provider = DistributionProvider::GITHUB_RELEASE;
    } else if (normalized == "gitee" || normalized == "gitee_release" || normalized == "gitee-release") {
        next_provider = DistributionProvider::GITEE_RELEASE;
    } else {
        return false;
    }

    bool provider_changed = distribution_provider != next_provider;
    distribution_provider = next_provider;

    if (persist_selection) {
        persist_distribution_preferences();
    }

    if (provider_changed && refresh_version_cache) {
        reload_active_distribution_cache(true);
    } else if (provider_changed) {
        reload_active_distribution_cache(false);
    }

    TOOLKIT_LOG("TemplateManager: Distribution provider set to ", get_distribution_provider());
    return true;
}

void TemplateManager::load_distribution_preferences() {
    if (!Engine::get_singleton()->is_editor_hint()) {
        return;
    }

    EditorInterface *editor_interface = EditorInterface::get_singleton();
    if (!editor_interface) {
        return;
    }

    Ref<EditorSettings> editor_settings = editor_interface->get_editor_settings();
    if (editor_settings.is_null()) {
        return;
    }

    github_repo_owner = String(editor_settings->get_project_metadata(
            TOOLKIT_EDITOR_METADATA_SECTION,
            TOOLKIT_EDITOR_METADATA_GITHUB_OWNER,
            github_repo_owner)).strip_edges();
    github_repo_name = String(editor_settings->get_project_metadata(
            TOOLKIT_EDITOR_METADATA_SECTION,
            TOOLKIT_EDITOR_METADATA_GITHUB_REPO,
            github_repo_name)).strip_edges();
    github_release_tag = String(editor_settings->get_project_metadata(
            TOOLKIT_EDITOR_METADATA_SECTION,
            TOOLKIT_EDITOR_METADATA_GITHUB_RELEASE_TAG,
            github_release_tag)).strip_edges();
    if (github_release_tag.is_empty()) {
        github_release_tag = "latest";
    }

    gitee_repo_owner = String(editor_settings->get_project_metadata(
            TOOLKIT_EDITOR_METADATA_SECTION,
            TOOLKIT_EDITOR_METADATA_GITEE_OWNER,
            gitee_repo_owner)).strip_edges();
    gitee_repo_name = String(editor_settings->get_project_metadata(
            TOOLKIT_EDITOR_METADATA_SECTION,
            TOOLKIT_EDITOR_METADATA_GITEE_REPO,
            gitee_repo_name)).strip_edges();
    gitee_release_tag = String(editor_settings->get_project_metadata(
            TOOLKIT_EDITOR_METADATA_SECTION,
            TOOLKIT_EDITOR_METADATA_GITEE_RELEASE_TAG,
            gitee_release_tag)).strip_edges();
    if (gitee_release_tag.is_empty()) {
        gitee_release_tag = "latest";
    }

    String provider = editor_settings->get_project_metadata(
            TOOLKIT_EDITOR_METADATA_SECTION,
            TOOLKIT_EDITOR_METADATA_DISTRIBUTION_PROVIDER,
            String("github"));
    apply_distribution_provider(provider, false, false);

    // Allow headless tests and CI to inject release source config without
    // mutating editor settings in the user's global profile.
    String env_provider = _get_env_trimmed("TOOLKIT_RELEASE_PROVIDER").to_lower();
    String env_owner = _get_env_trimmed("TOOLKIT_RELEASE_OWNER");
    String env_repo = _get_env_trimmed("TOOLKIT_RELEASE_REPO");
    String env_tag = _get_env_trimmed("TOOLKIT_RELEASE_TAG");

    if (!env_provider.is_empty()) {
        apply_distribution_provider(env_provider, false, false);
    }

    if (!env_owner.is_empty() || !env_repo.is_empty() || !env_tag.is_empty()) {
        String effective_tag = env_tag.is_empty() ? String("latest") : env_tag;
        switch (distribution_provider) {
            case DistributionProvider::GITHUB_RELEASE:
                if (!env_owner.is_empty()) {
                    github_repo_owner = env_owner;
                }
                if (!env_repo.is_empty()) {
                    github_repo_name = env_repo;
                }
                github_release_tag = effective_tag;
                break;
            case DistributionProvider::GITEE_RELEASE:
                if (!env_owner.is_empty()) {
                    gitee_repo_owner = env_owner;
                }
                if (!env_repo.is_empty()) {
                    gitee_repo_name = env_repo;
                }
                gitee_release_tag = effective_tag;
                break;
            default:
                break;
        }
    }

    download_states.clear();
    load_download_states();
}

void TemplateManager::persist_distribution_preferences() const {
    if (!Engine::get_singleton()->is_editor_hint()) {
        return;
    }

    EditorInterface *editor_interface = EditorInterface::get_singleton();
    if (!editor_interface) {
        return;
    }

    Ref<EditorSettings> editor_settings = editor_interface->get_editor_settings();
    if (editor_settings.is_null()) {
        return;
    }

    editor_settings->set_project_metadata(
            TOOLKIT_EDITOR_METADATA_SECTION,
            TOOLKIT_EDITOR_METADATA_DISTRIBUTION_PROVIDER,
            get_distribution_provider());
    editor_settings->set_project_metadata(
            TOOLKIT_EDITOR_METADATA_SECTION,
            TOOLKIT_EDITOR_METADATA_GITHUB_OWNER,
            github_repo_owner);
    editor_settings->set_project_metadata(
            TOOLKIT_EDITOR_METADATA_SECTION,
            TOOLKIT_EDITOR_METADATA_GITHUB_REPO,
            github_repo_name);
    editor_settings->set_project_metadata(
            TOOLKIT_EDITOR_METADATA_SECTION,
            TOOLKIT_EDITOR_METADATA_GITHUB_RELEASE_TAG,
            github_release_tag);
    editor_settings->set_project_metadata(
            TOOLKIT_EDITOR_METADATA_SECTION,
            TOOLKIT_EDITOR_METADATA_GITEE_OWNER,
            gitee_repo_owner);
    editor_settings->set_project_metadata(
            TOOLKIT_EDITOR_METADATA_SECTION,
            TOOLKIT_EDITOR_METADATA_GITEE_REPO,
            gitee_repo_name);
    editor_settings->set_project_metadata(
            TOOLKIT_EDITOR_METADATA_SECTION,
            TOOLKIT_EDITOR_METADATA_GITEE_RELEASE_TAG,
            gitee_release_tag);
}

void TemplateManager::reload_active_distribution_cache(bool load_remote_versions) {
    download_states.clear();
    load_download_states();

    versions_cache.clear();
    available_versions.clear();
    versions_loaded = false;

    if (load_versions_from_local_cache() != OK) {
        load_versions_from_embedded();
    }

    if (load_remote_versions && Engine::get_singleton()->is_editor_hint()) {
        load_versions_from_remote();
    }
}

String TemplateManager::get_distribution_cache_root_dir() const {
    String templates_dir = ProjectSettings::get_singleton()->globalize_path("user://toolkit/templates");
    String owner;
    String repo;
    String release_tag;

    switch (distribution_provider) {
        case DistributionProvider::GITHUB_RELEASE:
            owner = github_repo_owner;
            repo = github_repo_name;
            release_tag = github_release_tag;
            break;
        case DistributionProvider::GITEE_RELEASE:
            owner = gitee_repo_owner;
            repo = gitee_repo_name;
            release_tag = gitee_release_tag;
            break;
        default:
            owner = "default";
            repo = "default";
            release_tag = "latest";
            break;
    }

    String provider_dir = templates_dir.path_join(
            String("sources/")
            + get_distribution_provider()
            + "/"
            + _sanitize_cache_component(owner)
            + "__"
            + _sanitize_cache_component(repo)
            + "__"
            + _sanitize_cache_component(release_tag));
    DirAccess::make_dir_recursive_absolute(provider_dir);
    return provider_dir;
}

bool TemplateManager::has_template_updates_available() const {
    if (!versions_loaded) {
        return false;
    }

    // Check if there are newer templates available that we don't have locally
    Array missing = get_missing_templates();
    return !missing.is_empty();
}

Array TemplateManager::get_missing_templates() const {
    Array missing_templates;

    if (!versions_loaded) {
        return missing_templates;
    }

    // Check each available version to see if we have it locally (downloaded or embedded)
    for (int i = 0; i < available_versions.size(); i++) {
        Dictionary version_info = available_versions[i];
        String filename = version_info.get("filename", "");

        if (!filename.is_empty()) {
            bool is_available = is_template_embedded(filename) || is_template_downloaded(filename);
            if (!is_available) {
                missing_templates.append(version_info);
            }
        }
    }

    return missing_templates;
}

String TemplateManager::check_editor_template_status() const {
    if (!versions_loaded) {
        return "not_initialized";
    }

    String current_version = get_current_godot_version();
    String major_version = get_godot_major_version();

    // Check for exact version match
    if (has_version(major_version, current_version)) {
        String exact_template = get_template_filename(major_version, current_version);
        if (is_template_embedded(exact_template) || is_template_downloaded(exact_template)) {
            return "exact_match_available";
        } else {
            return "exact_match_missing";  // Template exists in versions but not downloaded/embedded
        }
    }

    // Check for compatible version (latest in same major version)
    String latest_template = get_latest_version_for_godot_major(major_version);
    if (!latest_template.is_empty()) {
        if (is_template_embedded(latest_template) || is_template_downloaded(latest_template)) {
            return "compatible_available";
        } else {
            return "compatible_missing";
        }
    }

    return "no_compatible_version";
}

// Removed the old HTTPClient infrastructure

void TemplateManager::_on_template_download_completed(int p_result, int p_response_code, const PackedStringArray& p_headers, const PackedByteArray& p_body, const String& filename, const String& output_path) {
    TOOLKIT_LOG("TemplateManager: Download completed for '", filename, "'. Result: ", p_result, ", Response Code: ", p_response_code);

    // The HTTPRequest node will free itself after the signal is emitted.
    // We can find it by name and queue_free it.
    // The HTTPRequest node is a child of the editor main screen, it will be freed automatically.

    if (p_result == HTTPRequest::RESULT_SUCCESS && p_response_code == 200) {
        // Manually write the downloaded data to the file
        Ref<FileAccess> file = FileAccess::open(output_path, FileAccess::WRITE);
        if (file.is_valid()) {
            file->store_buffer(p_body);
            file->close();

            update_download_state(filename, "completed", 1.0f);
            TOOLKIT_LOG_RICH("[color=green]TemplateManager: Downloaded file written successfully to: ", output_path, "[/color]");
            emit_signal("template_download_finished", filename, true);
        } else {
            update_download_state(filename, "failed", 0.0f);
            TOOLKIT_LOG_RICH("[color=red]TemplateManager: Failed to open file for writing: ", output_path, "[/color]");
            emit_signal("template_download_finished", filename, false);
        }
    } else {
        update_download_state(filename, "failed", 0.0f);
        TOOLKIT_LOG_RICH("[color=red]TemplateManager: Download failed for '", filename, "'[/color]");
        emit_signal("template_download_finished", filename, false);
    }
}

void TemplateManager::_on_versions_download_completed(int p_result, int p_response_code, const PackedStringArray& p_headers, const PackedByteArray& p_body) {
    TOOLKIT_LOG("TemplateManager: versions.yaml download completed. Result: ", p_result, ", Response Code: ", p_response_code);

    // The HTTPRequest node is a child of the editor main screen, it will be freed automatically.

    if (p_result == HTTPRequest::RESULT_SUCCESS && p_response_code == 200) {
        String yaml_content = p_body.get_string_from_utf8();
        Error parse_err = parse_versions_yaml(yaml_content);
        if (parse_err == OK) {
            TOOLKIT_LOG("TemplateManager: Successfully parsed remote versions.yaml");
            save_versions_to_local_cache();
            emit_signal("versions_loaded");
        } else {
            TOOLKIT_LOG_RICH("[color=red]TemplateManager: Failed to parse remote versions.yaml[/color]");
            // Fallback to embedded if remote parsing fails
            load_versions_from_embedded();
        }
    } else {
        TOOLKIT_LOG_RICH("[color=red]TemplateManager: Failed to download versions.yaml, falling back to local/embedded.[/color]");
        // Fallback to local cache or embedded
        if (load_versions_from_local_cache() != OK) {
            load_versions_from_embedded();
        }
    }
}

void TemplateManager::update_polling() {
    // This method is kept for potential future use but is not used for downloads anymore.
}

} // namespace templates
} // namespace toolkit
