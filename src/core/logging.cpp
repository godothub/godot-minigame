#include "core/logging.h"

namespace toolkit {
namespace core {

static bool g_log_enabled = false;

void Logger::set_enabled(bool enabled) {
    g_log_enabled = enabled;
}

bool Logger::is_enabled() {
    return g_log_enabled;
}

} // namespace core
} // namespace toolkit
