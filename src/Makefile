# Generated automatically from Makefile.in by configure.
# Makefile.in generated automatically by automake 1.4 from Makefile.am

# Copyright (C) 1994, 1995-8, 1999 Free Software Foundation, Inc.
# This Makefile.in is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY, to the extent permitted by law; without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE.


SHELL = /bin/sh

srcdir = .
top_srcdir = ..
prefix = /tmp
exec_prefix = ${prefix}

bindir = ${exec_prefix}/bin
sbindir = ${exec_prefix}/sbin
libexecdir = ${exec_prefix}/libexec
datadir = ${prefix}/share
sysconfdir = ${prefix}/etc
sharedstatedir = ${prefix}/com
localstatedir = ${prefix}/var
libdir = ${exec_prefix}/lib
infodir = ${prefix}/info
mandir = ${prefix}/man
includedir = ${prefix}/include
oldincludedir = /usr/include

DESTDIR =

pkgdatadir = $(datadir)/apple2-emul
pkglibdir = $(libdir)/apple2-emul
pkgincludedir = $(includedir)/apple2-emul

top_builddir = ..

ACLOCAL = aclocal
AUTOCONF = autoconf
AUTOMAKE = automake
AUTOHEADER = autoheader

INSTALL = /usr/bin/install -c
INSTALL_PROGRAM = ${INSTALL} $(AM_INSTALL_PROGRAM_FLAGS)
INSTALL_DATA = ${INSTALL} -m 644
INSTALL_SCRIPT = ${INSTALL_PROGRAM}
transform = s,x,x,

NORMAL_INSTALL = :
PRE_INSTALL = :
POST_INSTALL = :
NORMAL_UNINSTALL = :
PRE_UNINSTALL = :
POST_UNINSTALL = :
CC = gcc
DEBUGGER_O = 
JOYSTICK_O = joystick.o
PACKAGE = apple2-emul
PROGS = xapple2 xapple2-80col
VERSION = 0.7.4

noinst_HEADERS = apple2.h debug.h disk.h interface.h keys.h misc.h                  video.h cpu.h glue.h gluepro.h prefs.h


SUFFIXES = .l -80.o

man_MANS = apple2.6

EXTRA_PROGRAMS = apple2 xapple2 xapple2-80col
bin_PROGRAMS = xapple2 xapple2-80col

EXTRA_apple2_SOURCES = debugger.c opcodes.c debug.c joystick.c
apple2_SOURCES = cpu.S memory.S display.S glue.S keys.c prefs.c disk.c                  interface.c misc.c font.c svideo.c compact.c cpu-supp.c 		 vidsup.c


apple2_LDADD =  joystick.o -lvga
apple2_DEPENDENCIES =  joystick.o

EXTRA_xapple2_SOURCES = debugger.c opcodes.c debug.c joystick.c

xapple2_SOURCES = cpu.S memory.S display.S glue.S keys.c prefs.c                   disk.c interface.c misc.c font.c xvideo.c compact.c                   cpu-supp.c vidsup.c


xapple2_LDADD =  joystick.o  -L/usr/lib/i386-linux-gnu -lX11 -lXext
xapple2_DEPENDENCIES =  joystick.o

EXTRA_xapple2_80col_SOURCES = debugger.c opcodes.c debug.c joystick.c

xapple2_80col_SOURCES = cpu.S memory.S glue.S keys.c prefs.c disk.c                     font.c compact.c cpu-supp.c misc.c interface.c


xapple2_80col_LDADD =  joystick.o                       vidsup-80.o xvideo-80.o display-80.o                        -L/usr/lib/i386-linux-gnu -lX11 -lXext

xapple2_80col_DEPENDENCIES =  joystick.o                       vidsup-80.o xvideo-80.o display-80.o


noinst_PROGRAMS = genfont

BUILT_SOURCES = font.c debug.c glue.S

EXTRA_DIST = font.txt apple2.6 debug.l genglue

CLEANFILES = font.c glue.S
mkinstalldirs = $(SHELL) $(top_srcdir)/mkinstalldirs
CONFIG_CLEAN_FILES = 
PROGRAMS =  $(bin_PROGRAMS) $(noinst_PROGRAMS)


