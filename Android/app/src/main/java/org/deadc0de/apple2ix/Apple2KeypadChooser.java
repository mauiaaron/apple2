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

package org.deadc0de.apple2ix;

import android.content.Context;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import java.util.ArrayList;

public class Apple2KeypadChooser implements Apple2MenuView {

    private final static String TAG = "Apple2KeypadChooser";

    private Apple2Activity mActivity = null;
    private View mSettingsView = null;
    private ArrayList<Apple2MenuView> mViewStack = null;
    private TextView mTextViewAxisChosenKeys = null;
    private TextView mTextViewButtonsChosenKeys = null;

    private STATE_MACHINE mChooserState = STATE_MACHINE.CHOOSE_NORTHWEST;

    private boolean mTouchMenuEnabled = false;
    private int mSavedTouchDevice = Apple2Preferences.TouchDeviceVariant.NONE.ordinal();

    public Apple2KeypadChooser(Apple2Activity activity, ArrayList<Apple2MenuView> viewStack) {
        mActivity = activity;
        mViewStack = viewStack;
        setup();
    }

    public final boolean isCalibrating() {
        return true;
    }

    public void onKeyTapCalibrationEvent(char ascii, int scancode) {
        if (ascii == Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION) {
            scancode = -1;
        }
        if (scancode == 0) {
            return;
        }
        Log.d(TAG, "ascii:'" + asciiRepresentation(ascii) + "' scancode:" + scancode);
        mChooserState.setValues(mActivity, ascii, scancode);
        mChooserState = mChooserState.next();

        mTextViewAxisChosenKeys.setText(getConfiguredAxisString());
        mTextViewButtonsChosenKeys.setText(getConfiguredButtonsString());
    }

    public void show() {
        if (isShowing()) {
            return;
        }
        mActivity.pushApple2View(this);
    }

    public void dismiss() {
        for (Apple2MenuView apple2MenuView : mViewStack) {
            if (apple2MenuView != this) {
                mActivity.pushApple2View(apple2MenuView);
            }
        }

        Apple2Preferences.nativeSetTouchMenuEnabled(mTouchMenuEnabled);
        Apple2Preferences.nativeSetCurrentTouchDevice(mSavedTouchDevice);
        Apple2Preferences.nativeTouchJoystickEndCalibrationMode(); // FIXME TODO : this should set not-joystick, not-keyboard, but keypad emulation

        mActivity.popApple2View(this);
    }

    public void dismissAll() {
        dismiss();
    }

    public boolean isShowing() {
        return mSettingsView.isShown();
    }

    public View getView() {
        return mSettingsView;
    }

    // ------------------------------------------------------------------------
    // internals

    private void setup() {
        LayoutInflater inflater = (LayoutInflater) mActivity.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        mSettingsView = inflater.inflate(R.layout.activity_chooser_keypad, null, false);

        // FIXME TODO: need to convert mousetext glyphs to equivalent Java/Android glyphs (or something akin to that)

        mTextViewAxisChosenKeys = (TextView) mSettingsView.findViewById(R.id.textViewAxisChosenKeys);
        mTextViewAxisChosenKeys.setText(getConfiguredAxisString());

        mTextViewButtonsChosenKeys = (TextView) mSettingsView.findViewById(R.id.textViewButtonsChosenKeys);
        mTextViewButtonsChosenKeys.setText(getConfiguredButtonsString());

        // temporarily undo these native touch settings while calibrating...
        mTouchMenuEnabled = Apple2Preferences.TOUCH_MENU_ENABLED.booleanValue(mActivity);
        Apple2Preferences.nativeSetTouchMenuEnabled(false);
        mSavedTouchDevice = Apple2Preferences.CURRENT_TOUCH_DEVICE.intValue(mActivity);
        Apple2Preferences.nativeSetCurrentTouchDevice(Apple2Preferences.TouchDeviceVariant.KEYBOARD.ordinal());

        Apple2Preferences.nativeTouchJoystickBeginCalibrationMode(); // FIXME TODO : this should set not-joystick, not-keyboard, but keypad emulation
    }

