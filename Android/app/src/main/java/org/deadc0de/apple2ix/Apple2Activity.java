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

import android.Manifest;
import android.content.DialogInterface;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.StrictMode;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.Toast;

import org.deadc0de.apple2ix.basic.BuildConfig;
import org.deadc0de.apple2ix.basic.R;
import org.json.JSONObject;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.InterruptedIOException;
import java.io.Reader;
import java.util.ArrayList;
import java.util.Collections;
import java.util.concurrent.atomic.AtomicBoolean;

public class Apple2Activity extends AppCompatActivity implements Apple2DiskChooserActivity.Callback, Apple2EmailerActivity.Callback
{

    private final static String TAG = "Apple2Activity";

    private static volatile boolean DEBUG_STRICT = false;

    private Apple2View mView = null;
    private Apple2SplashScreen mSplashScreen = null;
    private Apple2MainMenu mMainMenu = null;
    private Apple2SettingsMenu mSettingsMenu = null;
    private Apple2DisksMenu mDisksMenu = null;

    private ArrayList<Apple2MenuView> mMenuStack = new ArrayList<Apple2MenuView>();
    private ArrayList<AlertDialog> mAlertDialogs = new ArrayList<AlertDialog>();

    private AtomicBoolean mPausing = new AtomicBoolean(false);
    private AtomicBoolean mSwitchingToPortrait = new AtomicBoolean(false);

    private static DiskArgs sDisksChosen = null;

    // non-null if we failed to load/link the native code ... likely we are running on some bizarre 'droid variant
    private static Throwable sNativeBarfedThrowable = null;
    private static boolean sNativeBarfed = false;

    static {
        try {
            System.loadLibrary("apple2ix");
        } catch (Throwable barf) {
            sNativeBarfed = true;
            sNativeBarfedThrowable = barf;
        }
    }

    public final static int REQUEST_PERMISSION_RWSTORE = 42;


    private static native void nativeOnCreate(String dataDir, int sampleRate, int monoBufferSize, int stereoBufferSize);

    private static native void nativeOnKeyDown(int keyCode, int metaState);

    private static native void nativeOnKeyUp(int keyCode, int metaState);

    private static native void nativeSaveState(String saveStateJson);

    private static native String nativeStateExtractDiskPaths(String extractStateJson);

    private static native String nativeLoadState(String loadStateJson);

    private static native boolean nativeEmulationResume();

    private static native boolean nativeEmulationPause();

    private static native void nativeOnQuit();

    private static native void nativeReboot(int resetState);

    private static native void nativeLogMessage(String jsonStr);

