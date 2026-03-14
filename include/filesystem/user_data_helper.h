#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

namespace toolkit {
namespace filesystem {

// Godot-accessible wrapper for UserDataPath functionality
class UserDataHelper : public RefCounted {
    GDCLASS(UserDataHelper, RefCounted);

protected:
    static void _bind_methods();

public:
    UserDataHelper();
    ~UserDataHelper();

    // Main path getter with modes
    static String get_user_data_path(int mode = 0, const String& custom_dir_name = "");

    // Specific path getters
    static String get_app_data_path(const String& app_name = "");
    static String get_godot_project_path(const String& project_name);
    static String get_custom_path(const String& custom_dir_name);

    // Platform info
    static String get_current_platform();
    static String get_home_directory();
    static String get_app_data_directory();

    // Directory operations
    static bool create_directory(const String& path);
    static bool is_directory_writable(const String& path);

    // Constants for modes (matching enum values)
    enum {
        MODE_APP_DEFAULT = 0,    // Uses app name as directory
        MODE_GODOT_PROJECT = 1,  // Uses Godot's app_userdata structure
        MODE_CUSTOM_ONLY = 2     // Complete custom directory
    };
};

} // namespace filesystem
} // namespace toolkit