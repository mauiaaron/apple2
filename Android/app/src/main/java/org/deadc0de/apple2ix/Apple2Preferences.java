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

import android.app.Activity;
import android.util.Log;

import org.deadc0de.apple2ix.basic.BuildConfig;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.util.ArrayList;
import java.util.Map;
import java.util.concurrent.atomic.AtomicBoolean;

public class Apple2Preferences {

    public final static String TAG = "Apple2Preferences";

    public final static int DECENT_AMOUNT_OF_CHOICES = 20;
    public final static int ALPHA_SLIDER_NUM_CHOICES = DECENT_AMOUNT_OF_CHOICES;

    public final static String PREFS_FILE = ".apple2.json";

    // ------------------------------------------------------------------------
    // preference domains

    public final static String PREF_DOMAIN_AUDIO = "audio";
    public final static String PREF_DOMAIN_INTERFACE = "interface";
    public final static String PREF_DOMAIN_JOYSTICK = "joystick";
    public final static String PREF_DOMAIN_KEYBOARD = "keyboard";
    public final static String PREF_DOMAIN_TOUCHSCREEN = "touchscreen";
    public final static String PREF_DOMAIN_VIDEO = "video";
    public final static String PREF_DOMAIN_VM = "vm";

    public final static String PREF_CALIBRATING = "isCalibrating";
    public final static String PREF_DEVICE_HEIGHT = "deviceHeight";
    public final static String PREF_DEVICE_WIDTH = "deviceWidth";
    public final static String PREF_EMULATOR_VERSION = "emulatorVersion";
    public final static String PREF_RELEASE_NOTES = "shownReleaseNotes";

    // JSON preferences
    private static JSONObject sSettings = null;
    private static AtomicBoolean sNativeIsDirty = new AtomicBoolean(true);

    // ------------------------------------------------------------------------

    private static JSONObject _prefDomain(String domain) {
        JSONObject map = null;
        try {
            map = sSettings.getJSONObject(domain);
        } catch (JSONException e) {
            Log.v(TAG, "Could not get preference domain " + domain);
        }
        if (map == null) {
            map = new JSONObject();
            try {
                sSettings.put(domain, map);
            } catch (JSONException e) {
                e.printStackTrace();
            }
        }
        return map;
    }

    public static void setJSONPref(Apple2AbstractMenu.IMenuEnum menu, Object val) {
        try {
            JSONObject map = _prefDomain(menu.getPrefDomain());
            map.put(menu.getPrefKey(), val);
        } catch (JSONException e) {
            e.printStackTrace();
        }
        sNativeIsDirty.set(true);
    }

    public static void setJSONPref(String domain, String key, Object val) {
        try {
            JSONObject map = _prefDomain(domain);
            map.put(key, val);
        } catch (JSONException e) {
            e.printStackTrace();
        }
        sNativeIsDirty.set(true);
    }

    public static Object getJSONPref(Apple2AbstractMenu.IMenuEnum menu) {
        String key = null;
        Object val = null;
        JSONObject map = null;
        try {
            map = _prefDomain(menu.getPrefDomain());
            key = menu.getPrefKey();
            val = map.get(key);
        } catch (JSONException e) {
            Apple2Activity.logMessage(Apple2Activity.LogType.DEBUG, TAG, "Did not find value for domain:" + menu.getPrefDefault() + " key:" + key);
        }
        if (val == null && key != null) {
            val = menu.getPrefDefault();
            try {
                map.put(key, val);
            } catch (JSONException e) {
                e.printStackTrace();
            }
        }
        return val;
    }

    public static Object getJSONPref(String domain, String key, Object defaultVal) {
        Object val = null;
        JSONObject map = null;
        try {
            map = _prefDomain(domain);
            val = map.get(key);
        } catch (JSONException e) {
            Apple2Activity.logMessage(Apple2Activity.LogType.DEBUG, TAG, "Did not find value for domain:" + domain + " key:" + key);
        }
        if (val == null) {
            val = defaultVal;
            try {
                map.put(key, val);
            } catch (JSONException e) {
                e.printStackTrace();
            }
        }
        return val;
    }