DEFS =  -DPACKAGE=\"apple2-emul\" -DVERSION=\"0.7.4\" -DPC_JOYSTICK=1 -DHAVE_UNISTD_H=1 -DHAVE_GETPAGESIZE=1 -DHAVE_MMAP=1 -DNO_UNDERSCORES=1 -DDEBUGGER=1 -DAPPLE_IIE=1 -DMAX_APPLE_DELAY=1000  -I. -I$(srcdir) 
CPPFLAGS = 
LDFLAGS = 
LIBS = 
X_CFLAGS = 
X_LIBS = 
X_EXTRA_LIBS = 
X_PRE_LIBS =  -lSM -lICE
apple2_OBJECTS =  cpu.o memory.o display.o glue.o keys.o prefs.o disk.o     debugger.o opcodes.o debug.o      interface.o misc.o font.o svideo.o compact.o cpu-supp.o vidsup.o
apple2_LDFLAGS = 
xapple2_OBJECTS =  cpu.o memory.o display.o glue.o keys.o prefs.o disk.o     debugger.o opcodes.o debug.o       interface.o misc.o font.o xvideo.o compact.o cpu-supp.o vidsup.o
xapple2_LDFLAGS = 
xapple2_80col_OBJECTS =  cpu.o memory.o glue.o keys.o prefs.o disk.o      debugger.o opcodes.o debug.o       font.o compact.o cpu-supp.o misc.o interface.o
xapple2_80col_LDFLAGS = 
genfont_SOURCES = genfont.c
genfont_OBJECTS =  genfont.o
genfont_LDADD = $(LDADD)
genfont_DEPENDENCIES = 
genfont_LDFLAGS = 
CFLAGS = -std=gnu11 -Wall -Werror -g -O0 -m32 -Xassembler --32
COMPILE = $(CC) $(DEFS) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS)
CCLD = $(CC)
LINK = $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@
man6dir = $(mandir)/man6
MANS = $(man_MANS)

NROFF = nroff
HEADERS =  $(noinst_HEADERS)

DIST_COMMON = 


DISTFILES = $(DIST_COMMON) $(SOURCES) $(HEADERS) $(TEXINFOS) $(EXTRA_DIST)

TAR = tar
GZIP_ENV = --best
SOURCES = $(apple2_SOURCES) $(EXTRA_apple2_SOURCES) $(xapple2_SOURCES) $(EXTRA_xapple2_SOURCES) $(xapple2_80col_SOURCES) $(EXTRA_xapple2_80col_SOURCES) genfont.c
OBJECTS = $(apple2_OBJECTS) $(xapple2_OBJECTS) $(xapple2_80col_OBJECTS) genfont.o

all: all-redirect
.SUFFIXES:
.SUFFIXES: -80.o .S .c .l .o .s



mostlyclean-binPROGRAMS:

clean-binPROGRAMS:
	-test -z "$(bin_PROGRAMS)" || rm -f $(bin_PROGRAMS)

distclean-binPROGRAMS:

maintainer-clean-binPROGRAMS:

install-binPROGRAMS: $(bin_PROGRAMS)
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(DESTDIR)$(bindir)
	@list='$(bin_PROGRAMS)'; for p in $$list; do \
	  if test -f $$p; then \
	    echo "  $(INSTALL_PROGRAM) $$p $(DESTDIR)$(bindir)/`echo $$p|sed 's/$(EXEEXT)$$//'|sed '$(transform)'|sed 's/$$/$(EXEEXT)/'`"; \
	     $(INSTALL_PROGRAM) $$p $(DESTDIR)$(bindir)/`echo $$p|sed 's/$(EXEEXT)$$//'|sed '$(transform)'|sed 's/$$/$(EXEEXT)/'`; \
	  else :; fi; \
	done

uninstall-binPROGRAMS:
	@$(NORMAL_UNINSTALL)
	list='$(bin_PROGRAMS)'; for p in $$list; do \
	  rm -f $(DESTDIR)$(bindir)/`echo $$p|sed 's/$(EXEEXT)$$//'|sed '$(transform)'|sed 's/$$/$(EXEEXT)/'`; \
	done

mostlyclean-noinstPROGRAMS:

clean-noinstPROGRAMS:
	-test -z "$(noinst_PROGRAMS)" || rm -f $(noinst_PROGRAMS)

distclean-noinstPROGRAMS:

maintainer-clean-noinstPROGRAMS:

.c.o:
	$(COMPILE) -c $<

.s.o:
	$(COMPILE) -c $<

.S.o:
	$(COMPILE) -c $<

mostlyclean-compile:
	-rm -f *.o core *.core

clean-compile:

distclean-compile:
	-rm -f *.tab.c

maintainer-clean-compile:

apple2: $(apple2_OBJECTS) $(apple2_DEPENDENCIES)
	@rm -f apple2
	$(LINK) $(apple2_LDFLAGS) $(apple2_OBJECTS) $(apple2_LDADD) $(LIBS)

