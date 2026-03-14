#pragma once

#include "core/types.h"
#include <godot_cpp/classes/http_request.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <memory>
#include <queue>

namespace toolkit {
namespace network {

// Download task information
struct DownloadTask {
    String url;
    String destination;
    String id;
    DownloadState state = DownloadState::PENDING;
    DownloadProgress progress;
    ProgressCallback progress_callback;
    CompletionCallback completion_callback;
    Dictionary headers;
    int max_retries = 3;
    int current_retries = 0;
    int64_t timeout_ms = 30000; // 30 seconds
    
    String error_message;
    int64_t start_time = 0;
    int64_t end_time = 0;
};

// Download manager class
class DownloadManager : public godot::RefCounted {
    GDCLASS(DownloadManager, godot::RefCounted);

public:
    static DownloadManager* get_singleton();
    static void initialize();
    static void shutdown();
    
    // Download operations
    String start_download(const String& url, const String& destination, 
                         ProgressCallback progress_cb = nullptr,
                         CompletionCallback completion_cb = nullptr);
    
    Result start_download_sync(const String& url, const String& destination, 
                              ProgressCallback progress_cb = nullptr);
    
    bool cancel_download(const String& download_id);
    bool pause_download(const String& download_id);
    bool resume_download(const String& download_id);
    
    // Download management
    Array get_active_downloads();
    Array get_completed_downloads();
    DownloadProgress get_download_progress(const String& download_id);
    DownloadTask get_download_info(const String& download_id);
    
    void set_max_concurrent_downloads(int max_count);
    int get_max_concurrent_downloads() const;
    
    void set_global_timeout(int64_t timeout_ms);
    int64_t get_global_timeout() const;
    
    // Configuration
    void set_user_agent(const String& user_agent);
    String get_user_agent() const;
    
    void set_global_headers(const Dictionary& headers);
    Dictionary get_global_headers() const;
    
    void enable_ssl_verification(bool enabled);
    bool is_ssl_verification_enabled() const;
    
    // Statistics
    Dictionary get_statistics();
    void reset_statistics();
    
    // Cleanup
    void clear_completed_downloads();
    void clear_failed_downloads();
    void cancel_all_downloads();

protected:
    static void _bind_methods();

private:
    DownloadManager();
    ~DownloadManager();
    
    static DownloadManager* singleton;
    
    std::map<String, std::unique_ptr<DownloadTask>> downloads;
    std::queue<String> download_queue;
    std::vector<String> active_downloads;
    
    int max_concurrent = 3;
    int64_t global_timeout = 30000;
    String user_agent = "ToolkitAddons/1.0";
    Dictionary global_headers;
    bool ssl_verification = true;
    
    // Statistics
    int total_downloads = 0;
    int successful_downloads = 0;
    int failed_downloads = 0;
    int64_t total_bytes_downloaded = 0;
    
    void process_download_queue();
    void start_next_download();
    String generate_download_id();
    Result create_http_request(DownloadTask* task);
    void on_download_progress(const String& download_id, int64_t downloaded, int64_t total);
    void on_download_completed(const String& download_id, bool success, const String& error);
    void cleanup_download(const String& download_id);
    
    // HTTP request management
    std::map<String, godot::HTTPRequest*> http_requests;
    void setup_http_request(godot::HTTPRequest* request, DownloadTask* task);
    void on_request_completed(int result, int response_code, 
                             const godot::PackedStringArray& headers, 
                             const godot::PackedByteArray& body, 
                             const String& download_id);
};

// Version checker utility
class VersionChecker {
public:
    VersionChecker();
    ~VersionChecker();
    
    // Version checking
    void check_version_async(const String& version_url, 
                           std::function<void(bool has_update, const VersionInfo& latest, const String& error)> callback);
    
    Result check_version_sync(const String& version_url, VersionInfo& version_info);
    
    // Configuration
    void set_current_version(const VersionInfo& version);
    VersionInfo get_current_version() const;
    
    void set_check_interval(int64_t interval_ms);
    int64_t get_check_interval() const;
    
    void enable_automatic_checks(bool enabled);
    bool is_automatic_checks_enabled() const;
    
    // Update information
    struct UpdateInfo {
        bool has_update = false;
        VersionInfo current_version;
        VersionInfo latest_version;
        String download_url;
        String changelog_url;
        String release_notes;
        Dictionary metadata;
    };
    
    UpdateInfo get_last_check_result() const;
    int64_t get_last_check_time() const;
    
    // Automatic checking
    void start_periodic_checks();
    void stop_periodic_checks();

private:
    VersionInfo current_version;
    UpdateInfo last_check_result;
    int64_t last_check_time = 0;
    int64_t check_interval = 24 * 60 * 60 * 1000; // 24 hours
    bool automatic_checks = false;
    
    Result parse_version_response(const String& response, VersionInfo& version_info);
    bool compare_versions(const VersionInfo& current, const VersionInfo& latest);
};

// Network utilities
class NetworkUtils {
public:
    static bool is_url_valid(const String& url);
    static String extract_filename_from_url(const String& url);
    static String get_mime_type_from_extension(const String& extension);
    static Dictionary parse_http_headers(const godot::PackedStringArray& headers);
    static String format_bytes(int64_t bytes);
    static String format_duration(int64_t milliseconds);
    static Result test_connectivity(const String& test_url = "https://www.google.com");
    
    // URL manipulation
    static String join_url(const String& base, const String& path);
    static Dictionary parse_url(const String& url);
    static String encode_url_component(const String& component);
    static String decode_url_component(const String& component);
};

} // namespace network
} // namespace toolkit