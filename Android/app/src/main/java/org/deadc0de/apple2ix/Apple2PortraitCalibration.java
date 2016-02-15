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
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.SeekBar;
import android.widget.VerticalSeekBar;

import org.deadc0de.apple2ix.basic.R;

import java.util.ArrayList;

public class Apple2PortraitCalibration implements Apple2MenuView {

    public enum States {
        CALIBRATE_KEYBOARD_HEIGHT_SCALE(0),
        CALIBRATE_FRAMEBUFFER_POSITION_SCALE(1),
        CALIBRATE_KEYBOARD_POSITION_SCALE(2);
        private int val;

        public static final int size = States.values().length;

        States(int val) {
            this.val = val;
        }

        States next() {
            int ord = (val + 1) % size;
            return States.values()[ord];
        }
    }

    private final static String TAG = "Apple2PortraitCalibration";


    private Apple2Activity mActivity = null;
    private View mSettingsView = null;
    private ArrayList<Apple2MenuView> mViewStack = null;
    private boolean mTouchMenuEnabled = false;
    private int mSavedTouchDevice = Apple2Preferences.TouchDeviceVariant.NONE.ordinal();
    private States mStateMachine = States.CALIBRATE_KEYBOARD_HEIGHT_SCALE;

    public Apple2PortraitCalibration(Apple2Activity activity, ArrayList<Apple2MenuView> viewStack) {
        mActivity = activity;
        mViewStack = viewStack;

        LayoutInflater inflater = (LayoutInflater) mActivity.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        mSettingsView = inflater.inflate(R.layout.activity_calibrate_portrait, null, false);

        final Button calibrateButton = (Button) mSettingsView.findViewById(R.id.button_calibrate);
        final VerticalSeekBar vsb = (VerticalSeekBar) mSettingsView.findViewById(R.id.seekbar_vertical);

        vsb.setProgress(Apple2Preferences.PORTRAIT_KEYBOARD_HEIGHT_SCALE.intValue(mActivity));

        calibrateButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mStateMachine = mStateMachine.next();
                switch (mStateMachine) {
                    case CALIBRATE_KEYBOARD_HEIGHT_SCALE:
                        calibrateButton.setText(R.string.portrait_calibrate_keyboard_height);
                        vsb.setProgress(Apple2Preferences.PORTRAIT_KEYBOARD_HEIGHT_SCALE.intValue(mActivity));
                        break;
                    case CALIBRATE_FRAMEBUFFER_POSITION_SCALE:
                        calibrateButton.setText(R.string.portrait_calibrate_framebuffer);
                        vsb.setProgress(Apple2Preferences.PORTRAIT_FRAMEBUFFER_POSITION_SCALE.intValue(mActivity));
                        break;
                    case CALIBRATE_KEYBOARD_POSITION_SCALE:
                    default:
                        calibrateButton.setText(R.string.portrait_calibrate_keyboard_position);
                        vsb.setProgress(Apple2Preferences.PORTRAIT_KEYBOARD_POSITION_SCALE.intValue(mActivity));
                        break;
                }
            }
        });

        vsb.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                switch (mStateMachine) {
                    case CALIBRATE_KEYBOARD_HEIGHT_SCALE:
                        Apple2Preferences.PORTRAIT_KEYBOARD_HEIGHT_SCALE.saveInt(mActivity, progress);
                        break;
                    case CALIBRATE_FRAMEBUFFER_POSITION_SCALE:
                        Apple2Preferences.PORTRAIT_FRAMEBUFFER_POSITION_SCALE.saveInt(mActivity, progress);
                        break;
                    case CALIBRATE_KEYBOARD_POSITION_SCALE:
                    default:
                        Apple2Preferences.PORTRAIT_KEYBOARD_POSITION_SCALE.saveInt(mActivity, progress);
                        break;
                }
            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
            }
        });

        mTouchMenuEnabled = Apple2Preferences.TOUCH_MENU_ENABLED.booleanValue(mActivity);
        Apple2Preferences.nativeSetTouchMenuEnabled(false);
        mSavedTouchDevice = Apple2Preferences.CURRENT_TOUCH_DEVICE.intValue(mActivity);
        Apple2Preferences.nativeSetCurrentTouchDevice(Apple2Preferences.TouchDeviceVariant.KEYBOARD.ordinal());
        Apple2Preferences.nativeTouchDeviceBeginCalibrationMode();
    }

    public final boolean isCalibrating() {
        return true;
    }

    public void onKeyTapCalibrationEvent(char ascii, int scancode) {
        /* ... */
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
        return mSettingsView.getParent() != null;
    }

    public View getView() {
        return mSettingsView;
    }
}
