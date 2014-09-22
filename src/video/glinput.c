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

// glinput -- Created by Aaron Culliney

#include "common.h"
#include "video/glinput.h"
#include "video/vgl.h"

//----------------------------------------------------------------------------
//
// keyboard
//

static inline void _capslock_hackaround(void) {
    // NOTE : Unfortunately it appears that we cannot get a raw key down/up notification for CAPSlock, so hack that here
    // ALSO : Emulator initially sets CAPS state based on a user preference, but sync to system state if user changes it
    static bool modified_caps_lock = false;
    int modifiers = glutGetModifiers();
    if (!c_keys_is_shifted()) {
        if (modifiers & GLUT_ACTIVE_SHIFT) {
            modified_caps_lock = true;
            caps_lock = true;
        } else if (modified_caps_lock) {
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
            key = c_keys_ascii_to_scancode(key);
            break;
    }

    assert(key < 0x80);
    return key;
}

#if !defined(TESTING)
void gldriver_on_key_down(unsigned char key, int x, int y) {
    _capslock_hackaround();
    //LOG("onKeyDown %02x(%d)'%c'", key, key, key);
    c_keys_handle_input(key, 1, 1);
}

void gldriver_on_key_up(unsigned char key, int x, int y) {
    _capslock_hackaround();
    //LOG("onKeyUp %02x(%d)'%c'", key, key, key);
    c_keys_handle_input(key, 0, 1);
}

void gldriver_on_key_special_down(int key, int x, int y) {
    _capslock_hackaround();
    int scancode = _glutkey_to_scancode(key);
    //LOG("onKeySpecialDown %08x(%d) -> %02X(%d)", key, key, scancode, scancode);
    c_keys_handle_input(scancode, 1, 0);
}

void gldriver_on_key_special_up(int key, int x, int y) {
    _capslock_hackaround();
    int scancode = _glutkey_to_scancode(key);
    //LOG("onKeySpecialDown %08x(%d) -> %02X(%d)", key, key, scancode, scancode);
    c_keys_handle_input(scancode, 0, 0);
}
#endif

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

