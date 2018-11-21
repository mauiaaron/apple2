Apple //ix
==========

Apple2ix is an Apple //e (8bit 65c02 CPU) emulator designed to work on various POSIX platforms.

Lineage
-------

Apple2ix derives from the apple2-emul-linux project originally coded by various developers in the mid 1990's and FTP-uploaded as source tarballs to the original `tsx-11.mit.edu` Linux archive.

The original software was designed to work from the Linux console rendering via SVGAlib.  It ran on par to the 1MHz Apple //e on an i386 (Pentium 100 class) or better machine.  Later ports added X11 graphics support based on the original X11 DOOM source code drop, ty JC!

As of October 2016, the resurrected Apple2ix runs on x86 and ARM Android devices, x64 macOS 10.9+, and x64 GNU/Linux

Project Goals
-------------

* Portability and code resilience across a wide range of modern POSIXy systems including macOS, desktop Linux/BSD, iOS, Android. *If you are on Windows, just use the excellent [AppleWin](https://github.com/AppleWin/AppleWin) emulator!*
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

Maintained Ports
----------------

* Android 4.0+ (Android Studio & NDK build)
* Desktop GNU/Linux (GNU tools build)
* macOS/iOS (Xcode build)

Published Android App
---------------------

[Available on Google Play](https://play.google.com/store/apps/details?id=org.deadc0de.apple2ix.basic).

Running at 60FPS on Android 4.0+ devices:
![Apple2ix on Nexus 6](https://raw.github.com/mauiaaron/apple2/develop/docs/android-nexus6.png "Apple //ix")

Semi-Ordered TODO
-----------------

* Emulation feature implementations ... (mouse, printer, 3.5" disk, AppleHD, Phasor, ethernet?, ...)
* OS Ports : Net/Open/Free-BSD ports
* CPU arch ports: aarch64, RISC-V, Clang IR (bitcode)
* Port to web assembly

![DOS 3.3](https://raw.github.com/mauiaaron/apple2/master/docs/DOS33.png "DOS 3.3 Applesoft BASIC and //e monitor")