    private String asciiRepresentation(char ascii) {
        switch (ascii) {
            case Apple2KeyboardSettingsMenu.MOUSETEXT_OPENAPPLE:
                return mActivity.getResources().getString(R.string.key_open_apple);
            case Apple2KeyboardSettingsMenu.MOUSETEXT_CLOSEDAPPLE:
                return mActivity.getResources().getString(R.string.key_closed_apple);
            case Apple2KeyboardSettingsMenu.MOUSETEXT_UP:
                return mActivity.getResources().getString(R.string.key_up);
            case Apple2KeyboardSettingsMenu.MOUSETEXT_LEFT:
                return mActivity.getResources().getString(R.string.key_left);
            case Apple2KeyboardSettingsMenu.MOUSETEXT_RIGHT:
                return mActivity.getResources().getString(R.string.key_right);
            case Apple2KeyboardSettingsMenu.MOUSETEXT_DOWN:
                return mActivity.getResources().getString(R.string.key_down);
            case Apple2KeyboardSettingsMenu.ICONTEXT_CTRL:
                return mActivity.getResources().getString(R.string.key_ctrl);
            case Apple2KeyboardSettingsMenu.ICONTEXT_ESC:
                return mActivity.getResources().getString(R.string.key_esc);
            case Apple2KeyboardSettingsMenu.ICONTEXT_RETURN:
                return mActivity.getResources().getString(R.string.key_ret);
            case Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION:
                return mActivity.getResources().getString(R.string.key_none);
            case ' ':
            case Apple2KeyboardSettingsMenu.ICONTEXT_VISUAL_SPACE:
                return mActivity.getResources().getString(R.string.key_space);
            default:
                return "" + ascii;
        }
    }

    private String getConfiguredAxisString() {
        StringBuilder configuredAxes = new StringBuilder();
        configuredAxes.append(mActivity.getResources().getString(R.string.keypad_chosen_axis_keys));
        configuredAxes.append(" ");

        configuredAxes.append(mActivity.getResources().getString(R.string.keypad_key_axis_up));
        configuredAxes.append(":");
        configuredAxes.append(asciiRepresentation(Apple2Preferences.KEYPAD_NORTH_KEY.asciiValue(mActivity)));
        configuredAxes.append(", ");

        configuredAxes.append(mActivity.getResources().getString(R.string.keypad_key_axis_left));
        configuredAxes.append(":");
        configuredAxes.append(asciiRepresentation(Apple2Preferences.KEYPAD_WEST_KEY.asciiValue(mActivity)));
        configuredAxes.append(", ");

        configuredAxes.append(mActivity.getResources().getString(R.string.keypad_key_axis_right));
        configuredAxes.append(":");
        configuredAxes.append(asciiRepresentation(Apple2Preferences.KEYPAD_EAST_KEY.asciiValue(mActivity)));
        configuredAxes.append(", ");

        configuredAxes.append(mActivity.getResources().getString(R.string.keypad_key_axis_down));
        configuredAxes.append(":");
        configuredAxes.append(asciiRepresentation(Apple2Preferences.KEYPAD_SOUTH_KEY.asciiValue(mActivity)));

        return configuredAxes.toString();
    }

    private String getConfiguredButtonsString() {
        StringBuilder configuredButtons = new StringBuilder();
        configuredButtons.append(mActivity.getResources().getString(R.string.keypad_chosen_buttons_keys));
        configuredButtons.append(" ");

        configuredButtons.append(mActivity.getResources().getString(R.string.keypad_key_button_tap));
        configuredButtons.append(":");
        configuredButtons.append(asciiRepresentation(Apple2Preferences.KEYPAD_TAP_KEY.asciiValue(mActivity)));
        configuredButtons.append(", ");

        configuredButtons.append(mActivity.getResources().getString(R.string.keypad_key_button_swipeup));
        configuredButtons.append(":");
        configuredButtons.append(asciiRepresentation(Apple2Preferences.KEYPAD_SWIPEUP_KEY.asciiValue(mActivity)));
        configuredButtons.append(", ");

        configuredButtons.append(mActivity.getResources().getString(R.string.keypad_key_button_swipedown));
        configuredButtons.append(":");
        configuredButtons.append(asciiRepresentation(Apple2Preferences.KEYPAD_SWIPEDOWN_KEY.asciiValue(mActivity)));

        return configuredButtons.toString();
    }

