# Android.mk
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := godot-prebuilt
LOCAL_SRC_FILES := ../godot-cpp/bin/libgodot-cpp.android.release.arm64v8.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := opus
LOCAL_SRC_FILES := ../libs/android/arm64-v8a/bin/libopus.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := godot-opus
LOCAL_CPPFLAGS := -std=c++17
LOCAL_CPP_FEATURES := rtti exceptions
LOCAL_CFLAGS := -Wno-error
LOCAL_LDLIBS := -llog

LOCAL_SRC_FILES := \
../src/init.cpp \
../src/OpusDecoderNode.cpp \
../src/OpusEncoderNode.cpp \

#LOCAL_SHARED_LIBRARIES := 

LOCAL_C_INCLUDES := \
godot-cpp/godot_headers \
godot-cpp/include/ \
godot-cpp/include/core \
godot-cpp/include/gen \
src/ \
opus/ \

LOCAL_STATIC_LIBRARIES := \
godot-prebuilt \
opus

include $(BUILD_SHARED_LIBRARY)