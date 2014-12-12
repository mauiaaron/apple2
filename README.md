Apple //ix
==========

Apple2ix is an Apple //e emulator written primarily in C and x86 assembly language with a smattering of Objective-C
(Cocoa port).  Apple2ix is derived from the apple2-emul-linux project originally coded in the mid 90's.

Project Goals
-------------

The project seeks to be portable across a wide range of modern POSIX systesm including MacOSX, desktop Linux/BSD, iOS,
and Android.

As of December 2014, the emulator runs on MacOSX 10.9+ and Debian GNU/Linux, and mobile ports are currently on the
drawing board.

Mac Package
-----------

![Apple2Mac](https://raw.github.com/mauiaaron/apple2/master/docs/Apple2Mac.png "Apple2Mac")

A binary package for Macintosh is available at [deadc0de.org](http://deadc0de.org/Apple2Mac/Apple2Mac-0.9.dmg)
Size : 10240000 (10MB)
SHASUM : 81f2d55c2daaa0d3f9b33af9b50f69f6789738bf

Alt Size : 76820480 (75MB)
ALTSUM : 488a40d7f1187bcfd16d0045258f606a95f448cb

Linux Package
-------------

At the moment consists of `./configure --prefix=...`, `make`, `make install` ;-)  You will need GCC or Clang compiler
and other tools as determined by the `configure` script.

![Apple //ix](https://raw.github.com/mauiaaron/apple2/master/docs/Apple2ix.png "Apple //ix")

Project Tech
------------

* C language for the majority of the project (still the most portable/reliable language after all these years ;-)
* Assembly language for 65c02 CPU tightloop
* Extensive tests for 65c02 CPU, Apple //e VM, and display (expected framebuffer output)
* OpenGLES 2.x graphics
* OpenAL audio (emulated speaker and emulated Mockingboard/Phasor soundcards)
* Objective-C and Cocoa APIs (Mac/iOS variant)

![DOS 3.3](https://raw.github.com/mauiaaron/apple2/master/docs/DOS33.png "DOS 3.3 Applesoft BASIC and //e monitor")

Semi-Ordered TODO
-----------------

* Proper VBL timing
* ProDOS-order Disk Images
* ARM assembly/ABI variant (in prep for mobile)
* iOS port
* Android NDK port
* Emulator save/restore and image compatibility with AppleWin
* Other feature parity with AppleWin
* Improved debugger routines
* Emscripten/web frontend?

