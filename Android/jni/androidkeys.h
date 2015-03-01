/*
 * Apple // emulator for *nix
 *
 * This software package is subject to the GNU General Public License
 * version 2 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * THERE ARE NO WARRANTIES WHATSOEVER.
 *
 */

#define ASCII_UCASE_OFFSET      0x24
#define ASCII_LCASE_OFFSET      0x44
#define ASCII_0_OFFSET          0x29
#define ASCII_NUMPAD_0_OFFSET   0x60
#define FN_OFFSET               0x48

#define KEYCODE_0               0x07
#define KEYCODE_1               0x08
#define KEYCODE_2               0x09
#define KEYCODE_3               0x0a
#define KEYCODE_4               0x0b
#define KEYCODE_5               0x0c
#define KEYCODE_6               0x0d
#define KEYCODE_7               0x0e
#define KEYCODE_8               0x0f
#define KEYCODE_9               0x10
#define KEYCODE_A               0x1d
#define KEYCODE_Z               0x36

#define KEYCODE_APOSTROPHE      0x4b
#define KEYCODE_AT              0x4d
#define KEYCODE_BACKSLASH       0x49
#define KEYCODE_COMMA           0x37
#define KEYCODE_EQUALS          0x46
#define KEYCODE_GRAVE           0x44
#define KEYCODE_LEFT_BRACKET    0x47
#define KEYCODE_MINUS           0x45
#define KEYCODE_PERIOD          0x38
#define KEYCODE_PLUS            0x51
#define KEYCODE_POUND           0x12
#define KEYCODE_RIGHT_BRACKET   0x48
#define KEYCODE_SEMICOLON       0x4a
#define KEYCODE_SLASH           0x4c
#define KEYCODE_SPACE           0x3e
#define KEYCODE_STAR            0x11

// numpad
#define KEYCODE_NUMPAD_0        0x90
#define KEYCODE_NUMPAD_9        0x99
#define KEYCODE_NUMPAD_ADD      0x9d
#define KEYCODE_NUMPAD_COMMA    0x9f
#define KEYCODE_NUMPAD_DIVIDE   0x9a
#define KEYCODE_NUMPAD_DOT      0x9e
#define KEYCODE_NUMPAD_EQUALS   0xa1
#define KEYCODE_NUMPAD_LEFT_PAREN  0xa2
#define KEYCODE_NUMPAD_RIGHT_PAREN 0xa3
#define KEYCODE_NUMPAD_MULTIPLY 0x9b
#define KEYCODE_NUMPAD_SUBTRACT 0x9c

// various meta keys
#define KEYCODE_ALT_LEFT        0x39
#define KEYCODE_ALT_RIGHT       0x3a
#define KEYCODE_BREAKPAUSE      0x79
#define KEYCODE_CAPS_LOCK       0x73
#define KEYCODE_CTRL_LEFT       0x71
#define KEYCODE_CTRL_RIGHT      0x72
#define KEYCODE_DEL             0x43
#define KEYCODE_ENTER           0x42
#define KEYCODE_ESC             0x6f
#define KEYCODE_INSERT          0x7c
#define KEYCODE_META_LEFT       0x75
#define KEYCODE_META_RIGHT      0x76
#define KEYCODE_PAGE_DOWN       0x5d
#define KEYCODE_PAGE_UP         0x5c
#define KEYCODE_SHIFT_LEFT      0x3b
#define KEYCODE_SHIFT_RIGHT     0x3c
#define KEYCODE_TAB             0x3d

// F1-F12
#define KEYCODE_F1              0x83
#define KEYCODE_F12             0x8e

// Directional pad movements
#define KEYCODE_DPAD_CENTER     0x17
#define KEYCODE_DPAD_DOWN       0x14
#define KEYCODE_DPAD_LEFT       0x15
#define KEYCODE_DPAD_RIGHT      0x16
#define KEYCODE_DPAD_UP         0x13

// Game pad buttons
#define KEYCODE_BUTTON_1        0xbc
#define KEYCODE_BUTTON_16       0xcb
#define KEYCODE_BUTTON_A        0x60
#define KEYCODE_BUTTON_B        0x61
#define KEYCODE_BUTTON_C        0x62
#define KEYCODE_BUTTON_L1       0x66
#define KEYCODE_BUTTON_L2       0x68
#define KEYCODE_BUTTON_R1       0x67
#define KEYCODE_BUTTON_R2       0x69

#define META_ALT_LEFT_ON        0x00000010
#define META_ALT_RIGHT_ON       0x00000020
#define META_CAPS_LOCK_ON       0x00100000
#define META_CTRL_LEFT_ON       0x00002000
#define META_CTRL_RIGHT_ON      0x00004000
#define META_SHIFT_MASK         0x000000c1
#define META_SHIFT_ON           0x00000040
#define META_SHIFT_LEFT_ON      0x00000040
#define META_SHIFT_RIGHT_ON     0x00000080

void android_keycode_to_emulator(int keyCode, int metaState, bool pressed);

