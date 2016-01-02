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

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.util.Log;

import java.io.File;

import org.deadc0de.apple2ix.basic.BuildConfig;
import org.deadc0de.apple2ix.basic.R;

public enum Apple2Preferences {
    EMULATOR_VERSION {
        @Override
        public void load(Apple2Activity activity) {
            /* ... */
        }

        @Override
        public void saveInt(Apple2Activity activity, int version) {
            activity.getPreferences(Context.MODE_PRIVATE).edit().putInt(toString(), version).apply();
        }
    },
    CURRENT_DISK_PATH {
        @Override
        public void load(final Apple2Activity activity) {
            activity.getDisksMenu().setPathStackJSON(stringValue(activity));
        }

        @Override
        public String stringValue(Apple2Activity activity) {
            return activity.getPreferences(Context.MODE_PRIVATE).getString(toString(), "[]");
        }

        @Override
        public void saveString(Apple2Activity activity, String value) {
            activity.getPreferences(Context.MODE_PRIVATE).edit().putString(toString(), value).apply();
            //load(activity);
        }
    },
    CURRENT_DRIVE_A_BUTTON {
        @Override
        public void load(final Apple2Activity activity) {
            /* ... */
        }

        @Override
        public boolean booleanValue(Apple2Activity activity) {
            return activity.getPreferences(Context.MODE_PRIVATE).getBoolean(toString(), true);
        }

        @Override
        public void saveBoolean(Apple2Activity activity, boolean value) {
            activity.getPreferences(Context.MODE_PRIVATE).edit().putBoolean(toString(), value).apply();
            //load(activity);
        }
    },
    CURRENT_DISK_RO_BUTTON {
        @Override
        public void load(final Apple2Activity activity) {
            /* ... */
        }

        @Override
        public boolean booleanValue(Apple2Activity activity) {
            return activity.getPreferences(Context.MODE_PRIVATE).getBoolean(toString(), true);
        }

        @Override
        public void saveBoolean(Apple2Activity activity, boolean value) {
            activity.getPreferences(Context.MODE_PRIVATE).edit().putBoolean(toString(), value).apply();
            //load(activity);
        }
    },
    CURRENT_DISK_A {
        @Override
        public void load(final Apple2Activity activity) {
            insertDisk(activity, stringValue(activity), /*driveA:*/true, /*readOnly:*/CURRENT_DISK_A_RO.booleanValue(activity));
        }

        @Override
        public String stringValue(Apple2Activity activity) {
            return activity.getPreferences(Context.MODE_PRIVATE).getString(toString(), "");
        }

        @Override
        public void saveString(Apple2Activity activity, String str) {
            activity.getPreferences(Context.MODE_PRIVATE).edit().putString(toString(), str).apply();
            load(activity);
        }

        @Override
        public void setPath(Apple2Activity activity, String str) {
            activity.getPreferences(Context.MODE_PRIVATE).edit().putString(toString(), str).apply();
        }
    },
    CURRENT_DISK_A_RO {
        @Override
        public void load(final Apple2Activity activity) {
            /* ... */
        }

        @Override
        public boolean booleanValue(Apple2Activity activity) {
            return activity.getPreferences(Context.MODE_PRIVATE).getBoolean(toString(), true);
        }

        @Override
        public void saveBoolean(Apple2Activity activity, boolean value) {
            activity.getPreferences(Context.MODE_PRIVATE).edit().putBoolean(toString(), value).apply();
            //load(activity);
        }
    },
    CURRENT_DISK_B {
        @Override
        public void load(final Apple2Activity activity) {
            insertDisk(activity, stringValue(activity), /*driveA:*/false, /*readOnly:*/CURRENT_DISK_B_RO.booleanValue(activity));
        }

        @Override
        public String stringValue(Apple2Activity activity) {
            return activity.getPreferences(Context.MODE_PRIVATE).getString(toString(), "");
        }

        @Override
        public void saveString(Apple2Activity activity, String str) {
            activity.getPreferences(Context.MODE_PRIVATE).edit().putString(toString(), str).apply();
            load(activity);
        }

        @Override
        public void setPath(Apple2Activity activity, String str) {
            activity.getPreferences(Context.MODE_PRIVATE).edit().putString(toString(), str).apply();
        }
    },
    CURRENT_DISK_B_RO {
        @Override
        public void load(final Apple2Activity activity) {
            /* ... */
        }

        @Override
        public boolean booleanValue(Apple2Activity activity) {
            return activity.getPreferences(Context.MODE_PRIVATE).getBoolean(toString(), true);
        }

        @Override
        public void saveBoolean(Apple2Activity activity, boolean value) {
            activity.getPreferences(Context.MODE_PRIVATE).edit().putBoolean(toString(), value).apply();
            //load(activity);
        }
    },
    CPU_SPEED_PERCENT {
        @Override
        public void load(Apple2Activity activity) {
            nativeSetCPUSpeed(intValue(activity));
        }

        @Override
        public int intValue(Apple2Activity activity) {
            return activity.getPreferences(Context.MODE_PRIVATE).getInt(toString(), 100);
        }
    },
    HIRES_COLOR {
        @Override
        public void load(Apple2Activity activity) {
            nativeSetColor(intValue(activity));
        }

        @Override
        public int intValue(Apple2Activity activity) {
            return activity.getPreferences(Context.MODE_PRIVATE).getInt(toString(), HiresColor.INTERPOLATED.ordinal());
        }
    },
    SPEAKER_VOLUME {
        @Override
        public void load(Apple2Activity activity) {
            nativeSetSpeakerVolume(intValue(activity));
        }

        @Override
        public int intValue(Apple2Activity activity) {
            return activity.getPreferences(Context.MODE_PRIVATE).getInt(toString(), Volume.MEDIUM.ordinal());
        }
    },
    MOCKINGBOARD_ENABLED {
        @Override
        public void load(Apple2Activity activity) {
            boolean enabled = booleanValue(activity);
            boolean result = nativeSetMockingboardEnabled(enabled);
            if (enabled && !result) {
                warnError(activity, R.string.mockingboard_disabled_title, R.string.mockingboard_disabled_mesg);
                activity.getPreferences(Context.MODE_PRIVATE).edit().putBoolean(toString(), false).apply();
            }
        }
    },
    MOCKINGBOARD_VOLUME {
        @Override
        public void load(Apple2Activity activity) {
            nativeSetMockingboardVolume(intValue(activity));
        }

        @Override
        public int intValue(Apple2Activity activity) {
            return activity.getPreferences(Context.MODE_PRIVATE).getInt(toString(), Volume.MEDIUM.ordinal());
        }
    },
    AUDIO_LATENCY {
        @Override
        public void load(Apple2Activity activity) {
            int tick = intValue(activity);
            nativeSetAudioLatency(((float) tick / AUDIO_LATENCY_NUM_CHOICES));
        }

        @Override
        public int intValue(Apple2Activity activity) {

            int defaultLatency = 0;
            int sampleRateCanary = DevicePropertyCalculator.getRecommendedSampleRate(activity);
            if (sampleRateCanary == DevicePropertyCalculator.defaultSampleRate) {
                // quite possibly an audio-challenged device
                defaultLatency = 8; // /AUDIO_LATENCY_NUM_CHOICES ->  0.4f
            } else {
                // reasonable default for high-end devices
                defaultLatency = 5; // /AUDIO_LATENCY_NUM_CHOICES -> 0.25f
            }

            return activity.getPreferences(Context.MODE_PRIVATE).getInt(toString(), defaultLatency);
        }
    },
    CURRENT_TOUCH_DEVICE {
        @Override
        public void load(Apple2Activity activity) {
            int intVariant = intValue(activity);
            nativeSetCurrentTouchDevice(intVariant);
            TouchDeviceVariant variant = TouchDeviceVariant.values()[intVariant];
            switch (variant) {
                case JOYSTICK:
                    loadAllJoystickButtons(activity);
                    break;
                case JOYSTICK_KEYPAD:
                    loadAllKeypadKeys(activity);
                    break;
                case KEYBOARD:
                    break;
                default:
                    break;
            }
        }

        @Override
        public int intValue(Apple2Activity activity) {
            return activity.getPreferences(Context.MODE_PRIVATE).getInt(toString(), TouchDeviceVariant.KEYBOARD.ordinal());
        }
    },
    TOUCH_MENU_ENABLED {
        @Override
        public void load(Apple2Activity activity) {
            boolean enabled = booleanValue(activity);
            nativeSetTouchMenuEnabled(enabled);
        }

        @Override
        public boolean booleanValue(Apple2Activity activity) {
            return activity.getPreferences(Context.MODE_PRIVATE).getBoolean(toString(), true);
        }
    },
    SHOW_DISK_OPERATIONS {
        @Override
        public void load(Apple2Activity activity) {
            boolean enabled = booleanValue(activity);
            nativeSetShowDiskOperationAnimation(enabled);
        }

        @Override
        public boolean booleanValue(Apple2Activity activity) {
            return activity.getPreferences(Context.MODE_PRIVATE).getBoolean(toString(), true);
        }
    },
    JOYSTICK_AXIS_SENSITIVIY {
        @Override
        public void load(Apple2Activity activity) {
            float sensitivity = floatValue(activity);
            nativeSetTouchJoystickAxisSensitivity(sensitivity);
        }

        @Override
        public int intValue(Apple2Activity activity) {
            final int pivot = JOYSTICK_AXIS_SENSITIVITY_DEC_NUMCHOICES;
            return activity.getPreferences(Context.MODE_PRIVATE).getInt(toString(), pivot);
        }

        @Override
        public float floatValue(Apple2Activity activity) {
            int tick = intValue(activity);
            final int pivot = JOYSTICK_AXIS_SENSITIVITY_DEC_NUMCHOICES;
            float sensitivity = 1.f;
            if (tick < pivot) {
                int decAmount = (pivot - tick);
                sensitivity -= (JOYSTICK_AXIS_SENSITIVITY_DEC_STEP * decAmount);
            } else if (tick > pivot) {
                int incAmount = (tick - pivot);
                sensitivity += (JOYSTICK_AXIS_SENSITIVITY_INC_STEP * incAmount);
            }
            return sensitivity;
        }
    },
    JOYSTICK_BUTTON_THRESHOLD {
        @Override
        public void load(Apple2Activity activity) {
            int tick = intValue(activity);
            nativeSetTouchJoystickButtonSwitchThreshold(tick * JOYSTICK_BUTTON_THRESHOLD_STEP);
        }

        @Override
        public int intValue(Apple2Activity activity) {
            return activity.getPreferences(Context.MODE_PRIVATE).getInt(toString(), 5);
        }
    },
    JOYSTICK_TAP_BUTTON {
        @Override
        public void load(Apple2Activity activity) {
            loadAllJoystickButtons(activity);
        }

        @Override
        public int intValue(Apple2Activity activity) {
            return activity.getPreferences(Context.MODE_PRIVATE).getInt(toString(), TouchJoystickButtons.BUTTON1.ordinal());
        }
    },
    JOYSTICK_TAPDELAY {
        @Override
        public void load(Apple2Activity activity) {
            int tick = intValue(activity);
            nativeSetTouchJoystickTapDelay(((float) tick / TAPDELAY_NUM_CHOICES) * TAPDELAY_SCALE);
        }

        @Override
        public int intValue(Apple2Activity activity) {
            int defaultLatency = 3; // /TAPDELAY_NUM_CHOICES * TAPDELAY_SCALE -> 0.075f
            return activity.getPreferences(Context.MODE_PRIVATE).getInt(toString(), defaultLatency);
        }
    },
    JOYSTICK_SWIPEUP_BUTTON {
        @Override
        public void load(Apple2Activity activity) {
            loadAllJoystickButtons(activity);
        }

        @Override
        public int intValue(Apple2Activity activity) {
            return activity.getPreferences(Context.MODE_PRIVATE).getInt(toString(), TouchJoystickButtons.BOTH.ordinal());
        }
    },
    JOYSTICK_SWIPEDOWN_BUTTON {
        @Override
        public void load(Apple2Activity activity) {
            loadAllJoystickButtons(activity);
        }

        @Override
        public int intValue(Apple2Activity activity) {
            return activity.getPreferences(Context.MODE_PRIVATE).getInt(toString(), TouchJoystickButtons.BUTTON2.ordinal());
        }
    },
    JOYSTICK_AXIS_ON_LEFT {
        @Override
        public void load(Apple2Activity activity) {
            nativeTouchJoystickSetAxisOnLeft(booleanValue(activity));
        }

        @Override
        public boolean booleanValue(Apple2Activity activity) {
            return activity.getPreferences(Context.MODE_PRIVATE).getBoolean(toString(), true);
        }
    },
    JOYSTICK_DIVIDER {
        @Override
        public void load(Apple2Activity activity) {
            int tick = intValue(activity);
            nativeTouchJoystickSetScreenDivision(((float) tick / JOYSTICK_DIVIDER_NUM_CHOICES));
        }

        @Override
        public int intValue(Apple2Activity activity) {
            return activity.getPreferences(Context.MODE_PRIVATE).getInt(toString(), JOYSTICK_DIVIDER_NUM_CHOICES >> 1);
        }
    },
    JOYSTICK_VISIBILITY {
        @Override
        public void load(Apple2Activity activity) {
            nativeSetTouchJoystickVisibility(booleanValue(activity));
        }

        @Override
        public boolean booleanValue(Apple2Activity activity) {
            return activity.getPreferences(Context.MODE_PRIVATE).getBoolean(toString(), true);
        }
    },
    KEYPAD_KEYS {
        @Override
        public void load(Apple2Activity activity) {
            /* ... */
        }

        @Override
        public int intValue(Apple2Activity activity) {
            return activity.getPreferences(Context.MODE_PRIVATE).getInt(toString(), KeypadPreset.IJKM_SPACE.ordinal() + 1);
        }
    },
    KEYPAD_NORTHWEST_KEY {
        @Override
        public void load(Apple2Activity activity) {
            loadAllKeypadKeys(activity);
        }
    },
    KEYPAD_NORTH_KEY {
        @Override
        public void load(Apple2Activity activity) {
            loadAllKeypadKeys(activity);
        }
    },
    KEYPAD_NORTHEAST_KEY {
        @Override
        public void load(Apple2Activity activity) {
            loadAllKeypadKeys(activity);
        }
    },
    KEYPAD_WEST_KEY {
        @Override
        public void load(Apple2Activity activity) {
            loadAllKeypadKeys(activity);
        }
    },
    KEYPAD_CENTER_KEY {
        @Override
        public void load(Apple2Activity activity) {
            loadAllKeypadKeys(activity);
        }
    },
    KEYPAD_EAST_KEY {
        @Override
        public void load(Apple2Activity activity) {
            loadAllKeypadKeys(activity);
        }
    },
    KEYPAD_SOUTHWEST_KEY {
        @Override
        public void load(Apple2Activity activity) {
            loadAllKeypadKeys(activity);
        }
    },
    KEYPAD_SOUTH_KEY {
        @Override
        public void load(Apple2Activity activity) {
            loadAllKeypadKeys(activity);
        }
    },
    KEYPAD_SOUTHEAST_KEY {
        @Override
        public void load(Apple2Activity activity) {
            loadAllKeypadKeys(activity);
        }
    },
    KEYPAD_TAP_KEY {
        @Override
        public void load(Apple2Activity activity) {
            loadAllKeypadKeys(activity);
        }
    },
    KEYPAD_SWIPEUP_KEY {
        @Override
        public void load(Apple2Activity activity) {
            loadAllKeypadKeys(activity);
        }
    },
    KEYPAD_SWIPEDOWN_KEY {
        @Override
        public void load(Apple2Activity activity) {
            loadAllKeypadKeys(activity);
        }
    },
    KEYREPEAT_THRESHOLD {
        @Override
        public void load(Apple2Activity activity) {
            int tick = intValue(activity);
            nativeSetTouchDeviceKeyRepeatThreshold((float) tick / KEYREPEAT_NUM_CHOICES);
        }

        @Override
        public int intValue(Apple2Activity activity) {
            int defaultLatency = KEYREPEAT_NUM_CHOICES / 4;
            return activity.getPreferences(Context.MODE_PRIVATE).getInt(toString(), defaultLatency);
        }
    },
    KEYBOARD_ALT {
        @Override
        public void load(Apple2Activity activity) {
            /* ... */
        }

        @Override
        public int intValue(Apple2Activity activity) {
            return activity.getPreferences(Context.MODE_PRIVATE).getInt(toString(), 0);
        }
    },
    KEYBOARD_ALT_PATH {
        @Override
        public void load(Apple2Activity activity) {
            nativeLoadTouchKeyboardJSON(stringValue(activity));
        }

        @Override
        public String stringValue(Apple2Activity activity) {
            return activity.getPreferences(Context.MODE_PRIVATE).getString(toString(), "");
        }
    },
    KEYBOARD_VISIBILITY_ACTIVE {
        @Override
        public void load(Apple2Activity activity) {
            int inactiveTick = KEYBOARD_VISIBILITY_INACTIVE.intValue(activity);
            int activeTick = intValue(activity);
            nativeSetTouchKeyboardVisibility((float) inactiveTick / ALPHA_SLIDER_NUM_CHOICES, (float) activeTick / ALPHA_SLIDER_NUM_CHOICES);
        }

        @Override
        public int intValue(Apple2Activity activity) {
            return activity.getPreferences(Context.MODE_PRIVATE).getInt(toString(), ALPHA_SLIDER_NUM_CHOICES);
        }
    },
    KEYBOARD_VISIBILITY_INACTIVE {
        @Override
        public void load(Apple2Activity activity) {
            int inactiveTick = intValue(activity);
            int activeTick = KEYBOARD_VISIBILITY_ACTIVE.intValue(activity);
            nativeSetTouchKeyboardVisibility((float) inactiveTick / ALPHA_SLIDER_NUM_CHOICES, (float) activeTick / ALPHA_SLIDER_NUM_CHOICES);
        }

        @Override
        public int intValue(Apple2Activity activity) {
            return activity.getPreferences(Context.MODE_PRIVATE).getInt(toString(), 5);
        }
    },
    KEYBOARD_LOWERCASE_ENABLED {
        @Override
        public void load(Apple2Activity activity) {
            boolean enabled = booleanValue(activity);
            nativeSetTouchKeyboardLowercaseEnabled(enabled);
        }

        @Override
        public boolean booleanValue(Apple2Activity activity) {
            return activity.getPreferences(Context.MODE_PRIVATE).getBoolean(toString(), false);
        }
    },
    KEYBOARD_CLICK_ENABLED {
        @Override
        public void load(Apple2Activity activity) {
            boolean enabled = booleanValue(activity);
        }

        @Override
        public boolean booleanValue(Apple2Activity activity) {
            return activity.getPreferences(Context.MODE_PRIVATE).getBoolean(toString(), true);
        }
    },
    CRASH_CHECK {
        @Override
        public void load(Apple2Activity activity) {
            boolean enabled = booleanValue(activity);
        }

        @Override
        public boolean booleanValue(Apple2Activity activity) {
            return activity.getPreferences(Context.MODE_PRIVATE).getBoolean(toString(), true);
        }
    },
    GL_VENDOR {
        @Override
        public void load(Apple2Activity activity) {
            /* ... */
        }

        @Override
        public String stringValue(Apple2Activity activity) {
            return activity.getPreferences(Context.MODE_PRIVATE).getString(toString(), "");
        }
    },
    GL_RENDERER {
        @Override
        public void load(Apple2Activity activity) {
            /* ... */
        }

        @Override
        public String stringValue(Apple2Activity activity) {
            return activity.getPreferences(Context.MODE_PRIVATE).getString(toString(), "");
        }
    },
    GL_VERSION {
        @Override
        public void load(Apple2Activity activity) {
            /* ... */
        }

        @Override
        public String stringValue(Apple2Activity activity) {
            return activity.getPreferences(Context.MODE_PRIVATE).getString(toString(), "");
        }
    };


