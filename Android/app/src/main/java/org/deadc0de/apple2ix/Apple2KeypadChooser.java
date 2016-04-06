/*
 * Apple // emulator for *nix
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 2015 Aaron Culliney
 *
 */

package org.deadc0de.apple2ix;

import android.content.Context;
import android.os.Handler;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import java.util.ArrayList;

import org.deadc0de.apple2ix.basic.R;
import org.json.JSONArray;
import org.json.JSONException;

public class Apple2KeypadChooser implements Apple2MenuView {

    private final static String TAG = "Apple2KeypadChooser";

    private Apple2Activity mActivity = null;
    private View mSettingsView = null;
    private ArrayList<Apple2MenuView> mViewStack = null;
    private TextView mCurrentChoicePrompt = null;

    private STATE_MACHINE mChooserState = STATE_MACHINE.CHOOSE_NORTHWEST;

    private boolean mTouchMenuEnabled = false;
    private int mSavedTouchDevice = Apple2SettingsMenu.TouchDeviceVariant.NONE.ordinal();

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

        String asciiStr = asciiRepresentation(ascii);
        Log.d(TAG, "ascii:'" + asciiStr + "' scancode:" + scancode);
        mChooserState.setKey(mActivity, ascii, scancode);
        Apple2Preferences.setJSONPref(Apple2SettingsMenu.SETTINGS.CURRENT_INPUT, Apple2SettingsMenu.TouchDeviceVariant.JOYSTICK_KEYPAD.ordinal());
        Apple2Preferences.sync(mActivity, Apple2Preferences.PREF_DOMAIN_TOUCHSCREEN);

        mCurrentChoicePrompt.setText(getNextChoiceString() + asciiStr);
        switch (mChooserState) {
            case CHOOSE_TAP:
                Apple2View.nativeOnTouch(MotionEvent.ACTION_DOWN, 1, 0, new float[]{400.f}, new float[]{400.f});
                Apple2View.nativeOnTouch(MotionEvent.ACTION_UP, 1, 0, new float[]{400.f}, new float[]{400.f});
                break;
            case CHOOSE_SWIPEDOWN:
                Apple2View.nativeOnTouch(MotionEvent.ACTION_DOWN, 1, 0, new float[]{400.f}, new float[]{400.f});
                Apple2View.nativeOnTouch(MotionEvent.ACTION_MOVE, 1, 0, new float[]{400.f}, new float[]{600.f});
                Apple2View.nativeOnTouch(MotionEvent.ACTION_UP, 1, 0, new float[]{400.f}, new float[]{600.f});
                break;
            case CHOOSE_SWIPEUP:
                Apple2View.nativeOnTouch(MotionEvent.ACTION_DOWN, 1, 0, new float[]{400.f}, new float[]{400.f});
                Apple2View.nativeOnTouch(MotionEvent.ACTION_MOVE, 1, 0, new float[]{400.f}, new float[]{200.f});
                Apple2View.nativeOnTouch(MotionEvent.ACTION_UP, 1, 0, new float[]{400.f}, new float[]{200.f});
                break;
            default:
                break;
        }

        final Handler handler = new Handler();
        handler.postDelayed(new Runnable() {
            @Override
            public void run() {
                mChooserState = mChooserState.next();
                mCurrentChoicePrompt.setText(getNextChoiceString());
                Apple2Preferences.setJSONPref(Apple2SettingsMenu.SETTINGS.CURRENT_INPUT, Apple2SettingsMenu.TouchDeviceVariant.KEYBOARD.ordinal());
                Apple2Preferences.sync(mActivity, Apple2Preferences.PREF_DOMAIN_TOUCHSCREEN);
            }
        }, 1000);
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

        Apple2Preferences.setJSONPref(Apple2Preferences.PREF_DOMAIN_TOUCHSCREEN, Apple2Preferences.PREF_CALIBRATING, false);

        Apple2Preferences.setJSONPref(Apple2KeyboardSettingsMenu.SETTINGS.TOUCH_MENU_ENABLED, mTouchMenuEnabled);
        Apple2Preferences.setJSONPref(Apple2SettingsMenu.SETTINGS.CURRENT_INPUT, mSavedTouchDevice);

        Apple2Preferences.sync(mActivity, Apple2Preferences.PREF_DOMAIN_TOUCHSCREEN);

