#include "core/types.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <sstream>
#include <regex>
#include "core/logging.h"

using namespace godot;

namespace toolkit {

// VersionInfo implementation
bool VersionInfo::is_newer_than(const VersionInfo& other) const {
    if (major != other.major) {
        return major > other.major;
    }
    if (minor != other.minor) {
        return minor > other.minor;
    }
    return patch > other.patch;
}

String VersionInfo::to_string() const {
    if (!full_string.is_empty()) {
        return full_string;
    }
    
    String result = String::num_int64(major) + "." + String::num_int64(minor) + "." + String::num_int64(patch);
    if (!build.is_empty()) {
        result += "-" + build;
    }
    return result;
}

VersionInfo VersionInfo::from_string(const String& version_str) {
    VersionInfo info;
    info.full_string = version_str;
    
    // Parse version string (e.g., "1.2.3-beta" or "1.2.3")
    std::string std_str = version_str.utf8().get_data();
    std::regex version_regex(R"(^(\d+)\.(\d+)\.(\d+)(?:-(.+))?$)");
    std::smatch matches;
    
    if (std::regex_match(std_str, matches, version_regex)) {
        info.major = std::stoi(matches[1].str());
        info.minor = std::stoi(matches[2].str());
        info.patch = std::stoi(matches[3].str());
        if (matches[4].matched) {
            info.build = String(matches[4].str().c_str());
        }
    } else {
        // Fallback parsing
        PackedStringArray parts = version_str.split(".");
        if (parts.size() >= 1) {
            String major_part = parts[0];
            if (major_part.contains("-")) {
                PackedStringArray major_split = major_part.split("-");
                info.major = major_split[0].to_int();
                if (major_split.size() > 1) {
                    info.build = major_split[1];
                }
            } else {
                info.major = major_part.to_int();
            }
        }
        if (parts.size() >= 2) {
            info.minor = parts[1].to_int();
        }
        if (parts.size() >= 3) {
            String patch_part = parts[2];
            if (patch_part.contains("-")) {
                PackedStringArray patch_split = patch_part.split("-");
                info.patch = patch_split[0].to_int();
                if (patch_split.size() > 1 && info.build.is_empty()) {
                    info.build = patch_split[1];
                }
            } else {
                info.patch = patch_part.to_int();
            }
        }
    }
    
    return info;
}

// Result implementation
Result Result::ok(const Variant& data) {
    Result result;
    result.error_code = ErrorCode::OK;
    result.data = data;
    return result;
}

Result Result::error(ErrorCode code, const String& message) {
    Result result;
    result.error_code = code;
    result.message = message;
    return result;
}

// PluginConfig implementation
PluginConfig PluginConfig::load_from_file(const String& path) {
    PluginConfig config;
    
    // Load configuration from file
    Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
    if (file.is_null()) {
        TOOLKIT_LOG_RICH("[color=red]Failed to open config file: " + path + "[/color]");
        return config;
    }
    
    String content = file->get_as_text();
    file->close();
    
    // Parse JSON configuration
    JSON json;
    Error parse_error = json.parse(content);
    if (parse_error != OK) {
        TOOLKIT_LOG_RICH("[color=red]Failed to parse config file: " + path + "[/color]");
        return config;
    }
    
    Dictionary data = json.get_data();
    
    // Extract configuration values
    if (data.has("version")) {
        config.version = data["version"];
    }
    if (data.has("name")) {
        config.name = data["name"];
    }
    if (data.has("description")) {
        config.description = data["description"];
    }
    if (data.has("settings")) {
        config.settings = data["settings"];
    }
    if (data.has("enabled_features")) {
        config.enabled_features = data["enabled_features"];
    }
    
    return config;
}

bool PluginConfig::save_to_file(const String& path) const {
    Dictionary data;
    
    // Build configuration dictionary
    if (!version.is_empty()) {
        data["version"] = version;
    }
    if (!name.is_empty()) {
        data["name"] = name;
    }
    if (!description.is_empty()) {
        data["description"] = description;
    }
    if (!settings.is_empty()) {
        data["settings"] = settings;
    }
    if (!enabled_features.is_empty()) {
        data["enabled_features"] = enabled_features;
    }
    
    // Convert to JSON
    JSON json;
    String json_string = json.stringify(data, "\t");
    
    // Write to file
    Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
    if (file.is_null()) {
        TOOLKIT_LOG_RICH("[color=red]Failed to create config file: " + path + "[/color]");
        return false;
    }
    
    file->store_string(json_string);
    file->close();
    
    return true;
}

} // namespace toolkit