    private static float _convertToFloat(Object obj) {
        if (obj == null) {
            return Float.NaN;
        }
        try {
            return (float) obj;
        } catch (ClassCastException e) {
            Apple2Activity.logMessage(Apple2Activity.LogType.DEBUG, TAG, "could not cast object as float");
        }
        try {
            return (float) ((double) obj);
        } catch (ClassCastException e) {
            Apple2Activity.logMessage(Apple2Activity.LogType.DEBUG, TAG, "could not cast object as double");
        }
        try {
            return (float) ((int) obj);
        } catch (ClassCastException e) {
            Apple2Activity.logMessage(Apple2Activity.LogType.DEBUG, TAG, "could not cast object as int");
        }
        return (float) ((long) obj);
    }

    private static int _convertToInt(Object obj) {
        if (obj == null) {
            return 0;
        }
        try {
            return (int) obj;
        } catch (ClassCastException e) {
            Apple2Activity.logMessage(Apple2Activity.LogType.DEBUG, TAG, "could not cast object as int");
        }
        try {
            return (int) ((long) obj);
        } catch (ClassCastException e) {
            Apple2Activity.logMessage(Apple2Activity.LogType.DEBUG, TAG, "could not cast object as long");
        }
        try {
            return (int) ((float) obj);
        } catch (ClassCastException e) {
            Apple2Activity.logMessage(Apple2Activity.LogType.DEBUG, TAG, "could not cast object as float");
        }
        return (int) ((double) obj);
    }

    public static float getFloatJSONPref(Apple2AbstractMenu.IMenuEnum menu) {
        return _convertToFloat(getJSONPref(menu));
    }

    public static float getFloatJSONPref(String domain, String key, Object defaultVal) {
        return _convertToFloat(getJSONPref(domain, key, defaultVal));
    }

    public static int getIntJSONPref(Apple2AbstractMenu.IMenuEnum menu) {
        return _convertToInt(getJSONPref(menu));
    }

    public static int getIntJSONPref(String domain, String key, Object defaultVal) {
        return _convertToInt(getJSONPref(domain, key, defaultVal));
    }

    public static boolean migrate(Apple2Activity activity) {
        int versionCode = (int) getJSONPref(PREF_DOMAIN_INTERFACE, PREF_EMULATOR_VERSION, 0);
        final boolean firstTime = (versionCode != BuildConfig.VERSION_CODE);
        if (!firstTime) {
            return firstTime;
        }

        Log.v(TAG, "Triggering migration to Apple2ix version : " + BuildConfig.VERSION_NAME);
        setJSONPref(PREF_DOMAIN_INTERFACE, PREF_EMULATOR_VERSION, BuildConfig.VERSION_CODE);
        setJSONPref(PREF_DOMAIN_INTERFACE, PREF_RELEASE_NOTES, false);

        if (versionCode < 22) {
            // force upgrade to defaults here ...
            setJSONPref(Apple2VideoSettingsMenu.SETTINGS.SHOW_HALF_SCANLINES, Apple2VideoSettingsMenu.SETTINGS.SHOW_HALF_SCANLINES.getPrefDefault());
            setJSONPref(Apple2VideoSettingsMenu.SETTINGS.COLOR_MODE_CONFIGURE, Apple2VideoSettingsMenu.SETTINGS.COLOR_MODE_CONFIGURE.getPrefDefault());
        }

        Apple2Utils.migrateToExternalStorage(activity);

        if (versionCode < 24) {
            // migrate tap delay from seconds to frames ...
            float secs = getFloatJSONPref(PREF_DOMAIN_JOYSTICK, "jsTapDelaySecs", 9999f);
            if (secs != 9999f) {
                // UtAIIe 3-13 : "The duration of the television scan is 262 horizontal scans.  This is [16.688 milliseconds]"
                // recalculate this to a frames value between 0-30 inclusive ...
                int framesDelay = Math.round(secs / 0.016688f);
                if (framesDelay < 0) {
                    framesDelay = 0;
                } else if (framesDelay > 30) {
                    framesDelay = 30;
                }
                setJSONPref(Apple2JoystickSettingsMenu.JoystickAdvanced.SETTINGS.JOYSTICK_TAPDELAY, framesDelay);
            }

            // migrate axis rosette arrays to new format ...
            try {
                ArrayList<Apple2KeypadSettingsMenu.KeyTuple> axisRosette = new ArrayList<Apple2KeypadSettingsMenu.KeyTuple>();
                axisRosette.add(new Apple2KeypadSettingsMenu.KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));
                axisRosette.add(new Apple2KeypadSettingsMenu.KeyTuple('I', Apple2KeyboardSettingsMenu.SCANCODE_I));
                axisRosette.add(new Apple2KeypadSettingsMenu.KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));

                axisRosette.add(new Apple2KeypadSettingsMenu.KeyTuple('J', Apple2KeyboardSettingsMenu.SCANCODE_J));
                axisRosette.add(new Apple2KeypadSettingsMenu.KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));
                axisRosette.add(new Apple2KeypadSettingsMenu.KeyTuple('K', Apple2KeyboardSettingsMenu.SCANCODE_K));

