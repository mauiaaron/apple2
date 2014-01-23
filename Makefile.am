
AM_CPPFLAGS = -Isrc
AM_CPPFLAGS += -I../src # work around some bug in make distcheck
#LDFLAGS =
#LDADD =

man_MANS = docs/apple2ix.6

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

genfont_SOURCES = src/genfont.c

bin_PROGRAMS = apple2ix


# selectively enabled through configuration process ...
EXTRA_apple2ix_SOURCES = \
	src/asm386/glue.S src/asm386/cpu.S src/asm386/display.S src/asm386/memory.S \
	\
	src/meta/debug.l src/meta/debugger.c src/meta/opcodes.c \
	\
	src/video/xvideo.c \
	\
	src/audio/soundcore.c src/audio/soundcore-openal.c src/audio/speaker.c \
	src/audio/win-shim.c src/audio/alhelpers.c src/audio/mockingboard.c \
	src/audio/AY8910.c

apple2ix_SOURCES = src/font.c src/misc.c src/vidsup.c src/interface.c \
	src/timing.c src/zlib-helpers.c src/joystick.c src/keys.c src/prefs.c \
	src/disk.c src/cpu-supp.c

apple2ix_CFLAGS = @AM_CFLAGS@ @X_CFLAGS@
apple2ix_LDFLAGS = @ARCHOS_HACK_LDFLAGS@
apple2ix_LDADD = @ASM_O@ @VIDEO_O@ @AUDIO_O@ @META_O@ @X_LIBS@
apple2ix_DEPENDENCIES = @ASM_O@ @VIDEO_O@ @AUDIO_O@ @META_O@

src/font.c: src/font.txt genfont
	./genfont < $< > $@

src/asm386/glue.S: src/disk.c src/misc.c @AUDIO_GLUE_C@
	./src/asm386/genglue $^ > $@

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
