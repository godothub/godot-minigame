#include "filesystem/filesystem_interface.h"
#include <godot_cpp/variant/utility_functions.hpp>
#include "core/logging.h"

#ifdef _WIN32
#include <windows.h>
#include <shlwapi.h>
#include <direct.h>
#include <io.h>
#define getcwd _getcwd
#define chdir _chdir
#define mkdir(path, mode) _mkdir(path)
#define access _access
#define F_OK 0
#define R_OK 4
#define W_OK 2
#else
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <cstring>
#endif

using namespace godot;

namespace toolkit {
namespace filesystem {

// Static instance
std::unique_ptr<IFileSystem> FileSystem::fs_instance;

// FileSystem static methods
void FileSystem::initialize() {
    if (fs_instance) {
        return; // Already initialized
    }
    
    fs_instance = FileSystemFactory::create_native_filesystem();
    TOOLKIT_LOG("FileSystem: Initialized");
}

void FileSystem::shutdown() {
    if (!fs_instance) {
        return; // Not initialized
    }
    
    fs_instance.reset();
    TOOLKIT_LOG("FileSystem: Shut down");
}

IFileSystem* FileSystem::instance() {
    return fs_instance.get();
}

// FileSystemFactory implementation
std::unique_ptr<IFileSystem> FileSystemFactory::create_native_filesystem() {
#ifdef _WIN32
    return std::make_unique<WindowsFileSystem>();
#else
    return std::make_unique<UnixFileSystem>();
#endif
}

std::unique_ptr<IFileSystem> FileSystemFactory::create_virtual_filesystem() {
    // TODO: Implement virtual filesystem for embedded resources
    return create_native_filesystem();
}

#ifdef _WIN32

// Windows implementation
PathString WindowsFileSystem::to_native_path(const String& path) {
    std::string utf8_path = path.utf8().get_data();
    // Convert UTF-8 to wide string
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8_path.c_str(), -1, NULL, 0);
    if (size_needed <= 0) {
        return L"";
    }
    
    std::wstring result(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8_path.c_str(), -1, &result[0], size_needed);
    result.resize(size_needed - 1); // Remove null terminator
    
    // Convert forward slashes to backslashes
    for (auto& c : result) {
        if (c == L'/') {
            c = L'\\';
        }
    }
    
    return result;
}

String WindowsFileSystem::from_native_path(const PathString& path) {
    // Convert wide string to UTF-8
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, NULL, 0, NULL, NULL);
    if (size_needed <= 0) {
        return String();
    }
    
    std::string result(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, &result[0], size_needed, NULL, NULL);
    result.resize(size_needed - 1); // Remove null terminator
    
    return String(result.c_str());
}

Result WindowsFileSystem::read_file(const String& path, ByteArray& data) {
    PathString native_path = to_native_path(path);
    
    HANDLE file = CreateFileW(native_path.c_str(), GENERIC_READ, FILE_SHARE_READ, 
                             NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    
    if (file == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND) {
            return Result::error(ErrorCode::FILE_NOT_FOUND, "File not found: " + path);
        } else if (error == ERROR_ACCESS_DENIED) {
            return Result::error(ErrorCode::ACCESS_DENIED, "Access denied: " + path);
        } else {
            return Result::error(ErrorCode::GENERIC_ERROR, "Failed to open file: " + path);
        }
    }
    
    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(file, &file_size)) {
        CloseHandle(file);
        return Result::error(ErrorCode::GENERIC_ERROR, "Failed to get file size: " + path);
    }
    
    data.resize(file_size.QuadPart);
    
    DWORD bytes_read;
    if (!ReadFile(file, data.ptrw(), static_cast<DWORD>(file_size.QuadPart), &bytes_read, NULL)) {
        CloseHandle(file);
        return Result::error(ErrorCode::GENERIC_ERROR, "Failed to read file: " + path);
    }
    
    CloseHandle(file);
    return Result::ok();
}

