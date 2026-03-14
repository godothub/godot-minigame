#include "filesystem/user_data_helper.h"
#include "filesystem/user_data_path.h"

using namespace godot;

namespace toolkit {
namespace filesystem {

UserDataHelper::UserDataHelper() {
}

UserDataHelper::~UserDataHelper() {
}

void UserDataHelper::_bind_methods() {
    // Static methods for path getting
    ClassDB::bind_static_method("UserDataHelper", D_METHOD("get_user_data_path", "mode", "custom_dir_name"),
                                &UserDataHelper::get_user_data_path, DEFVAL(MODE_APP_DEFAULT), DEFVAL(""));
    ClassDB::bind_static_method("UserDataHelper", D_METHOD("get_app_data_path", "app_name"),
                                &UserDataHelper::get_app_data_path, DEFVAL(""));
    ClassDB::bind_static_method("UserDataHelper", D_METHOD("get_godot_project_path", "project_name"),
                                &UserDataHelper::get_godot_project_path);
    ClassDB::bind_static_method("UserDataHelper", D_METHOD("get_custom_path", "custom_dir_name"),
                                &UserDataHelper::get_custom_path);

    // Platform info methods
    ClassDB::bind_static_method("UserDataHelper", D_METHOD("get_current_platform"),
                                &UserDataHelper::get_current_platform);
    ClassDB::bind_static_method("UserDataHelper", D_METHOD("get_home_directory"),
                                &UserDataHelper::get_home_directory);
    ClassDB::bind_static_method("UserDataHelper", D_METHOD("get_app_data_directory"),
                                &UserDataHelper::get_app_data_directory);

    // Directory operations
    ClassDB::bind_static_method("UserDataHelper", D_METHOD("create_directory", "path"),
                                &UserDataHelper::create_directory);
    ClassDB::bind_static_method("UserDataHelper", D_METHOD("is_directory_writable", "path"),
                                &UserDataHelper::is_directory_writable);

    // Constants
    BIND_CONSTANT(MODE_APP_DEFAULT);
    BIND_CONSTANT(MODE_GODOT_PROJECT);
    BIND_CONSTANT(MODE_CUSTOM_ONLY);
}

String UserDataHelper::get_user_data_path(int mode, const String& custom_dir_name) {
    UserDataPath::PathMode path_mode = static_cast<UserDataPath::PathMode>(mode);
    return UserDataPath::get_user_data_base_path(path_mode, custom_dir_name);
}

String UserDataHelper::get_app_data_path(const String& app_name) {
    return UserDataPath::get_app_data_path(app_name);
}

String UserDataHelper::get_godot_project_path(const String& project_name) {
    return UserDataPath::get_godot_project_path(project_name);
}

String UserDataHelper::get_custom_path(const String& custom_dir_name) {
    return UserDataPath::get_custom_path(custom_dir_name);
}

String UserDataHelper::get_current_platform() {
    return UserDataPath::get_current_platform_name();
}

String UserDataHelper::get_home_directory() {
    return UserDataPath::get_platform_home_dir();
}

String UserDataHelper::get_app_data_directory() {
    return UserDataPath::get_platform_app_data_dir();
}

bool UserDataHelper::create_directory(const String& path) {
    return UserDataPath::create_directory_if_not_exists(path);
}

bool UserDataHelper::is_directory_writable(const String& path) {
    return UserDataPath::is_path_writable(path);
}

} // namespace filesystem
} // namespace toolkit