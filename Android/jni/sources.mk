# -----------------------------------------------------------------------------
# Common emulator sources and build settings

APPLE2_SRC_PATH := apple2ix-src

APPLE2_X86_SRC := \
    $(APPLE2_SRC_PATH)/x86/glue.S $(APPLE2_SRC_PATH)/x86/cpu.S

APPLE2_ARM_SRC := \
    $(APPLE2_SRC_PATH)/arm/glue.S $(APPLE2_SRC_PATH)/arm/cpu.S

APPLE2_VIDEO_SRC = \
    $(APPLE2_SRC_PATH)/video/glvideo.c \
    $(APPLE2_SRC_PATH)/video/glnode.c \
    $(APPLE2_SRC_PATH)/video/glhudmodel.c \
    $(APPLE2_SRC_PATH)/video/glalert.c \
    $(APPLE2_SRC_PATH)/video/gltouchjoy.c \
    $(APPLE2_SRC_PATH)/video/gltouchjoy_joy.c \
    $(APPLE2_SRC_PATH)/video/gltouchjoy_kpad.c \
    $(APPLE2_SRC_PATH)/video/gltouchkbd.c \
    $(APPLE2_SRC_PATH)/video/gltouchmenu.c \
    $(APPLE2_SRC_PATH)/video_util/matrixUtil.c \
    $(APPLE2_SRC_PATH)/video_util/modelUtil.c \
    $(APPLE2_SRC_PATH)/video_util/sourceUtil.c \
    $(APPLE2_SRC_PATH)/video_util/vectorUtil.c

APPLE2_AUDIO_SRC = \
    $(APPLE2_SRC_PATH)/audio/soundcore.c $(APPLE2_SRC_PATH)/audio/soundcore-opensles.c $(APPLE2_SRC_PATH)/audio/speaker.c \
    $(APPLE2_SRC_PATH)/audio/mockingboard.c $(APPLE2_SRC_PATH)/audio/AY8910.c

APPLE2_META_SRC = \
    $(APPLE2_SRC_PATH)/meta/debug.c $(APPLE2_SRC_PATH)/meta/debugger.c $(APPLE2_SRC_PATH)/meta/opcodes.c \
    $(APPLE2_SRC_PATH)/meta/lintrace.c $(APPLE2_SRC_PATH)/test/sha1.c $(APPLE2_SRC_PATH)/json_parse.c \
    $(APPLE2_SRC_PATH)/memmngt.c $(APPLE2_SRC_PATH)/../externals/jsmn/jsmn.c

APPLE2_MAIN_SRC = \
    $(APPLE2_SRC_PATH)/font.c $(APPLE2_SRC_PATH)/rom.c $(APPLE2_SRC_PATH)/misc.c $(APPLE2_SRC_PATH)/display.c $(APPLE2_SRC_PATH)/vm.c \
    $(APPLE2_SRC_PATH)/timing.c $(APPLE2_SRC_PATH)/zlib-helpers.c $(APPLE2_SRC_PATH)/joystick.c $(APPLE2_SRC_PATH)/keys.c \
    $(APPLE2_SRC_PATH)/interface.c $(APPLE2_SRC_PATH)/disk.c $(APPLE2_SRC_PATH)/cpu-supp.c $(APPLE2_SRC_PATH)/prefs.c \
    jnihooks.c androidkeys.c

APPLE2_OPTIM_CFLAGS := -Os
APPLE2_BASE_CFLAGS := -DAPPLE2IX=1 -DINTERFACE_TOUCH=1 -DMOBILE_DEVICE=1 -DVIDEO_OPENGL=1 -std=gnu11 -fPIC $(APPLE2_OPTIM_CFLAGS) -I$(APPLE2_SRC_PATH)
APPLE2_BASE_LDLIBS := -Wl,-z,text -Wl,-z,noexecstack -llog -landroid -lGLESv2 -lz -lOpenSLES -latomic

LOCAL_WHOLE_STATIC_LIBRARIES += cpufeatures

