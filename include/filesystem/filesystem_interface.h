#pragma once

#include "core/types.h"
#include <memory>

namespace toolkit {
namespace filesystem {

// Abstract filesystem interface - Node.js-like API
class IFileSystem {
public:
    virtual ~IFileSystem() = default;
    
    // File operations
    virtual Result read_file(const String& path, ByteArray& data) = 0;
    virtual Result write_file(const String& path, const ByteArray& data) = 0;
    virtual Result append_file(const String& path, const ByteArray& data) = 0;
    virtual Result delete_file(const String& path) = 0;
    virtual Result copy_file(const String& source, const String& destination) = 0;
    virtual Result move_file(const String& source, const String& destination) = 0;
    virtual Result exists(const String& path, bool& exists) = 0;
    virtual Result get_file_info(const String& path, FileInfo& info) = 0;
    
    // Directory operations
    virtual Result create_directory(const String& path, bool recursive = true) = 0;
    virtual Result delete_directory(const String& path, bool recursive = false) = 0;
    virtual Result list_directory(const String& path, Array& entries) = 0;
    virtual Result get_working_directory(String& path) = 0;
    virtual Result set_working_directory(const String& path) = 0;
    
    // Path operations
    virtual String join_path(const Array& parts) = 0;
    virtual String normalize_path(const String& path) = 0;
    virtual String get_directory_name(const String& path) = 0;
    virtual String get_file_name(const String& path) = 0;
    virtual String get_file_extension(const String& path) = 0;
    virtual String resolve_path(const String& path) = 0;
    virtual bool is_absolute_path(const String& path) = 0;
    
    // Async operations (callbacks)
    virtual void read_file_async(const String& path, FileOperationCallback callback) = 0;
    virtual void write_file_async(const String& path, const ByteArray& data, CompletionCallback callback) = 0;
    virtual void list_directory_async(const String& path, FileOperationCallback callback) = 0;
    
    // Permissions
    virtual Result set_permissions(const String& path, int mode) = 0;
    virtual Result get_permissions(const String& path, int& mode) = 0;
    
    // Temporary files
    virtual Result create_temp_file(String& path) = 0;
    virtual Result create_temp_directory(String& path) = 0;
    
    // File watching (for future use)
    virtual Result watch_file(const String& path, std::function<void()> callback) = 0;
    virtual Result unwatch_file(const String& path) = 0;
};

// Platform-specific implementations
class WindowsFileSystem : public IFileSystem {
public:
    // Implement all virtual methods for Windows
    Result read_file(const String& path, ByteArray& data) override;
    Result write_file(const String& path, const ByteArray& data) override;
    Result append_file(const String& path, const ByteArray& data) override;
    Result delete_file(const String& path) override;
    Result copy_file(const String& source, const String& destination) override;
    Result move_file(const String& source, const String& destination) override;
    Result exists(const String& path, bool& exists) override;
    Result get_file_info(const String& path, FileInfo& info) override;
    
    Result create_directory(const String& path, bool recursive = true) override;
    Result delete_directory(const String& path, bool recursive = false) override;
    Result list_directory(const String& path, Array& entries) override;
    Result get_working_directory(String& path) override;
    Result set_working_directory(const String& path) override;
    
    String join_path(const Array& parts) override;
    String normalize_path(const String& path) override;
    String get_directory_name(const String& path) override;
    String get_file_name(const String& path) override;
    String get_file_extension(const String& path) override;
    String resolve_path(const String& path) override;
    bool is_absolute_path(const String& path) override;
    
    void read_file_async(const String& path, FileOperationCallback callback) override;
    void write_file_async(const String& path, const ByteArray& data, CompletionCallback callback) override;
    void list_directory_async(const String& path, FileOperationCallback callback) override;
    
    Result set_permissions(const String& path, int mode) override;
    Result get_permissions(const String& path, int& mode) override;
    
    Result create_temp_file(String& path) override;
    Result create_temp_directory(String& path) override;
    
    Result watch_file(const String& path, std::function<void()> callback) override;
    Result unwatch_file(const String& path) override;

private:
    PathString to_native_path(const String& path);
    String from_native_path(const PathString& path);
};

class UnixFileSystem : public IFileSystem {
public:
    // Implement all virtual methods for Unix-like systems (Linux, macOS)
    Result read_file(const String& path, ByteArray& data) override;
    Result write_file(const String& path, const ByteArray& data) override;
    Result append_file(const String& path, const ByteArray& data) override;
    Result delete_file(const String& path) override;
    Result copy_file(const String& source, const String& destination) override;
    Result move_file(const String& source, const String& destination) override;
    Result exists(const String& path, bool& exists) override;
    Result get_file_info(const String& path, FileInfo& info) override;
    
    Result create_directory(const String& path, bool recursive = true) override;
    Result delete_directory(const String& path, bool recursive = false) override;
    Result list_directory(const String& path, Array& entries) override;
    Result get_working_directory(String& path) override;
    Result set_working_directory(const String& path) override;
    
    String join_path(const Array& parts) override;
    String normalize_path(const String& path) override;
    String get_directory_name(const String& path) override;
    String get_file_name(const String& path) override;
    String get_file_extension(const String& path) override;
    String resolve_path(const String& path) override;
    bool is_absolute_path(const String& path) override;
    
    void read_file_async(const String& path, FileOperationCallback callback) override;
    void write_file_async(const String& path, const ByteArray& data, CompletionCallback callback) override;
    void list_directory_async(const String& path, FileOperationCallback callback) override;
    
    Result set_permissions(const String& path, int mode) override;
    Result get_permissions(const String& path, int& mode) override;
    
    Result create_temp_file(String& path) override;
    Result create_temp_directory(String& path) override;
    
    Result watch_file(const String& path, std::function<void()> callback) override;
    Result unwatch_file(const String& path) override;
};

// Factory for creating platform-specific filesystem
class FileSystemFactory {
public:
    static std::unique_ptr<IFileSystem> create_native_filesystem();
    static std::unique_ptr<IFileSystem> create_virtual_filesystem();
};

// Global filesystem instance
class FileSystem {
public:
    static void initialize();
    static void shutdown();
    static IFileSystem* instance();
    
private:
    static std::unique_ptr<IFileSystem> fs_instance;
};

// Node.js-like API wrapper functions
namespace fs {
    Result readFile(const String& path, ByteArray& data);
    Result writeFile(const String& path, const ByteArray& data);
    Result appendFile(const String& path, const ByteArray& data);
    Result unlink(const String& path);
    Result copyFile(const String& source, const String& destination);
    Result rename(const String& source, const String& destination);
    Result stat(const String& path, FileInfo& info);
    Result exists(const String& path, bool& exists);
    
    Result mkdir(const String& path, bool recursive = true);
    Result rmdir(const String& path, bool recursive = false);
    Result readdir(const String& path, Array& entries);
    Result getcwd(String& path);
    Result chdir(const String& path);
    
    // Async versions
    void readFile(const String& path, FileOperationCallback callback);
    void writeFile(const String& path, const ByteArray& data, CompletionCallback callback);
    void readdir(const String& path, FileOperationCallback callback);
}

} // namespace filesystem
} // namespace toolkit