#ifndef UPDATE_MANAGER_H
#define UPDATE_MANAGER_H

#include <godot_cpp/classes/http_request.hpp>
#include <godot_cpp/classes/timer.hpp>
#include <godot_cpp/core/object.hpp>

namespace toolkit {

class UpdateManager : public godot::Object {
	GDCLASS(UpdateManager, godot::Object);

public:
	enum UpdateState {
		STATE_IDLE,
		STATE_CHECKING,
		STATE_UPDATE_AVAILABLE,
		STATE_DOWNLOADING,
		STATE_DOWNLOADED,
		STATE_UP_TO_DATE,
		STATE_ERROR,
	};

private:
	static UpdateManager *singleton;
	godot::HTTPRequest *version_checker;
	godot::HTTPRequest *downloader;

	// HTTPClient for progress tracking downloads is no longer used
	godot::HTTPClient *http_client;
	godot::Timer *polling_timer;

	// Track HTTP nodes for proper cleanup
	godot::Array active_http_nodes;
	godot::String download_url;
	godot::String download_file_path;
	int64_t download_total_bytes;
	int64_t download_received_bytes;
	bool is_downloading_with_progress;

	godot::String local_version;
	godot::Dictionary remote_version_info;
	UpdateState current_state = STATE_IDLE;

	void _on_version_check_completed(int p_result, int p_response_code, const godot::PackedStringArray &p_headers, const godot::PackedByteArray &p_body);
	void _on_download_completed(int p_result, int p_response_code, const godot::PackedStringArray &p_headers, const godot::PackedByteArray &p_body);
	void set_state(UpdateState p_state);

	// HTTP nodes lifecycle management
	void track_http_node(godot::Node* node);
	void cleanup_http_nodes();
	godot::String resolve_distribution_asset_url(const godot::String &asset_name) const;
	godot::String resolve_platform_asset_name(const godot::Dictionary &platform_data) const;

	// HTTPClient-based download methods are no longer needed
	// void start_progress_download(const godot::String& url, const godot::String& file_path);
	// void poll_progress_download();
	// void finish_progress_download(bool success, const godot::String& error = "");

public:
	// This method is kept for potential future use but is not used for downloads anymore.
	void update_polling();

protected:
	static void _bind_methods();

public:
	static UpdateManager *get_singleton();

	UpdateManager();
	~UpdateManager();

	void initialize();
	void check_for_updates(const godot::String &p_local_version);
	void download_update();
	void cancel_download();
	bool perform_update();
	void restart_editor_for_update();

	bool is_properly_configured() const;
	godot::String get_local_version() const;
	godot::Dictionary get_remote_version_info() const;

	UpdateState get_current_state() const;

	// Signals
	enum {
		SIGNAL_UPDATE_AVAILABLE = 0,
		SIGNAL_DOWNLOAD_FINISHED,
		SIGNAL_ERROR,
		SIGNAL_DOWNLOAD_PROGRESS_CHANGED,
		SIGNAL_UPDATE_STATE_CHANGED,
	};
};

} // namespace toolkit

VARIANT_ENUM_CAST(toolkit::UpdateManager::UpdateState);

#endif // UPDATE_MANAGER_H
