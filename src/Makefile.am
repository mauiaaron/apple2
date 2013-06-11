noinst_HEADERS = apple2.h debug.h disk.h interface.h keys.h misc.h \
                 video.h cpu.h glue.h gluepro.h prefs.h

SUFFIXES = .l-cpp -80.o

man_MANS = apple2.6

EXTRA_PROGRAMS = apple2 xapple2 xapple2-80col
bin_PROGRAMS = @PROGS@

EXTRA_apple2_SOURCES = debugger.c opcodes.c debug.c joystick.c
apple2_SOURCES = cpu.S memory.S display.S glue.S keys.c prefs.c disk.c \
                 interface.c misc.c font.c svideo.c compact.c cpu-supp.c \
		 vidsup.c

apple2_LDADD = @DEBUGGER_O@ @JOYSTICK_O@ -lvga
apple2_DEPENDENCIES = @DEBUGGER_O@ @JOYSTICK_O@

EXTRA_xapple2_SOURCES = debugger.c opcodes.c debug.c joystick.c

xapple2_SOURCES = cpu.S memory.S display.S glue.S keys.c prefs.c \
                  disk.c interface.c misc.c font.c xvideo.c compact.c \
                  cpu-supp.c vidsup.c

xapple2_LDADD = @DEBUGGER_O@ @JOYSTICK_O@ @X_LIBS@ -lX11 -lXext
xapple2_DEPENDENCIES = @DEBUGGER_O@ @JOYSTICK_O@

EXTRA_xapple2_80col_SOURCES = debugger.c opcodes.c debug.c joystick.c

xapple2_80col_SOURCES = cpu.S memory.S glue.S keys.c prefs.c disk.c \
                    font.c compact.c cpu-supp.c misc.c interface.c
                         
xapple2_80col_LDADD = @DEBUGGER_O@ @JOYSTICK_O@ \
                      vidsup-80.o xvideo-80.o display-80.o \
                      @X_LIBS@ -lX11 -lXext
xapple2_80col_DEPENDENCIES = @DEBUGGER_O@ @JOYSTICK_O@ \
                      vidsup-80.o xvideo-80.o display-80.o

noinst_PROGRAMS = genfont

BUILT_SOURCES = font.c debug.c glue.S

EXTRA_DIST = font.txt apple2.6 debug.l-cpp genglue

CLEANFILES = font.c debug.c glue.S

# Warning: this will trip on a cross-compile
# (not that we're otherwise portable to non-Linux yet...)
font.c : font.txt genfont
	./genfont < $< > $@

glue.S : disk.c misc.c
	$(srcdir)/genglue $^ > $@

%-80.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(DEFS) -D_640x400 -c -o $@ $<

display-80.o: display.S
	$(CC) $(CPPFLAGS) $(DEFS) -D_640x400 -c -o $@ $<

# I couldn't get Automake's internal Lex support to work with this, as
# Automake assumes the program was built to cope with no -P option. These
# files require it and also need preprocessing. 
# 
# Of course this is really our fault for being nonportable to standard Lex...  
# (which I presume has no -P)

%.c: %.l-cpp
	$(CC) -x c -E -P $(CPPFLAGS) $(DEFS) $< | flex -i -P$* -o$@ 


# Another hack. I can't see any other way to get across to automake that some
# built sources should not be distributed. (since they vary with 
# configuration)

dist-hook:
	rm -f $(distdir)/font.c $(distdir)/debug.c $(distdir)/glue.S