xapple2: $(xapple2_OBJECTS) $(xapple2_DEPENDENCIES)
	@rm -f xapple2
	$(LINK) $(xapple2_LDFLAGS) $(xapple2_OBJECTS) $(xapple2_LDADD) $(LIBS)

xapple2-80col: $(xapple2_80col_OBJECTS) $(xapple2_80col_DEPENDENCIES)
	@rm -f xapple2-80col
	$(LINK) $(xapple2_80col_LDFLAGS) $(xapple2_80col_OBJECTS) $(xapple2_80col_LDADD) $(LIBS)

genfont: $(genfont_OBJECTS) $(genfont_DEPENDENCIES)
	@rm -f genfont
	$(LINK) $(genfont_LDFLAGS) $(genfont_OBJECTS) $(genfont_LDADD) $(LIBS)

install-man6:
	$(mkinstalldirs) $(DESTDIR)$(man6dir)
	@list='$(man6_MANS)'; \
	l2='$(man_MANS)'; for i in $$l2; do \
	  case "$$i" in \
	    *.6*) list="$$list $$i" ;; \
	  esac; \
	done; \
	for i in $$list; do \
	  if test -f $(srcdir)/$$i; then file=$(srcdir)/$$i; \
	  else file=$$i; fi; \
	  ext=`echo $$i | sed -e 's/^.*\\.//'`; \
	  inst=`echo $$i | sed -e 's/\\.[0-9a-z]*$$//'`; \
	  inst=`echo $$inst | sed '$(transform)'`.$$ext; \
	  echo " $(INSTALL_DATA) $$file $(DESTDIR)$(man6dir)/$$inst"; \
	  $(INSTALL_DATA) $$file $(DESTDIR)$(man6dir)/$$inst; \
	done

uninstall-man6:
	@list='$(man6_MANS)'; \
	l2='$(man_MANS)'; for i in $$l2; do \
	  case "$$i" in \
	    *.6*) list="$$list $$i" ;; \
	  esac; \
	done; \
	for i in $$list; do \
	  ext=`echo $$i | sed -e 's/^.*\\.//'`; \
	  inst=`echo $$i | sed -e 's/\\.[0-9a-z]*$$//'`; \
	  inst=`echo $$inst | sed '$(transform)'`.$$ext; \
	  echo " rm -f $(DESTDIR)$(man6dir)/$$inst"; \
	  rm -f $(DESTDIR)$(man6dir)/$$inst; \
	done
install-man: $(MANS)
	@$(NORMAL_INSTALL)
	$(MAKE) $(AM_MAKEFLAGS) install-man6
uninstall-man:
	@$(NORMAL_UNINSTALL)
	$(MAKE) $(AM_MAKEFLAGS) uninstall-man6

tags: TAGS

ID: $(HEADERS) $(SOURCES) $(LISP)
	list='$(SOURCES) $(HEADERS)'; \
	unique=`for i in $$list; do echo $$i; done | \
	  awk '    { files[$$0] = 1; } \
	       END { for (i in files) print i; }'`; \
	here=`pwd` && cd $(srcdir) \
	  && mkid -f$$here/ID $$unique $(LISP)

TAGS:  $(HEADERS) $(SOURCES)  $(TAGS_DEPENDENCIES) $(LISP)
	tags=; \
	here=`pwd`; \
	list='$(SOURCES) $(HEADERS)'; \
	unique=`for i in $$list; do echo $$i; done | \
	  awk '    { files[$$0] = 1; } \
	       END { for (i in files) print i; }'`; \
	test -z "$(ETAGS_ARGS)$$unique$(LISP)$$tags" \
	  || (cd $(srcdir) && etags $(ETAGS_ARGS) $$tags  $$unique $(LISP) -o $$here/TAGS)

mostlyclean-tags:

clean-tags:

distclean-tags:
	-rm -f TAGS ID

maintainer-clean-tags:

distdir = $(top_builddir)/$(PACKAGE)-$(VERSION)/$(subdir)

subdir = src

distdir: $(DISTFILES)
	@for file in $(DISTFILES); do \
	  d=$(srcdir); \
	  if test -d $$d/$$file; then \
	    cp -pr $$/$$file $(distdir)/$$file; \
	  else \
	    test -f $(distdir)/$$file \
	    || ln $$d/$$file $(distdir)/$$file 2> /dev/null \
	    || cp -p $$d/$$file $(distdir)/$$file || :; \
	  fi; \
	done
	$(MAKE) $(AM_MAKEFLAGS) top_distdir="$(top_distdir)" distdir="$(distdir)" dist-hook
