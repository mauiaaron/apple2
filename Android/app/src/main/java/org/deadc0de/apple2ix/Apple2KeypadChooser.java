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
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import java.util.ArrayList;

import org.deadc0de.apple2ix.basic.R;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

public class Apple2KeypadChooser implements Apple2MenuView {

    private final static String TAG = "Apple2KeypadChooser";

    private Apple2Activity mActivity = null;
    private View mSettingsView = null;
    private ArrayList<Apple2MenuView> mViewStack = null;
    private TextView mCurrentChoicePrompt = null;

    private STATE_MACHINE mChooserState = STATE_MACHINE.CHOOSE_AXIS_NORTHWEST;

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

    public static boolean isShiftedKey(char ascii) {
        switch (ascii) {
            case '~':
            case '!':
            case '@':
            case '#':
            case '$':
            case '%':
            case '^':
            case '&':
            case '*':
            case '(':
            case ')':
            case '_':
            case '+':
            case '{':
            case '}':
            case '|':
            case ':':
            case '"':
            case '<':
            case '>':
            case '?':
                return true;

            default:
                return false;
        }
    }

    public void onKeyTapCalibrationEvent(char ascii, int scancode) {
        if (ascii == Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION) {
            scancode = -1;
        }
        if (scancode == 0) {
            return;
        }

        String asciiStr = asciiRepresentation(mActivity, ascii);
        Log.d(TAG, "ascii:'" + asciiStr + "' scancode:" + scancode);
        if (ascii == ' ') {
            ascii = Apple2KeyboardSettingsMenu.ICONTEXT_VISUAL_SPACE;
        }
        Apple2KeypadSettingsMenu.KeyTuple tuple = new Apple2KeypadSettingsMenu.KeyTuple((long) ascii, (long) scancode, isShiftedKey(ascii));
        mChooserState.setKey(mActivity, tuple);
        Apple2Preferences.setJSONPref(Apple2SettingsMenu.SETTINGS.CURRENT_INPUT, Apple2SettingsMenu.TouchDeviceVariant.JOYSTICK_KEYPAD.ordinal());
        Apple2Preferences.sync(mActivity, Apple2Preferences.PREF_DOMAIN_TOUCHSCREEN);

        mCurrentChoicePrompt.setText(getNextChoiceString() + asciiStr);

        calibrationContinue();
    }

