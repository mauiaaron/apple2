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

    public final static int JOYSTICK_DIVIDER_NUM_CHOICES = Apple2Preferences.DECENT_AMOUNT_OF_CHOICES;
    public final static String PREF_SCREEN_DIVISION = "screenDivider";

    private Apple2Activity mActivity = null;
    private View mSettingsView = null;
    private ArrayList<Apple2MenuView> mViewStack = null;
    private boolean mTouchMenuEnabled = false;
    private int mSavedTouchDevice = Apple2SettingsMenu.TouchDeviceVariant.NONE.ordinal();

    public Apple2JoystickCalibration(Apple2Activity activity, ArrayList<Apple2MenuView> viewStack, Apple2SettingsMenu.TouchDeviceVariant variant) {
        mActivity = activity;
        mViewStack = viewStack;
        if (!(variant == Apple2SettingsMenu.TouchDeviceVariant.JOYSTICK || variant == Apple2SettingsMenu.TouchDeviceVariant.JOYSTICK_KEYPAD)) {
            throw new RuntimeException("You're doing it wrong");
        }

        setup(variant);
    }

    private void setup(Apple2SettingsMenu.TouchDeviceVariant variant) {
        LayoutInflater inflater = (LayoutInflater) mActivity.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        mSettingsView = inflater.inflate(R.layout.activity_calibrate_joystick, null, false);

        SeekBar sb = (SeekBar) mSettingsView.findViewById(R.id.seekBar);
        sb.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                if (!fromUser) {
                    return;
                }
                Apple2Preferences.setJSONPref(Apple2Preferences.PREF_DOMAIN_JOYSTICK, PREF_SCREEN_DIVISION, (float) progress / JOYSTICK_DIVIDER_NUM_CHOICES);
                Apple2Preferences.sync(mActivity, Apple2Preferences.PREF_DOMAIN_JOYSTICK);
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
            }
        });

        sb.setMax(0); // http://stackoverflow.com/questions/10278467/seekbar-not-setting-actual-progress-setprogress-not-working-on-early-android
        sb.setMax(JOYSTICK_DIVIDER_NUM_CHOICES);
        float val = Apple2Preferences.getFloatJSONPref(Apple2Preferences.PREF_DOMAIN_JOYSTICK, PREF_SCREEN_DIVISION, (JOYSTICK_DIVIDER_NUM_CHOICES >> 1) / (float) JOYSTICK_DIVIDER_NUM_CHOICES);
        sb.setProgress((int) (val * JOYSTICK_DIVIDER_NUM_CHOICES));

        mTouchMenuEnabled = (boolean) Apple2Preferences.getJSONPref(Apple2KeyboardSettingsMenu.SETTINGS.TOUCH_MENU_ENABLED);
        Apple2Preferences.setJSONPref(Apple2KeyboardSettingsMenu.SETTINGS.TOUCH_MENU_ENABLED, false);
        mSavedTouchDevice = (int) Apple2Preferences.getJSONPref(Apple2SettingsMenu.SETTINGS.CURRENT_INPUT);
        Apple2Preferences.setJSONPref(Apple2SettingsMenu.SETTINGS.CURRENT_INPUT, variant.ordinal());

        Apple2Preferences.setJSONPref(Apple2Preferences.PREF_DOMAIN_TOUCHSCREEN, Apple2Preferences.PREF_CALIBRATING, true);
        Apple2Preferences.sync(mActivity, Apple2Preferences.PREF_DOMAIN_TOUCHSCREEN);
    }

    public final boolean isCalibrating() {
        return true;
    }

    public void onKeyTapCalibrationEvent(char ascii, int scancode) {
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
}
