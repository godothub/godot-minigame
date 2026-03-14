#ifndef EXAMPLE_NODE_H
#define EXAMPLE_NODE_H

#include "YAML_defines.h"

#include <godot_cpp/classes/ref_counted.hpp>

class YAML : public RefCounted {
	GDCLASS(YAML, RefCounted);

protected:
	static void _bind_methods();

public:
	static Variant parse_string(const String &p_string);
	static Variant parse_file(const String &p_path);
};

#endif // EXAMPLE_NODE_H
