Apple //ix
==========

Apple2ix is an Apple //e emulator implemented primarily in C with x86, x86-64, and ARM assembly language for the 65c02 CPU emulation module.  In addition you will find a smattering of interface code in Objective-C/Cocoa, Java/Android, and GLSL.

Lineage
-------

Apple2ix derives from the apple2-emul-linux project originally coded by various developers in the mid 1990's and FTP-uploaded as source tarballs to the original `tsx-11.mit.edu` Linux archive.

The original software was designed to work from the Linux console rendering via SVGAlib.  It ran on par to the 1MHz Apple //e on an i386 (Pentium 100 class) or better machine.  Later ports added X11 graphics support based on the original X11 DOOM source code drop, ty JC!

As of November 2015, the resurrected Apple2ix runs on x64 MacOSX 10.9+, x64 Debian GNU/Linux, and ARM Android

Project Goals
-------------

* Portability and code resilience across a wide range of modern platforms including MacOSX, desktop Linux/BSD, iOS, Android -- *But not Windows, just use the excellent [AppleWin](https://github.com/AppleWin/AppleWin) emulator if you're on Windows!*
* Reasonable emulation fidelity to the original Apple //e machine (timing, video, audio, etc...)
* Language minimalism for core emulation modules (prefer coding in POSIX C over all else), except for CPU module which should be in assembly
* Good platform citizenship for menu system (prefer coding in language-of-choice promoted by platform--e.g.: Objective-C/Swift on Darwin, Java on Android, ...)

Android
-------

[Available on Google Play](https://play.google.com/store/apps/details?id=org.deadc0de.apple2ix.basic).

Running at 30FPS on Gingerbread (Android 2.3.3):
![Apple2ix on Samsung Galaxy Y running Gingerbread](https://raw.github.com/mauiaaron/apple2/develop/docs/android-galaxyY.png "Apple //ix")

Running at 60FPS on Nexus 6 running Lollipop (Android 5.1.1):
![Apple2ix on Nexus 6](https://raw.github.com/mauiaaron/apple2/develop/docs/android-nexus6.png "Apple //ix")

Mac Package
-----------

![Apple2Mac](https://raw.github.com/mauiaaron/apple2/master/docs/Apple2Mac.png "Apple2Mac")

A binary package for Macintosh is available at [deadc0de.org](http://deadc0de.org/Apple2Mac/Apple2Mac-0.9.dmg)  
Size : 10240000 (10MB)  
SHASUM : 81f2d55c2daaa0d3f9b33af9b50f69f6789738bf  

Alt Size : 76820480 (75MB)  
ALTSUM : 488a40d7f1187bcfd16d0045258f606a95f448cb  

Linux+ Package
--------------

For Linux and *BSD, I do not personally relish being a package/port maintainer, so you should `./configure --prefix=...`, `make`, `make install` like it's 1999 ;-)

You will need GCC or Clang compiler and other tools as determined by the `configure` script.  Use the source!

![Apple //ix](https://raw.github.com/mauiaaron/apple2/master/docs/Apple2ix.png "Apple //ix")

Project Tech
------------

* C language for the majority of the project (still the most portable/reliable language after all these years ;-)
* x86 and ARM assembly language for 65c02 CPU tightloop
* Extensive tests for 65c02 CPU, Apple //e VM, disks, and display (expected framebuffer output)
* OpenGLES 2.x graphics with GLSL shaders
* OpenAL and OpenSLES audio (emulated speaker and emulated Mockingboard/Phasor soundcards)
* Objective-C and Cocoa APIs (Mac/iOS variant)
* Java and Android APIs (Android app)

![DOS 3.3](https://raw.github.com/mauiaaron/apple2/master/docs/DOS33.png "DOS 3.3 Applesoft BASIC and //e monitor")

Semi-Ordered TODO
-----------------

* DHIRES graphics are ugly, fix 'em
* iOS/iWatch ports
* Proper VBL timing and vSync matching to the device (if available)
* OpenGL shaders/tricks for style (various screen artifacts) and functionality (Disk ][ status overlays, etc)
* Emulator save/restore and image compatibility with AppleWin
* Other feature parity with AppleWin
* Improved debugger routines (CLI/curses debugger?)
* CPU module variants as-needed in aarch64, plain C, Clang IR, ...
* Emscripten/web frontend?