Result WindowsFileSystem::write_file(const String& path, const ByteArray& data) {
    PathString native_path = to_native_path(path);
    
    HANDLE file = CreateFileW(native_path.c_str(), GENERIC_WRITE, 0, 
                             NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    
    if (file == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error == ERROR_ACCESS_DENIED) {
            return Result::error(ErrorCode::ACCESS_DENIED, "Access denied: " + path);
        } else {
            return Result::error(ErrorCode::GENERIC_ERROR, "Failed to create file: " + path);
        }
    }
    
    DWORD bytes_written;
    if (!WriteFile(file, data.ptr(), data.size(), &bytes_written, NULL)) {
        CloseHandle(file);
        return Result::error(ErrorCode::GENERIC_ERROR, "Failed to write file: " + path);
    }
    
    CloseHandle(file);
    return Result::ok();
}

// Stub implementations for other methods (to be completed)
Result WindowsFileSystem::append_file(const String& path, const ByteArray& data) {
    // TODO: Implement
    return Result::error(ErrorCode::GENERIC_ERROR, "Not implemented");
}

Result WindowsFileSystem::delete_file(const String& path) {
    PathString native_path = to_native_path(path);
    if (DeleteFileW(native_path.c_str())) {
        return Result::ok();
    } else {
        return Result::error(ErrorCode::GENERIC_ERROR, "Failed to delete file: " + path);
    }
}

Result WindowsFileSystem::copy_file(const String& source, const String& destination) {
    PathString src = to_native_path(source);
    PathString dst = to_native_path(destination);
    if (CopyFileW(src.c_str(), dst.c_str(), FALSE)) {
        return Result::ok();
    } else {
        return Result::error(ErrorCode::GENERIC_ERROR, "Failed to copy file");
    }
}

Result WindowsFileSystem::move_file(const String& source, const String& destination) {
    PathString src = to_native_path(source);
    PathString dst = to_native_path(destination);
    if (MoveFileW(src.c_str(), dst.c_str())) {
        return Result::ok();
    } else {
        return Result::error(ErrorCode::GENERIC_ERROR, "Failed to move file");
    }
}

Result WindowsFileSystem::exists(const String& path, bool& exists) {
    PathString native_path = to_native_path(path);
    DWORD attrs = GetFileAttributesW(native_path.c_str());
    exists = (attrs != INVALID_FILE_ATTRIBUTES);
    return Result::ok();
}

