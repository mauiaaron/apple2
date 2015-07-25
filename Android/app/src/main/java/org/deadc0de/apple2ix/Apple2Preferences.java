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
import android.util.Log;

public enum Apple2Preferences {
    PREFS_CONFIGURED {
        @Override public void load(Apple2Activity activity) {
            // ...
        }
        @Override public void saveBoolean(Apple2Activity activity, boolean ignored) {
            activity.getPreferences(Context.MODE_PRIVATE).edit().putBoolean(toString(), true).apply();
        }
    },
    HIRES_COLOR {
        @Override public void load(Apple2Activity activity) {
            nativeSetColor(intValue(activity));
        }
        @Override public int intValue(Apple2Activity activity) {
            return activity.getPreferences(Context.MODE_PRIVATE).getInt(toString(), HiresColor.INTERPOLATED.ordinal());
        }
    },
    SPEAKER_ENABLED {
        @Override public void load(Apple2Activity activity) {
            boolean enabled = booleanValue(activity);
            boolean result = nativeSetSpeakerEnabled(enabled);
            if (enabled && !result) {
                warnError(activity, R.string.speaker_disabled_title, R.string.speaker_disabled_mesg);
                activity.getPreferences(Context.MODE_PRIVATE).edit().putBoolean(toString(), false).apply();
            }
        }
        @Override public boolean booleanValue(Apple2Activity activity) {
            return activity.getPreferences(Context.MODE_PRIVATE).getBoolean(toString(), true);
        }
    },
    SPEAKER_VOLUME {
        @Override public void load(Apple2Activity activity) {
            nativeSetSpeakerVolume(intValue(activity));
        }
        @Override public int intValue(Apple2Activity activity) {
            return activity.getPreferences(Context.MODE_PRIVATE).getInt(toString(), Volume.MEDIUM.ordinal());
        }
    },
    MOCKINGBOARD_ENABLED {
        @Override public void load(Apple2Activity activity) {
            boolean enabled = booleanValue(activity);
            boolean result = nativeSetMockingboardEnabled(enabled);
            if (enabled && !result) {
                warnError(activity, R.string.mockingboard_disabled_title, R.string.mockingboard_disabled_mesg);
                activity.getPreferences(Context.MODE_PRIVATE).edit().putBoolean(toString(), false).apply();
            }
        }
    },
    MOCKINGBOARD_VOLUME {
        @Override public void load(Apple2Activity activity) {
            nativeSetMockingboardVolume(intValue(activity));
        }
        @Override public int intValue(Apple2Activity activity) {
            return activity.getPreferences(Context.MODE_PRIVATE).getInt(toString(), Volume.MEDIUM.ordinal());
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

    public final static String TAG = "Apple2Preferences";

    // set and apply

    public void saveInt(Apple2Activity activity, int value) {
        activity.getPreferences(Context.MODE_PRIVATE).edit().putInt(toString(), value).apply();
        load(activity);
    }
    public void saveBoolean(Apple2Activity activity, boolean value) {
        activity.getPreferences(Context.MODE_PRIVATE).edit().putBoolean(toString(), value).apply();
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

    // accessors

    public boolean booleanValue(Apple2Activity activity) {
        return activity.getPreferences(Context.MODE_PRIVATE).getBoolean(toString(), false);
    }

    public int intValue(Apple2Activity activity) {
        return activity.getPreferences(Context.MODE_PRIVATE).getInt(toString(), 0);
    }

    public static void loadPreferences(Apple2Activity activity) {
        for (Apple2Preferences pref : Apple2Preferences.values()) {
            pref.load(activity);
        }
    }

    private static native void nativeEnableTouchJoystick(boolean enabled);
    private static native void nativeEnableTiltJoystick(boolean enabled);
    private static native void nativeSetColor(int color);
    private static native boolean nativeSetSpeakerEnabled(boolean enabled);
    private static native void nativeSetSpeakerVolume(int volume);
    private static native boolean nativeSetMockingboardEnabled(boolean enabled);
    private static native void nativeSetMockingboardVolume(int volume);
}
