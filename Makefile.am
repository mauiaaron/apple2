
AM_CPPFLAGS = -Isrc
AM_CPPFLAGS += -I../src # work around some bug in make distcheck
AM_LFLAGS = -i
#AM_COLOR_TESTS = no
#LDFLAGS =
#LDADD =

###############################################################################
# No install

noinst_HEADERS = src/apple2.h src/common.h src/cpu.h src/disk.h src/glue.h \
	src/interface.h src/joystick.h src/keys.h src/misc.h src/prefs.h \
	src/timing.h src/uthash.h src/video/video.h src/zlib-helpers.h \
	\
	src/x86/glue-prologue.h \
	src/meta/debug.h \
	\
	src/audio/alhelpers.h src/audio/AY8910.h src/audio/ds-shim.h \
	src/audio/mockingboard.h src/audio/peripherals.h src/audio/soundcore.h \
	src/audio/soundcore-openal.h src/audio/speaker.h \
	src/audio/SSI263Phonemes.h src/audio/win-shim.h

noinst_PROGRAMS = genfont genrom

###############################################################################
# Apple //ix and supporting sources

bin_PROGRAMS = apple2ix

ASM_SRC_x86 = \
	src/x86/glue.S src/x86/cpu.S

INTERFACE_CLASSIC_SRC = \
	src/interface.c

VIDEO_SRC = \
	src/video/xvideo.c

AUDIO_SRC = \
	src/audio/soundcore.c src/audio/soundcore-openal.c src/audio/speaker.c \
	src/audio/win-shim.c src/audio/alhelpers.c src/audio/mockingboard.c \
	src/audio/AY8910.c

META_SRC = \
	src/meta/debug.l src/meta/debugger.c src/meta/opcodes.c

# NOTE : selectively enabled through configuration process ...
EXTRA_apple2ix_SOURCES = \
	$(ASM_SRC_x86) \
	\
	$(INTERFACE_CLASSIC_SRC) \
	\
	$(VIDEO_SRC) \
	\
	$(AUDIO_SRC) \
	\
	$(META_SRC)

apple2ix_SOURCES = src/font.c src/rom.c src/misc.c src/display.c src/vm.c \
	src/timing.c src/zlib-helpers.c src/joystick.c src/keys.c src/prefs.c \
	src/disk.c src/cpu-supp.c

apple2ix_CFLAGS = @AM_CFLAGS@ @X_CFLAGS@
apple2ix_LDFLAGS =
apple2ix_LDADD = @ASM_O@ @INTERFACE_O@ @VIDEO_O@ @AUDIO_O@ @META_O@ @X_LIBS@
apple2ix_DEPENDENCIES = @ASM_O@ @INTERFACE_O@ @VIDEO_O@ @AUDIO_O@ @META_O@

genfont_SOURCES = src/genfont.c

genrom_SOURCES = src/genrom.c

src/font.c: src/font.txt genfont
	./genfont < $< > $@

src/rom.c: src/rom/apple_IIe.rom src/rom/slot6.rom genrom
	./genrom $^ > $@

src/x86/glue.S: src/disk.c src/misc.c src/display.c src/vm.c @AUDIO_GLUE_C@
	./src/x86/genglue $^ > $@

###############################################################################
# Testing

LOG_DRIVER = testcpu  ## hack TODO/FIXME ... should be wrapper shell script accepting standard GNU testing API args ...

A2_TEST_SOURCES = $(apple2ix_SOURCES) src/test/testcommon.c
A2_TEST_CFLAGS = -DTESTING=1 -Isrc/test

TESTS          = testcpu testdisplay testvm
check_PROGRAMS = testcpu testdisplay testvm

testcpu_SOURCES = src/test/testcpu.c $(A2_TEST_SOURCES) $(META_SRC) $(VIDEO_SRC)
testcpu_CFLAGS = $(apple2ix_CFLAGS) $(A2_TEST_CFLAGS) -UAUDIO_ENABLED -UINTERFACE_CLASSIC -DHEADLESS=1
testcpu_LDFLAGS = $(apple2ix_LDFLAGS)
testcpu_LDADD = @ASM_O@
testcpu_DEPENDENCIES = @ASM_O@ @META_O@

EXTRA_testcpu_SOURCES = $(ASM_SRC_x86)

testdisplay_SOURCES = src/test/testdisplay.c $(A2_TEST_SOURCES) $(META_SRC) $(VIDEO_SRC)
# HACK FIXME TODO NOTE: why don't these CFLAGS here pass down to the .S and .c files in the subdirectories?
testdisplay_CFLAGS = $(apple2ix_CFLAGS) $(A2_TEST_CFLAGS) -UAUDIO_ENABLED -UINTERFACE_CLASSIC -DHEADLESS=0
testdisplay_LDFLAGS = $(apple2ix_LDFLAGS)
testdisplay_LDADD = @ASM_O@
testdisplay_DEPENDENCIES = @ASM_O@ @META_O@ @VIDEO_O@

EXTRA_testdisplay_SOURCES = $(ASM_SRC_x86)

testvm_SOURCES = src/test/testvm.c $(A2_TEST_SOURCES) $(META_SRC) $(VIDEO_SRC)
# HACK FIXME TODO NOTE: why don't these CFLAGS here pass down to the .S and .c files in the subdirectories?
testvm_CFLAGS = $(apple2ix_CFLAGS) $(A2_TEST_CFLAGS) -UAUDIO_ENABLED -UINTERFACE_CLASSIC -DHEADLESS=0
testvm_LDFLAGS = $(apple2ix_LDFLAGS)
testvm_LDADD = @ASM_O@
testvm_DEPENDENCIES = @ASM_O@ @META_O@ @VIDEO_O@

EXTRA_testvm_SOURCES = $(ASM_SRC_x86)

###############################################################################
# Misc

man_MANS = docs/apple2ix.6

EXTRA_DIST = reconf.sh configure README.debugger PROBLEMS .apple2 \
	\
	disks/README disks/blank.dsk.gz disks/blank.nib.gz disks/etc.dsk.gz \
	disks/mystery.dsk.gz disks/speedtest.dsk.gz disks/speedtest.txt \
	\
	docs/apple2ix.6 \
	\
	src/font.txt \
	src/x86/genglue

CLEANFILES = src/font.c src/rom.c src/meta/debug.c src/x86/glue.S
