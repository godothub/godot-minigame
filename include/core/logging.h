#pragma once

#include <godot_cpp/variant/utility_functions.hpp>
#include <utility>

namespace toolkit {
namespace core {

class Logger {
public:
    static void set_enabled(bool enabled);
    static bool is_enabled();
};

template <typename... Args>
inline void log(Args &&...args) {
    if (!Logger::is_enabled()) {
        return;
    }
    godot::UtilityFunctions::print(std::forward<Args>(args)...);
}

template <typename... Args>
inline void log_rich(Args &&...args) {
    if (!Logger::is_enabled()) {
        return;
    }
    godot::UtilityFunctions::print_rich(std::forward<Args>(args)...);
}

} // namespace core
} // namespace toolkit

#define TOOLKIT_LOG(...) ::toolkit::core::log(__VA_ARGS__)
#define TOOLKIT_LOG_RICH(...) ::toolkit::core::log_rich(__VA_ARGS__)
