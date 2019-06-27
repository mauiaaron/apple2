/*
 * Apple // emulator for *ix
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 2013-2015 Aaron Culliney
 *
 */

#include "common.h"
#include "video/glinput.h"
#include <math.h>

//----------------------------------------------------------------------------
//
// keyboard
//

static inline void _capslock_hackaround(void) {
    // NOTE : Unfortunately it appears that we cannot get a raw key down/up notification for CAPSlock, so hack that here
    // ALSO : Emulator initially sets CAPS state based on a user preference, but sync to system state if user changes it
    int modifiers = glutGetModifiers();
    if (!keys_isShifted()) {
        if (modifiers & GLUT_ACTIVE_SHIFT) {
            use_system_caps_lock = true;
            caps_lock = true;
        } else if (use_system_caps_lock) {
            caps_lock = false;
        }
    }
}

// Map glut keys into Apple//ix internal-representation scancodes.
static int _glutkey_to_scancode(int key) {

    switch (key) {
        case GLUT_KEY_F1:
            key = SCODE_F1;
            break;
        case GLUT_KEY_F2:
            key = SCODE_F2;
            break;
        case GLUT_KEY_F3:
            key = SCODE_F3;
            break;
        case GLUT_KEY_F4:
            key = SCODE_F4;
            break;
        case GLUT_KEY_F5:
            key = SCODE_F5;
            break;
        case GLUT_KEY_F6:
            key = SCODE_F6;
            break;
        case GLUT_KEY_F7:
            key = SCODE_F7;
            break;
        case GLUT_KEY_F8:
            key = SCODE_F8;
            break;
        case GLUT_KEY_F9:
            key = SCODE_F9;
            break;
        case GLUT_KEY_F10:
            key = SCODE_F10;
            break;
        case GLUT_KEY_F11:
            key = SCODE_F11;
            break;
        case GLUT_KEY_F12:
            key = SCODE_F12;
            break;

        case GLUT_KEY_LEFT:
            key = SCODE_L;
            break;
        case GLUT_KEY_RIGHT:
            key = SCODE_R;
            break;
        case GLUT_KEY_DOWN:
            key = SCODE_D;
            break;
        case GLUT_KEY_UP:
            key = SCODE_U;
            break;

        case GLUT_KEY_PAGE_UP:
            key = SCODE_PGUP;
            break;
        case GLUT_KEY_PAGE_DOWN:
            key = SCODE_PGDN;
            break;
        case GLUT_KEY_HOME:
            key = SCODE_HOME;
            break;
        case GLUT_KEY_END:
            key = SCODE_END;
            break;
        case GLUT_KEY_INSERT:
            key = SCODE_INS;
            break;

        case GLUT_KEY_SHIFT_L:
            key = SCODE_L_SHIFT;
            break;
        case GLUT_KEY_SHIFT_R:
            key = SCODE_R_SHIFT;
            break;

        case GLUT_KEY_CTRL_L:
            key = SCODE_L_CTRL;
            break;
        case GLUT_KEY_CTRL_R:
            key = SCODE_R_CTRL;
            break;

        case GLUT_KEY_ALT_L:
            key = SCODE_L_ALT;
            break;
        case GLUT_KEY_ALT_R:
            key = SCODE_R_ALT;
            break;

        //---------------------------------------------------------------------
        // GLUT does not appear to differentiate keypad keys?
        //case XK_KP_5:
        case GLUT_KEY_BEGIN:
            key = SCODE_KPAD_C;
            break;

        default:
            key = keys_ascii2Scancode(key);
            break;
    }

    assert(key < 0x80);
    return key;
}

void gldriver_on_key_down(unsigned char key, int x, int y) {
    _capslock_hackaround();
    //LOG("onKeyDown %02x(%d)'%c'", key, key, key);
    keys_handleInput(key, /*is_pressed:*/true, /*is_ascii:*/true);
}

void gldriver_on_key_up(unsigned char key, int x, int y) {
    _capslock_hackaround();
    //LOG("onKeyUp %02x(%d)'%c'", key, key, key);
    keys_handleInput(key, /*is_pressed:*/false, /*is_ascii:*/true);
}

void gldriver_on_key_special_down(int key, int x, int y) {
    _capslock_hackaround();
    int scancode = _glutkey_to_scancode(key);
    if (scancode == SCODE_F11) {
        glutFullScreenToggle();
    }
    //LOG("onKeySpecialDown %08x(%d) -> %02X(%d)", key, key, scancode, scancode);
    keys_handleInput(scancode, /*is_pressed:*/true, /*is_ascii:*/false);
}

void gldriver_on_key_special_up(int key, int x, int y) {
    _capslock_hackaround();
    int scancode = _glutkey_to_scancode(key);
    //LOG("onKeySpecialDown %08x(%d) -> %02X(%d)", key, key, scancode, scancode);
    keys_handleInput(scancode, /*is_pressed:*/false, /*is_ascii:*/false);
}

#define JOYSTICK_POLL_INTERVAL_MILLIS (ceilf(1000.f/60))

static void gldriver_joystick_callback(unsigned int buttonMask, int x, int y, int z) {

#ifdef KEYPAD_JOYSTICK
    if (joy_mode == JOY_KPAD) {
        return;
    }
#endif

    // sample buttons only if apple keys aren't pressed. keys get set to 0xff, and js buttons are set to 0x80.
    if (!(run_args.joy_button0 & 0x7f)) {
        run_args.joy_button0 = (buttonMask & 0x01) ? 0x80 : 0x0;
    }
    if (!(run_args.joy_button1 & 0x7f)) {
        run_args.joy_button1 = (buttonMask & 0x02) ? 0x80 : 0x0;
    }

    // normalize GLUT range
    static const float normalizer = 256.f/2000.f;

    joy_x = (uint16_t)((x+1000)*normalizer);
    joy_y = (uint16_t)((y+1000)*normalizer);
    if (joy_x > 0xFF) {
        joy_x = 0xFF;
    }
    if (joy_y > 0xFF) {
        joy_y = 0xFF;
    }
}

//----------------------------------------------------------------------------
//
// mouse
//

#if 0
void gldriver_mouse_drag(int x, int y) {
    //LOG("drag %d,%d", x, y);
    ivec2 currentMouse(x, y);
    applicationEngine->OnFingerMove(previousMouse, currentMouse);
    previousMouse.x = x;
    previousMouse.y = y;
}

void gldriver_mouse(int button, int state, int x, int y) {
    //LOG("mouse button:%d state:%d %d,%d", button, state, x, y);
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        previousMouse.x = x;
        previousMouse.y = y;
        applicationEngine->OnFingerDown(ivec2(x, y));
    } else if (button == GLUT_LEFT_BUTTON && state == GLUT_UP) {
        applicationEngine->OnFingerUp(ivec2(x, y));
    }
}
#endif

void _glutJoystickReset(void) {
    glutJoystickFunc(NULL, 0);
    glutJoystickFunc(gldriver_joystick_callback, (int)JOYSTICK_POLL_INTERVAL_MILLIS);
}

