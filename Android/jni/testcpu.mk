LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

PACKAGE_IDENTIFIER := "org.deadc0de.apple2"
PACKAGE_NAME := "apple2ix"
COMMON_SOURCES_MK := sources.mk
include $(COMMON_SOURCES_MK)

# -----------------------------------------------------------------------------
# Android build config

LOCAL_MODULE    := apple2ix
LOCAL_SRC_FILES := $(APPLE2_SRC_PATH)/test/testcommon.c $(APPLE2_SRC_PATH)/test/testcpu.c
LOCAL_CFLAGS    := $(APPLE2_BASE_CFLAGS) -DHEADLESS=1 -DDEBUGGER=1 -DHAVE_OPENSSL=0
LOCAL_LDLIBS    := -llog -landroid -lGLESv2 -lz

# Add assembly files first ... mostly for the benefit of the ARM assembler ...
ifeq ($(TARGET_ARCH_ABI),x86)
    LOCAL_SRC_FILES += $(APPLE2_X86_SRC)
else
    LOCAL_SRC_FILES += $(APPLE2_ARM_SRC)
endif

LOCAL_SRC_FILES += $(APPLE2_MAIN_SRC) $(APPLE2_META_SRC)

# Build a shared library and let Java/Dalvik drive
#include $(BUILD_SHARED_LIBRARY)

# --OR-- Build an executable so native can drive this show
include $(BUILD_EXECUTABLE)

