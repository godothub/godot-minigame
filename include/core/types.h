#pragma once

#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/array.hpp>
#include <memory>
#include <functional>
#include <string>
#include <vector>
#include <map>

namespace toolkit {

// Core types and aliases
using String = godot::String;
using Variant = godot::Variant;
using Dictionary = godot::Dictionary;
using Array = godot::Array;
using ByteArray = godot::PackedByteArray;

// Cross-platform path type
#ifdef _WIN32
using PathChar = wchar_t;
using PathString = std::wstring;
#define PATH_SEPARATOR L"\\"
#define PATH_SEPARATOR_CHAR L'\\'
#else
using PathChar = char;
using PathString = std::string;
#define PATH_SEPARATOR "/"
#define PATH_SEPARATOR_CHAR '/'
#endif

// File system types
enum class FileMode {
    READ,
    WRITE,
    APPEND,
    READ_WRITE
};

enum class FileType {
    UNKNOWN,
    REGULAR_FILE,
    DIRECTORY,
    SYMBOLIC_LINK
};

struct FileInfo {
    String path;
    FileType type = FileType::UNKNOWN;
    int64_t size = 0;
    int64_t modified_time = 0;
    bool is_readable = false;
    bool is_writable = false;
    bool is_executable = false;
};

// Network types
enum class DownloadState {
    PENDING,
    DOWNLOADING,
    COMPLETED,
    FAILED,
    CANCELLED
};

struct DownloadProgress {
    int64_t bytes_received = 0;
    int64_t total_bytes = 0;
    double percentage = 0.0;
    DownloadState state = DownloadState::PENDING;
    String error_message;
};

// Resource types
struct EmbeddedResource {
    String name;
    String path;
    ByteArray data;
    String mime_type;
    Dictionary metadata;
};

// Version information
struct VersionInfo {
    int major = 0;
    int minor = 0;
    int patch = 0;
    String build;
    String full_string;
    
    bool is_newer_than(const VersionInfo& other) const;
    String to_string() const;
    static VersionInfo from_string(const String& version_str);
};

// Callback types
using ProgressCallback = std::function<void(const DownloadProgress&)>;
using CompletionCallback = std::function<void(bool success, const String& error)>;
using FileOperationCallback = std::function<void(bool success, const Array& files)>;

// Error handling
enum class ErrorCode {
    OK = 0,
    GENERIC_ERROR,
    FILE_NOT_FOUND,
    ACCESS_DENIED,
    INVALID_PATH,
    NETWORK_ERROR,
    PARSE_ERROR,
    RESOURCE_NOT_FOUND,
    VERSION_MISMATCH,
    CORRUPTED_DATA
};

struct Result {
    ErrorCode error_code = ErrorCode::OK;
    String message;
    Variant data;
    
    bool is_ok() const { return error_code == ErrorCode::OK; }
    bool is_error() const { return error_code != ErrorCode::OK; }
    
    static Result ok(const Variant& data = Variant());
    static Result error(ErrorCode code, const String& message = String());
};

// Configuration types
struct PluginConfig {
    String version;
    String name;
    String description;
    Dictionary settings;
    Array enabled_features;
    
    static PluginConfig load_from_file(const String& path);
    bool save_to_file(const String& path) const;
};

} // namespace toolkit