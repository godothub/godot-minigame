#pragma once

#include <godot_cpp/variant/string.hpp>

using namespace godot;

namespace toolkit {
namespace filesystem {

// Cross-platform user data path utilities
class UserDataPath {
public:
    // Path modes similar to your electron function
    enum class PathMode {
        APP_DEFAULT,        // Uses app name as directory
        GODOT_PROJECT,      // Uses Godot's app_userdata structure
        CUSTOM_ONLY         // Complete custom directory
    };

    // Get user data base path with different modes
    static String get_user_data_base_path(
        PathMode mode = PathMode::APP_DEFAULT,
        const String& custom_dir_name = ""
    );

    // Specific path getters (similar to your electron branches)
    static String get_app_data_path(const String& app_name = "");
    static String get_godot_project_path(const String& project_name);
    static String get_custom_path(const String& custom_dir_name);

    // Platform-specific base directories
    static String get_platform_app_data_dir();
    static String get_platform_home_dir();

    // Utility functions
    static String get_current_platform_name();
    static bool create_directory_if_not_exists(const String& path);
    static bool is_path_writable(const String& path);

    // Path joining utilities
    static String join_paths(const String& base, const String& path1);
    static String join_paths(const String& base, const String& path1, const String& path2);
    static String join_paths(const String& base, const String& path1, const String& path2, const String& path3);
    static String join_paths(const String& base, const String& path1, const String& path2, const String& path3, const String& path4);

private:
};

} // namespace filesystem
} // namespace toolkit