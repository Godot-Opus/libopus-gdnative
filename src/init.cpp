#include <Godot.hpp>
#include "OpusEncoderNode.h"
#include "OpusDecoderNode.h"


extern "C" void GDN_EXPORT opus_gdnative_init(godot_gdnative_init_options *o) {
	godot::Godot::gdnative_init(o);
}

extern "C" void GDN_EXPORT opus_gdnative_terminate(godot_gdnative_terminate_options *o) {
	godot::Godot::gdnative_terminate(o);
}

extern "C" void GDN_EXPORT opus_nativescript_init(void *handle) {
	godot::Godot::nativescript_init(handle);

	godot::register_class<opus::OpusEncoderNode>();
	godot::register_class<opus::OpusDecoderNode>();
}
