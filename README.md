Apple //ix
==========

Apple2ix is an Apple //e emulator written primarily in C and x86 assembly language with a smattering of Objective-C
(Cocoa port).  Apple2ix is derived from the apple2-emul-linux project originally coded in the mid 90's.

Project Goals
-------------

The project seeks to be portable across a wide range of modern POSIX systesm including MacOSX desktop, Desktop
Linux/BSD, iOS, and Android.

As of December 2014, the emulator run on MacOSX 10.9+ and Debian GNU/Linux, and mobile ports are currently on the
drawing board.

Screenshots show an earlier version of the Linux variant.

![Apple //ix](https://raw.github.com/mauiaaron/apple2/master/docs/Apple2ix.png "Apple //ix")

Mac Package
-----------

A binary package for Macintosh is available at [bitr0t.com](http://bitr0t.com/Apple2Mac/)

Linux Package
-------------

At the moment consists of `./configure --prefix=...`, `make`, `make install` ;-)

Project Tech
------------

* Majority of coding in the C language (still the most portable/reliable after all these years ;-)
* Assembly language for 65c02 CPU tightloop
* Extensive CPU, VirtualMachine, and display (expected output) tests
* OpenGLES 2.x
* OpenAL
* Cocoa APIs

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

