/*
 * Apple // emulator for *ix
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 2015 Aaron Culliney
 *
 */

extern unsigned long android_deviceSampleRateHz;
extern unsigned long android_monoBufferSubmitSizeSamples;
extern unsigned long android_stereoBufferSubmitSizeSamples;

// architectures

extern bool android_armArch;
extern bool android_armArchV7A;
extern bool android_arm64Arch;

extern bool android_x86;
extern bool android_x86_64;

// vector instructions availability

extern bool android_armNeonEnabled;
extern bool android_x86SSSE3Enabled;

