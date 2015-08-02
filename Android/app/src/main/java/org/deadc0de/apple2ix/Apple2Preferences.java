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

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;

public enum Apple2Preferences {
    PREFS_CONFIGURED {
        @Override
        public void load(Apple2Activity activity) {
            // ...
        }

        @Override
        public void saveBoolean(Apple2Activity activity, boolean ignored) {
            activity.getPreferences(Context.MODE_PRIVATE).edit().putBoolean(toString(), true).apply();
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
    SPEAKER_ENABLED {
        @Override
        public void load(Apple2Activity activity) {
            boolean enabled = booleanValue(activity);
            boolean result = nativeSetSpeakerEnabled(enabled);
            if (enabled && !result) {
                warnError(activity, R.string.speaker_disabled_title, R.string.speaker_disabled_mesg);
                activity.getPreferences(Context.MODE_PRIVATE).edit().putBoolean(toString(), false).apply();
            }
        }

        @Override
        public boolean booleanValue(Apple2Activity activity) {
            return activity.getPreferences(Context.MODE_PRIVATE).getBoolean(toString(), true);
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
            nativeSetCurrentTouchDevice(intValue(activity));
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
    TOUCH_MENU_VISIBILITY {
        @Override
        public void load(Apple2Activity activity) {
            int setting = intValue(activity);
            float alpha = (float) setting / AUDIO_LATENCY_NUM_CHOICES;
            nativeSetTouchMenuVisibility(alpha);
        }

        @Override
        public int intValue(Apple2Activity activity) {
            return activity.getPreferences(Context.MODE_PRIVATE).getInt(toString(), 5);
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
            nativeSetTouchJoystickButtonTypes(JOYSTICK_TAP_BUTTON.intValue(activity), JOYSTICK_SWIPEUP_BUTTON.intValue(activity), JOYSTICK_SWIPEDOWN_BUTTON.intValue(activity));
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
            int defaultLatency = 2; // /TAPDELAY_NUM_CHOICES * TAPDELAY_SCALE -> 0.05f
            return activity.getPreferences(Context.MODE_PRIVATE).getInt(toString(), defaultLatency);
        }
    },
    JOYSTICK_SWIPEUP_BUTTON {
        @Override
        public void load(Apple2Activity activity) {
            nativeSetTouchJoystickButtonTypes(JOYSTICK_TAP_BUTTON.intValue(activity), JOYSTICK_SWIPEUP_BUTTON.intValue(activity), JOYSTICK_SWIPEDOWN_BUTTON.intValue(activity));
        }

        @Override
        public int intValue(Apple2Activity activity) {
            return activity.getPreferences(Context.MODE_PRIVATE).getInt(toString(), TouchJoystickButtons.BOTH.ordinal());
        }
    },
    JOYSTICK_SWIPEDOWN_BUTTON {
        @Override
        public void load(Apple2Activity activity) {
            nativeSetTouchJoystickButtonTypes(JOYSTICK_TAP_BUTTON.intValue(activity), JOYSTICK_SWIPEUP_BUTTON.intValue(activity), JOYSTICK_SWIPEDOWN_BUTTON.intValue(activity));
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
            return activity.getPreferences(Context.MODE_PRIVATE).getInt(toString(), JOYSTICK_DIVIDER_NUM_CHOICES>>1);
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

        TouchDeviceVariant(int dev) {
            this.dev = dev;
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

    protected abstract void load(Apple2Activity activity);

    protected void warnError(Apple2Activity activity, int titleId, int mesgId) {
        AlertDialog dialog = new AlertDialog.Builder(activity).setIcon(R.drawable.ic_launcher).setCancelable(true).setTitle(titleId).setMessage(mesgId).setPositiveButton(R.string.ok, new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                dialog.dismiss();
            }
        }).create();
        dialog.show();
    }

    public final static int DECENT_AMOUNT_OF_CHOICES = 20;
    public final static int AUDIO_LATENCY_NUM_CHOICES = DECENT_AMOUNT_OF_CHOICES;
    public final static int ALPHA_SLIDER_NUM_CHOICES = DECENT_AMOUNT_OF_CHOICES;
    public final static int JOYSTICK_DIVIDER_NUM_CHOICES = DECENT_AMOUNT_OF_CHOICES;

    public final static int TAPDELAY_NUM_CHOICES = DECENT_AMOUNT_OF_CHOICES;
    public final static float TAPDELAY_SCALE = 0.5f;

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

    public static void loadPreferences(Apple2Activity activity) {
        for (Apple2Preferences pref : Apple2Preferences.values()) {
            pref.load(activity);
        }
    }

    public static void resetPreferences(Apple2Activity activity) {
        activity.getPreferences(Context.MODE_PRIVATE).edit().clear().commit();
        loadPreferences(activity);
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

    public static native void nativeSetTouchMenuEnabled(boolean enabled);

    private static native void nativeSetTouchMenuVisibility(float alpha);

    public static native int nativeGetCurrentTouchDevice();

    public static native int nativeGetCPUSpeed();

    public static native void nativeSetCPUSpeed(int percentSpeed);

    public static native void nativeTouchJoystickSetScreenDivision(float division);

    public static native void nativeTouchJoystickSetAxisOnLeft(boolean axisIsOnLeft);

    public static native void nativeTouchJoystickBeginCalibrationMode();

    public static native void nativeTouchJoystickEndCalibrationMode();
}
