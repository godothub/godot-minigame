#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/http_request.hpp>
#include <godot_cpp/classes/http_client.hpp>
#include <godot_cpp/classes/timer.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

namespace toolkit {
namespace templates {

// Template version information
struct TemplateVersion {
    String godot_major;     // "godot3" or "godot4"
    String version;         // "4.4.0", "4.3.0", etc.
    String filename;        // "minigame4.4.0.1.tpz"
    bool is_embedded;       // true if bundled in DLL
};

// Template manager for version handling and downloads
class TemplateManager : public RefCounted {
    GDCLASS(TemplateManager, RefCounted);

private:
    enum class DistributionProvider {
        GITHUB_RELEASE = 0,
        GITEE_RELEASE = 1,
    };

    static TemplateManager* singleton;

    DistributionProvider distribution_provider = DistributionProvider::GITHUB_RELEASE;
    String github_repo_owner = "citizenll";
    String github_repo_name = "toolkit-addons";
    String github_release_tag = "latest";
    String gitee_repo_owner = "citizenll";
    String gitee_repo_name = "toolkit-addons";
    String gitee_release_tag = "latest";

    Dictionary versions_cache;
    Array available_versions;
    bool versions_loaded = false;

protected:
    static void _bind_methods();

public:
    TemplateManager();
    ~TemplateManager();

    static TemplateManager* get_singleton();

    // Version management
    Error load_versions_from_remote();
    Error load_versions_from_remote_sync();
    Error load_versions_from_embedded();
    Error load_versions_from_local_cache();
    Array get_available_versions() const;
    Dictionary get_versions_data() const;

    // Version selection with nearest match
    String get_best_version_for_editor() const;
    String resolve_template_filename_for_version(const String& target_version, const String& major_version = "") const;
    String get_nearest_compatible_version(const String& target_version, const String& major_version) const;
    String get_latest_version_for_godot_major(const String& major_version) const;
    bool has_version(const String& godot_major, const String& version) const;
    String get_template_filename(const String& godot_major, const String& version) const;
    Array get_compatible_versions_for_major(const String& major_version) const;

    // Template availability (priority: embedded -> cached -> remote)
    bool is_template_embedded(const String& filename) const;
    bool is_template_downloaded(const String& filename) const;
    bool is_template_available_remotely(const String& filename) const;
    String get_template_path(const String& filename) const;
    String get_best_available_template_for_editor() const;
    String get_best_available_template_for_version(const String& target_version, const String& major_version = "") const;

    // Update detection
    bool has_template_updates_available() const;
    Array get_missing_templates() const;
    String check_editor_template_status() const;

    // Download management
    Error download_template(const String& filename, const String& target_path = "");
    Error download_template_sync(const String& filename, const String& target_path = ""); // Synchronous download for testing
    Error download_template_async(const String& filename, const String& target_path = "");
    bool is_downloading(const String& filename) const;
    float get_download_progress(const String& filename) const;

    // Utility functions
    String get_current_godot_version() const;
    String get_godot_major_version() const;
    String format_version_string(const String& version) const;

    // Template extraction
    Error extract_template(const String& template_path, const String& output_path);
    Error extract_embedded_template(const String& filename, const String& output_path);

    // Cache management
    void clear_cache();
    Error purge_distribution_cache();
    Error refresh_versions();
    Error initialize_template_system();

    // Configuration
    void set_distribution_provider(const String& provider);
    String get_distribution_provider() const;
    void set_current_release_config(const String& owner, const String& repo, const String& release_tag = "latest");
    Dictionary get_current_release_config() const;
    void set_github_release_config(const String& owner, const String& repo, const String& release_tag = "latest");
    Dictionary get_github_release_config() const;
    void set_gitee_release_config(const String& owner, const String& repo, const String& release_tag = "latest");
    Dictionary get_gitee_release_config() const;
    String get_versions_remote_url() const;
    String get_update_manifest_url() const;
    String get_distribution_asset_url(const String& asset_name) const;

    void set_download_timeout(int timeout_seconds);
    int get_download_timeout() const;

private:
    Dictionary download_states;
    int download_timeout = 30;

    // HTTP download infrastructure (no longer needed)
    // HTTPClient *http_client = nullptr;
    // Timer *polling_timer = nullptr;
    // String current_download_url;
    // String current_download_filename;
    // String current_download_file_path;
    // int64_t download_total_bytes = 0;
    // int64_t download_received_bytes = 0;
    // bool is_downloading_with_progress = false;

    Error parse_versions_yaml(const String& yaml_content);
    TemplateVersion parse_version_entry(const String& godot_major, const String& version, const String& filename);
    String build_versions_url() const;
    String build_release_download_url(DistributionProvider provider, const String& owner, const String& repo, const String& release_tag, const String& filename) const;
    String build_download_url(const String& filename) const;
    String get_download_cache_path(const String& filename) const;
    String get_local_versions_cache_path() const;
    String get_download_state_cache_path() const;
    void load_download_states();
    void save_download_states() const;
    bool apply_distribution_provider(const String& provider, bool persist_selection, bool refresh_version_cache);
    void load_distribution_preferences();
    void persist_distribution_preferences() const;
    void reload_active_distribution_cache(bool load_remote_versions);

    void update_download_state(const String& filename, const String& state, float progress = 0.0f);

    // HTTP download methods (no longer needed)
    // void initialize_http_components();
    // void cleanup_http_components();
    // Error start_progress_download(const String& url, const String& filename, const String& file_path);
    // void poll_progress_download();
    // void finish_progress_download(bool success, const String& error = "");
    void update_polling();

    // New helper methods
    bool is_template_available_anywhere(const String& filename) const;
    Error save_versions_to_local_cache();
    Error http_get_sync_follow_redirects(const String& url, PackedByteArray& r_body, int& r_response_code, Dictionary* r_response_headers = nullptr) const;
    int compare_version_numbers(const String& version1, const String& version2) const;
    Array parse_version_components(const String& version) const;
    String dictionary_to_simple_yaml(const Dictionary& dict) const;
    String get_distribution_cache_root_dir() const;

    // Signal handlers
    void _on_template_download_completed(int p_result, int p_response_code, const PackedStringArray& p_headers, const PackedByteArray& p_body, const String& filename, const String& output_path);
    void _on_versions_download_completed(int p_result, int p_response_code, const PackedStringArray& p_headers, const PackedByteArray& p_body);
};

} // namespace templates
} // namespace toolkit