    private void calibrationContinue() {
        final Handler handler = new Handler();
        handler.postDelayed(new Runnable() {
            @Override
            public void run() {
                mChooserState = mChooserState.next();
                if (mChooserState.ordinal() == 0) {
                    dismiss();
                } else {
                    mCurrentChoicePrompt.setText(getNextChoiceString());
                    Apple2Preferences.setJSONPref(Apple2SettingsMenu.SETTINGS.CURRENT_INPUT, Apple2SettingsMenu.TouchDeviceVariant.KEYBOARD.ordinal());
                    Apple2Preferences.sync(mActivity, Apple2Preferences.PREF_DOMAIN_TOUCHSCREEN);
                }
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
        mChooserState.start();

        LayoutInflater inflater = (LayoutInflater) mActivity.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        mSettingsView = inflater.inflate(R.layout.activity_chooser_keypad, null, false);

        mCurrentChoicePrompt = (TextView) mSettingsView.findViewById(R.id.currentChoicePrompt);
        mCurrentChoicePrompt.setText(getNextChoiceString());

        Button skipButton = (Button) mSettingsView.findViewById(R.id.skipButton);
        skipButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Apple2Preferences.setJSONPref(Apple2SettingsMenu.SETTINGS.CURRENT_INPUT, Apple2SettingsMenu.TouchDeviceVariant.JOYSTICK_KEYPAD.ordinal());
                Apple2Preferences.sync(mActivity, Apple2Preferences.PREF_DOMAIN_TOUCHSCREEN);
                calibrationContinue();
            }
        });

        Button noneButton = (Button) mSettingsView.findViewById(R.id.noneButton);
        noneButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                onKeyTapCalibrationEvent((char) Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1);
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

    public static String asciiRepresentation(Apple2Activity activity, char ascii) {
        switch (ascii) {
            case Apple2KeyboardSettingsMenu.MOUSETEXT_OPENAPPLE:
                return activity.getResources().getString(R.string.key_open_apple);
            case Apple2KeyboardSettingsMenu.MOUSETEXT_CLOSEDAPPLE:
                return activity.getResources().getString(R.string.key_closed_apple);
            case Apple2KeyboardSettingsMenu.kUP:
            case Apple2KeyboardSettingsMenu.MOUSETEXT_UP:
                return activity.getResources().getString(R.string.key_up);
            case Apple2KeyboardSettingsMenu.kLT:
            case Apple2KeyboardSettingsMenu.MOUSETEXT_LEFT:
                return activity.getResources().getString(R.string.key_left);
            case Apple2KeyboardSettingsMenu.kRT:
            case Apple2KeyboardSettingsMenu.MOUSETEXT_RIGHT:
                return activity.getResources().getString(R.string.key_right);
            case Apple2KeyboardSettingsMenu.kDN:
            case Apple2KeyboardSettingsMenu.MOUSETEXT_DOWN:
                return activity.getResources().getString(R.string.key_down);
            case Apple2KeyboardSettingsMenu.ICONTEXT_CTRL:
                return activity.getResources().getString(R.string.key_ctrl);
            case Apple2KeyboardSettingsMenu.kESC:
            case Apple2KeyboardSettingsMenu.ICONTEXT_ESC:
                return activity.getResources().getString(R.string.key_esc);
            case Apple2KeyboardSettingsMenu.kRET:
            case Apple2KeyboardSettingsMenu.ICONTEXT_RETURN:
                return activity.getResources().getString(R.string.key_ret);
            case Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION:
                return activity.getResources().getString(R.string.key_none);
            case ' ':
            case Apple2KeyboardSettingsMenu.ICONTEXT_VISUAL_SPACE:
                return activity.getResources().getString(R.string.key_space);
            case Apple2KeyboardSettingsMenu.kDEL:
                return activity.getResources().getString(R.string.key_del);
            case Apple2KeyboardSettingsMenu.kTAB:
                return activity.getResources().getString(R.string.key_tab);
            default:
                return "" + ascii;
        }
    }

    private String getNextChoiceString() {
        String choose = mActivity.getResources().getString(R.string.keypad_choose_current);
        return choose.replace("XXX", mChooserState.getKeyName(mActivity));
    }

    private enum STATE_MACHINE {
        CHOOSE_AXIS_NORTHWEST {
            @Override
            public String getKeyName(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_key_axis_ul);
            }
        },
        CHOOSE_AXIS_NORTH {
            @Override
            public String getKeyName(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_key_axis_up);
            }
        },
        CHOOSE_AXIS_NORTHEAST {
            @Override
            public String getKeyName(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_key_axis_ur);
            }
        },
        CHOOSE_AXIS_WEST {
            @Override
            public String getKeyName(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_key_axis_l);
            }
        },
        CHOOSE_AXIS_CENTER {
            @Override
            public String getKeyName(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_key_axis_c);
            }
        },
        CHOOSE_AXIS_EAST {
            @Override
            public String getKeyName(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_key_axis_r);
            }
        },
        CHOOSE_AXIS_SOUTHWEST {
            @Override
            public String getKeyName(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_key_axis_dl);
            }
        },
        CHOOSE_AXIS_SOUTH {
            @Override
            public String getKeyName(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_key_axis_dn);
            }
        },
        CHOOSE_AXIS_SOUTHEAST {
            @Override
            public String getKeyName(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_key_axis_dr);
            }
        },
        CHOOSE_BUTT_NORTHWEST {
            @Override
            public String getKeyName(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_key_axis_ul);
            }
        },
        CHOOSE_BUTT_NORTH {
            @Override
            public String getKeyName(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_key_axis_up);
            }
        },
        CHOOSE_BUTT_NORTHEAST {
            @Override
            public String getKeyName(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_key_axis_ur);
            }
        },
        CHOOSE_BUTT_WEST {
            @Override
            public String getKeyName(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_key_axis_l);
            }
        },
        CHOOSE_BUTT_CENTER {
            @Override
            public String getKeyName(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_key_axis_c);
            }
        },
        CHOOSE_BUTT_EAST {
            @Override
            public String getKeyName(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_key_axis_r);
            }
        },
        CHOOSE_BUTT_SOUTHWEST {
            @Override
            public String getKeyName(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_key_axis_dl);
            }
        },
        CHOOSE_BUTT_SOUTH {
            @Override
            public String getKeyName(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_key_axis_dn);
            }
        },
        CHOOSE_BUTT_SOUTHEAST {
            @Override
            public String getKeyName(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_key_axis_dr);
            }
        };

        public static final int size = STATE_MACHINE.values().length;

        private static ArrayList<Apple2KeypadSettingsMenu.KeyTuple> axisRosette = new ArrayList<Apple2KeypadSettingsMenu.KeyTuple>();
        private static ArrayList<Apple2KeypadSettingsMenu.KeyTuple> buttRosette = new ArrayList<Apple2KeypadSettingsMenu.KeyTuple>();

        public void setKey(Apple2Activity activity, Apple2KeypadSettingsMenu.KeyTuple tuple) {
            int ord = ordinal();
            int buttbegin = CHOOSE_BUTT_NORTHWEST.ordinal();
            if (ord < buttbegin) {
                axisRosette.set(ord, tuple);
                Apple2KeypadSettingsMenu.KeypadPreset.saveAxisRosette(axisRosette);
            } else {
                ord -= buttbegin;
                buttRosette.set(ord, tuple);
                Apple2KeypadSettingsMenu.KeypadPreset.saveButtRosette(buttRosette);
            }
            Apple2Preferences.sync(activity, Apple2Preferences.PREF_DOMAIN_JOYSTICK);
        }

        public abstract String getKeyName(Apple2Activity activity);

        public void start() {
            setupCharsAndScans(axisRosette, Apple2KeypadSettingsMenu.PREF_KPAD_AXIS_ROSETTE);

            setupCharsAndScans(buttRosette, Apple2KeypadSettingsMenu.PREF_KPAD_BUTT_ROSETTE);
        }

        private void setupCharsAndScans(final ArrayList<Apple2KeypadSettingsMenu.KeyTuple> rosette, final String pref) {
            rosette.clear();

            try {
                JSONArray jsonArray = (JSONArray) Apple2Preferences.getJSONPref(Apple2Preferences.PREF_DOMAIN_JOYSTICK, pref, null);

                if (jsonArray == null) {
                    jsonArray = new JSONArray();
                    for (int i = 0; i < Apple2KeypadSettingsMenu.ROSETTE_SIZE; i++) {
                        JSONObject map = new JSONObject();
                        map.put("ch", (long) Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION);
                        map.put("scan", -1L);
                        map.put("isShifted", false);
                    }
                }

                int len = jsonArray.length();
                if (len != Apple2KeypadSettingsMenu.ROSETTE_SIZE) {
                    throw new RuntimeException("rosette not expected length");
                }

                for (int i = 0; i < len; i++) {
                    JSONObject obj = jsonArray.getJSONObject(i);
                    long ch = obj.getLong("ch");
                    long scan = obj.getLong("scan");
                    boolean isShifted = obj.getBoolean("isShifted");
                    rosette.add(new Apple2KeypadSettingsMenu.KeyTuple(ch, scan, isShifted));
                }
            } catch (JSONException e) {
                e.printStackTrace();
            }

            if (rosette.size() != Apple2KeypadSettingsMenu.ROSETTE_SIZE) {
                throw new RuntimeException("rosette chars is not correct size");
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