                axisRosette.add(new Apple2KeypadSettingsMenu.KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));
                axisRosette.add(new Apple2KeypadSettingsMenu.KeyTuple('M', Apple2KeyboardSettingsMenu.SCANCODE_M));
                axisRosette.add(new Apple2KeypadSettingsMenu.KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));

                JSONArray jsonArray;

                jsonArray = (JSONArray) getJSONPref(PREF_DOMAIN_JOYSTICK, "kpAxisRosetteChars", null);
                if (jsonArray == null || jsonArray.length() != Apple2KeypadSettingsMenu.ROSETTE_SIZE) {
                    Apple2Activity.logMessage(Apple2Activity.LogType.ERROR, TAG, "Oops, kpAxisRosetteChars is not expected length");
                } else {
                    for (int i = 0; i < Apple2KeypadSettingsMenu.ROSETTE_SIZE; i++) {
                        Apple2KeypadSettingsMenu.KeyTuple tuple = axisRosette.get(i);
                        tuple.ch = jsonArray.getLong(i);
                    }
                }

                jsonArray = (JSONArray) getJSONPref(PREF_DOMAIN_JOYSTICK, "kpAxisRosetteScancodes", null);
                if (jsonArray == null || jsonArray.length() != Apple2KeypadSettingsMenu.ROSETTE_SIZE) {
                    Apple2Activity.logMessage(Apple2Activity.LogType.ERROR, TAG, "Oops, kpAxisRosetteScancodes is not expected length");
                } else {
                    for (int i = 0; i < Apple2KeypadSettingsMenu.ROSETTE_SIZE; i++) {
                        Apple2KeypadSettingsMenu.KeyTuple tuple = axisRosette.get(i);
                        tuple.scan = jsonArray.getLong(i);
                    }
                }

                Apple2KeypadSettingsMenu.KeypadPreset.saveAxisRosette(axisRosette);
            } catch (Exception e) {
                e.printStackTrace();
            }

            // migrate individual keypad button actions to new button rosette actions ...
            {
                ArrayList<Apple2KeypadSettingsMenu.KeyTuple> buttRosette = new ArrayList<Apple2KeypadSettingsMenu.KeyTuple>();

                int northChar = getIntJSONPref(PREF_DOMAIN_JOYSTICK, "kpSwipeNorthChar", Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION);
                int northScan = getIntJSONPref(PREF_DOMAIN_JOYSTICK, "kpSwipeNorthScancode", -1);

                int downChar = getIntJSONPref(PREF_DOMAIN_JOYSTICK, "kpTouchDownChar", Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION);
                int downScan = getIntJSONPref(PREF_DOMAIN_JOYSTICK, "kpTouchDownScancode", -1);

                int southChar = getIntJSONPref(PREF_DOMAIN_JOYSTICK, "kpSwipeSouthChar", Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION);
                int southScan = getIntJSONPref(PREF_DOMAIN_JOYSTICK, "kpSwipeSouthScancode", -1);

                if (northScan < 0 && downScan < 0 && southScan < 0) {
                    downChar = Apple2KeyboardSettingsMenu.ICONTEXT_VISUAL_SPACE;
                    downScan = Apple2KeyboardSettingsMenu.SCANCODE_SPACE;
                }

                buttRosette.add(new Apple2KeypadSettingsMenu.KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));
                buttRosette.add(new Apple2KeypadSettingsMenu.KeyTuple(northChar, northScan));
                buttRosette.add(new Apple2KeypadSettingsMenu.KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));

                buttRosette.add(new Apple2KeypadSettingsMenu.KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));
                buttRosette.add(new Apple2KeypadSettingsMenu.KeyTuple(downChar, downScan));
                buttRosette.add(new Apple2KeypadSettingsMenu.KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));

                buttRosette.add(new Apple2KeypadSettingsMenu.KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));
                buttRosette.add(new Apple2KeypadSettingsMenu.KeyTuple(southChar, southScan));
                buttRosette.add(new Apple2KeypadSettingsMenu.KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));

                Apple2KeypadSettingsMenu.KeypadPreset.saveButtRosette(buttRosette);
            }

            JSONObject map = _prefDomain(PREF_DOMAIN_JOYSTICK);
            map.remove("jsTapDelaySecs");
            map.remove("kpAxisRosetteChars");
            map.remove("kpAxisRosetteScancodes");
            map.remove("kpButtRosetteChars");
            map.remove("kpButtRosetteScancodes");
            map.remove("kpSwipeNorthChar");
            map.remove("kpSwipeNorthScancode");
            map.remove("kpSwipeSouthChar");
            map.remove("kpSwipeSouthScancode");
            map.remove("kpTouchDownChar");
            map.remove("kpTouchDownScancode");
        }

        save(activity);
        return firstTime;
    }

    public static String getPrefsFile(Apple2Activity activity) {
        String file = System.getenv("APPLE2IX_JSON");
        return file == null ? (Apple2Utils.getDataDir(activity) + File.separator + PREFS_FILE) : file;
    }

    public static void load(Apple2Activity activity) {
        File prefsFile = new File(getPrefsFile(activity));

        StringBuilder jsonString = new StringBuilder();
        if (!Apple2Utils.readEntireFile(prefsFile, jsonString)) {
            Apple2Activity.logMessage(Apple2Activity.LogType.DEBUG, TAG, "Oops, could not read JSON file : " + prefsFile);
        }

        try {
            sSettings = new JSONObject(jsonString.toString());
        } catch (JSONException e) {
            e.printStackTrace();
        }
        if (sSettings == null) {
            sSettings = new JSONObject();
        }
    }

    public static void save(Apple2Activity activity) {

        // bespoke reset temporary values...
        boolean wasDirty = sNativeIsDirty.get();
        Apple2Preferences.setJSONPref(Apple2Preferences.PREF_DOMAIN_TOUCHSCREEN, Apple2Preferences.PREF_CALIBRATING, false);
        sNativeIsDirty.set(wasDirty);

        File prefsFile = new File(getPrefsFile(activity));
        String jsonString = null;
        JSONException ex = null;
        try {
            jsonString = sSettings.toString(2);
        } catch (JSONException e) {
            Apple2Activity.logMessage(Apple2Activity.LogType.WARN, TAG, "Error attempting to pretty-print JSON : " + e);
            ex = e;
            jsonString = sSettings.toString();
        }

        if (jsonString != null) {
            Apple2Utils.writeFile(new StringBuilder(jsonString), prefsFile);
        } else {
            Apple2Utils.writeFile(new StringBuilder("{}"), prefsFile);
            throw new RuntimeException(ex); // force reset and hopefully send report to me ;-)
        }
    }

    public static void reset(Apple2Activity activity) {
        sSettings = new JSONObject();
        save(activity);
        activity.quitEmulator();
    }

    public static void sync(Apple2Activity activity, String domain) {
        save(activity);
        if (sNativeIsDirty.getAndSet(false)) {
            Log.v(TAG, "Syncing prefs to native...");
            nativePrefsSync(domain);
        } else {
            Log.v(TAG, "No changes, not syncing prefs...");
        }
    }

    private static native void nativePrefsSync(String domain);
}
