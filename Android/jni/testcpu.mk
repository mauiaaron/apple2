LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

PACKAGE_IDENTIFIER := "org.deadc0de.apple2"
PACKAGE_NAME := "apple2ix"
COMMON_SOURCES_MK := $(LOCAL_PATH)/sources.mk
include $(COMMON_SOURCES_MK)

# -----------------------------------------------------------------------------
# Android build config

LOCAL_MODULE    := libapple2ix
LOCAL_SRC_FILES := jnihooks.c $(APPLE2_SRC_PATH)/test/testcommon.c $(APPLE2_SRC_PATH)/test/testcpu.c
LOCAL_CFLAGS    := $(APPLE2_BASE_CFLAGS) -DTEST_CPU -DTESTING=1 -DHEADLESS=1
LOCAL_LDLIBS    := -llog -landroid -lGLESv2 -lz

# Add assembly files first ... mostly for the benefit of the ARM assembler ...
ifeq ($(TARGET_ARCH_ABI),x86)
    LOCAL_SRC_FILES += $(APPLE2_X86_SRC)
else
    LOCAL_SRC_FILES += $(APPLE2_ARM_SRC)
endif

LOCAL_SRC_FILES += $(APPLE2_MAIN_SRC) $(APPLE2_META_SRC) $(APPLE2_VIDEO_SRC)

# Build a shared library and let Java/Dalvik drive
include $(BUILD_SHARED_LIBRARY)

# --OR-- Build an executable so native can drive this show
#include $(BUILD_EXECUTABLE)

