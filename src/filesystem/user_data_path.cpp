#include "filesystem/user_data_path.h"
#include "core/logging.h"

#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

namespace toolkit
{
    namespace filesystem
    {

        String UserDataPath::get_user_data_base_path(PathMode mode, const String &custom_dir_name)
        {
            String platform = get_current_platform_name();
            String base_path;

            switch (mode)
            {
            case PathMode::CUSTOM_ONLY:
            {
                if (custom_dir_name.is_empty())
                {
                    TOOLKIT_LOG_RICH("[color=yellow]Warning: Custom directory name is empty[/color]");
                    return "";
                }
                return get_custom_path(custom_dir_name);
            }

            case PathMode::GODOT_PROJECT:
            {
                String project_name = custom_dir_name.is_empty() ? "DefaultProject" : custom_dir_name;
                return get_godot_project_path(project_name);
            }

            case PathMode::APP_DEFAULT:
            default:
            {
                String app_name = custom_dir_name.is_empty() ? "godot-toolkit" : custom_dir_name;
                return get_app_data_path(app_name);
            }
            }
        }

        String UserDataPath::get_app_data_path(const String &app_name)
        {
            String platform = get_current_platform_name();
            String actual_app_name = app_name.is_empty() ? "godot-toolkit" : app_name;

            if (platform == "windows")
            {
                // Windows: %APPDATA%\AppName
                String app_data = get_platform_app_data_dir();
                return join_paths(app_data, actual_app_name);
            }
            else if (platform == "macos")
            {
                // macOS: ~/Library/Application Support/AppName
                String home = get_platform_home_dir();
                return join_paths(home, "Library", "Application Support", actual_app_name);
            }
            else
            {
                // Linux: ~/.local/share/AppName
                String home = get_platform_home_dir();
                return join_paths(home, ".local", "share", actual_app_name);
            }
        }

        String UserDataPath::get_godot_project_path(const String &project_name)
        {
            String platform = get_current_platform_name();

            if (platform == "windows")
            {
                // Windows: %APPDATA%\Godot\app_userdata\ProjectName
                String app_data = get_platform_app_data_dir();
                return join_paths(app_data, "Godot", "app_userdata", project_name);
            }
            else if (platform == "macos")
            {
                // macOS: ~/Library/Application Support/Godot/app_userdata/ProjectName
                String home = get_platform_home_dir();
                return join_paths(join_paths(home, "Library", "Application Support", "Godot"), "app_userdata", project_name);
            }
            else
            {
                // Linux: ~/.local/share/godot/app_userdata/ProjectName
                String home = get_platform_home_dir();
                return join_paths(join_paths(home, ".local", "share", "godot"), "app_userdata", project_name);
            }
        }

        String UserDataPath::get_custom_path(const String &custom_dir_name)
        {
            String platform = get_current_platform_name();

            if (platform == "windows")
            {
                // Windows: %APPDATA%\CustomDirName
                String app_data = get_platform_app_data_dir();
                return join_paths(app_data, custom_dir_name);
            }
            else if (platform == "macos")
            {
                // macOS: ~/Library/Application Support/CustomDirName
                String home = get_platform_home_dir();
                return join_paths(home, "Library", "Application Support", custom_dir_name);
            }
            else
            {
                // Linux: ~/.local/share/CustomDirName
                String home = get_platform_home_dir();
                return join_paths(home, ".local", "share", custom_dir_name);
            }
        }

        String UserDataPath::get_platform_app_data_dir()
        {
            // Get platform-specific application data directory
            String platform = get_current_platform_name();

            if (platform == "windows")
            {
                // Try to get APPDATA environment variable
                String app_data = OS::get_singleton()->get_environment("APPDATA");
                if (!app_data.is_empty())
                {
                    return app_data;
                }
                // Fallback to user data dir
                return OS::get_singleton()->get_user_data_dir();
            }

            // For other platforms, use home directory as base
            return get_platform_home_dir();
        }