Result WindowsFileSystem::get_file_info(const String& path, FileInfo& info) {
    PathString native_path = to_native_path(path);
    WIN32_FILE_ATTRIBUTE_DATA attrs;
    if (GetFileAttributesExW(native_path.c_str(), GetFileExInfoStandard, &attrs)) {
        info.path = path;
        info.size = (static_cast<int64_t>(attrs.nFileSizeHigh) << 32) | attrs.nFileSizeLow;
        info.type = (attrs.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? FileType::DIRECTORY : FileType::REGULAR_FILE;
        return Result::ok();
    } else {
        return Result::error(ErrorCode::FILE_NOT_FOUND, "File not found");
    }
}

Result WindowsFileSystem::create_directory(const String& path, bool recursive) {
    PathString native_path = to_native_path(path);
    if (CreateDirectoryW(native_path.c_str(), NULL)) {
        return Result::ok();
    } else {
        return Result::error(ErrorCode::GENERIC_ERROR, "Failed to create directory");
    }
}

Result WindowsFileSystem::delete_directory(const String& path, bool recursive) {
    PathString native_path = to_native_path(path);
    if (RemoveDirectoryW(native_path.c_str())) {
        return Result::ok();
    } else {
        return Result::error(ErrorCode::GENERIC_ERROR, "Failed to delete directory");
    }
}

Result WindowsFileSystem::list_directory(const String& path, Array& entries) {
    entries.clear();
    // TODO: Implement directory listing
    return Result::ok();
}

Result WindowsFileSystem::get_working_directory(String& path) {
    wchar_t buffer[MAX_PATH];
    if (GetCurrentDirectoryW(MAX_PATH, buffer)) {
        path = from_native_path(PathString(buffer));
        return Result::ok();
    } else {
        return Result::error(ErrorCode::GENERIC_ERROR, "Failed to get working directory");
    }
}

Result WindowsFileSystem::set_working_directory(const String& path) {
    PathString native_path = to_native_path(path);
    if (SetCurrentDirectoryW(native_path.c_str())) {
        return Result::ok();
    } else {
        return Result::error(ErrorCode::GENERIC_ERROR, "Failed to set working directory");
    }
}

String WindowsFileSystem::join_path(const Array& parts) {
    String result;
    for (int i = 0; i < parts.size(); i++) {
        if (i > 0) result += "\\";
        result += String(parts[i]);
    }
    return result;
}

String WindowsFileSystem::normalize_path(const String& path) {
    return path.replace("/", "\\");
}

String WindowsFileSystem::get_directory_name(const String& path) {
    int last_sep = path.rfind("\\");
    if (last_sep == -1) last_sep = path.rfind("/");
    return last_sep != -1 ? path.substr(0, last_sep) : String();
}

String WindowsFileSystem::get_file_name(const String& path) {
    int last_sep = path.rfind("\\");
    if (last_sep == -1) last_sep = path.rfind("/");
    return last_sep != -1 ? path.substr(last_sep + 1) : path;
}

String WindowsFileSystem::get_file_extension(const String& path) {
    int last_dot = path.rfind(".");
    return last_dot != -1 ? path.substr(last_dot) : String();
}

String WindowsFileSystem::resolve_path(const String& path) {
    return normalize_path(path);
}

bool WindowsFileSystem::is_absolute_path(const String& path) {
    return path.length() > 2 && path[1] == ':' && (path[2] == '\\' || path[2] == '/');
}

void WindowsFileSystem::read_file_async(const String& path, FileOperationCallback callback) {
    // TODO: Implement async read
}

void WindowsFileSystem::write_file_async(const String& path, const ByteArray& data, CompletionCallback callback) {
    // TODO: Implement async write
}

void WindowsFileSystem::list_directory_async(const String& path, FileOperationCallback callback) {
    // TODO: Implement async directory listing
}

Result WindowsFileSystem::set_permissions(const String& path, int mode) {
    // TODO: Implement permission setting
    return Result::ok();
}

Result WindowsFileSystem::get_permissions(const String& path, int& mode) {
    // TODO: Implement permission getting
    mode = 0644;
    return Result::ok();
}

Result WindowsFileSystem::create_temp_file(String& path) {
    // TODO: Implement temp file creation
    return Result::error(ErrorCode::GENERIC_ERROR, "Not implemented");
}

Result WindowsFileSystem::create_temp_directory(String& path) {
    // TODO: Implement temp directory creation
    return Result::error(ErrorCode::GENERIC_ERROR, "Not implemented");
}

Result WindowsFileSystem::watch_file(const String& path, std::function<void()> callback) {
    // TODO: Implement file watching
    return Result::error(ErrorCode::GENERIC_ERROR, "Not implemented");
}

Result WindowsFileSystem::unwatch_file(const String& path) {
    // TODO: Implement file unwatching
    return Result::error(ErrorCode::GENERIC_ERROR, "Not implemented");
}

#else

// Unix implementation stubs
Result UnixFileSystem::read_file(const String& path, ByteArray& data) {
    std::string native_path = path.utf8().get_data();
    
    FILE* file = fopen(native_path.c_str(), "rb");
    if (!file) {
        if (errno == ENOENT) {
            return Result::error(ErrorCode::FILE_NOT_FOUND, "File not found: " + path);
        } else if (errno == EACCES) {
            return Result::error(ErrorCode::ACCESS_DENIED, "Access denied: " + path);
        } else {
            return Result::error(ErrorCode::GENERIC_ERROR, "Failed to open file: " + path);
        }
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size < 0) {
        fclose(file);
        return Result::error(ErrorCode::GENERIC_ERROR, "Failed to get file size: " + path);
    }
    
    data.resize(file_size);
    size_t bytes_read = fread(data.ptrw(), 1, file_size, file);
    fclose(file);
    
    if (bytes_read != static_cast<size_t>(file_size)) {
        return Result::error(ErrorCode::GENERIC_ERROR, "Failed to read file: " + path);
    }
    
    return Result::ok();
}

Result UnixFileSystem::write_file(const String& path, const ByteArray& data) {
    std::string native_path = path.utf8().get_data();
    
    FILE* file = fopen(native_path.c_str(), "wb");
    if (!file) {
        if (errno == EACCES) {
            return Result::error(ErrorCode::ACCESS_DENIED, "Access denied: " + path);
        } else {
            return Result::error(ErrorCode::GENERIC_ERROR, "Failed to create file: " + path);
        }
    }
    
    size_t bytes_written = fwrite(data.ptr(), 1, data.size(), file);
    fclose(file);
    
    if (bytes_written != data.size()) {
        return Result::error(ErrorCode::GENERIC_ERROR, "Failed to write file: " + path);
    }
    
    return Result::ok();
}

Result UnixFileSystem::append_file(const String& path, const ByteArray& data) {
    return Result::error(ErrorCode::GENERIC_ERROR, "Not implemented");
}

Result UnixFileSystem::delete_file(const String& path) {
    std::string native_path = path.utf8().get_data();
    if (remove(native_path.c_str()) == 0) {
        return Result::ok();
    } else {
        return Result::error(ErrorCode::GENERIC_ERROR, "Failed to delete file: " + path);
    }
}

Result UnixFileSystem::copy_file(const String& source, const String& destination) {
    return Result::error(ErrorCode::GENERIC_ERROR, "Not implemented");
}

Result UnixFileSystem::move_file(const String& source, const String& destination) {
    std::string src = source.utf8().get_data();
    std::string dst = destination.utf8().get_data();
    if (rename(src.c_str(), dst.c_str()) == 0) {
        return Result::ok();
    } else {
        return Result::error(ErrorCode::GENERIC_ERROR, "Failed to move file");
    }
}

Result UnixFileSystem::exists(const String& path, bool& exists) {
    std::string native_path = path.utf8().get_data();
    exists = (access(native_path.c_str(), F_OK) != -1);
    return Result::ok();
}

Result UnixFileSystem::get_file_info(const String& path, FileInfo& info) {
    std::string native_path = path.utf8().get_data();
    struct stat st;
    if (stat(native_path.c_str(), &st) == 0) {
        info.path = path;
        info.size = st.st_size;
        info.type = S_ISDIR(st.st_mode) ? FileType::DIRECTORY : FileType::REGULAR_FILE;
        return Result::ok();
    } else {
        return Result::error(ErrorCode::FILE_NOT_FOUND, "File not found");
    }
}

Result UnixFileSystem::create_directory(const String& path, bool recursive) {
    std::string native_path = path.utf8().get_data();
    if (mkdir(native_path.c_str(), 0777) == 0) {
        return Result::ok();
    } else {
        return Result::error(ErrorCode::GENERIC_ERROR, "Failed to create directory");
    }
}

Result UnixFileSystem::delete_directory(const String& path, bool recursive) {
    std::string native_path = path.utf8().get_data();
    if (rmdir(native_path.c_str()) == 0) {
        return Result::ok();
    } else {
        return Result::error(ErrorCode::GENERIC_ERROR, "Failed to delete directory");
    }
}

Result UnixFileSystem::list_directory(const String& path, Array& entries) {
    entries.clear();
    std::string native_path = path.utf8().get_data();
    DIR *dir = opendir(native_path.c_str());
    if (dir) {
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            entries.push_back(String(ent->d_name));
        }
        closedir(dir);
    }
    return Result::ok();
}