    public enum HiresColor {
        BW,
        COLOR,
        INTERPOLATED
    }

    public enum Volume {
        OFF(0),
        ONE(1),
        TWO(2),
        THREE(3),
        FOUR(4),
        MEDIUM(5),
        FIVE(5),
        SIX(6),
        SEVEN(7),
        EIGHT(8),
        NINE(9),
        MAX(10),
        ELEVEN(11);
        private int vol;

        Volume(int vol) {
            this.vol = vol;
        }
    }

    public enum TouchDeviceVariant {
        NONE(0),
        JOYSTICK(1),
        JOYSTICK_KEYPAD(2),
        KEYBOARD(3);
        private int dev;

        public static final int size = TouchDeviceVariant.values().length;

        TouchDeviceVariant(int dev) {
            this.dev = dev;
        }

        static TouchDeviceVariant next(int ord) {
            ++ord;
            if (ord >= size) {
                ord = 1;
            }
            return TouchDeviceVariant.values()[ord];
        }
    }

    public enum TouchJoystickButtons {
        NONE(0),
        BUTTON1(1),
        BUTTON2(2),
        BOTH(3);
        private int butt;

        TouchJoystickButtons(int butt) {
            this.butt = butt;
        }
    }

    public enum KeypadPreset {
        ARROWS_SPACE {
            @Override
            public String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_preset_arrows_space);
            }

