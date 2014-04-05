
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
	src/asm386/glue-prologue.h \
	src/meta/debug.h \
	\
	src/audio/alhelpers.h src/audio/AY8910.h src/audio/ds-shim.h \
	src/audio/mockingboard.h src/audio/peripherals.h src/audio/soundcore.h \
	src/audio/soundcore-openal.h src/audio/speaker.h \
	src/audio/SSI263Phonemes.h src/audio/win-shim.h

noinst_PROGRAMS = genfont

###############################################################################
# Apple //ix and supporting sources

bin_PROGRAMS = apple2ix

VM_SRC = \
	src/asm386/glue.S src/asm386/cpu.S src/asm386/display.S \
	src/asm386/memory.S

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
	$(VM_SRC) \
	\
	$(INTERFACE_CLASSIC_SRC) \
	\
	$(VIDEO_SRC) \
	\
	$(AUDIO_SRC) \
	\
	$(META_SRC)

apple2ix_SOURCES = src/font.c src/misc.c src/display.c \
	src/timing.c src/zlib-helpers.c src/joystick.c src/keys.c src/prefs.c \
	src/disk.c src/cpu-supp.c

apple2ix_CFLAGS = @AM_CFLAGS@ @X_CFLAGS@
apple2ix_LDFLAGS = @ARCHOS_HACK_LDFLAGS@
apple2ix_LDADD = @VM_O@ @INTERFACE_O@ @VIDEO_O@ @AUDIO_O@ @META_O@ @X_LIBS@
apple2ix_DEPENDENCIES = @VM_O@ @INTERFACE_O@ @VIDEO_O@ @AUDIO_O@ @META_O@

genfont_SOURCES = src/genfont.c

src/font.c: src/font.txt genfont
	./genfont < $< > $@

src/asm386/glue.S: src/disk.c src/misc.c src/display.c @AUDIO_GLUE_C@
	./src/asm386/genglue $^ > $@

###############################################################################
# Testing

LOG_DRIVER = testcpu  ## hack TODO/FIXME ... should be wrapper shell script accepting standard GNU testing API args ...

A2_TEST_SOURCES = $(apple2ix_SOURCES)
A2_TEST_CFLAGS = -DTESTING=1 -Isrc/greatest

TESTS          = testcpu testvm
check_PROGRAMS = testcpu testvm

testcpu_SOURCES = src/test/testcpu.c $(A2_TEST_SOURCES) $(EXTRA_apple2ix_SOURCES)
testcpu_CFLAGS = $(apple2ix_CFLAGS) $(A2_TEST_CFLAGS)
testcpu_LDFLAGS = $(apple2ix_LDFLAGS)
testcpu_DEPENDENCIES = $(apple2ix_DEPENDENCIES)

testvm_SOURCES = src/test/testvm.c $(A2_TEST_SOURCES) $(VM_SRC) $(META_SRC)
# HACK FIXME TODO NOTE: why don't these CFLAGS here pass down to the .S and .c files in the subdirectories?
testvm_CFLAGS = $(apple2ix_CFLAGS) $(A2_TEST_CFLAGS) -UAUDIO_ENABLED -UVIDEO_X11 -UINTERFACE_CLASSIC
testvm_LDFLAGS = $(apple2ix_LDFLAGS)
testvm_DEPENDENCIES = @VM_O@ @META_O@

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
	src/asm386/genglue

CLEANFILES = src/font.c src/meta/debug.c src/asm386/glue.S