Result UnixFileSystem::get_working_directory(String& path) {
    char buffer[1024];
    if (getcwd(buffer, sizeof(buffer))) {
        path = String(buffer);
        return Result::ok();
    } else {
        return Result::error(ErrorCode::GENERIC_ERROR, "Failed to get working directory");
    }
}

Result UnixFileSystem::set_working_directory(const String& path) {
    std::string native_path = path.utf8().get_data();
    if (chdir(native_path.c_str()) == 0) {
        return Result::ok();
    } else {
        return Result::error(ErrorCode::GENERIC_ERROR, "Failed to set working directory");
    }
}

String UnixFileSystem::join_path(const Array& parts) {
    String result;
    for (int i = 0; i < parts.size(); i++) {
        if (i > 0) result += "/";
        result += String(parts[i]);
    }
    return result;
}

String UnixFileSystem::normalize_path(const String& path) {
    return path.replace("\\", "/");
}

String UnixFileSystem::get_directory_name(const String& path) {
    int last_sep = path.rfind("/");
    return last_sep != -1 ? path.substr(0, last_sep) : String();
}

String UnixFileSystem::get_file_name(const String& path) {
    int last_sep = path.rfind("/");
    return last_sep != -1 ? path.substr(last_sep + 1) : path;
}

String UnixFileSystem::get_file_extension(const String& path) {
    int last_dot = path.rfind(".");
    return last_dot != -1 ? path.substr(last_dot) : String();
}

