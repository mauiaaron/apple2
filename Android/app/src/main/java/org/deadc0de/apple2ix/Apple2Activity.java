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
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.StrictMode;
import android.util.Log;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.Toast;

import java.io.File;
import java.util.ArrayList;
import java.util.Collections;
import java.util.StringTokenizer;
import java.util.concurrent.atomic.AtomicBoolean;

import org.deadc0de.apple2ix.basic.BuildConfig;
import org.deadc0de.apple2ix.basic.R;

public class Apple2Activity extends Activity {

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

    private static native void nativeSaveState(String path);

    private static native String nativeLoadState(String path);

    private static native boolean nativeEmulationResume();

    private static native boolean nativeEmulationPause();

    private static native void nativeOnQuit();

    private static native void nativeReboot(int resetState);

    public final static boolean isNativeBarfed() {
        return sNativeBarfed;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
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

        Log.e(TAG, "onCreate()");

        // placeholder view on initial launch
        if (mView == null) {
            setContentView(new View(this));
        }

        Apple2CrashHandler.getInstance().initializeAndSetCustomExceptionHandler(this);
        if (sNativeBarfed) {
            Log.e(TAG, "NATIVE BARFED...", sNativeBarfedThrowable);
            return;
        }

        int sampleRate = DevicePropertyCalculator.getRecommendedSampleRate(this);
        int monoBufferSize = DevicePropertyCalculator.getRecommendedBufferSize(this, /*isStereo:*/false);
        int stereoBufferSize = DevicePropertyCalculator.getRecommendedBufferSize(this, /*isStereo:*/true);
        Log.d(TAG, "Device sampleRate:" + sampleRate + " mono bufferSize:" + monoBufferSize + " stereo bufferSize:" + stereoBufferSize);

        String dataDir = Apple2Utils.getDataDir(this);
        nativeOnCreate(dataDir, sampleRate, monoBufferSize, stereoBufferSize);

        // NOTE: ordering here is important!
        Apple2Preferences.load(this);
        final boolean firstTime = Apple2Preferences.migrate(this);
        mSwitchingToPortrait.set(false);
        boolean switchingToPortrait = Apple2VideoSettingsMenu.SETTINGS.applyLandscapeMode(this);
        Apple2Preferences.sync(this, null);

        Apple2DisksMenu.insertDisk((String) Apple2Preferences.getJSONPref(Apple2DisksMenu.SETTINGS.CURRENT_DISK_PATH_A), /*driveA:*/true, (boolean) Apple2Preferences.getJSONPref(Apple2DisksMenu.SETTINGS.CURRENT_DISK_PATH_A_RO));
        Apple2DisksMenu.insertDisk((String) Apple2Preferences.getJSONPref(Apple2DisksMenu.SETTINGS.CURRENT_DISK_PATH_B), /*driveA:*/false, (boolean) Apple2Preferences.getJSONPref(Apple2DisksMenu.SETTINGS.CURRENT_DISK_PATH_B_RO));

        showSplashScreen(!firstTime);

        // Is there a way to persist the user orientation setting such that we launch in the previously set orientation and avoid  get multiple onCreate() onResume()?! ... Android lifecycle edge cases are so damn kludgishly annoying ...
        mSwitchingToPortrait.set(switchingToPortrait);
        if (!switchingToPortrait) {
            Apple2CrashHandler.getInstance().checkForCrashes(this);
        }

        boolean extperm = true;
        if (android.os.Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            // On Marshmallow+ specifically ask for permission to read/write storage
            String readPermission = Manifest.permission.READ_EXTERNAL_STORAGE;
            String writePermission = Manifest.permission.WRITE_EXTERNAL_STORAGE;
            int hasReadPermission = checkSelfPermission(readPermission);
            int hasWritePermission = checkSelfPermission(writePermission);
            ArrayList<String> permissions = new ArrayList<String>();
            if (hasReadPermission != PackageManager.PERMISSION_GRANTED) {
                permissions.add(readPermission);
            }
            if (hasWritePermission != PackageManager.PERMISSION_GRANTED) {
                permissions.add(writePermission);
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
                    }
                    mSplashScreen.setDismissable(true);
                    Log.d(TAG, "Finished first time copying...");
                }
            }).start();
        }

        mSettingsMenu = new Apple2SettingsMenu(this);
        mDisksMenu = new Apple2DisksMenu(this);

        Intent intent = getIntent();
        String path = null;
        if (intent != null) {
            Uri data = intent.getData();
            if (data != null) {
                path = data.getPath();
            }
        }
        if (path != null && Apple2DisksMenu.hasDiskExtension(path)) {
            handleInsertDiskIntent(path);
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
                // this will force copying APK files (now that we have permission
                Apple2Utils.exposeAPKAssetsToExternal(Apple2Activity.this);
            } // else ... we keep nagging on app startup ...
        } else {
            super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (sNativeBarfed) {
            Apple2CrashHandler.getInstance().abandonAllHope(this, sNativeBarfedThrowable);
            return;
        }

        Log.d(TAG, "onResume()");
        showSplashScreen(/*dismissable:*/true);
        if (!mSwitchingToPortrait.get()) {
            Apple2CrashHandler.getInstance().checkForCrashes(this); // NOTE : needs to be called again to clean-up
        }
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
            Log.d(TAG, "Letting native save preferences...");
        }

        Log.d(TAG, "onPause()");
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

    public Apple2MainMenu getMainMenu() {
        return mMainMenu;
    }

    public synchronized Apple2DisksMenu getDisksMenu() {
        return mDisksMenu;
    }

    public synchronized Apple2SettingsMenu getSettingsMenu() {
        return mSettingsMenu;
    }

    private void handleInsertDiskIntent(final String path) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                synchronized (Apple2Activity.this) {
                    if (mMainMenu == null) {
                        return;
                    }
                    String diskPath = path;
                    File diskFile = new File(diskPath);
                    if (!diskFile.canRead()) {
                        Toast.makeText(Apple2Activity.this, Apple2Activity.this.getString(R.string.disk_insert_could_not_read), Toast.LENGTH_SHORT).show();
                        return;
                    }

                    Apple2Preferences.setJSONPref(Apple2DisksMenu.SETTINGS.CURRENT_DISK_PATH_A_RO, true);
                    final int len = diskPath.length();
                    final String suffix = diskPath.substring(len - 3, len);
                    if (suffix.equalsIgnoreCase(".gz")) { // HACK FIXME TODO : small amount of code duplication of Apple2DisksMenu
                        diskPath = diskPath.substring(0, len - 3);
                    }

                    Apple2DisksMenu.insertDisk(diskPath, /*driveA:*/true, /*readOnly:*/true);

                    while (mDisksMenu.popPathStack() != null) {
                        /* ... */
                    }

                    File storageDir = Apple2Utils.getExternalStorageDirectory(Apple2Activity.this);
                    if (storageDir != null) {
                        String storagePath = storageDir.getAbsolutePath();
                        if (diskPath.contains(storagePath)) {
                            diskPath = diskPath.replace(storagePath + File.separator, "");
                            mDisksMenu.pushPathStack(storagePath);
                        }
                    }
                    StringTokenizer tokenizer = new StringTokenizer(diskPath, File.separator);
                    while (tokenizer.hasMoreTokens()) {
                        String token = tokenizer.nextToken();
                        if (token.equals("")) {
                            continue;
                        }
                        if (Apple2DisksMenu.hasDiskExtension(token)) {
                            continue;
                        }
                        mDisksMenu.pushPathStack(token);
                    }

                    Toast.makeText(Apple2Activity.this, Apple2Activity.this.getString(R.string.disk_insert_toast), Toast.LENGTH_SHORT).show();
                }
            }
        });
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
            boolean previouslyPaused = nativeEmulationResume();
            if (BuildConfig.DEBUG && !previouslyPaused) {
                throw new RuntimeException("expecting native CPU thread to have been paused");
            }
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

    public void saveState(String stateFile) {
        nativeSaveState(stateFile);
    }

    public String loadState(String stateFile) {
        return Apple2Activity.nativeLoadState(stateFile);
    }

    public void quitEmulator() {
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
}
