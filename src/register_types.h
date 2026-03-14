#pragma once

#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void initialize_gdextension_types(ModuleInitializationLevel p_level);
void uninitialize_gdextension_types(ModuleInitializationLevel p_level);