        mActivity.popApple2View(this);
    }

    public void dismissAll() {
        dismiss();
    }

    public boolean isShowing() {
        return mSettingsView.getParent() != null;
    }

    public View getView() {
        return mSettingsView;
    }

    // ------------------------------------------------------------------------
    // internals

    private void setup() {
        JSONArray jsonChars = (JSONArray) Apple2Preferences.getJSONPref(Apple2Preferences.PREF_DOMAIN_JOYSTICK, Apple2KeypadSettingsMenu.PREF_KPAD_ROSETTE_CHAR_ARRAY, null);
        JSONArray jsonScans = (JSONArray) Apple2Preferences.getJSONPref(Apple2Preferences.PREF_DOMAIN_JOYSTICK, Apple2KeypadSettingsMenu.PREF_KPAD_ROSETTE_SCAN_ARRAY, null);
        mChooserState.start(jsonChars, jsonScans);

        LayoutInflater inflater = (LayoutInflater) mActivity.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        mSettingsView = inflater.inflate(R.layout.activity_chooser_keypad, null, false);

        mCurrentChoicePrompt = (TextView) mSettingsView.findViewById(R.id.currentChoicePrompt);
        mCurrentChoicePrompt.setText(getNextChoiceString());

        Button skipButton = (Button) mSettingsView.findViewById(R.id.skipButton);
        skipButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Apple2KeypadChooser.this.onKeyTapCalibrationEvent((char) Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1);
            }
        });

        // temporarily undo these native touch settings while calibrating...
        mTouchMenuEnabled = (boolean) Apple2Preferences.getJSONPref(Apple2KeyboardSettingsMenu.SETTINGS.TOUCH_MENU_ENABLED);
        Apple2Preferences.setJSONPref(Apple2KeyboardSettingsMenu.SETTINGS.TOUCH_MENU_ENABLED, false);
        mSavedTouchDevice = (int) Apple2Preferences.getJSONPref(Apple2SettingsMenu.SETTINGS.CURRENT_INPUT);
        Apple2Preferences.setJSONPref(Apple2SettingsMenu.SETTINGS.CURRENT_INPUT, Apple2SettingsMenu.TouchDeviceVariant.KEYBOARD.ordinal());

        Apple2Preferences.setJSONPref(Apple2Preferences.PREF_DOMAIN_TOUCHSCREEN, Apple2Preferences.PREF_CALIBRATING, true);
        Apple2Preferences.sync(mActivity, Apple2Preferences.PREF_DOMAIN_TOUCHSCREEN);
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

    private String getNextChoiceString() {
        String choose = mActivity.getResources().getString(R.string.keypad_choose_current);
        return choose.replace("XXX", mChooserState.getKeyName(mActivity));
    }

    private enum STATE_MACHINE {
        CHOOSE_NORTHWEST {
            @Override
            public String getKeyName(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_key_axis_ul);
            }
        },
        CHOOSE_NORTH {
            @Override
            public String getKeyName(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_key_axis_up);
            }
        },
        CHOOSE_NORTHEAST {
            @Override
            public String getKeyName(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_key_axis_ur);
            }
        },
        CHOOSE_WEST {
            @Override
            public String getKeyName(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_key_axis_l);
            }
        },
        CHOOSE_CENTER {
            @Override
            public String getKeyName(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_key_axis_c);
            }
        },
        CHOOSE_EAST {
            @Override
            public String getKeyName(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_key_axis_r);
            }
        },
        CHOOSE_SOUTHWEST {
            @Override
            public String getKeyName(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_key_axis_dl);
            }
        },
        CHOOSE_SOUTH {
            @Override
            public String getKeyName(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_key_axis_dn);
            }
        },
        CHOOSE_SOUTHEAST {
            @Override
            public String getKeyName(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_key_axis_dr);
            }
        },
        CHOOSE_TAP {
            @Override
            public String getKeyName(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_key_button_tap);
            }
        },
        CHOOSE_SWIPEUP {
            @Override
            public String getKeyName(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_key_button_swipeup);
            }
        },
        CHOOSE_SWIPEDOWN {
            @Override
            public String getKeyName(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_key_button_swipedown);
            }
        };

        public static final int size = STATE_MACHINE.values().length;

        private static ArrayList<String> chars = new ArrayList<String>();
        private static ArrayList<String> scans = new ArrayList<String>();

        public void setKey(Apple2Activity activity, int ascii, int scancode) {
            int ord = ordinal();
            if (ord < CHOOSE_TAP.ordinal()) {
                chars.set(ord, "" + ascii);
                scans.set(ord, "" + scancode);
                Apple2KeypadSettingsMenu.KeypadPreset.saveRosettes(chars, scans);
            } else if (ord == CHOOSE_TAP.ordinal()) {
                Apple2KeypadSettingsMenu.KeypadPreset.saveTouchDownKey(ascii, scancode);
            } else if (ord == CHOOSE_SWIPEUP.ordinal()) {
                Apple2KeypadSettingsMenu.KeypadPreset.saveSwipeNorthKey(ascii, scancode);
            } else if (ord == CHOOSE_SWIPEDOWN.ordinal()) {
                Apple2KeypadSettingsMenu.KeypadPreset.saveSwipeSouthKey(ascii, scancode);
            } else {
                throw new RuntimeException();
            }
            Apple2Preferences.sync(activity, Apple2Preferences.PREF_DOMAIN_JOYSTICK);
        }

        public abstract String getKeyName(Apple2Activity activity);

        public void start(JSONArray jsonChars, JSONArray jsonScans) {
            int len = jsonChars.length();
            if (len != size) {
                throw new RuntimeException("jsonChars not expected length");
            }
            if (len != jsonScans.length()) {
                throw new RuntimeException("jsonScans not expected length");
            }
            try {
                for (int i = 0; i < len; i++) {
                    Apple2KeypadSettingsMenu.KeypadPreset.addRosetteKey(chars, scans, jsonChars.getInt(i), jsonScans.getInt(i));
                }
            } catch (JSONException e) {
                e.printStackTrace();
            }
        }

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
