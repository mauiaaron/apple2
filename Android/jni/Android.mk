LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := apple2ix
LOCAL_SRC_FILES := jnihooks.c

LOCAL_LDLIBS := -llog -landroid

# Build a shared library and let Java/Dalvik drive
include $(BUILD_SHARED_LIBRARY)

# --OR-- Build an executable so native can drive this show
#include $(BUILD_EXECUTABLE)