String UnixFileSystem::resolve_path(const String& path) {
    return normalize_path(path);
}

bool UnixFileSystem::is_absolute_path(const String& path) {
    return path.begins_with("/");
}

void UnixFileSystem::read_file_async(const String& path, FileOperationCallback callback) {
    // TODO: Implement async read
}

void UnixFileSystem::write_file_async(const String& path, const ByteArray& data, CompletionCallback callback) {
    // TODO: Implement async write
}

void UnixFileSystem::list_directory_async(const String& path, FileOperationCallback callback) {
    // TODO: Implement async directory listing
}

Result UnixFileSystem::set_permissions(const String& path, int mode) {
    return Result::ok();
}

Result UnixFileSystem::get_permissions(const String& path, int& mode) {
    mode = 0644;
    return Result::ok();
}

Result UnixFileSystem::create_temp_file(String& path) {
    return Result::error(ErrorCode::GENERIC_ERROR, "Not implemented");
}

Result UnixFileSystem::create_temp_directory(String& path) {
    return Result::error(ErrorCode::GENERIC_ERROR, "Not implemented");
}

Result UnixFileSystem::watch_file(const String& path, std::function<void()> callback) {
    return Result::error(ErrorCode::GENERIC_ERROR, "Not implemented");
}

Result UnixFileSystem::unwatch_file(const String& path) {
    return Result::error(ErrorCode::GENERIC_ERROR, "Not implemented");
}

#endif

// Node.js-like API implementation
namespace fs {

Result readFile(const String& path, ByteArray& data) {
    if (!FileSystem::instance()) {
        return Result::error(ErrorCode::GENERIC_ERROR, "FileSystem not initialized");
    }
    return FileSystem::instance()->read_file(path, data);
}

Result writeFile(const String& path, const ByteArray& data) {
    if (!FileSystem::instance()) {
        return Result::error(ErrorCode::GENERIC_ERROR, "FileSystem not initialized");
    }
    return FileSystem::instance()->write_file(path, data);
}

Result unlink(const String& path) {
    if (!FileSystem::instance()) {
        return Result::error(ErrorCode::GENERIC_ERROR, "FileSystem not initialized");
    }
    return FileSystem::instance()->delete_file(path);
}

Result exists(const String& path, bool& exists) {
    if (!FileSystem::instance()) {
        return Result::error(ErrorCode::GENERIC_ERROR, "FileSystem not initialized");
    }
    return FileSystem::instance()->exists(path, exists);
}

} // namespace fs

} // namespace filesystem
} // namespace toolkit