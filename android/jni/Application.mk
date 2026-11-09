# android/jni/Application.mk — ndk-build settings for the starter.
#
#   cd android && ndk-build
#
# Needs: the Android NDK, and raylib built for Android (raylib ships its own
# Android.mk; point NDK_MODULE_PATH at it). Not built in this repository's CI:
# there is no NDK there, so this file is scaffolding, not a verified build.
APP_PLATFORM := android-24
APP_ABI := arm64-v8a armeabi-v7a x86_64
APP_STL := none
APP_CFLAGS := -std=c11 -O2 -Wall
