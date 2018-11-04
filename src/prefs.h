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
#define PREF_DOMAIN_TOUCHSCREEN "touchscreen"
#define PREF_DOMAIN_VIDEO "video"
#define PREF_DOMAIN_VM "vm"

// audio
#define PREF_MOCKINGBOARD_ENABLED "mbEnabled"
#define PREF_MOCKINGBOARD_VOLUME "mbVolume"
#define PREF_SPEAKER_VOLUME "speakerVolume"
#define PREF_AUDIO_LATENCY "audioLatency"

// interface
#define PREF_DEVICE_HEIGHT "deviceHeight"
#define PREF_DEVICE_WIDTH "deviceWidth"
#define PREF_DEVICE_LANDSCAPE "landscapeEnabled"
#define PREF_DISK_PATH "diskPath"
#define PREF_DISK_ANIMATIONS_ENABLED "diskAnimationsEnabled"
#define PREF_SOFTHUD_COLOR "hudColorMode"

// joystick
#define PREF_JOYSTICK_KPAD_AUTO_RECENTER "kpAutoRecenter"
#define PREF_JOYSTICK_KPAD_STEP "kpStep"
#define PREF_JOYSTICK_MODE "joystickMode"
#define PREF_JOYSTICK_CLIP_TO_RADIUS "clipToRadius"
// joystick (touchscreen)
#define PREF_AXIS_ON_LEFT "axisIsOnLeft"
#define PREF_AXIS_SENSITIVITY "axisSensitivity"
#define PREF_JOY_SWIPE_NORTH_CHAR "jsSwipeNorthChar"
#define PREF_JOY_SWIPE_SOUTH_CHAR "jsSwipeSouthChar"
#define PREF_JOY_TAP_DELAY "jsTapDelaySecs"
#define PREF_JOY_TOUCHDOWN_CHAR "jsTouchDownChar"
#define PREF_KPAD_REPEAT_THRESH "keyRepeatThresholdSecs"
#define PREF_KPAD_ROSETTE_CHAR_ARRAY "kpAxisRosetteChars"
#define PREF_KPAD_ROSETTE_SCAN_ARRAY "kpAxisRosetteScancodes"
#define PREF_KPAD_SWIPE_NORTH_CHAR "kpSwipeNorthChar"
#define PREF_KPAD_SWIPE_NORTH_SCAN "kpSwipeNorthScancode"
#define PREF_KPAD_SWIPE_SOUTH_CHAR "kpSwipeSouthChar"
#define PREF_KPAD_SWIPE_SOUTH_SCAN "kpSwipeSouthScancode"
#define PREF_KPAD_TOUCHDOWN_CHAR "kpTouchDownChar"
#define PREF_KPAD_TOUCHDOWN_SCAN "kpTouchDownScancode"
#define PREF_SCREEN_DIVISION "screenDivider"
#define PREF_SHOW_CONTROLS "showControls"
#define PREF_SWITCH_THRESHOLD "switchThreshold"

// keyboard
#define PREF_GLYPH_MULTIPLIER "glyphMultiplier"
#define PREF_KEYBOARD_CAPS "caps"
#define PREF_KEYBOARD_ALT_PATH "altPath"
#define PREF_KEYBOARD_VARIANT "variant"
#define PREF_LOWERCASE_ENABLED "lowercaseEnabled"
#define PREF_PORTRAIT_HEIGHT_SCALE "portraitHeightScale"
#define PREF_PORTRAIT_POSITION_SCALE "portraitPositionScale"

// touchscreen
#define PREF_CALIBRATING "isCalibrating"
#define PREF_MAX_ALPHA "maxAlpha"
#define PREF_MIN_ALPHA "minAlpha"
#define PREF_SCREEN_OWNER "screenOwner"
#define PREF_TOUCHMENU_ENABLED "touchMenuEnabled"

// video
#define PREF_COLOR_MODE "colorMode"

// vm
#define PREF_CPU_SCALE "cpuScale"
#define PREF_CPU_SCALE_ALT "cpuScaleAlt"

typedef void (*prefs_change_callback_f)(const char * _NONNULL domain);

// load preferences from persistent store
extern void prefs_load(void);

#if TESTING
extern void prefs_load_file(const char *filePath);
#endif

// load preferences from JSON string
extern void prefs_loadString(const char * _NONNULL jsonString);

// save preferences to persistent store
extern bool prefs_save(void);

// copy string value for key in prefs domain, returns true upon success and strdup()'d value in *val
extern bool prefs_copyStringValue(const char * _NONNULL domain, const char * _NONNULL key, INOUT char ** _NONNULL val);

// create JSON_ref value for key in prefs domain, returns true upon success and newly allocated value in jsonVal
extern bool prefs_copyJSONValue(const char * _NONNULL domain, const char * _NONNULL key, INOUT JSON_ref *jsonVal);

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
extern void prefs_shutdown(void);

#endif

