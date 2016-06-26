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
            Log.d(TAG, "Did not find value for domain:" + menu.getPrefDefault() + " key:" + key);
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
            Log.d(TAG, "Did not find value for domain:" + domain + " key:" + key);
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
            Log.d(TAG, "could not cast object as float");
        }
        try {
            return (float) ((double) obj);
        } catch (ClassCastException e) {
            Log.d(TAG, "could not cast object as double");
        }
        try {
            return (float) ((int) obj);
        } catch (ClassCastException e) {
            Log.d(TAG, "could not cast object as int");
        }
        return (float) ((long) obj);
    }

    public static float getFloatJSONPref(Apple2AbstractMenu.IMenuEnum menu) {
        return _convertToFloat(getJSONPref(menu));
    }

    public static float getFloatJSONPref(String domain, String key, Object defaultVal) {
        return _convertToFloat(getJSONPref(domain, key, defaultVal));
    }

    public static boolean migrate(Apple2Activity activity) {
        int versionCode = (int) getJSONPref(PREF_DOMAIN_INTERFACE, PREF_EMULATOR_VERSION, 0);
        final boolean firstTime = (versionCode != BuildConfig.VERSION_CODE);
        if (!firstTime) {
            return firstTime;
        }

        Log.v(TAG, "Triggering migration to Apple2ix version : " + BuildConfig.VERSION_NAME);
        setJSONPref(PREF_DOMAIN_INTERFACE, PREF_EMULATOR_VERSION, BuildConfig.VERSION_CODE);
        if (BuildConfig.VERSION_CODE >= 17) {
            // FIXME TODO : remove this after shipping 1.1.7+

            boolean keypadPreset = false;
            Apple2AbstractMenu.IMenuEnum menuEnum = null;
            ArrayList<String> keypadJSONChars = new ArrayList<String>();
            ArrayList<String> keypadJSONScans = new ArrayList<String>();
            for (int i = 0; i < Apple2KeypadSettingsMenu.ROSETTE_SIZE; i++) {
                keypadJSONChars.add("" + Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION);
                keypadJSONScans.add("-1");
            }
            int keypadTapChar = Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION;
            int keypadTapScan = -1;
            int keypadSwipeDownChar = Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION;
            int keypadSwipeDownScan = -1;
            int keypadSwipeUpChar = Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION;
            int keypadSwipeUpScan = -1;

            Map<String, ?> oldPrefs = activity.getPreferences(Activity.MODE_PRIVATE).getAll();
            for (Map.Entry<String, ?> entry : oldPrefs.entrySet()) {

                String key = entry.getKey();
                Object val = entry.getValue();
                Log.d("OLDPREFS", key + " : " + val.toString());

                // remove olde preference ...
                activity.getPreferences(Activity.MODE_PRIVATE).edit().remove(key).commit();

                try {
                    switch (key) {

                        case "HIRES_COLOR": // long
                            menuEnum = Apple2VideoSettingsMenu.SETTINGS.COLOR_CONFIGURE;
                            break;
                        case "LANDSCAPE_MODE": // bool
                            menuEnum = Apple2VideoSettingsMenu.SETTINGS.LANDSCAPE_MODE;
                            break;
                        case "PORTRAIT_KEYBOARD_POSITION_SCALE": //-> float
                            menuEnum = Apple2PortraitCalibration.States.CALIBRATE_KEYBOARD_POSITION_SCALE;
                            val = (int) val / (float) Apple2PortraitCalibration.PORTRAIT_CALIBRATE_NUM_CHOICES;
                            break;
                        case "PORTRAIT_FRAMEBUFFER_POSITION_SCALE": // -> float
                            menuEnum = Apple2PortraitCalibration.States.CALIBRATE_FRAMEBUFFER_POSITION_SCALE;
                            val = (int) val / (float) Apple2PortraitCalibration.PORTRAIT_CALIBRATE_NUM_CHOICES;
                            break;
                        case "PORTRAIT_KEYBOARD_HEIGHT_SCALE": // -> float
                            menuEnum = Apple2PortraitCalibration.States.CALIBRATE_KEYBOARD_HEIGHT_SCALE;
                            val = (int) val / (float) Apple2PortraitCalibration.PORTRAIT_CALIBRATE_NUM_CHOICES;
                            break;

                        case "SPEAKER_VOLUME": // long
                            menuEnum = Apple2AudioSettingsMenu.SETTINGS.SPEAKER_VOLUME;
                            break;
                        case "MOCKINGBOARD_ENABLED": // bool
                            menuEnum = Apple2AudioSettingsMenu.SETTINGS.MOCKINGBOARD_ENABLED;
                            break;
                        case "MOCKINGBOARD_VOLUME": // long
                            menuEnum = Apple2AudioSettingsMenu.SETTINGS.MOCKINGBOARD_VOLUME;
                            break;
                        case "AUDIO_LATENCY": // -> float
                            menuEnum = Apple2AudioSettingsMenu.SETTINGS.AUDIO_LATENCY;
                            val = (int) val / (float) Apple2AudioSettingsMenu.AUDIO_LATENCY_NUM_CHOICES;
                            break;

                        case "TOUCH_MENU_ENABLED": // bool
                            menuEnum = Apple2KeyboardSettingsMenu.SETTINGS.TOUCH_MENU_ENABLED;
                            break;
                        case "KEYBOARD_VISIBILITY_ACTIVE": // -> float
                            menuEnum = Apple2KeyboardSettingsMenu.SETTINGS.KEYBOARD_VISIBILITY_ACTIVE;
                            val = (int) val / (float) ALPHA_SLIDER_NUM_CHOICES;
                            break;
                        case "KEYBOARD_VISIBILITY_INACTIVE": // -> float
                            menuEnum = Apple2KeyboardSettingsMenu.SETTINGS.KEYBOARD_VISIBILITY_INACTIVE;
                            val = (int) val / (float) ALPHA_SLIDER_NUM_CHOICES;
                            break;
                        case "KEYBOARD_GLYPH_SCALE": // long
                            menuEnum = Apple2KeyboardSettingsMenu.SETTINGS.KEYBOARD_GLYPH_SCALE;
                            break;
                        case "KEYBOARD_LOWERCASE_ENABLED": // bool
                            menuEnum = Apple2KeyboardSettingsMenu.SETTINGS.KEYBOARD_ENABLE_LOWERCASE;
                            break;
                        case "KEYBOARD_CLICK_ENABLED": // bool
                            menuEnum = Apple2KeyboardSettingsMenu.SETTINGS.KEYBOARD_ENABLE_CLICK;
                            break;
                        case "KEYBOARD_ALT": // long
                            menuEnum = Apple2KeyboardSettingsMenu.SETTINGS.KEYBOARD_CHOOSE_ALT;
                            break;
                        case "KEYBOARD_ALT_PATH": // String
                            setJSONPref(Apple2Preferences.PREF_DOMAIN_KEYBOARD, "altPath", val);
                            continue;

                        case "JOYSTICK_SWIPEUP_BUTTON":
                            menuEnum = Apple2JoystickSettingsMenu.SETTINGS.JOYSTICK_SWIPEUP_BUTTON;
                            break;
                        case "JOYSTICK_TAP_BUTTON":
                            menuEnum = Apple2JoystickSettingsMenu.SETTINGS.JOYSTICK_TAP_BUTTON;
                            break;
                        case "JOYSTICK_SWIPEDOWN_BUTTON":
                            menuEnum = Apple2JoystickSettingsMenu.SETTINGS.JOYSTICK_SWIPEDOWN_BUTTON;
                            break;
                        case "JOYSTICK_VISIBILITY": // boolean
                            menuEnum = Apple2JoystickSettingsMenu.JoystickAdvanced.SETTINGS.JOYSTICK_VISIBILITY;
                            break;
                        case "JOYSTICK_BUTTON_THRESHOLD": // -> float
                            menuEnum = Apple2JoystickSettingsMenu.JoystickAdvanced.SETTINGS.JOYSTICK_BUTTON_THRESHOLD;
                            val = (int) val * Apple2JoystickSettingsMenu.getJoystickButtonSwitchThresholdScale(activity);
                            break;
                        case "JOYSTICK_DIVIDER": // -> float
                            Apple2Preferences.setJSONPref(Apple2Preferences.PREF_DOMAIN_JOYSTICK, Apple2JoystickCalibration.PREF_SCREEN_DIVISION, (int) val / (float) Apple2JoystickCalibration.JOYSTICK_DIVIDER_NUM_CHOICES);
                            continue;
                        case "JOYSTICK_AXIS_ON_LEFT": // bool
                            menuEnum = Apple2JoystickSettingsMenu.JoystickAdvanced.SETTINGS.JOYSTICK_AXIS_ON_LEFT;
                            break;
                        case "JOYSTICK_TAPDELAY": // -> float
                            menuEnum = Apple2JoystickSettingsMenu.SETTINGS.JOYSTICK_TAPDELAY;
                            val = ((int) val / (float) Apple2JoystickSettingsMenu.TAPDELAY_NUM_CHOICES * Apple2JoystickSettingsMenu.TAPDELAY_SCALE);
                            break;
                        case "JOYSTICK_AZIMUTH_VISIBILITY": // boolean
                            continue; // removed
                        case "JOYSTICK_AXIS_SENSITIVIY": // -> float
                            menuEnum = Apple2JoystickSettingsMenu.JoystickAdvanced.SETTINGS.JOYSTICK_AXIS_SENSITIVITY;
                            final int pivot = Apple2JoystickSettingsMenu.JOYSTICK_AXIS_SENSITIVITY_DEC_NUMCHOICES;
                            float sensitivity = 1.f;
                            if ((int) val < pivot) {
                                int decAmount = (pivot - (int) val);
                                sensitivity -= (Apple2JoystickSettingsMenu.JOYSTICK_AXIS_SENSITIVITY_DEC_STEP * decAmount);
                            } else if ((int) val > pivot) {
                                int incAmount = ((int) val - pivot);
                                sensitivity += (Apple2JoystickSettingsMenu.JOYSTICK_AXIS_SENSITIVITY_INC_STEP * incAmount);
                            }
                            val = sensitivity;
                            break;

                        case "CURRENT_DISK_PATH": // String
                            menuEnum = Apple2DisksMenu.SETTINGS.CURRENT_DISK_SEARCH_PATH;
                            try {
                                val = new JSONArray((String) val);
                            } catch (JSONException e) {
                                Log.v(TAG, "JSON error parsing disk path : " + e);
                                continue;
                            }
                            break;
                        case "CURRENT_DRIVE_A_BUTTON": // bool
                            menuEnum = Apple2DisksMenu.SETTINGS.CURRENT_DRIVE_A;
                            break;
                        case "CURRENT_DISK_A": // String
                            menuEnum = Apple2DisksMenu.SETTINGS.CURRENT_DISK_PATH_A;
                            break;
                        case "CURRENT_DISK_A_RO": // bool
                            menuEnum = Apple2DisksMenu.SETTINGS.CURRENT_DISK_PATH_A_RO;
                            break;
                        case "CURRENT_DISK_B": // String
                            menuEnum = Apple2DisksMenu.SETTINGS.CURRENT_DISK_PATH_B;
                            break;
                        case "CURRENT_DISK_B_RO": // bool
                            menuEnum = Apple2DisksMenu.SETTINGS.CURRENT_DISK_PATH_B_RO;
                            break;

                        case "CURRENT_DISK_RO_BUTTON": // bool
                            menuEnum = Apple2DisksMenu.SETTINGS.CURRENT_DISK_RO_BUTTON;
                            break;
                        case "CURRENT_TOUCH_DEVICE": // long
                            menuEnum = Apple2SettingsMenu.SETTINGS.CURRENT_INPUT;
                            break;

                        case "CRASH_CHECK": // boolean
                            menuEnum = Apple2SettingsMenu.SETTINGS.CRASH;
                            break;
                        case "SHOW_DISK_OPERATIONS": // bool
                            menuEnum = Apple2SettingsMenu.SETTINGS.SHOW_DISK_OPERATIONS;
                            break;

                        case "KEYPAD_KEYS": // long
                            menuEnum = Apple2KeypadSettingsMenu.SETTINGS.KEYPAD_CHOOSE_KEYS;
                            if ((int) val != 0) {
                                Apple2KeypadSettingsMenu.KeypadPreset preset = Apple2KeypadSettingsMenu.KeypadPreset.values()[(int) val - 1];
                                preset.apply(activity);
                                keypadPreset = true;
                            }
                            break;
                        case "KEYREPEAT_THRESHOLD": // -> float
                            menuEnum = Apple2KeypadSettingsMenu.KeypadAdvanced.SETTINGS.KEYREPEAT_THRESHOLD;
                            val = (int) val / (float) Apple2KeypadSettingsMenu.KEYREPEAT_NUM_CHOICES;
                            break;
                        case "KEYPAD_TAP_KEY_SCAN":
                            keypadTapScan = (int) val;
                            continue;
                        case "KEYPAD_TAP_KEY_ASCII":
                            keypadTapChar = (int) val;
                            continue;
                        case "KEYPAD_SWIPEDOWN_KEY_SCAN":
                            keypadSwipeDownScan = (int) val;
                            continue;
                        case "KEYPAD_SWIPEDOWN_KEY_ASCII":
                            keypadSwipeDownChar = (int) val;
                            continue;
                        case "KEYPAD_SWIPEUP_KEY_SCAN":
                            keypadSwipeUpScan = (int) val;
                            continue;
                        case "KEYPAD_SWIPEUP_KEY_ASCII":
                            keypadSwipeUpChar = (int) val;
                            continue;

                        case "KEYPAD_NORTHWEST_KEY_SCAN":
                            keypadJSONScans.set(0, val.toString());
                            continue;
                        case "KEYPAD_NORTHWEST_KEY_ASCII":
                            keypadJSONChars.set(0, val.toString());
                            continue;
                        case "KEYPAD_NORTH_KEY_SCAN":
                            keypadJSONScans.set(1, val.toString());
                            continue;
                        case "KEYPAD_NORTH_KEY_ASCII":
                            keypadJSONChars.set(1, val.toString());
                            continue;
                        case "KEYPAD_NORTHEAST_KEY_SCAN":
                            keypadJSONScans.set(2, val.toString());
                            continue;
                        case "KEYPAD_NORTHEAST_KEY_ASCII":
                            keypadJSONChars.set(2, val.toString());
                            continue;

                        case "KEYPAD_WEST_KEY_SCAN":
                            keypadJSONScans.set(3, val.toString());
                            continue;
                        case "KEYPAD_WEST_KEY_ASCII":
                            keypadJSONChars.set(3, val.toString());
                            continue;
                        case "KEYPAD_CENTER_KEY_SCAN":
                            keypadJSONScans.set(4, val.toString());
                            continue;
                        case "KEYPAD_CENTER_KEY_ASCII":
                            keypadJSONChars.set(4, val.toString());
                            continue;
                        case "KEYPAD_EAST_KEY_SCAN":
                            keypadJSONScans.set(5, val.toString());
                            continue;
                        case "KEYPAD_EAST_KEY_ASCII":
                            keypadJSONChars.set(5, val.toString());
                            continue;

                        case "KEYPAD_SOUTHWEST_KEY_SCAN":
                            keypadJSONScans.set(6, val.toString());
                            continue;
                        case "KEYPAD_SOUTHWEST_KEY_ASCII":
                            keypadJSONChars.set(6, val.toString());
                            continue;
                        case "KEYPAD_SOUTH_KEY_SCAN":
                            keypadJSONScans.set(7, val.toString());
                            continue;
                        case "KEYPAD_SOUTH_KEY_ASCII":
                            keypadJSONChars.set(7, val.toString());
                            continue;
                        case "KEYPAD_SOUTHEAST_KEY_SCAN":
                            keypadJSONScans.set(8, val.toString());
                            continue;
                        case "KEYPAD_SOUTHEAST_KEY_ASCII":
                            keypadJSONChars.set(8, val.toString());
                            continue;

                        default:
                            continue;
                    }

                    setJSONPref(menuEnum, val);
                    save(activity);
                } catch (ClassCastException cce) {
                    Log.v(TAG, "" + cce);
                }
            }

            // handle keypad arrays
            if (oldPrefs.size() > 0 && !keypadPreset) {
                Apple2KeypadSettingsMenu.KeypadPreset.saveRosettes(keypadJSONChars, keypadJSONScans);
                Apple2KeypadSettingsMenu.KeypadPreset.saveTouchDownKey(keypadTapChar, keypadTapScan);
                Apple2KeypadSettingsMenu.KeypadPreset.saveSwipeNorthKey(keypadSwipeUpChar, keypadSwipeUpScan);
                Apple2KeypadSettingsMenu.KeypadPreset.saveSwipeSouthKey(keypadSwipeDownChar, keypadSwipeDownScan);
            }
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
            Log.d(TAG, "Oops, could not read JSON file : " + prefsFile);
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
            Log.w(TAG, "Error attempting to pretty-print JSON : " + e);
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
