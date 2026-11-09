# android/jni/Android.mk — the game as a shared library for Android.
#
# raylib's Android support supplies the native glue (android_main, the window and
# the GL context) and calls your main() from it, so main.c needs no #ifdef here.
#
#   export NDK_MODULE_PATH=/path/to/raylib
#   cd android && ndk-build
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE    := gk
LOCAL_SRC_FILES := ../../main.c
LOCAL_CFLAGS    := -std=c11 -O2 -Wall -I$(LOCAL_PATH)/../..
LOCAL_LDLIBS    := -llog -landroid -lEGL -lGLESv2 -lm
LOCAL_STATIC_LIBRARIES := raylib
include $(BUILD_SHARED_LIBRARY)

$(call import-module,raylib)
