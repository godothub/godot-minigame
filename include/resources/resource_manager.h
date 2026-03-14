#pragma once

#include "core/types.h"
#include <godot_cpp/variant/callable.hpp>
#include <memory>
#include <map>

namespace toolkit {
namespace resources {

// Resource embedding and management
class ResourceManager {
public:
    static ResourceManager* get_singleton();
    static void initialize();
    static void shutdown();
    
    // Embedded resource access
    Result get_embedded_resource(const String& path, ByteArray& data);
    Result get_embedded_resource_info(const String& path, EmbeddedResource& info);
    Array list_embedded_resources();
    bool has_embedded_resource(const String& path);
    
    // ZIP file operations
    Result extract_zip(const String& zip_path, const String& destination);
    Result extract_zip_from_memory(const ByteArray& zip_data, const String& destination);
    Result create_zip(const String& source_dir, const String& zip_path);
    Result list_zip_contents(const String& zip_path, Array& entries);
    Result read_file_from_zip(const String& zip_path, const String& file_path, ByteArray& data);
    
    // JavaScript execution context
    Result execute_embedded_javascript(const String& script_name, const Dictionary& context, Variant& result);
    Result load_javascript_module(const String& module_name);
    void register_javascript_api(const String& name, const godot::Callable& function);
    
    // Resource manifest management
    Result load_manifest(const String& path);
    Result save_manifest(const String& path);
    void clear_manifest();
    Dictionary get_manifest();
    
    // Virtual file system overlay
    void mount_embedded_resources(const String& mount_point = "/embedded");
    void unmount_embedded_resources();
    bool is_virtual_path(const String& path);
    Result resolve_virtual_path(const String& virtual_path, String& real_path);
    
    // Resource caching
    void enable_caching(bool enabled = true);
    void clear_cache();
    void set_cache_limit(size_t max_size_mb);

private:
    ResourceManager();
    ~ResourceManager();
    
    static ResourceManager* singleton;
    
    std::map<String, EmbeddedResource> embedded_resource_cache;
    Dictionary resource_manifest;
    bool caching_enabled = true;
    std::map<String, ByteArray> resource_cache;
    size_t cache_limit_bytes = 100 * 1024 * 1024; // 100MB default
    size_t current_cache_size = 0;
    
    void initialize_embedded_resources();
    void load_embedded_data();
    Result decompress_data(const ByteArray& compressed, ByteArray& decompressed);
    void cleanup_cache();
    void evict_lru_cache_entries();
};

// ZIP file utility class
class ZipArchive {
public:
    ZipArchive();
    ~ZipArchive();
    
    Result open(const String& path, bool create = false);
    Result open_from_memory(const ByteArray& data);
    void close();
    
    Result extract_all(const String& destination);
    Result extract_file(const String& file_path, const String& destination);
    Result extract_file_to_memory(const String& file_path, ByteArray& data);
    Result add_file(const String& file_path, const ByteArray& data);
    Result add_directory(const String& dir_path);
    Result remove_file(const String& file_path);
    
    Array list_files();
    bool contains_file(const String& file_path);
    Result get_file_info(const String& file_path, FileInfo& info);
    
    Result save();
    Result save_to_memory(ByteArray& data);

private:
    class ZipImpl;
    std::unique_ptr<ZipImpl> impl;
};

// JavaScript execution engine
class JavaScriptEngine {
public:
    JavaScriptEngine();
    ~JavaScriptEngine();
    
    Result initialize();
    void shutdown();
    
    Result execute_script(const String& script, const Dictionary& context, Variant& result);
    Result load_module(const String& module_name, const String& source);
    Result call_function(const String& function_name, const Array& args, Variant& result);
    
    void register_global_function(const String& name, const godot::Callable& function);
    void register_global_object(const String& name, const Dictionary& object);
    void set_global_variable(const String& name, const Variant& value);
    Variant get_global_variable(const String& name);
    
    // Node.js-like API registration
    void register_nodejs_apis();
    void register_filesystem_api();
    void register_network_api();
    void register_process_api();

private:
    class JSImpl;
    std::unique_ptr<JSImpl> impl;
    
    Dictionary registered_globals;
    std::map<String, String> loaded_modules;
};

// Resource builder tool (for embedding resources during build)
class ResourceBuilder {
public:
    static Result build_resource_archive(const String& source_dir, const String& output_path);
    static Result generate_resource_header(const Array& resource_files, const String& output_path);
    static Result embed_resources_in_binary(const Array& resource_files, const String& binary_path);
    
private:
    static Result compress_file(const String& file_path, ByteArray& compressed_data);
    static String generate_resource_code(const Array& resources);
    static String sanitize_identifier(const String& name);
};

} // namespace resources
} // namespace toolkit