/*
 * Apple // emulator for *ix
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 1994 Alexander Jean-Claude Bottema
 * Copyright 1995 Stephen Lee
 * Copyright 1997, 1998 Aaron Culliney
 * Copyright 1998, 1999, 2000 Michael Deutschmann
 * Copyright 2013-2016 Aaron Culliney
 *
 */

#ifndef _PREFS_H_
#define _PREFS_H_

#include "common.h"

// pref domains
#define PREF_DOMAIN_AUDIO "audio"
#define PREF_DOMAIN_INTERFACE "interface"
#define PREF_DOMAIN_JOYSTICK "joystick"
#define PREF_DOMAIN_KEYBOARD "keyboard"
#define PREF_DOMAIN_VIDEO "video"
#define PREF_DOMAIN_VM "vm"

// audio
#define PREF_SPEAKER_VOLUME "speakerVolume"
#define PREF_MOCKINGBOARD_VOLUME "mbVolume"

// video
#define PREF_COLOR_MODE "colorMode"

// joystick
#define PREF_JOYSTICK_MODE "joystickMode"
#define PREF_JOYSTICK_KPAD_AUTO_RECENTER "kpAutoRecenter"
#define PREF_JOYSTICK_KPAD_STEP "kpStep"

// keyboard
#define PREF_KEYBOARD_CAPS "caps"

// interface
#define PREF_DISK_PATH "diskPath"

// vm
#define PREF_CPU_SCALE "cpuScale"
#define PREF_CPU_SCALE_ALT "cpuScaleAlt"

typedef void (*prefs_change_callback_f)(const char * _NONNULL domain);

// load preferences from persistent store
extern void prefs_load(void);

// load preferences from JSON string
extern void prefs_loadString(const char * _NONNULL jsonString);

// save preferences to persistent store
extern bool prefs_save(void);

// copy string value for key in prefs domain, returns true upon success and strdup()'d value in *val
extern bool prefs_copyStringValue(const char * _NONNULL domain, const char * _NONNULL key, INOUT char ** _NONNULL val);

// get long value for key in prefs domain, returns true upon success
extern bool prefs_parseLongValue(const char * _NONNULL domain, const char * _NONNULL key, INOUT long *val, const long base);

// get long value for key in prefs domain, returns true upon success
extern bool prefs_parseBoolValue(const char * _NONNULL domain, const char * _NONNULL key, INOUT bool *val);

// get float value for key in prefs domain, returns true upon success
extern bool prefs_parseFloatValue(const char * _NONNULL domain, const char *key, INOUT float * _NONNULL val);

// set string value for key in prefs domain
extern bool prefs_setStringValue(const char *domain, const char * _NONNULL key, const char * _NONNULL val);

// set long value for key in prefs domain
extern bool prefs_setLongValue(const char * _NONNULL domain, const char * _NONNULL key, long val);

// set bool value for key in map JSON, returns true upon success
extern bool prefs_setBoolValue(const char * _NONNULL domain, const char * _NONNULL key, bool val);

// set float value for key in prefs domain
extern bool prefs_setFloatValue(const char * _NONNULL domain, const char * _NONNULL key, float val);

// register a preferences listener for a particular domain
extern void prefs_registerListener(const char * _NONNULL domain, _NONNULL prefs_change_callback_f callback);

// send change notification to all domain listeners
extern void prefs_sync(const char * _NULLABLE domain);

// cleans up and removes listener in preparation for app shutdown
extern void prefs_shutdown(bool emulatorShuttingDown);

#endif

