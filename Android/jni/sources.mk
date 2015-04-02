# -----------------------------------------------------------------------------
# Common emulator sources and build settings

APPLE2_SRC_PATH := apple2ix-src

APPLE2_X86_SRC := \
    $(APPLE2_SRC_PATH)/x86/glue.S $(APPLE2_SRC_PATH)/x86/cpu.S

APPLE2_ARM_SRC := \
    $(APPLE2_SRC_PATH)/arm/glue.S $(APPLE2_SRC_PATH)/arm/cpu.S

APPLE2_VIDEO_SRC = \
    $(APPLE2_SRC_PATH)/video/glvideo.c \
    $(APPLE2_SRC_PATH)/video/glanimation.c \
    $(APPLE2_SRC_PATH)/video/glcpuanim.c \
    $(APPLE2_SRC_PATH)/video/gltouchjoy.c \
    $(APPLE2_SRC_PATH)/video_util/matrixUtil.c \
    $(APPLE2_SRC_PATH)/video_util/modelUtil.c \
    $(APPLE2_SRC_PATH)/video_util/sourceUtil.c \
    $(APPLE2_SRC_PATH)/video_util/vectorUtil.c

APPLE2_AUDIO_SRC = \
    $(APPLE2_SRC_PATH)/audio/soundcore.c $(APPLE2_SRC_PATH)/audio/soundcore-openal.c $(APPLE2_SRC_PATH)/audio/speaker.c \
    $(APPLE2_SRC_PATH)/audio/win-shim.c $(APPLE2_SRC_PATH)/audio/alhelpers.c $(APPLE2_SRC_PATH)/audio/mockingboard.c \
    $(APPLE2_SRC_PATH)/audio/AY8910.c

APPLE2_META_SRC = \
    $(APPLE2_SRC_PATH)/meta/debug.c $(APPLE2_SRC_PATH)/meta/debugger.c $(APPLE2_SRC_PATH)/meta/opcodes.c \
    $(APPLE2_SRC_PATH)/test/sha1.c

APPLE2_MAIN_SRC = \
    $(APPLE2_SRC_PATH)/font.c $(APPLE2_SRC_PATH)/rom.c $(APPLE2_SRC_PATH)/misc.c $(APPLE2_SRC_PATH)/display.c $(APPLE2_SRC_PATH)/vm.c \
    $(APPLE2_SRC_PATH)/timing.c $(APPLE2_SRC_PATH)/zlib-helpers.c $(APPLE2_SRC_PATH)/joystick.c $(APPLE2_SRC_PATH)/keys.c \
    $(APPLE2_SRC_PATH)/disk.c $(APPLE2_SRC_PATH)/cpu-supp.c

APPLE2_BASE_CFLAGS := -DAPPLE2IX=1 -DTOUCH_JOYSTICK=1 -DMOBILE_DEVICE=1 -DVIDEO_OPENGL=1 -DDEBUGGER=1 -std=gnu11 -I$(APPLE2_SRC_PATH)

