LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

# -----------------------------------------------------------------------------
# Various sources

APPLE2_SRC_PATH := ../../src

APPLE2_X86_SRC := \
    $(APPLE2_SRC_PATH)/x86/glue.S $(APPLE2_SRC_PATH)/x86/cpu.S

APPLE2_ARM_SRC := \
    $(APPLE2_SRC_PATH)/arm/glue.S $(APPLE2_SRC_PATH)/arm/cpu.S

APPLE2_VIDEO_SRC = \
	$(APPLE2_SRC_PATH)/video/glvideo.c \
	$(APPLE2_SRC_PATH)/video/glinput.c \
	$(APPLE2_SRC_PATH)/video_util/matrixUtil.c \
	$(APPLE2_SRC_PATH)/video_util/modelUtil.c \
	$(APPLE2_SRC_PATH)/video_util/sourceUtil.c \
	$(APPLE2_SRC_PATH)/video_util/vectorUtil.c

APPLE2_AUDIO_SRC = \
	$(APPLE2_SRC_PATH)/audio/soundcore.c $(APPLE2_SRC_PATH)/audio/soundcore-openal.c $(APPLE2_SRC_PATH)/audio/speaker.c \
	$(APPLE2_SRC_PATH)/audio/win-shim.c $(APPLE2_SRC_PATH)/audio/alhelpers.c $(APPLE2_SRC_PATH)/audio/mockingboard.c \
	$(APPLE2_SRC_PATH)/audio/AY8910.c

APPLE2_META_SRC = \
	$(APPLE2_SRC_PATH)/meta/debug.c $(APPLE2_SRC_PATH)/meta/debugger.c $(APPLE2_SRC_PATH)/meta/opcodes.c

APPLE2_MAIN_SRC = \
    $(APPLE2_SRC_PATH)/font.c $(APPLE2_SRC_PATH)/rom.c $(APPLE2_SRC_PATH)/misc.c $(APPLE2_SRC_PATH)/display.c $(APPLE2_SRC_PATH)/vm.c \
	$(APPLE2_SRC_PATH)/timing.c $(APPLE2_SRC_PATH)/zlib-helpers.c $(APPLE2_SRC_PATH)/joystick.c $(APPLE2_SRC_PATH)/keys.c \
    $(APPLE2_SRC_PATH)/disk.c $(APPLE2_SRC_PATH)/cpu-supp.c


$(APPLE2_SRC_PATH)/rom.c: genrom
	./genrom src/rom/apple_IIe.rom src/rom/slot6.rom > $@

# -----------------------------------------------------------------------------
# Build flags

APPLE2_BASE_CFLAGS = -std=gnu11 -I$(APPLE2_SRC_PATH)
APPLE2_CFLAGS   := $(APPLE2_BASE_CFLAGS) -DHEADLESS=0 -DAPPLE2IX=1 -DVIDEO_OPENGL=1 -DAUDIO_OPENAL=1 -DDEBUGGER=1 -DHAVE_OPENSSL=0

$(warning TODO FIXME flags settings for tests)
$(warning need to include SHA-processing code)
TESTCPU_CFLAGS  := $(APPLE2_BASE_CFLAGS) -DHEADLESS=1 -DAPPLE2IX=1 -DVIDEO_OPENGL=0 -DAUDIO_OPENAL=0 -DDEBUGGER=1 -DHAVE_OPENSSL=0
TESTVM_CLFAGS   :=
TESTDISK_CLFAGS :=
TESTDISPLAY_CLFAGS :=

# -----------------------------------------------------------------------------
# Android build config

LOCAL_MODULE    := apple2ix
LOCAL_SRC_FILES := jnihooks.c
LOCAL_CFLAGS    := $(APPLE2_CFLAGS)
LOCAL_LDLIBS    := -llog -landroid

# Add assembly files first ... mostly for the benefit of the ARM assembler ...
ifeq ($(TARGET_ARCH_ABI),x86)
    LOCAL_SRC_FILES += $(APPLE2_X86_SRC)
else ifeq($(TARGET_ARCH_ABI),$(filter ($TARGET_ARCH_ABI), foo, baz))
    LOCAL_SRC_FILES += $(APPLE2_ARM_SRC)
else
    $(error building for $(TARGET_ARCH_ABI) device untested...)
endif

LOCAL_SRC_FILES += $(APPLE2_MAIN_SRC) $(APPLE2_META_SRC)

# Build a shared library and let Java/Dalvik drive
include $(BUILD_SHARED_LIBRARY)

# --OR-- Build an executable so native can drive this show
#include $(BUILD_EXECUTABLE)