        String UserDataPath::get_platform_home_dir()
        {
            // Get user home directory
            String home = OS::get_singleton()->get_environment("HOME");
            if (home.is_empty())
            {
                // Windows fallback
                String user_profile = OS::get_singleton()->get_environment("USERPROFILE");
                if (!user_profile.is_empty())
                {
                    return user_profile;
                }

                // Final fallback - use Godot's user data dir parent
                String user_data = OS::get_singleton()->get_user_data_dir();
                // Try to extract home from user_data path
                return user_data.get_base_dir().get_base_dir(); // Go up two levels typically
            }
            return home;
        }

        String UserDataPath::get_current_platform_name()
        {
            String os_name = OS::get_singleton()->get_name();

            if (os_name == "Windows")
            {
                return "windows";
            }
            else if (os_name == "macOS")
            {
                return "macos";
            }
            else if (os_name == "Linux" || os_name == "FreeBSD" || os_name == "NetBSD" || os_name == "OpenBSD")
            {
                return "linux";
            }

            // Default to linux for unknown Unix-like systems
            return "linux";
        }

        bool UserDataPath::create_directory_if_not_exists(const String &path)
        {
            if (path.is_empty())
            {
                return false;
            }

            if (path.is_absolute_path())
            {
                if (DirAccess::dir_exists_absolute(path))
                {
                    return true;
                }

                Error abs_err = DirAccess::make_dir_recursive_absolute(path);
                if (abs_err != OK)
                {
                    TOOLKIT_LOG_RICH("[color=red]Error: Failed to create absolute directory: ", path, " (Error: ", abs_err, ")[/color]");
                    return false;
                }

                TOOLKIT_LOG("Created directory: ", path);
                return true;
            }

            Ref<DirAccess> dir = DirAccess::open(path.get_base_dir());
            if (dir.is_null())
            {
                TOOLKIT_LOG_RICH("[color=red]Error: Cannot access parent directory: ", path.get_base_dir(), "[/color]");
                return false;
            }

            if (dir->dir_exists(path))
            {
                return true; // Directory already exists
            }

            Error err = dir->make_dir_recursive(path);
            if (err != OK)
            {
                TOOLKIT_LOG_RICH("[color=red]Error: Failed to create directory: ", path, " (Error: ", err, ")[/color]");
                return false;
            }

            TOOLKIT_LOG("Created directory: ", path);
            return true;
        }

        bool UserDataPath::is_path_writable(const String &path)
        {
            // Test if we can write to the directory by creating a temporary file
            String test_file = path + String("/test_write.tmp");

            Ref<FileAccess> file = FileAccess::open(test_file, FileAccess::WRITE);
            if (file.is_null())
            {
                return false;
            }

            file->store_string("test");
            file->close();

            // Clean up test file
            Ref<DirAccess> dir = DirAccess::open(path);
            if (dir.is_valid())
            {
                dir->remove(test_file);
            }

            return true;
        }

        // Path joining utility functions
        String UserDataPath::join_paths(const String &base, const String &path1)
        {
            String separator = "/";
            if (get_current_platform_name() == "windows")
            {
                separator = "\\";
            }

            String result = base;
            if (!result.ends_with(separator) && !path1.begins_with(separator))
            {
                result += separator;
            }
            result += path1;
            return result;
        }

        String UserDataPath::join_paths(const String &base, const String &path1, const String &path2)
        {
            return join_paths(join_paths(base, path1), path2);
        }

        String UserDataPath::join_paths(const String &base, const String &path1, const String &path2, const String &path3)
        {
            return join_paths(join_paths(join_paths(base, path1), path2), path3);
        }

        String UserDataPath::join_paths(const String &base, const String &path1, const String &path2, const String &path3, const String &path4)
        {
            return join_paths(join_paths(join_paths(join_paths(base, path1), path2), path3), path4);
        }

    } // namespace filesystem
} // namespace toolkit
