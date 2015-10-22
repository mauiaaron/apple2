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
import android.widget.SeekBar;

import org.deadc0de.apple2ix.basic.R;

import java.util.ArrayList;

public class Apple2JoystickCalibration implements Apple2MenuView {

    private final static String TAG = "Apple2JoystickCalibration";

    private Apple2Activity mActivity = null;
    private View mSettingsView = null;
    private ArrayList<Apple2MenuView> mViewStack = null;
    private boolean mTouchMenuEnabled = false;
    private int mSavedTouchDevice = Apple2Preferences.TouchDeviceVariant.NONE.ordinal();

    public Apple2JoystickCalibration(Apple2Activity activity, ArrayList<Apple2MenuView> viewStack, Apple2Preferences.TouchDeviceVariant variant) {
        mActivity = activity;
        mViewStack = viewStack;
        if (!(variant == Apple2Preferences.TouchDeviceVariant.JOYSTICK || variant == Apple2Preferences.TouchDeviceVariant.JOYSTICK_KEYPAD)) {
            throw new RuntimeException("You're doing it wrong");
        }

        setup(variant);
    }

    private void setup(Apple2Preferences.TouchDeviceVariant variant) {
        LayoutInflater inflater = (LayoutInflater) mActivity.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        mSettingsView = inflater.inflate(R.layout.activity_calibrate_joystick, null, false);

        SeekBar sb = (SeekBar) mSettingsView.findViewById(R.id.seekBar);
        sb.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                if (!fromUser) {
                    return;
                }
                Apple2Preferences.JOYSTICK_DIVIDER.saveInt(mActivity, progress);
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
            }
        });

        sb.setMax(0); // http://stackoverflow.com/questions/10278467/seekbar-not-setting-actual-progress-setprogress-not-working-on-early-android
        sb.setMax(Apple2Preferences.JOYSTICK_DIVIDER_NUM_CHOICES);
        sb.setProgress(Apple2Preferences.JOYSTICK_DIVIDER.intValue(mActivity));

        mTouchMenuEnabled = Apple2Preferences.TOUCH_MENU_ENABLED.booleanValue(mActivity);
        Apple2Preferences.nativeSetTouchMenuEnabled(false);
        mSavedTouchDevice = Apple2Preferences.CURRENT_TOUCH_DEVICE.intValue(mActivity);
        Apple2Preferences.nativeSetCurrentTouchDevice(variant.ordinal());
        if (variant == Apple2Preferences.TouchDeviceVariant.JOYSTICK) {
            Apple2Preferences.loadAllJoystickButtons(mActivity);
        } else {
            Apple2Preferences.loadAllKeypadKeys(mActivity);
        }

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
        return mSettingsView.isShown();
    }

    public View getView() {
        return mSettingsView;
    }
}