            @Override
            public void apply(Apple2Activity activity) {
                Apple2Preferences.KEYPAD_NORTHWEST_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1);
                Apple2Preferences.KEYPAD_NORTH_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.MOUSETEXT_UP, Apple2KeyboardSettingsMenu.SCANCODE_UP);
                Apple2Preferences.KEYPAD_NORTHEAST_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1);
                Apple2Preferences.KEYPAD_WEST_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.MOUSETEXT_LEFT, Apple2KeyboardSettingsMenu.SCANCODE_LEFT);
                Apple2Preferences.KEYPAD_CENTER_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1);
                Apple2Preferences.KEYPAD_EAST_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.MOUSETEXT_RIGHT, Apple2KeyboardSettingsMenu.SCANCODE_RIGHT);
                Apple2Preferences.KEYPAD_SOUTHWEST_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1);
                Apple2Preferences.KEYPAD_SOUTH_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.MOUSETEXT_DOWN, Apple2KeyboardSettingsMenu.SCANCODE_DOWN);
                Apple2Preferences.KEYPAD_SOUTHEAST_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1);
                Apple2Preferences.KEYPAD_TAP_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.ICONTEXT_VISUAL_SPACE, Apple2KeyboardSettingsMenu.SCANCODE_SPACE);
                Apple2Preferences.KEYPAD_SWIPEDOWN_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1);
                Apple2Preferences.KEYPAD_SWIPEUP_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1);
            }
        },
        AZ_LEFT_RIGHT_SPACE {
            @Override
            public String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_preset_az_left_right_space);
            }

            @Override
            public void apply(Apple2Activity activity) {
                Apple2Preferences.KEYPAD_NORTHWEST_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1);
                Apple2Preferences.KEYPAD_NORTH_KEY.saveChosenKey(activity, (char) 'A', Apple2KeyboardSettingsMenu.SCANCODE_A);
                Apple2Preferences.KEYPAD_NORTHEAST_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1);
                Apple2Preferences.KEYPAD_WEST_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.MOUSETEXT_LEFT, Apple2KeyboardSettingsMenu.SCANCODE_LEFT);
                Apple2Preferences.KEYPAD_CENTER_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1);
                Apple2Preferences.KEYPAD_EAST_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.MOUSETEXT_RIGHT, Apple2KeyboardSettingsMenu.SCANCODE_RIGHT);
                Apple2Preferences.KEYPAD_SOUTHWEST_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1);
                Apple2Preferences.KEYPAD_SOUTH_KEY.saveChosenKey(activity, (char) 'Z', Apple2KeyboardSettingsMenu.SCANCODE_Z);
                Apple2Preferences.KEYPAD_SOUTHEAST_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1);
                Apple2Preferences.KEYPAD_TAP_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.ICONTEXT_VISUAL_SPACE, Apple2KeyboardSettingsMenu.SCANCODE_SPACE);
                Apple2Preferences.KEYPAD_SWIPEDOWN_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1);
                Apple2Preferences.KEYPAD_SWIPEUP_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1);
            }
        },
        LEFT_RIGHT_SPACE {
            @Override
            public String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_preset_left_right_space);
            }

            @Override
            public void apply(Apple2Activity activity) {
                Apple2Preferences.KEYPAD_NORTHWEST_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1);
                Apple2Preferences.KEYPAD_NORTH_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1);
                Apple2Preferences.KEYPAD_NORTHEAST_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1);
                Apple2Preferences.KEYPAD_WEST_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.MOUSETEXT_LEFT, Apple2KeyboardSettingsMenu.SCANCODE_LEFT);
                Apple2Preferences.KEYPAD_CENTER_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1);
                Apple2Preferences.KEYPAD_EAST_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.MOUSETEXT_RIGHT, Apple2KeyboardSettingsMenu.SCANCODE_RIGHT);
                Apple2Preferences.KEYPAD_SOUTHWEST_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1);
                Apple2Preferences.KEYPAD_SOUTH_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1);
                Apple2Preferences.KEYPAD_SOUTHEAST_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1);
                Apple2Preferences.KEYPAD_TAP_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.ICONTEXT_VISUAL_SPACE, Apple2KeyboardSettingsMenu.SCANCODE_SPACE);
                Apple2Preferences.KEYPAD_SWIPEDOWN_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1);
                Apple2Preferences.KEYPAD_SWIPEUP_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1);
            }
        },
        IJKM_SPACE {
            @Override
            public String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_preset_ijkm_space);
            }

            @Override
            public void apply(Apple2Activity activity) {
                Apple2Preferences.KEYPAD_NORTHWEST_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1);
                Apple2Preferences.KEYPAD_NORTH_KEY.saveChosenKey(activity, (char) 'I', Apple2KeyboardSettingsMenu.SCANCODE_I);
                Apple2Preferences.KEYPAD_NORTHEAST_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1);
                Apple2Preferences.KEYPAD_WEST_KEY.saveChosenKey(activity, (char) 'J', Apple2KeyboardSettingsMenu.SCANCODE_J);
                Apple2Preferences.KEYPAD_CENTER_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1);
                Apple2Preferences.KEYPAD_EAST_KEY.saveChosenKey(activity, (char) 'K', Apple2KeyboardSettingsMenu.SCANCODE_K);
                Apple2Preferences.KEYPAD_SOUTHWEST_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1);
                Apple2Preferences.KEYPAD_SOUTH_KEY.saveChosenKey(activity, (char) 'M', Apple2KeyboardSettingsMenu.SCANCODE_M);
                Apple2Preferences.KEYPAD_SOUTHEAST_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1);
                Apple2Preferences.KEYPAD_TAP_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.ICONTEXT_VISUAL_SPACE, Apple2KeyboardSettingsMenu.SCANCODE_SPACE);
                Apple2Preferences.KEYPAD_SWIPEDOWN_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1);
                Apple2Preferences.KEYPAD_SWIPEUP_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1);
            }
        },
        WADX_SPACE {
            @Override
            public String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_preset_wadx_space);
            }

            @Override
            public void apply(Apple2Activity activity) {
                Apple2Preferences.KEYPAD_NORTHWEST_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1);
                Apple2Preferences.KEYPAD_NORTH_KEY.saveChosenKey(activity, (char) 'W', Apple2KeyboardSettingsMenu.SCANCODE_W);
                Apple2Preferences.KEYPAD_NORTHEAST_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1);
                Apple2Preferences.KEYPAD_WEST_KEY.saveChosenKey(activity, (char) 'A', Apple2KeyboardSettingsMenu.SCANCODE_A);
                Apple2Preferences.KEYPAD_CENTER_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1);
                Apple2Preferences.KEYPAD_EAST_KEY.saveChosenKey(activity, (char) 'D', Apple2KeyboardSettingsMenu.SCANCODE_D);
                Apple2Preferences.KEYPAD_SOUTHWEST_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1);
                Apple2Preferences.KEYPAD_SOUTH_KEY.saveChosenKey(activity, (char) 'X', Apple2KeyboardSettingsMenu.SCANCODE_X);
                Apple2Preferences.KEYPAD_SOUTHEAST_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1);
                Apple2Preferences.KEYPAD_TAP_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.ICONTEXT_VISUAL_SPACE, Apple2KeyboardSettingsMenu.SCANCODE_SPACE);
                Apple2Preferences.KEYPAD_SWIPEDOWN_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1);
                Apple2Preferences.KEYPAD_SWIPEUP_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1);
            }
        },
        CRAZY_SEAFOX_KEYS {
            @Override
            public String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_preset_crazy_seafox);
            }

            @Override
            public void apply(Apple2Activity activity) {
                // Heh, the entire purpose of the keypad-variant touch joystick is to make this possible ;-)
                Apple2Preferences.KEYPAD_NORTHWEST_KEY.saveChosenKey(activity, (char) 'Y', Apple2KeyboardSettingsMenu.SCANCODE_Y);
                Apple2Preferences.KEYPAD_NORTH_KEY.saveChosenKey(activity, (char) 'U', Apple2KeyboardSettingsMenu.SCANCODE_U);
                Apple2Preferences.KEYPAD_NORTHEAST_KEY.saveChosenKey(activity, (char) 'I', Apple2KeyboardSettingsMenu.SCANCODE_I);
                Apple2Preferences.KEYPAD_WEST_KEY.saveChosenKey(activity, (char) 'H', Apple2KeyboardSettingsMenu.SCANCODE_H);
                Apple2Preferences.KEYPAD_CENTER_KEY.saveChosenKey(activity, (char) 'J', Apple2KeyboardSettingsMenu.SCANCODE_J);
                Apple2Preferences.KEYPAD_EAST_KEY.saveChosenKey(activity, (char) 'K', Apple2KeyboardSettingsMenu.SCANCODE_K);
                Apple2Preferences.KEYPAD_SOUTHWEST_KEY.saveChosenKey(activity, (char) 'N', Apple2KeyboardSettingsMenu.SCANCODE_N);
                Apple2Preferences.KEYPAD_SOUTH_KEY.saveChosenKey(activity, (char) 'M', Apple2KeyboardSettingsMenu.SCANCODE_M);
                Apple2Preferences.KEYPAD_SOUTHEAST_KEY.saveChosenKey(activity, (char) ',', Apple2KeyboardSettingsMenu.SCANCODE_COMMA);
                Apple2Preferences.KEYPAD_TAP_KEY.saveChosenKey(activity, (char) 'D', Apple2KeyboardSettingsMenu.SCANCODE_D);
                Apple2Preferences.KEYPAD_SWIPEDOWN_KEY.saveChosenKey(activity, (char) 'F', Apple2KeyboardSettingsMenu.SCANCODE_F);
                Apple2Preferences.KEYPAD_SWIPEUP_KEY.saveChosenKey(activity, (char) Apple2KeyboardSettingsMenu.ICONTEXT_VISUAL_SPACE, Apple2KeyboardSettingsMenu.SCANCODE_SPACE);
            }
        };

        public abstract String getTitle(Apple2Activity activity);

        public abstract void apply(Apple2Activity activity);

        public static final int size = KeypadPreset.values().length;

        public static String[] titles(Apple2Activity activity) {
            String[] titles = new String[size];
            int i = 0;
            for (KeypadPreset preset : values()) {
                titles[i++] = preset.getTitle(activity);
            }
            return titles;
        }
    }

    public final static int DECENT_AMOUNT_OF_CHOICES = 20;
    public final static int AUDIO_LATENCY_NUM_CHOICES = DECENT_AMOUNT_OF_CHOICES;
    public final static int ALPHA_SLIDER_NUM_CHOICES = DECENT_AMOUNT_OF_CHOICES;
    public final static int JOYSTICK_DIVIDER_NUM_CHOICES = DECENT_AMOUNT_OF_CHOICES;

    public final static int TAPDELAY_NUM_CHOICES = DECENT_AMOUNT_OF_CHOICES;
    public final static float TAPDELAY_SCALE = 0.5f;

    public final static int KEYREPEAT_NUM_CHOICES = DECENT_AMOUNT_OF_CHOICES;

    public final static String TAG = "Apple2Preferences";

    public final static int JOYSTICK_BUTTON_THRESHOLD_NUM_CHOICES = DECENT_AMOUNT_OF_CHOICES;
    public final static int JOYSTICK_BUTTON_THRESHOLD_STEP = 5;

    public final static float JOYSTICK_AXIS_SENSITIVITY_MIN = 0.25f;
    public final static float JOYSTICK_AXIS_SENSITIVITY_DEFAULT = 1.f;
    public final static float JOYSTICK_AXIS_SENSITIVITY_MAX = 4.f;
    public final static float JOYSTICK_AXIS_SENSITIVITY_DEC_STEP = 0.05f;
    public final static float JOYSTICK_AXIS_SENSITIVITY_INC_STEP = 0.25f;
    public final static int JOYSTICK_AXIS_SENSITIVITY_DEC_NUMCHOICES = (int) ((JOYSTICK_AXIS_SENSITIVITY_DEFAULT - JOYSTICK_AXIS_SENSITIVITY_MIN) / JOYSTICK_AXIS_SENSITIVITY_DEC_STEP); // 15
    public final static int JOYSTICK_AXIS_SENSITIVITY_INC_NUMCHOICES = (int) ((JOYSTICK_AXIS_SENSITIVITY_MAX - JOYSTICK_AXIS_SENSITIVITY_DEFAULT) / JOYSTICK_AXIS_SENSITIVITY_INC_STEP); // 12
    public final static int JOYSTICK_AXIS_SENSITIVITY_NUM_CHOICES = JOYSTICK_AXIS_SENSITIVITY_DEC_NUMCHOICES + JOYSTICK_AXIS_SENSITIVITY_INC_NUMCHOICES; // 15 + 12

    // set and apply

    public void saveBoolean(Apple2Activity activity, boolean value) {
        activity.getPreferences(Context.MODE_PRIVATE).edit().putBoolean(toString(), value).apply();
        load(activity);
    }

    public void saveInt(Apple2Activity activity, int value) {
        activity.getPreferences(Context.MODE_PRIVATE).edit().putInt(toString(), value).apply();
        load(activity);
    }

    public void saveFloat(Apple2Activity activity, float value) {
        throw new RuntimeException("DENIED! You're doing it wrong! =P");
    }

    public void saveString(Apple2Activity activity, String value) {
        activity.getPreferences(Context.MODE_PRIVATE).edit().putString(toString(), value).apply();
        load(activity);
    }

    public void saveHiresColor(Apple2Activity activity, HiresColor value) {
        activity.getPreferences(Context.MODE_PRIVATE).edit().putInt(toString(), value.ordinal()).apply();
        load(activity);
    }

    public void saveVolume(Apple2Activity activity, Volume value) {
        activity.getPreferences(Context.MODE_PRIVATE).edit().putInt(toString(), value.ordinal()).apply();
        load(activity);
    }

    public void saveTouchDevice(Apple2Activity activity, TouchDeviceVariant value) {
        activity.getPreferences(Context.MODE_PRIVATE).edit().putInt(toString(), value.ordinal()).apply();
        load(activity);
    }

    public void saveTouchJoystickButtons(Apple2Activity activity, TouchJoystickButtons value) {
        activity.getPreferences(Context.MODE_PRIVATE).edit().putInt(toString(), value.ordinal()).apply();
        load(activity);
    }

    public void saveChosenKey(Apple2Activity activity, char ascii, int scancode) {
        activity.getPreferences(Context.MODE_PRIVATE).edit().putInt(asciiString(), ascii).apply();
        activity.getPreferences(Context.MODE_PRIVATE).edit().putInt(scancodeString(), scancode).apply();
        load(activity);
    }

    public void setPath(Apple2Activity activity, String path) {
        /* ... */
    }

    // accessors

    public boolean booleanValue(Apple2Activity activity) {
        return activity.getPreferences(Context.MODE_PRIVATE).getBoolean(toString(), false);
    }

    public int intValue(Apple2Activity activity) {
        return activity.getPreferences(Context.MODE_PRIVATE).getInt(toString(), 0);
    }

    public float floatValue(Apple2Activity activity) {
        return (float) activity.getPreferences(Context.MODE_PRIVATE).getInt(toString(), 0);
    }

    public String stringValue(Apple2Activity activity) {
        return activity.getPreferences(Context.MODE_PRIVATE).getString(toString(), null);
    }

    public char asciiValue(Apple2Activity activity) {
        return (char) activity.getPreferences(Context.MODE_PRIVATE).getInt(asciiString(), ' ');
    }

    public int scancodeValue(Apple2Activity activity) {
        return activity.getPreferences(Context.MODE_PRIVATE).getInt(scancodeString(), Apple2KeyboardSettingsMenu.SCANCODE_SPACE);
    }

    public static void loadPreferences(Apple2Activity activity) {
        for (Apple2Preferences pref : Apple2Preferences.values()) {
            pref.load(activity);
        }
        // HACK FIXME TODO 2015/12/13 : native GLTouchDevice is conflating various things ... forcefully reset the current touch device here for now
        Apple2Preferences.CURRENT_TOUCH_DEVICE.load(activity);
    }

    public static void resetPreferences(Apple2Activity activity) {
        activity.getPreferences(Context.MODE_PRIVATE).edit().clear().commit();
        activity.quitEmulator();
    }

    public String asciiString() {
        return toString() + "_ASCII";
    }

    public String scancodeString() {
        return toString() + "_SCAN";
    }

    // ------------------------------------------------------------------------
    // internals ...

    protected abstract void load(Apple2Activity activity);

    protected static void warnError(Apple2Activity activity, int titleId, int mesgId) {
        AlertDialog dialog = new AlertDialog.Builder(activity).setIcon(R.drawable.ic_launcher).setCancelable(true).setTitle(titleId).setMessage(mesgId).setPositiveButton(R.string.ok, new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                dialog.dismiss();
            }
        }).create();
        activity.registerAndShowDialog(dialog);
    }

    public static void loadAllKeypadKeys(Apple2Activity activity) {
        int[] rosetteChars = new int[]{
                KEYPAD_NORTHWEST_KEY.asciiValue(activity),
                KEYPAD_NORTH_KEY.asciiValue(activity),
                KEYPAD_NORTHEAST_KEY.asciiValue(activity),
                KEYPAD_WEST_KEY.asciiValue(activity),
                KEYPAD_CENTER_KEY.asciiValue(activity),
                KEYPAD_EAST_KEY.asciiValue(activity),
                KEYPAD_SOUTHWEST_KEY.asciiValue(activity),
                KEYPAD_SOUTH_KEY.asciiValue(activity),
                KEYPAD_SOUTHEAST_KEY.asciiValue(activity),
        };
        int[] rosetteScancodes = new int[]{
                KEYPAD_NORTHWEST_KEY.scancodeValue(activity),
                KEYPAD_NORTH_KEY.scancodeValue(activity),
                KEYPAD_NORTHEAST_KEY.scancodeValue(activity),
                KEYPAD_WEST_KEY.scancodeValue(activity),
                KEYPAD_CENTER_KEY.scancodeValue(activity),
                KEYPAD_EAST_KEY.scancodeValue(activity),
                KEYPAD_SOUTHWEST_KEY.scancodeValue(activity),
                KEYPAD_SOUTH_KEY.scancodeValue(activity),
                KEYPAD_SOUTHEAST_KEY.scancodeValue(activity),
        };
        int[] buttonsChars = new int[]{
                KEYPAD_TAP_KEY.asciiValue(activity),
                KEYPAD_SWIPEUP_KEY.asciiValue(activity),
                KEYPAD_SWIPEDOWN_KEY.asciiValue(activity),
        };
        int[] buttonsScancodes = new int[]{
                KEYPAD_TAP_KEY.scancodeValue(activity),
                KEYPAD_SWIPEUP_KEY.scancodeValue(activity),
                KEYPAD_SWIPEDOWN_KEY.scancodeValue(activity),
        };
        nativeTouchJoystickSetKeypadTypes(rosetteChars, rosetteScancodes, buttonsChars, buttonsScancodes);
    }

    public static void loadAllJoystickButtons(Apple2Activity activity) {
        nativeSetTouchJoystickButtonTypes(
                JOYSTICK_TAP_BUTTON.intValue(activity),
                JOYSTICK_SWIPEUP_BUTTON.intValue(activity),
                JOYSTICK_SWIPEDOWN_BUTTON.intValue(activity));
    }

    public static void insertDisk(Apple2Activity activity, String fullPath, boolean isDriveA, boolean isReadOnly) {
        File file = new File(fullPath);
        if (!file.exists()) {
            fullPath = fullPath + ".gz";
            file = new File(fullPath);
        }
        if (file.exists()) {
            activity.chooseDisk(fullPath, isDriveA, isReadOnly);
        } else {
            Log.d(TAG, "Cannot insert: " + fullPath);
        }
    }

    // native hooks

    private static native void nativeSetColor(int color);

    private static native boolean nativeSetSpeakerEnabled(boolean enabled);

    private static native void nativeSetSpeakerVolume(int volume);

    private static native boolean nativeSetMockingboardEnabled(boolean enabled);

    private static native void nativeSetMockingboardVolume(int volume);

    private static native void nativeSetAudioLatency(float latencySecs);

    public static native void nativeSetCurrentTouchDevice(int device);

    private static native void nativeSetTouchJoystickButtonTypes(int down, int north, int south);

    private static native void nativeSetTouchJoystickTapDelay(float secs);

    private static native void nativeSetTouchJoystickAxisSensitivity(float multiplier);

    private static native void nativeSetTouchJoystickButtonSwitchThreshold(int delta);

    private static native void nativeSetTouchJoystickVisibility(boolean visibility);

    public static native void nativeSetTouchMenuEnabled(boolean enabled);

    public static native void nativeSetShowDiskOperationAnimation(boolean enabled);

    private static native void nativeSetTouchKeyboardVisibility(float inactiveAlpha, float activeAlpha);

    private static native void nativeSetTouchKeyboardLowercaseEnabled(boolean enabled);

    public static native int nativeGetCurrentTouchDevice();

    public static native int nativeGetCPUSpeed();

    public static native void nativeSetCPUSpeed(int percentSpeed);

    public static native void nativeTouchJoystickSetScreenDivision(float division);

    public static native void nativeTouchJoystickSetAxisOnLeft(boolean axisIsOnLeft);

    public static native void nativeTouchDeviceBeginCalibrationMode();

    public static native void nativeTouchDeviceEndCalibrationMode();

    private static native void nativeTouchJoystickSetKeypadTypes(int[] rosetteChars, int[] rosetteScancodes, int[] buttonsChars, int[] buttonsScancodes);

    private static native void nativeSetTouchDeviceKeyRepeatThreshold(float threshold);

    private static native void nativeLoadTouchKeyboardJSON(String path);

}
