
AM_CPPFLAGS = -Isrc -DCONFIG_DATADIR=\"@datadir@\"
AM_CPPFLAGS += -I../src # work around some bug in make distcheck
AM_LFLAGS = -i -D_GNU_SOURCE=1
#AM_COLOR_TESTS = no
#LDFLAGS =
#LDADD =

###############################################################################
# No install

noinst_HEADERS = src/common.h src/cpu.h src/disk.h src/glue.h src/vm.h \
	src/interface.h src/joystick.h src/keys.h src/misc.h src/prefs.h \
	src/timing.h src/uthash.h src/video/video.h src/zlib-helpers.h \
	\
	src/x86/glue-prologue.h \
	src/meta/debug.h src/meta/trace.h \
	\
	src/audio/alhelpers.h src/audio/AY8910.h src/audio/mockingboard.h \
	src/audio/peripherals.h src/audio/soundcore.h src/audio/speaker.h \
	src/audio/SSI263Phonemes.h

noinst_PROGRAMS = genfont genrom

###############################################################################
# Apple //ix and supporting sources

bin_PROGRAMS = apple2ix

ASM_SRC_x86 = \
	src/x86/glue.S src/x86/cpu.S

VIDEO_SRC = \
	src/video/xvideo.c \
	src/video/glvideo.c \
	src/video/glutinput.c \
	src/video/glnode.c \
	src/video/glhudmodel.c \
	src/video/glalert.c \
	src/video_util/matrixUtil.c \
	src/video_util/modelUtil.c \
	src/video_util/sourceUtil.c \
	src/video_util/vectorUtil.c

AUDIO_SRC = \
	src/audio/soundcore.c src/audio/soundcore-openal.c src/audio/speaker.c \
	src/audio/playqueue.c src/audio/alhelpers.c src/audio/mockingboard.c \
	src/audio/AY8910.c

META_SRC = \
	src/meta/debug.l src/meta/debugger.c src/meta/opcodes.c src/test/sha1.c \
	src/meta/lintrace.c

# NOTE : selectively enabled through configuration process ...
EXTRA_apple2ix_SOURCES = $(ASM_SRC_x86) $(VIDEO_SRC) $(AUDIO_SRC) $(META_SRC)

apple2ix_SOURCES = src/font.c src/rom.c src/misc.c src/display.c src/vm.c \
	src/timing.c src/zlib-helpers.c src/joystick.c src/keys.c src/prefs.c \
	src/interface.c src/disk.c src/cpu-supp.c src/json_parse.c src/memmngt.c \
	externals/jsmn/jsmn.c

apple2ix_CFLAGS = @AM_CFLAGS@ @X_CFLAGS@
apple2ix_CCASFLAGS = $(apple2ix_CFLAGS)
apple2ix_LDFLAGS = -Wl,-z,noexecstack
apple2ix_LDADD = @ASM_O@ @VIDEO_O@ @AUDIO_O@ @META_O@ @X_LIBS@
apple2ix_DEPENDENCIES = @ASM_O@ @VIDEO_O@ @AUDIO_O@ @META_O@

genfont_SOURCES = src/genfont.c

genrom_SOURCES = src/genrom.c

src/font.c: src/font.txt genfont
	./genfont < $< > $@

src/rom.c: genrom
	./genrom src/rom/apple_IIe.rom src/rom/slot6.rom > $@

src/x86/glue.S: src/cpu-supp.c src/disk.c src/display.c src/vm.c @AUDIO_GLUE_C@
	TARGET_ARCH=x86 ./src/genglue.sh $^ > $@

###############################################################################
# Testing

#LOG_DRIVER =  ## hack TODO/FIXME ... should be wrapper shell script accepting standard GNU testing API args ...

A2_TEST_SOURCES = $(apple2ix_SOURCES) src/test/testcommon.c
A2_TEST_CFLAGS  = $(apple2ix_CFLAGS) -DTESTING=1 -Isrc/test

TESTS          = testcpu testdisk testdisplay testprefs testtrace testui testvm
check_PROGRAMS = testcpu testdisk testdisplay testprefs testtrace testui testvm

#######################################

testcpu_SOURCES = src/test/testcpu.c $(A2_TEST_SOURCES)
testcpu_CFLAGS = $(A2_TEST_CFLAGS) -DTEST_CPU=1
testcpu_CCASFLAGS = $(testcpu_CFLAGS)
testcpu_LDFLAGS = $(apple2ix_LDFLAGS)
testcpu_LDADD = @testcpu_ASM_O@ @testcpu_VIDEO_O@ @testcpu_AUDIO_O@ @testcpu_META_O@ @X_LIBS@
testcpu_DEPENDENCIES = @testcpu_ASM_O@ @testcpu_VIDEO_O@ @testcpu_AUDIO_O@ @testcpu_META_O@
EXTRA_testcpu_SOURCES = $(ASM_SRC_x86) $(VIDEO_SRC) $(AUDIO_SRC) $(META_SRC)

#######################################

testdisk_SOURCES = src/test/testdisk.c $(A2_TEST_SOURCES)
testdisk_CFLAGS = $(A2_TEST_CFLAGS) -DTEST_DISK=1 -DDISK_TRACING=1
testdisk_CCASFLAGS = $(testdisk_CFLAGS)
testdisk_LDFLAGS = $(apple2ix_LDFLAGS)
testdisk_LDADD = @testdisk_ASM_O@ @testdisk_VIDEO_O@ @testdisk_AUDIO_O@ @testdisk_META_O@ @X_LIBS@
testdisk_DEPENDENCIES = @testdisk_ASM_O@ @testdisk_VIDEO_O@ @testdisk_AUDIO_O@ @testdisk_META_O@
EXTRA_testdisk_SOURCES = $(ASM_SRC_x86) $(VIDEO_SRC) $(AUDIO_SRC) $(META_SRC)

