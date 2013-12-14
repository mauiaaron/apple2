Apple //ix
==========

A salvaged Apple //e emulator, originally written in the mid-90's in C and x86 assembly and currently suffering from
quite a bit of rot...

Project Goals
-------------

* Approach 100% emulation fidelity with Apple //e machine
* POSIX portability
* A tech playground for me :-) ... This is actually an important point...  I'm doing this because it's fun and allows me
  to play with a wide swath of fundamental tech : virtual CPU, virtual machines, assembly language programming, video
  and audio programming to name a few...  So you might say that the process and practice is as important as the goals.

![Apple //ix](https://raw.github.com/mauiaaron/apple2/master/docs/Apple2ix.png "Apple //ix")

Status Update
-------------

20131214 ...

* It builds and works for me :-) as a 32bit binary on GNU/Linux with X11 and OpenAL
* Ideally I'd like to maintain the CPU thread tightloop in assembly, and so will need to write new x86-64, ARM, (and
  also a generic C target) in addition to maintaining existing x86 assembly routines
* Before any significant platform/architecture porting is started, need to develop unit tests!!! :-)

![Public Domain Mystery House Disk Image](https://raw.github.com/mauiaaron/apple2/master/docs/MysteryHouse.png "Public Domain Mystery House Disk Image")

Semi-Ordered TODO
-----------------

* GNU/Linux x86 alpha-test binaries
* Unit tests
* POSIX x64 target (Linux, \*BSD, ...)
* MacOS port
* ARM tablets (iOS, Android, ...)
* General refactoring for modularity, clarity, and portability as I go (and have tests to double-check stuff :-)

![DOS 3.3](https://raw.github.com/mauiaaron/apple2/master/docs/DOS33.png "DOS 3.3 Applesoft BASIC and //e monitor")

