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

#include "common.h"
#include "androidkeys.h"

// Codepaths for Bluetooth or other HID keyboards attached to an Android device.
// For OpenGL emulated touchscreen keyboard, see gltouchkbd.c

static inline bool _is_shifted(int metaState) {
    return (metaState & META_SHIFT_MASK);
}

static inline bool _is_ctrl(int metaState) {
    return (metaState & META_CTRL_LEFT_ON) || (metaState & META_CTRL_RIGHT_ON);
}

void android_keycode_to_emulator(int keyCode, int metaState, bool pressed) {
    int key = -1;
    bool isASCII = true;

    do {
        if ((keyCode >= KEYCODE_NUMPAD_0) && (keyCode <= KEYCODE_NUMPAD_9)) {
            key = keyCode - ASCII_NUMPAD_0_OFFSET;
            break;
        } else if ((keyCode >= KEYCODE_A) && (keyCode <= KEYCODE_Z)) {
            key = caps_lock || _is_shifted(metaState) ? (keyCode + ASCII_UCASE_OFFSET) : (keyCode + ASCII_LCASE_OFFSET);
            break;
        } else if ((keyCode >= KEYCODE_F1) && (keyCode <= KEYCODE_F12)) {
            isASCII = false;
            key = keyCode - FN_OFFSET;
            break;
        }

        switch (keyCode) {
            case KEYCODE_0:
                key = _is_shifted(metaState) ? ')' : keyCode + ASCII_0_OFFSET;
                break;
            case KEYCODE_1:
                key = _is_shifted(metaState) ? '!' : keyCode + ASCII_0_OFFSET;
                break;
            case KEYCODE_2:
                key = _is_shifted(metaState) ? '@' : keyCode + ASCII_0_OFFSET;
                break;
            case KEYCODE_3:
                key = _is_shifted(metaState) ? '#' : keyCode + ASCII_0_OFFSET;
                break;
            case KEYCODE_4:
                key = _is_shifted(metaState) ? '$' : keyCode + ASCII_0_OFFSET;
                break;
            case KEYCODE_5:
                key = _is_shifted(metaState) ? '%' : keyCode + ASCII_0_OFFSET;
                break;
            case KEYCODE_6:
                key = _is_shifted(metaState) ? '^' : keyCode + ASCII_0_OFFSET;
                break;
            case KEYCODE_7:
                key = _is_shifted(metaState) ? '&' : keyCode + ASCII_0_OFFSET;
                break;
            case KEYCODE_8:
                key = _is_shifted(metaState) ? '*' : keyCode + ASCII_0_OFFSET;
                break;
            case KEYCODE_9:
                key = _is_shifted(metaState) ? '(' : keyCode + ASCII_0_OFFSET;
                break;
            case KEYCODE_APOSTROPHE:
                key = _is_shifted(metaState) ? '"' : '\'';
                break;
            case KEYCODE_AT:
                key = '@';
                break;
            case KEYCODE_BACKSLASH:
                key = _is_shifted(metaState) ? '|' : '\\';
                break;
            case KEYCODE_COMMA:
                key = _is_shifted(metaState) ? '<' : ',';
                break;
            case KEYCODE_EQUALS:
                key = _is_shifted(metaState) ? '+' : '=';
                break;
            case KEYCODE_GRAVE:
                key = _is_shifted(metaState) ? '~' : '`';
                break;
            case KEYCODE_LEFT_BRACKET:
                key = _is_shifted(metaState) ? '{' : '[';
                break;
            case KEYCODE_MINUS:
                key = _is_shifted(metaState) ? '_' : '-';
                break;
            case KEYCODE_NUMPAD_ADD:
                key = '+';
                break;
            case KEYCODE_NUMPAD_COMMA:
                key = ',';
                break;
            case KEYCODE_NUMPAD_DIVIDE:
                key = '/';
                break;
            case KEYCODE_NUMPAD_DOT:
                key = '.';
                break;
            case KEYCODE_NUMPAD_EQUALS:
                key = '=';
                break;
            case KEYCODE_NUMPAD_LEFT_PAREN:
                key = '(';
                break;
            case KEYCODE_NUMPAD_RIGHT_PAREN:
                key = ')';
                break;
            case KEYCODE_NUMPAD_MULTIPLY:
                key = '*';
                break;
            case KEYCODE_NUMPAD_SUBTRACT:
                key = '-';
                break;
            case KEYCODE_PERIOD:
                key = _is_shifted(metaState) ? '>' : '.';
                break;
            case KEYCODE_PLUS:
                key = '+';
                break;
            case KEYCODE_POUND:
                key = '#';
                break;
            case KEYCODE_RIGHT_BRACKET:
                key = _is_shifted(metaState) ? '}' : ']';
                break;
            case KEYCODE_SEMICOLON:
                key = _is_shifted(metaState) ? ':' : ';';
                break;
            case KEYCODE_SLASH:
                key = _is_shifted(metaState) ? '?' : '/';
                break;
            case KEYCODE_SPACE:
                key = ' ';
                break;
            case KEYCODE_STAR:
                key = '*';
                break;
            default:
                break;
        }

        if (key != -1) {
            break;
        }

        // META
        isASCII = false;

        switch (keyCode) {
            case KEYCODE_DPAD_LEFT:
                key = SCODE_L;
                break;
            case KEYCODE_DPAD_RIGHT:
                key = SCODE_R;
                break;
            case KEYCODE_DPAD_DOWN:
                key = SCODE_D;
                break;
            case KEYCODE_DPAD_UP:
                key = SCODE_U;
                break;
            case KEYCODE_DPAD_CENTER:
                key = SCODE_HOME;
                break;

            case KEYCODE_PAGE_UP:
                key = SCODE_PGUP;
                break;
            case KEYCODE_PAGE_DOWN:
                key = SCODE_PGDN;
                break;
            case KEYCODE_INSERT:
                key = SCODE_INS;
                break;

            case KEYCODE_SHIFT_LEFT:
                key = SCODE_L_SHIFT;
                break;
            case KEYCODE_SHIFT_RIGHT:
                key = SCODE_R_SHIFT;
                break;

            case KEYCODE_CTRL_LEFT:
                key = SCODE_L_CTRL;
                break;
            case KEYCODE_CTRL_RIGHT:
                key = SCODE_R_CTRL;
                break;

            case KEYCODE_ALT_LEFT:
            case KEYCODE_META_LEFT:
                key = SCODE_L_ALT;
                break;
            case KEYCODE_ALT_RIGHT:
            case KEYCODE_META_RIGHT:
                key = SCODE_R_ALT;
                break;
            case KEYCODE_BREAKPAUSE:
                key = SCODE_BRK;
                break;
            case KEYCODE_CAPS_LOCK:
                caps_lock = (metaState & META_CAPS_LOCK_ON);
                return;
            case KEYCODE_DEL:
                key = SCODE_L;// DEL is prolly not what they meant =P
                break;
            case KEYCODE_ENTER:
                key = SCODE_RET;
                break;
            case KEYCODE_TAB:
                key = SCODE_TAB;
                break;
            case KEYCODE_ESC:
                key = SCODE_ESC;
                break;

            default:
                break;
        }
    } while (0);

    LOG("keyCode:%08x -> key:%02x ('%c') metaState:%08x", keyCode, key, key, metaState);

    if (isASCII && _is_ctrl(metaState)) {
        key = c_keys_ascii_to_scancode(key);
        c_keys_handle_input(key, true, false);
        isASCII = false;
        pressed = false;
    }

    assert(key < 0x80);
    c_keys_handle_input(key, pressed, isASCII);
}