    public final static boolean isNativeBarfed() {
        return sNativeBarfed;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        if (Apple2Activity.DEBUG_STRICT && BuildConfig.DEBUG) {
            StrictMode.setThreadPolicy(new StrictMode.ThreadPolicy.Builder()
                    .detectDiskReads()
                    .detectDiskWrites()
                    .detectAll()
                    .penaltyLog()
                    .build());
            StrictMode.setVmPolicy(new StrictMode.VmPolicy.Builder()
                    .detectLeakedSqlLiteObjects()
                    /*.detectLeakedClosableObjects()*/
                    .penaltyLog()
                    .penaltyDeath()
                    .build());
        }
        super.onCreate(savedInstanceState);

        logMessage(LogType.ERROR, TAG, "onCreate()");

        // placeholder view on initial launch
        if (mView == null) {
            setContentView(new View(this));
        }

        Apple2CrashHandler.getInstance().initializeAndSetCustomExceptionHandler(this);
        if (sNativeBarfed) {
            logMessage(LogType.ERROR, TAG, "NATIVE BARFED : " + sNativeBarfedThrowable.getMessage());
            return;
        }

        int sampleRate = DevicePropertyCalculator.getRecommendedSampleRate(this);
        int monoBufferSize = DevicePropertyCalculator.getRecommendedBufferSize(this, /*isStereo:*/false);
        int stereoBufferSize = DevicePropertyCalculator.getRecommendedBufferSize(this, /*isStereo:*/true);
        logMessage(LogType.DEBUG, TAG, "Device sampleRate:" + sampleRate + " mono bufferSize:" + monoBufferSize + " stereo bufferSize:" + stereoBufferSize);

        String dataDir = Apple2Utils.getDataDir(this);
        nativeOnCreate(dataDir, sampleRate, monoBufferSize, stereoBufferSize);

        // NOTE: ordering here is important!
        Apple2Preferences.load(this);
        final boolean firstTime = Apple2Preferences.migrate(this);
        mSwitchingToPortrait.set(false);
        boolean switchingToPortrait = Apple2VideoSettingsMenu.SETTINGS.applyLandscapeMode(this);
        Apple2Preferences.sync(this, null);

        Apple2DisksMenu.insertDisk(this, new DiskArgs((String) Apple2Preferences.getJSONPref(Apple2DisksMenu.SETTINGS.CURRENT_DISK_PATH_A)), /*driveA:*/true, /*isReadOnly:*/(boolean) Apple2Preferences.getJSONPref(Apple2DisksMenu.SETTINGS.CURRENT_DISK_PATH_A_RO), /*onLaunch:*/true);
        Apple2DisksMenu.insertDisk(this, new DiskArgs((String) Apple2Preferences.getJSONPref(Apple2DisksMenu.SETTINGS.CURRENT_DISK_PATH_B)), /*driveA:*/false, /*isReadOnly:*/(boolean) Apple2Preferences.getJSONPref(Apple2DisksMenu.SETTINGS.CURRENT_DISK_PATH_B_RO), /*onLaunch:*/true);

        showSplashScreen(!firstTime);

        // Is there a way to persist the user orientation setting such that we launch in the previously set orientation and avoid getting multiple onCreate() onResume()?! ... Android lifecycle edge cases are so damn kludgishly annoying ...
        mSwitchingToPortrait.set(switchingToPortrait);

        Apple2CrashHandler.getInstance().checkForCrashes(this);

        boolean extperm = true;
        if (android.os.Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            // On Marshmallow+ specifically ask for permission to read/write storage
            int hasReadPermission = checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE);
            int hasWritePermission = checkSelfPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE);
            ArrayList<String> permissions = new ArrayList<String>();
            if (hasReadPermission != PackageManager.PERMISSION_GRANTED) {
                permissions.add(Manifest.permission.READ_EXTERNAL_STORAGE);
            }
            if (hasWritePermission != PackageManager.PERMISSION_GRANTED) {
                permissions.add(Manifest.permission.WRITE_EXTERNAL_STORAGE);
            }
            if (!permissions.isEmpty()) {
                extperm = false;
                String[] params = permissions.toArray(new String[permissions.size()]);
                requestPermissions(params, REQUEST_PERMISSION_RWSTORE);
            }
        }

        // first-time initializations
        final boolean externalStoragePermission = extperm;
        if (firstTime) {
            new Thread(new Runnable() {
                @Override
                public void run() {
                    Apple2Utils.exposeAPKAssets(Apple2Activity.this);
                    if (externalStoragePermission) {
                        Apple2Utils.exposeAPKAssetsToExternal(Apple2Activity.this);
                        logMessage(LogType.DEBUG, TAG, "Finished first time copying #1...");

                        if (!(boolean)Apple2Preferences.getJSONPref(Apple2Preferences.PREF_DOMAIN_INTERFACE, Apple2Preferences.PREF_RELEASE_NOTES, false)) {
                            Runnable myRunnable = new Runnable() {
                                @Override
                                public void run() {
                                    showReleaseNotes();
                                }
                            };
                            new Handler(Apple2Activity.this.getMainLooper()).post(myRunnable);
                        }
                    }

                    mSplashScreen.setDismissable(true);
                }
            }).start();
        }

        mSettingsMenu = new Apple2SettingsMenu(this);
        mDisksMenu = new Apple2DisksMenu(this);
    }

    @Override
    public void onEmailerFinished() {
        Apple2CrashHandler.getInstance().cleanCrashData(this);
    }

    @Override
    public void onDisksChosen(DiskArgs args) {
        final String name = args.name;
        if (Apple2DisksMenu.hasDiskExtension(name) || Apple2DisksMenu.hasStateExtension(name)) {
            sDisksChosen = args;
        } else if (!name.equals("")) {
            Toast.makeText(this, R.string.disk_insert_toast_cannot, Toast.LENGTH_SHORT).show();
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        // We should already be gracefully handling the case where user denies access.
        if (requestCode == REQUEST_PERMISSION_RWSTORE) {
            boolean grantedPermissions = true;
            for (int grant : grantResults) {
                if (grant == PackageManager.PERMISSION_DENIED) {
                    grantedPermissions = false;
                    break;
                }
            }
            if (grantedPermissions) {
                // perform migration(s) and assets exposure now
                Apple2Utils.migrateToExternalStorage(Apple2Activity.this);
                Apple2Utils.exposeAPKAssetsToExternal(Apple2Activity.this);
                logMessage(LogType.DEBUG, TAG, "Finished first time copying #2...");
            } // else ... we keep nagging on app startup ...
        } else {
            super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        }

        if (!(boolean)Apple2Preferences.getJSONPref(Apple2Preferences.PREF_DOMAIN_INTERFACE, Apple2Preferences.PREF_RELEASE_NOTES, false)) {
            showReleaseNotes();
        }
    }

    @Override
    protected void onResume() {
        super.onResume();

        do {
            if (sNativeBarfed) {
                Apple2CrashHandler.getInstance().abandonAllHope(this, sNativeBarfedThrowable);
                break;
            }

            logMessage(LogType.DEBUG, TAG, "onResume()");
            showSplashScreen(/*dismissable:*/true);

            if (mDisksMenu == null) {
                break;
            }

            if (sDisksChosen == null) {
                break;
            }

            DiskArgs args = sDisksChosen;
            sDisksChosen = null;

            if (args.pfd == null) {
                break;
            }

            String name = args.name;

            if (Apple2DisksMenu.hasStateExtension(name)) {
                boolean restored = Apple2MainMenu.restoreEmulatorState(this, args);
                dismissAllMenus();
                if (!restored) {
                    Toast.makeText(this, R.string.state_not_restored, Toast.LENGTH_SHORT).show();
                }
                break;
            }

            final String[] prefices = {"content://com.android.externalstorage.documents/document", "content://com.android.externalstorage.documents", "content://com.android.externalstorage.documents", "content://"};
            for (String prefix : prefices) {
                if (name.startsWith(prefix)) {
                    name = name.substring(prefix.length());
                    break;
                }
            }

            // strip out URL-encoded '/' directory separators
            String nameLower = name.toLowerCase();
            int idx = nameLower.lastIndexOf("%2f", /*fromIndex:*/name.length() - 3);
            if (idx >= 0) {
                name = name.substring(idx + 3);
            }

            mDisksMenu.showDiskInsertionAlertDialog(name, args);

        } while (false);
    }

    @Override
    protected void onPause() {
        super.onPause();
        if (sNativeBarfed) {
            return;
        }

        boolean wasPausing = mPausing.getAndSet(true);
        if (wasPausing) {
            return;
        }

        if (isEmulationPaused()) {
            Apple2Preferences.save(this);
        } else {
            logMessage(LogType.DEBUG, TAG, "Letting native save preferences...");
        }

        logMessage(LogType.INFO, TAG, "onPause() ...");
        if (mView != null) {
            mView.onPause();
        }

        // Apparently not good to leave popup/dialog windows showing when backgrounding.
        // Dismiss these popups to avoid android.view.WindowLeaked issues
        synchronized (this) {
            dismissAllMenus();
            dismissAllMenus(); // 2nd time should full exit calibration mode (if present)
            pauseEmulation();
        }

        mPausing.set(false);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (Apple2Activity.isNativeBarfed()) {
            return super.onKeyDown(keyCode, event);
        }
        if ((keyCode == KeyEvent.KEYCODE_VOLUME_DOWN) || (keyCode == KeyEvent.KEYCODE_VOLUME_MUTE) || (keyCode == KeyEvent.KEYCODE_VOLUME_UP)) {
            return super.onKeyDown(keyCode, event);
        }

        nativeOnKeyDown(keyCode, event.getMetaState());
        return true;
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        if (Apple2Activity.isNativeBarfed()) {
            return super.onKeyUp(keyCode, event);
        }

        if (keyCode == KeyEvent.KEYCODE_BACK) {
            Apple2MenuView apple2MenuView = peekApple2View();
            if (apple2MenuView == null) {
                showMainMenu();
            } else {
                apple2MenuView.dismiss();
            }
            return true;
        } else if (keyCode == KeyEvent.KEYCODE_MENU) {
            showMainMenu();
            return true;
        } else if ((keyCode == KeyEvent.KEYCODE_VOLUME_DOWN) || (keyCode == KeyEvent.KEYCODE_VOLUME_MUTE) || (keyCode == KeyEvent.KEYCODE_VOLUME_UP)) {
            return super.onKeyUp(keyCode, event);
        }

        nativeOnKeyUp(keyCode, event.getMetaState());
        return true;
    }

    public void showMainMenu() {
        if (mMainMenu != null) {
            if (!(mSettingsMenu.isShowing() || mDisksMenu.isShowing())) {
                mMainMenu.show();
            }
        }
    }

    public void showReleaseNotes() {

        Apple2Preferences.setJSONPref(Apple2Preferences.PREF_DOMAIN_INTERFACE, Apple2Preferences.PREF_RELEASE_NOTES, true);
        Apple2Preferences.save(this);

        String releaseNotes = "";
        final int maxAttempts = 5;
        int attempts = 0;
        do {
            InputStream is = null;
            try {
                AssetManager assetManager = getAssets();
                is = assetManager.open("release_notes.txt");

                final int bufferSize = 4096;
                final char[] buffer = new char[bufferSize];
                final StringBuilder out = new StringBuilder();
                Reader in = new InputStreamReader(is, "UTF-8");
                while (true) {
                    int siz = in.read(buffer, 0, buffer.length);
                    if (siz < 0) {
                        break;
                    }
                    out.append(buffer, 0, siz);
                }
                releaseNotes = out.toString();
                break;
            } catch (InterruptedIOException e) {
                /* EINTR, EAGAIN */
            } catch (IOException e) {
                logMessage(LogType.ERROR, TAG, "OOPS could not load release_notes.txt : " + e.getMessage());
            } finally {
                if (is != null) {
                    try {
                        is.close();
                    } catch (IOException e) {
                        // NOOP
                    }
                }
            }
            ++attempts;
        } while (attempts < maxAttempts);

        AlertDialog.Builder builder = new AlertDialog.Builder(Apple2Activity.this).setIcon(R.drawable.ic_launcher).setCancelable(false).setTitle(R.string.release_notes).setMessage(releaseNotes).setPositiveButton(R.string.ok, new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                dialog.dismiss();
            }
        });
        AlertDialog dialog = builder.create();
        registerAndShowDialog(dialog);
    }

    public Apple2MainMenu getMainMenu() {
        return mMainMenu;
    }

    public synchronized Apple2DisksMenu getDisksMenu() {
        return mDisksMenu;
    }

    public synchronized Apple2SettingsMenu getSettingsMenu() {
        return mSettingsMenu;
    }

    public Apple2SplashScreen getSplashScreen() {
        return mSplashScreen;
    }

    private void showSplashScreen(boolean dismissable) {
        if (mSplashScreen != null) {
            return;
        }

        mSplashScreen = new Apple2SplashScreen(this, dismissable);
        mSplashScreen.show();
    }

    private void setupGLView() {

        boolean glViewFirstTime = false;
        if (mView == null) {
            glViewFirstTime = true;
            mView = new Apple2View(this);
            mMainMenu = new Apple2MainMenu(this, mView);
        }

        if (glViewFirstTime) {
            // HACK NOTE : do not blanket setContentView() ... it appears to wedge Gingerbread
            setContentView(mView);
        } else {
            mView.onResume();
        }
    }

    public void registerAndShowDialog(AlertDialog dialog) {
        dialog.show();
        mAlertDialogs.add(dialog);
    }

    public synchronized void pushApple2View(Apple2MenuView apple2MenuView) {
        //
        mMenuStack.add(apple2MenuView);
        View menuView = apple2MenuView.getView();
        pauseEmulation();
        addContentView(menuView, new FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT));
    }

    public synchronized Apple2MenuView peekApple2View() {
        int lastIndex = mMenuStack.size() - 1;
        if (lastIndex < 0) {
            return null;
        }

        return mMenuStack.get(lastIndex);
    }

    public synchronized Apple2MenuView peekApple2View(int index) {
        int lastIndex = mMenuStack.size() - 1;
        if (lastIndex < 0) {
            return null;
        }

        try {
            return mMenuStack.get(index);
        } catch (IndexOutOfBoundsException e) {
            return null;
        }
    }

    public void dismissAllMenus() {
        if (mMainMenu != null) {
            mMainMenu.dismiss();
        }

        for (AlertDialog dialog : mAlertDialogs) {
            dialog.dismiss();
        }
        mAlertDialogs.clear();

        // Get rid of the menu hierarchy
        ArrayList<Apple2MenuView> menuHierarchy = new ArrayList<Apple2MenuView>(mMenuStack);
        Collections.reverse(menuHierarchy);
        for (Apple2MenuView view : menuHierarchy) {
            view.dismissAll();
        }
    }

    public synchronized Apple2MenuView popApple2View(Apple2MenuView apple2MenuView) {
        boolean wasRemoved = mMenuStack.remove(apple2MenuView);
        _disposeApple2View(apple2MenuView);
        return wasRemoved ? apple2MenuView : null;
    }

    private void _disposeApple2View(Apple2MenuView apple2MenuView) {

        boolean dismissedSplashScreen = false;

        // Actually remove View from view hierarchy
        {
            View menuView = apple2MenuView.getView();
            ViewGroup viewGroup = (ViewGroup) menuView.getParent();
            if (viewGroup != null) {
                viewGroup.removeView(menuView);
            }
            if (apple2MenuView instanceof Apple2SplashScreen) { // 20151101 HACK NOTE : use instanceof to avoid edge case where joystick calibration occurred (and thus the splash was already dismissed without proper mView initialization)
                mSplashScreen = null;
                dismissedSplashScreen = true;
            }
        }

        // if no more views on menu stack, resume emulation
        if (mMenuStack.size() == 0) {
            dismissAllMenus(); // NOTE : at this point, this should not be re-entrant into mMenuStack, it should just dismiss lingering popups
            if (!mPausing.get()) {
                if (dismissedSplashScreen) {
                    setupGLView();
                } else {
                    maybeResumeEmulation();
                }
            }
        }
    }

    public boolean isEmulationPaused() {
        boolean mainMenuShowing = (mMainMenu != null && mMainMenu.isShowing());
        boolean menusShowing = (mMenuStack.size() > 0);
        return mainMenuShowing || menusShowing;
    }

    public void maybeResumeEmulation() {
        if (mMenuStack.size() == 0 && !mPausing.get()) {
            Apple2Preferences.sync(this, null);
            nativeEmulationResume();
        }
    }

    public void pauseEmulation() {
        boolean previouslyRunning = nativeEmulationPause();
        if (previouslyRunning) {
            Apple2Preferences.load(this);
        }
    }

    public void rebootEmulation(int resetState) {
        nativeReboot(resetState);
    }

    public void saveState(String saveStateJson) {
        nativeSaveState(saveStateJson);
    }

    public String stateExtractDiskPaths(String extractStateJson) {
        return nativeStateExtractDiskPaths(extractStateJson);
    }

    public String loadState(String loadStateJson) {
        return nativeLoadState(loadStateJson);
    }

    public void quitEmulator() {
        logMessage(LogType.INFO, TAG, "Quitting...");
        nativeOnQuit();
        finish();
        new Runnable() {
            @Override
            public void run() {
                try {
                    Thread.sleep(2000);
                } catch (InterruptedException ex) {
                    // ...
                }
                System.exit(0);
            }
        }.run();
    }

    public enum LogType {
        // Constants match
        VERBOSE(2),
        DEBUG(3),
        INFO(4),
        WARN(5),
        ERROR(6);
        private int type;

        LogType(int type) {
            this.type = type;
        }
    }

    public static void logMessage(LogType type, String tag, String mesg) {
        JSONObject map = new JSONObject();
        try {
            map.put("type", type.type);
            map.put("tag", tag);
            map.put("mesg", mesg);
            nativeLogMessage(map.toString());
        } catch (Exception e) {
            Log.e(TAG, "OOPS: " + e.getMessage());
        }
    }
}
