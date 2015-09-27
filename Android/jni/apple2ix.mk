LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

PACKAGE_IDENTIFIER := "org.deadc0de.apple2ix"
PACKAGE_NAME := "apple2ix"
COMMON_SOURCES_MK := $(LOCAL_PATH)/sources.mk
include $(COMMON_SOURCES_MK)

# -----------------------------------------------------------------------------
# Breakpad crash reporter ...

LOCAL_STATIC_LIBRARIES := breakpad_client

# hmmm, Breakpad's README.ANDROID seems to suggest you shouldn't need to do this kludgery ...
BREAKPAD_SRC_PATH := $(APPLE2_SRC_PATH)/../externals/breakpad/src
BREAKPAD_CFLAGS   := -I$(BREAKPAD_SRC_PATH) -I$(APPLE2_SRC_PATH)/common/android/include

# -----------------------------------------------------------------------------
# Android build config

LOCAL_CPP_EXTENSION := .C
LOCAL_CPPFLAGS := -std=gnu++11

LOCAL_MODULE    := libapple2ix
LOCAL_SRC_FILES := jnicrash.c $(APPLE2_SRC_PATH)/breakpad.C
#LOCAL_ARM_MODE  := arm
LOCAL_CFLAGS    := $(APPLE2_BASE_CFLAGS) $(BREAKPAD_CFLAGS)
LOCAL_LDLIBS    := $(APPLE2_BASE_LDLIBS)

ifeq ($(TARGET_ARCH_ABI),x86)
    LOCAL_SRC_FILES += $(APPLE2_X86_SRC)
else
    LOCAL_SRC_FILES += $(APPLE2_ARM_SRC)
endif

ifeq ($(EMBEDDED_STACKWALKER),1)
    LOCAL_CPPFLAGS += -DEMBEDDED_STACKWALKER=1
else
    $(error OOPS, for now you should build with EMBEDDED_STACKWALKER=1)
endif

LOCAL_SRC_FILES += $(APPLE2_MAIN_SRC) $(APPLE2_META_SRC) $(APPLE2_VIDEO_SRC) $(APPLE2_AUDIO_SRC)

# Build a shared library and let Java/Dalvik drive
include $(BUILD_SHARED_LIBRARY)

# --OR-- Build an executable so native can drive this show
#include $(BUILD_EXECUTABLE)

$(call import-module, breakpad/android/google_breakpad)
$(call import-module, android/cpufeatures)