#######################################

testdisplay_SOURCES = src/test/testdisplay.c $(A2_TEST_SOURCES)
testdisplay_CFLAGS = $(A2_TEST_CFLAGS) -DTEST_DISPLAY=1
testdisplay_CCASFLAGS = $(testdisplay_CFLAGS)
testdisplay_LDFLAGS = $(apple2ix_LDFLAGS)
testdisplay_LDADD = @testdisplay_ASM_O@ @testdisplay_VIDEO_O@ @testdisplay_AUDIO_O@ @testdisplay_META_O@ @X_LIBS@
testdisplay_DEPENDENCIES = @testdisplay_ASM_O@ @testdisplay_VIDEO_O@ @testdisplay_AUDIO_O@ @testdisplay_META_O@
EXTRA_testdisplay_SOURCES = $(ASM_SRC_x86) $(VIDEO_SRC) $(AUDIO_SRC) $(META_SRC)

#######################################

testprefs_SOURCES = src/test/testprefs.c $(A2_TEST_SOURCES)
testprefs_CFLAGS = $(A2_TEST_CFLAGS) -DTEST_PREFS=1
testprefs_CCASFLAGS = $(testprefs_CFLAGS)
testprefs_LDFLAGS = $(apple2ix_LDFLAGS)
testprefs_LDADD = @testprefs_ASM_O@ @testprefs_VIDEO_O@ @testprefs_AUDIO_O@ @testprefs_META_O@ @X_LIBS@
testprefs_DEPENDENCIES = @testprefs_ASM_O@ @testprefs_VIDEO_O@ @testprefs_AUDIO_O@ @testprefs_META_O@
EXTRA_testprefs_SOURCES = $(ASM_SRC_x86) $(VIDEO_SRC) $(AUDIO_SRC) $(META_SRC)

#######################################

testtrace_SOURCES = src/test/testtrace.c $(A2_TEST_SOURCES)
testtrace_CFLAGS = $(A2_TEST_CFLAGS) -DTEST_TRACE=1 -DCPU_TRACING=1 -DDISK_TRACING=1 -DVM_TRACING=1 -DSPEAKER_TRACING=1 -DMB_TRACING=1
testtrace_CCASFLAGS = $(testtrace_CFLAGS)
testtrace_LDFLAGS = $(apple2ix_LDFLAGS)
testtrace_LDADD = @testtrace_ASM_O@ @testtrace_VIDEO_O@ @testtrace_AUDIO_O@ @testtrace_META_O@ @X_LIBS@
testtrace_DEPENDENCIES = @testtrace_ASM_O@ @testtrace_VIDEO_O@ @testtrace_AUDIO_O@ @testtrace_META_O@
EXTRA_testtrace_SOURCES = $(ASM_SRC_x86) $(VIDEO_SRC) $(AUDIO_SRC) $(META_SRC)

#######################################

testui_SOURCES = src/test/testui.c $(A2_TEST_SOURCES)
testui_CFLAGS = $(A2_TEST_CFLAGS) -DTEST_UI=1
testui_CCASFLAGS = $(testui_CFLAGS)
testui_LDFLAGS = $(apple2ix_LDFLAGS)
testui_LDADD = @testui_ASM_O@ @testui_VIDEO_O@ @testui_AUDIO_O@ @testui_META_O@ @X_LIBS@
testui_DEPENDENCIES = @testui_ASM_O@ @testui_VIDEO_O@ @testui_AUDIO_O@ @testui_META_O@
EXTRA_testui_SOURCES = $(ASM_SRC_x86) $(VIDEO_SRC) $(AUDIO_SRC) $(META_SRC)

#######################################

testvm_SOURCES = src/test/testvm.c $(A2_TEST_SOURCES)
testvm_CFLAGS = $(A2_TEST_CFLAGS) -DTEST_VM=1
testvm_CCASFLAGS = $(testvm_CFLAGS)
testvm_LDFLAGS = $(apple2ix_LDFLAGS)
testvm_LDADD = @testvm_ASM_O@ @testvm_VIDEO_O@ @testvm_AUDIO_O@ @testvm_META_O@ @X_LIBS@
testvm_DEPENDENCIES = @testvm_ASM_O@ @testvm_VIDEO_O@ @testvm_AUDIO_O@ @testvm_META_O@
EXTRA_testvm_SOURCES = $(ASM_SRC_x86) $(VIDEO_SRC) $(AUDIO_SRC) $(META_SRC)

###############################################################################
# Misc & Installation

# manpage
man_MANS = docs/apple2ix.6

# shaders
shadersdir = @datadir@/@PACKAGE@/shaders
shaders_DATA = src/video/Basic.vsh src/video/Basic.fsh

# Extra distribution stuff
EXTRA_DIST = reconf.sh configure README.debugger PROBLEMS .apple2 \
	\
	$(man_MANS) \
	\
	src/font.txt \
	src/genglue.sh \
	\
	$(shaders_DATA)

install-data-hook:
	rm -rf @datadir@/@PACKAGE@/disks
	cp -Rp external-disks/ @datadir@/@PACKAGE@/disks/

dist-hook:
	echo "TODO FIXME disks and external-disks"
	echo rm -rf $(distdir)/disks
	echo cp -Rp disks $(distdir)/disks
	echo cp -Rp external-disks $(distdir)/disks

CLEANFILES = src/font.c src/rom.c src/meta/debug.c src/x86/glue.S
