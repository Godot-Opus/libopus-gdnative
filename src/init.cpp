#include <gdextension_interface.h>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

#include "OpusEncoderNode.h"
#include "OpusDecoderNode.h"

using namespace godot;

void initialize_opus_module(ModuleInitializationLevel p_level)
{
	if(p_level != MODULE_INITIALIZATION_LEVEL_SCENE)
	{
		return;
	}

	GDREGISTER_CLASS(opus::OpusEncoderNode);
	GDREGISTER_CLASS(opus::OpusDecoderNode);
}

void uninitialize_opus_module(ModuleInitializationLevel p_level)
{
	if(p_level != MODULE_INITIALIZATION_LEVEL_SCENE)
	{
		return;
	}
}

extern "C" {
GDExtensionBool GDE_EXPORT opus_library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization)
{
	godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

	init_obj.register_initializer(initialize_opus_module);
	init_obj.register_terminator(uninitialize_opus_module);
	init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

	return init_obj.init();
}
}
