Apple //ix
==========

Apple2ix is an Apple //e (8bit 65c02 CPU) emulator designed to work on various POSIX platforms.

Lineage
-------

Apple2ix derives from the apple2-emul-linux project originally coded by various developers in the mid 1990's and FTP-uploaded as source tarballs to the original `tsx-11.mit.edu` Linux archive.

The original software was designed to work from the Linux console rendering via SVGAlib.  It ran on par to the 1MHz Apple //e on an i386 (Pentium 100 class) or better machine.  Later ports added X11 graphics support based on the original X11 DOOM source code drop, ty JC!

As of June 2016, the resurrected Apple2ix runs on x64 MacOSX 10.9+, x64 Debian GNU/Linux, and x86 and ARM Android devices

Project Goals
-------------

* Portability and code resilience across a wide range of modern POSIXy systems including MacOSX, desktop Linux/BSD, iOS, Android. *If you are on Windows, just use the excellent [AppleWin](https://github.com/AppleWin/AppleWin) emulator!*
* Language/platform/API minimalism for core emulation modules (prefer coding to POSIX APIs and using C99 over all other choices)
* Reasonable emulation fidelity to the original Apple //e machine (timing, video, audio, etc...)
* Good platform citizenship for each port (prefer coding in language-of-choice promoted by platform--e.g.: Objective-C/Swift on Darwin, Java on Android, ...)

Project Tech
------------

* C99 dialect of the C programming language for the majority of the project
* x86 and ARM assembly language for 65c02 CPU emulation loop
* Extensive tests for 65c02 CPU, Apple //e VM, disks, and display (expected framebuffer output)
* OpenGLES 2.x graphics with simple portable GLSL shaders
* OpenAL and OpenSLES audio (emulated speaker and emulated Mockingboard/Phasor soundcards)
* Objective-C and Cocoa APIs (Mac/iOS variant)
* Java and Android APIs (Android app)

Android
-------

[Available on Google Play](https://play.google.com/store/apps/details?id=org.deadc0de.apple2ix.basic).

Running at 30FPS on ancient Gingerbread (Android 2.3.3) devices:
![Apple2ix on Samsung Galaxy Y running Gingerbread](https://raw.github.com/mauiaaron/apple2/develop/docs/android-galaxyY.png "Apple //ix")

Running at 60FPS on modern Android devices:
![Apple2ix on Nexus 6](https://raw.github.com/mauiaaron/apple2/develop/docs/android-nexus6.png "Apple //ix")

Mac Package
-----------

![Apple2Mac](https://raw.github.com/mauiaaron/apple2/master/docs/Apple2Mac.png "Apple2Mac")

A dated binary package for Macintosh is available at [deadc0de.org](https://deadc0de.org/Apple2Mac/Apple2Mac-0.9.dmg)
Size : 10240000 (10MB)
SHASUM : 81f2d55c2daaa0d3f9b33af9b50f69f6789738bf

Alt Size : 76820480 (75MB)
ALTSUM : 488a40d7f1187bcfd16d0045258f606a95f448cb

Due to Apple's policy about emulators we are unlikely to ship this in the App Store any time soon.

iOS port in progress 2016, check this repo and fork(s) too!

Linux/POSIX
-----------

For Linux and *BSD, I do not personally relish being a package/port maintainer, so you should `./configure --prefix=...`, `make`, `make install` like it's 1999 ;-)

You will need GCC or Clang compiler and other tools as determined by the `configure` script.  Use the source!

![Apple //ix](https://raw.github.com/mauiaaron/apple2/master/docs/Apple2ix.png "Apple //ix")

Semi-Ordered TODO
-----------------

* Mockingboard is buggy.  Need to research/check upstream (AppleWin and beyond) for bugfixes and refactor.
* Double-LORES graphics (used in Dagen Brock's Flappy Bird clone) are ugly/incorrect ... fix 'em
* Double-HIRES graphics are also ugly/icorrect ... fix 'em
* Improved VBL timing and vSync matching to the device/system refresh rate
* CPU module ports: aarch64, Clang IR (bitcode)
* iOS/iWatch/TVos ports.  (iOS in progress 2016)
* Android TV and Android wear
* OpenGL shaders/tricks for style (emulating of various NTSC screen artifacts)
* Emulator save/restore image compatibility with AppleWin
* Emulation features ... (3.5" disk, AppleHD, Phasor, printer, ethernet, ...)
* Debugger rewrite with tests ... improved debugger routines (CLI/curses debugger? GDB/LLDB module?)
* Port to web ... emscripten/asmjs/web assembly

![DOS 3.3](https://raw.github.com/mauiaaron/apple2/master/docs/DOS33.png "DOS 3.3 Applesoft BASIC and //e monitor")
