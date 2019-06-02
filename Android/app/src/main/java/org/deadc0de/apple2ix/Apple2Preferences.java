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
        setJSONPref(PREF_DOMAIN_INTERFACE, PREF_RELEASE_NOTES, false);

        if (versionCode < 22) {
            // force upgrade to defaults here ...
            setJSONPref(Apple2VideoSettingsMenu.SETTINGS.SHOW_HALF_SCANLINES, Apple2VideoSettingsMenu.SETTINGS.SHOW_HALF_SCANLINES.getPrefDefault());
            setJSONPref(Apple2VideoSettingsMenu.SETTINGS.COLOR_MODE_CONFIGURE, Apple2VideoSettingsMenu.SETTINGS.COLOR_MODE_CONFIGURE.getPrefDefault());
        }

        Apple2Utils.migrateToExternalStorage(activity);

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