info-am:
info: info-am
dvi-am:
dvi: dvi-am
check-am: all-am
check: check-am
installcheck-am:
installcheck: installcheck-am
install-exec-am: install-binPROGRAMS
install-exec: install-exec-am

install-data-am: install-man
install-data: install-data-am

install-am: all-am
	@$(MAKE) $(AM_MAKEFLAGS) install-exec-am install-data-am
install: install-am
uninstall-am: uninstall-binPROGRAMS uninstall-man
uninstall: uninstall-am
all-am: Makefile $(PROGRAMS) $(MANS) $(HEADERS)
all-redirect: all-am
install-strip:
	$(MAKE) $(AM_MAKEFLAGS) AM_INSTALL_PROGRAM_FLAGS=-s install
installdirs:
	$(mkinstalldirs)  $(DESTDIR)$(bindir) $(DESTDIR)$(mandir)/man6


mostlyclean-generic:

clean-generic:
	-test -z "$(CLEANFILES)" || rm -f $(CLEANFILES)

distclean-generic:
	-rm -f Makefile $(CONFIG_CLEAN_FILES)
	-rm -f config.cache config.log stamp-h stamp-h[0-9]*

maintainer-clean-generic:
	-test -z "$(BUILT_SOURCES)" || rm -f $(BUILT_SOURCES)
mostlyclean-am:  mostlyclean-binPROGRAMS mostlyclean-noinstPROGRAMS \
		mostlyclean-compile mostlyclean-tags \
		mostlyclean-generic

mostlyclean: mostlyclean-am

clean-am:  clean-binPROGRAMS clean-noinstPROGRAMS clean-compile \
		clean-tags clean-generic mostlyclean-am

clean: clean-am

distclean-am:  distclean-binPROGRAMS distclean-noinstPROGRAMS \
		distclean-compile distclean-tags distclean-generic \
		clean-am

distclean: distclean-am

maintainer-clean-am:  maintainer-clean-binPROGRAMS \
		maintainer-clean-noinstPROGRAMS \
		maintainer-clean-compile maintainer-clean-tags \
		maintainer-clean-generic distclean-am
	@echo "This command is intended for maintainers to use;"
	@echo "it deletes files that may require special tools to rebuild."

maintainer-clean: maintainer-clean-am

.PHONY: mostlyclean-binPROGRAMS distclean-binPROGRAMS clean-binPROGRAMS \
maintainer-clean-binPROGRAMS uninstall-binPROGRAMS install-binPROGRAMS \
mostlyclean-noinstPROGRAMS distclean-noinstPROGRAMS \
clean-noinstPROGRAMS maintainer-clean-noinstPROGRAMS \
mostlyclean-compile distclean-compile clean-compile \
maintainer-clean-compile install-man6 uninstall-man6 install-man \
uninstall-man tags mostlyclean-tags distclean-tags clean-tags \
maintainer-clean-tags distdir info-am info dvi-am dvi check check-am \
installcheck-am installcheck install-exec-am install-exec \
install-data-am install-data install-am install uninstall-am uninstall \
all-redirect all-am all installdirs mostlyclean-generic \
distclean-generic clean-generic maintainer-clean-generic clean \
mostlyclean distclean maintainer-clean


# Warning: this will trip on a cross-compile
# (not that we're otherwise portable to non-Linux yet...)
font.c : font.txt genfont
	./genfont < $< > $@

glue.S : disk.c misc.c
	$(srcdir)/genglue $^ > $@

%-80.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(DEFS) -D_640x400 -c -o $@ $<

display-80.o: display.S
	$(CC) $(CFLAGS) $(CPPFLAGS) $(DEFS) -D_640x400 -c -o $@ $<

# I couldn't get Automake's internal Lex support to work with this, as
# Automake assumes the program was built to cope with no -P option. These
# files require it and also need preprocessing. 
# 
# Of course this is really our fault for being nonportable to standard Lex...  
# (which I presume has no -P)

#%.c: %.l-cpp
#	$(CC) -x c -E -P $(CPPFLAGS) $(DEFS) $< | flex -i -P$* -o$@ 
%.c: %.l
	flex -i -P$* -o$@ $<

# Another hack. I can't see any other way to get across to automake that some
# built sources should not be distributed. (since they vary with 
# configuration)

dist-hook:
	rm -f $(distdir)/font.c $(distdir)/glue.S

# Tell versions [3.59,3.63) of GNU make to not export all variables.
# Otherwise a system limit (for SysV at least) may be exceeded.
.NOEXPORT:
