#pragma once

#include <cstddef>

namespace toolkit {
namespace resources {

struct EmbeddedResourceEntry {
    const char* path;
    const unsigned char* data;
    size_t size;
};

extern const EmbeddedResourceEntry embedded_resources[];

} // namespace resources
} // namespace toolkit