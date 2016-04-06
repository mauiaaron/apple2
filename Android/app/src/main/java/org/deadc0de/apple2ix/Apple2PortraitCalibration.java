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

    public enum States implements Apple2AbstractMenu.IMenuEnum {
        CALIBRATE_KEYBOARD_HEIGHT_SCALE {
            @Override
            public String getPrefKey() {
                return "portraitHeightScale";
            }

            @Override
            public Object getPrefDefault() {
                return (float) (PORTRAIT_CALIBRATE_NUM_CHOICES >> 1) / PORTRAIT_CALIBRATE_NUM_CHOICES;
            }
        },
        CALIBRATE_FRAMEBUFFER_POSITION_SCALE {
            @Override
            public String getPrefDomain() {
                return Apple2Preferences.PREF_DOMAIN_VIDEO;
            }

            @Override
            public String getPrefKey() {
                return "portraitPositionScale";
            }

            @Override
            public Object getPrefDefault() {
                return 3.f / 4;
            }
        },
        CALIBRATE_KEYBOARD_POSITION_SCALE {
            @Override
            public String getPrefKey() {
                return "portraitPositionScale";
            }

            @Override
            public Object getPrefDefault() {
                return 0.f;
            }
        };

        public static final int size = States.values().length;

        States next() {
            int ord = (ordinal() + 1) % size;
            return States.values()[ord];
        }

        @Override
        public final String getTitle(Apple2Activity activity) {
            throw new RuntimeException();
        }

        @Override
        public final String getSummary(Apple2Activity activity) {
            throw new RuntimeException();
        }

        @Override
        public String getPrefDomain() {
            return Apple2Preferences.PREF_DOMAIN_KEYBOARD;
        }

        @Override
        public View getView(final Apple2Activity activity, View convertView) {
            throw new RuntimeException();
        }

        @Override
        public void handleSelection(final Apple2Activity activity, final Apple2AbstractMenu settingsMenu, boolean isChecked) {
            throw new RuntimeException();
        }
    }

    private final static String TAG = "Apple2PortraitCalibration";

    public final static int PORTRAIT_CALIBRATE_NUM_CHOICES = 100;

    private Apple2Activity mActivity = null;
    private View mSettingsView = null;
    private ArrayList<Apple2MenuView> mViewStack = null;
    private boolean mTouchMenuEnabled = false;
    private int mSavedTouchDevice = Apple2SettingsMenu.TouchDeviceVariant.NONE.ordinal();
    private States mStateMachine = States.CALIBRATE_KEYBOARD_HEIGHT_SCALE;

    public Apple2PortraitCalibration(Apple2Activity activity, ArrayList<Apple2MenuView> viewStack) {
        mActivity = activity;
        mViewStack = viewStack;

        LayoutInflater inflater = (LayoutInflater) mActivity.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        mSettingsView = inflater.inflate(R.layout.activity_calibrate_portrait, null, false);

        final Button calibrateButton = (Button) mSettingsView.findViewById(R.id.button_calibrate);
        final VerticalSeekBar vsb = (VerticalSeekBar) mSettingsView.findViewById(R.id.seekbar_vertical);

        final int firstProgress = Math.round(Apple2Preferences.getFloatJSONPref(States.CALIBRATE_KEYBOARD_HEIGHT_SCALE) * PORTRAIT_CALIBRATE_NUM_CHOICES);
        vsb.setProgress(firstProgress);

        calibrateButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mStateMachine = mStateMachine.next();
                switch (mStateMachine) {
                    case CALIBRATE_KEYBOARD_HEIGHT_SCALE:
                        calibrateButton.setText(R.string.portrait_calibrate_keyboard_height);
                        vsb.setProgress(Math.round(Apple2Preferences.getFloatJSONPref(States.CALIBRATE_KEYBOARD_HEIGHT_SCALE) * PORTRAIT_CALIBRATE_NUM_CHOICES));
                        break;
                    case CALIBRATE_FRAMEBUFFER_POSITION_SCALE:
                        calibrateButton.setText(R.string.portrait_calibrate_framebuffer);
                        vsb.setProgress(Math.round(Apple2Preferences.getFloatJSONPref(States.CALIBRATE_FRAMEBUFFER_POSITION_SCALE) * PORTRAIT_CALIBRATE_NUM_CHOICES));
                        break;
                    case CALIBRATE_KEYBOARD_POSITION_SCALE:
                    default:
                        calibrateButton.setText(R.string.portrait_calibrate_keyboard_position);
                        vsb.setProgress(Math.round(Apple2Preferences.getFloatJSONPref(States.CALIBRATE_KEYBOARD_POSITION_SCALE) * PORTRAIT_CALIBRATE_NUM_CHOICES));
                        break;
                }
            }
        });

        vsb.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                switch (mStateMachine) {
                    case CALIBRATE_KEYBOARD_HEIGHT_SCALE:
                        Apple2Preferences.setJSONPref(States.CALIBRATE_KEYBOARD_HEIGHT_SCALE, (float) progress / PORTRAIT_CALIBRATE_NUM_CHOICES);
                        break;
                    case CALIBRATE_FRAMEBUFFER_POSITION_SCALE:
                        Apple2Preferences.setJSONPref(States.CALIBRATE_FRAMEBUFFER_POSITION_SCALE, (float) progress / PORTRAIT_CALIBRATE_NUM_CHOICES);
                        break;
                    case CALIBRATE_KEYBOARD_POSITION_SCALE:
                    default:
                        Apple2Preferences.setJSONPref(States.CALIBRATE_KEYBOARD_POSITION_SCALE, (float) progress / PORTRAIT_CALIBRATE_NUM_CHOICES);
                        break;
                }
                Apple2Preferences.sync(mActivity, mStateMachine.getPrefDomain());
            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
            }
        });

        mTouchMenuEnabled = (boolean) Apple2Preferences.getJSONPref(Apple2KeyboardSettingsMenu.SETTINGS.TOUCH_MENU_ENABLED);
        Apple2Preferences.setJSONPref(Apple2KeyboardSettingsMenu.SETTINGS.TOUCH_MENU_ENABLED, false);
        mSavedTouchDevice = (int) Apple2Preferences.getJSONPref(Apple2SettingsMenu.SETTINGS.CURRENT_INPUT);
        Apple2Preferences.setJSONPref(Apple2SettingsMenu.SETTINGS.CURRENT_INPUT, Apple2SettingsMenu.TouchDeviceVariant.KEYBOARD.ordinal());

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
