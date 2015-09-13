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
import android.os.Handler;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import java.util.ArrayList;

import org.deadc0de.apple2ix.basic.R;

public class Apple2KeypadChooser implements Apple2MenuView {

    private final static String TAG = "Apple2KeypadChooser";

    private Apple2Activity mActivity = null;
    private View mSettingsView = null;
    private ArrayList<Apple2MenuView> mViewStack = null;
    private TextView mCurrentChoicePrompt = null;

    private String[] foo = null;

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

        String asciiStr = asciiRepresentation(ascii);
        Log.d(TAG, "ascii:'" + asciiStr + "' scancode:" + scancode);
        mChooserState.setValues(mActivity, ascii, scancode);
        Apple2Preferences.nativeSetCurrentTouchDevice(Apple2Preferences.TouchDeviceVariant.JOYSTICK_KEYPAD.ordinal());
        mCurrentChoicePrompt.setText(getNextChoiceString() + asciiStr);
        switch (mChooserState) {
            case CHOOSE_TAP:
                mActivity.nativeOnTouch(MotionEvent.ACTION_DOWN, 1, 0, new float[]{400.f}, new float[]{400.f});
                mActivity.nativeOnTouch(MotionEvent.ACTION_UP, 1, 0, new float[]{400.f}, new float[]{400.f});
                break;
            case CHOOSE_SWIPEDOWN:
                mActivity.nativeOnTouch(MotionEvent.ACTION_DOWN, 1, 0, new float[]{400.f}, new float[]{400.f});
                mActivity.nativeOnTouch(MotionEvent.ACTION_MOVE, 1, 0, new float[]{400.f}, new float[]{600.f});
                mActivity.nativeOnTouch(MotionEvent.ACTION_UP, 1, 0, new float[]{400.f}, new float[]{600.f});
                break;
            case CHOOSE_SWIPEUP:
                mActivity.nativeOnTouch(MotionEvent.ACTION_DOWN, 1, 0, new float[]{400.f}, new float[]{400.f});
                mActivity.nativeOnTouch(MotionEvent.ACTION_MOVE, 1, 0, new float[]{400.f}, new float[]{200.f});
                mActivity.nativeOnTouch(MotionEvent.ACTION_UP, 1, 0, new float[]{400.f}, new float[]{200.f});
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
                Apple2Preferences.nativeSetCurrentTouchDevice(Apple2Preferences.TouchDeviceVariant.KEYBOARD.ordinal());
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

        Apple2Preferences.nativeTouchDeviceEndCalibrationMode();
        Apple2Preferences.nativeSetTouchMenuEnabled(mTouchMenuEnabled);
        Apple2Preferences.nativeSetCurrentTouchDevice(mSavedTouchDevice);

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

        mCurrentChoicePrompt = (TextView) mSettingsView.findViewById(R.id.currentChoicePrompt);
        mCurrentChoicePrompt.setText(getNextChoiceString());

        Button skipButton = (Button) mSettingsView.findViewById(R.id.skipButton);
        skipButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Apple2KeypadChooser.this.onKeyTapCalibrationEvent((char)Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1);
            }
        });

        // temporarily undo these native touch settings while calibrating...
        mTouchMenuEnabled = Apple2Preferences.TOUCH_MENU_ENABLED.booleanValue(mActivity);
        Apple2Preferences.nativeSetTouchMenuEnabled(false);
        mSavedTouchDevice = Apple2Preferences.CURRENT_TOUCH_DEVICE.intValue(mActivity);
        Apple2Preferences.nativeSetCurrentTouchDevice(Apple2Preferences.TouchDeviceVariant.KEYBOARD.ordinal());

        Apple2Preferences.nativeTouchDeviceBeginCalibrationMode();
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

            @Override
            public void setValues(Apple2Activity activity, char ascii, int scancode) {
                Apple2Preferences.KEYPAD_NORTHWEST_KEY.saveChosenKey(activity, ascii, scancode);
            }
        },
        CHOOSE_NORTH {
            @Override
            public String getKeyName(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_key_axis_up);
            }

            @Override
            public void setValues(Apple2Activity activity, char ascii, int scancode) {
                Apple2Preferences.KEYPAD_NORTH_KEY.saveChosenKey(activity, ascii, scancode);
            }
        },
        CHOOSE_NORTHEAST {
            @Override
            public String getKeyName(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_key_axis_ur);
            }

            @Override
            public void setValues(Apple2Activity activity, char ascii, int scancode) {
                Apple2Preferences.KEYPAD_NORTHEAST_KEY.saveChosenKey(activity, ascii, scancode);
            }
        },
        CHOOSE_WEST {
            @Override
            public String getKeyName(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_key_axis_l);
            }

            @Override
            public void setValues(Apple2Activity activity, char ascii, int scancode) {
                Apple2Preferences.KEYPAD_WEST_KEY.saveChosenKey(activity, ascii, scancode);
            }
        },
        CHOOSE_CENTER {
            @Override
            public String getKeyName(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_key_axis_c);
            }

            @Override
            public void setValues(Apple2Activity activity, char ascii, int scancode) {
                Apple2Preferences.KEYPAD_CENTER_KEY.saveChosenKey(activity, ascii, scancode);
            }
        },
        CHOOSE_EAST {
            @Override
            public String getKeyName(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_key_axis_r);
            }

            @Override
            public void setValues(Apple2Activity activity, char ascii, int scancode) {
                Apple2Preferences.KEYPAD_EAST_KEY.saveChosenKey(activity, ascii, scancode);
            }
        },
        CHOOSE_SOUTHWEST {
            @Override
            public String getKeyName(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_key_axis_dl);
            }

            @Override
            public void setValues(Apple2Activity activity, char ascii, int scancode) {
                Apple2Preferences.KEYPAD_SOUTHWEST_KEY.saveChosenKey(activity, ascii, scancode);
            }
        },
        CHOOSE_SOUTH {
            @Override
            public String getKeyName(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_key_axis_dn);
            }

            @Override
            public void setValues(Apple2Activity activity, char ascii, int scancode) {
                Apple2Preferences.KEYPAD_SOUTH_KEY.saveChosenKey(activity, ascii, scancode);
            }
        },
        CHOOSE_SOUTHEAST {
            @Override
            public String getKeyName(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_key_axis_dr);
            }

            @Override
            public void setValues(Apple2Activity activity, char ascii, int scancode) {
                Apple2Preferences.KEYPAD_SOUTHEAST_KEY.saveChosenKey(activity, ascii, scancode);
            }
        },
        CHOOSE_TAP {
            @Override
            public String getKeyName(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_key_button_tap);
            }

            @Override
            public void setValues(Apple2Activity activity, char ascii, int scancode) {
                Apple2Preferences.KEYPAD_TAP_KEY.saveChosenKey(activity, ascii, scancode);
            }
        },
        CHOOSE_SWIPEUP {
            @Override
            public String getKeyName(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_key_button_swipeup);
            }

            @Override
            public void setValues(Apple2Activity activity, char ascii, int scancode) {
                Apple2Preferences.KEYPAD_SWIPEUP_KEY.saveChosenKey(activity, ascii, scancode);
            }
        },
        CHOOSE_SWIPEDOWN {
            @Override
            public String getKeyName(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_key_button_swipedown);
            }

            @Override
            public void setValues(Apple2Activity activity, char ascii, int scancode) {
                Apple2Preferences.KEYPAD_SWIPEDOWN_KEY.saveChosenKey(activity, ascii, scancode);
            }
        };

        public static final int size = STATE_MACHINE.values().length;

        public abstract void setValues(Apple2Activity activity, char ascii, int scancode);

        public abstract String getKeyName(Apple2Activity activity);

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