    private enum STATE_MACHINE {
        CHOOSE_NORTHWEST {
            @Override
            public void setValues(Apple2Activity activity, char ascii, int scancode) {
                Apple2Preferences.KEYPAD_NORTHWEST_KEY.saveChosenKey(activity, ascii, scancode);
            }
        },
        CHOOSE_NORTH {
            @Override
            public void setValues(Apple2Activity activity, char ascii, int scancode) {
                Apple2Preferences.KEYPAD_NORTH_KEY.saveChosenKey(activity, ascii, scancode);
            }
        },
        CHOOSE_NORTHEAST {
            @Override
            public void setValues(Apple2Activity activity, char ascii, int scancode) {
                Apple2Preferences.KEYPAD_NORTHEAST_KEY.saveChosenKey(activity, ascii, scancode);
            }
        },
        CHOOSE_WEST {
            @Override
            public void setValues(Apple2Activity activity, char ascii, int scancode) {
                Apple2Preferences.KEYPAD_WEST_KEY.saveChosenKey(activity, ascii, scancode);
            }
        },
        CHOOSE_CENTER {
            @Override
            public void setValues(Apple2Activity activity, char ascii, int scancode) {
                Apple2Preferences.KEYPAD_CENTER_KEY.saveChosenKey(activity, ascii, scancode);
            }
        },
        CHOOSE_EAST {
            @Override
            public void setValues(Apple2Activity activity, char ascii, int scancode) {
                Apple2Preferences.KEYPAD_EAST_KEY.saveChosenKey(activity, ascii, scancode);
            }
        },
        CHOOSE_SOUTHWEST {
            @Override
            public void setValues(Apple2Activity activity, char ascii, int scancode) {
                Apple2Preferences.KEYPAD_SOUTHWEST_KEY.saveChosenKey(activity, ascii, scancode);
            }
        },
        CHOOSE_SOUTH {
            @Override
            public void setValues(Apple2Activity activity, char ascii, int scancode) {
                Apple2Preferences.KEYPAD_SOUTH_KEY.saveChosenKey(activity, ascii, scancode);
            }
        },
        CHOOSE_SOUTHEAST {
            @Override
            public void setValues(Apple2Activity activity, char ascii, int scancode) {
                Apple2Preferences.KEYPAD_SOUTHEAST_KEY.saveChosenKey(activity, ascii, scancode);
            }
        },
        CHOOSE_TAP {
            @Override
            public void setValues(Apple2Activity activity, char ascii, int scancode) {
                Apple2Preferences.KEYPAD_TAP_KEY.saveChosenKey(activity, ascii, scancode);
            }
        },
        CHOOSE_SWIPEUP {
            @Override
            public void setValues(Apple2Activity activity, char ascii, int scancode) {
                Apple2Preferences.KEYPAD_SWIPEUP_KEY.saveChosenKey(activity, ascii, scancode);
            }
        },
        CHOOSE_SWIPEDOWN {
            @Override
            public void setValues(Apple2Activity activity, char ascii, int scancode) {
                Apple2Preferences.KEYPAD_SWIPEDOWN_KEY.saveChosenKey(activity, ascii, scancode);
            }
        };

        public static final int size = STATE_MACHINE.values().length;

        public abstract void setValues(Apple2Activity activity, char ascii, int scancode);

        public STATE_MACHINE next() {
            int nextOrd = this.ordinal() + 1;
            if (nextOrd >= size) {
                nextOrd = 0;
            }
            STATE_MACHINE nextState = STATE_MACHINE.values()[nextOrd];
            return nextState;
        }
    }
}
