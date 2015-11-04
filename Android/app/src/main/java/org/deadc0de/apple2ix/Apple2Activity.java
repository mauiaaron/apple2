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
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.media.AudioManager;
import android.net.Uri;
import android.os.Bundle;
import android.os.StrictMode;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;
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
    private final static int MAX_FINGERS = 32;// HACK ...
    private static volatile boolean DEBUG_STRICT = false;

    private Apple2View mView = null;
    private Runnable mGraphicsInitializedRunnable = null;
    private Apple2SplashScreen mSplashScreen = null;
    private Apple2MainMenu mMainMenu = null;
    private Apple2SettingsMenu mSettingsMenu = null;
    private Apple2DisksMenu mDisksMenu = null;

    private ArrayList<Apple2MenuView> mMenuStack = new ArrayList<Apple2MenuView>();
    private ArrayList<AlertDialog> mAlertDialogs = new ArrayList<AlertDialog>();

    private AtomicBoolean mPausing = new AtomicBoolean(false);

    private float[] mXCoords = new float[MAX_FINGERS];
    private float[] mYCoords = new float[MAX_FINGERS];

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

    public final static long NATIVE_TOUCH_HANDLED = (1 << 0);
    public final static long NATIVE_TOUCH_REQUEST_SHOW_MENU = (1 << 1);

    public final static long NATIVE_TOUCH_KEY_TAP = (1 << 4);
    public final static long NATIVE_TOUCH_KBD = (1 << 5);
    public final static long NATIVE_TOUCH_JOY = (1 << 6);
    public final static long NATIVE_TOUCH_MENU = (1 << 7);
    public final static long NATIVE_TOUCH_JOY_KPAD = (1 << 8);

    public final static long NATIVE_TOUCH_INPUT_DEVICE_CHANGED = (1 << 16);
    public final static long NATIVE_TOUCH_CPU_SPEED_DEC = (1 << 17);
    public final static long NATIVE_TOUCH_CPU_SPEED_INC = (1 << 18);

    public final static long NATIVE_TOUCH_ASCII_SCANCODE_SHIFT = 32;
    public final static long NATIVE_TOUCH_ASCII_SCANCODE_MASK = 0xFFFFL;
    public final static long NATIVE_TOUCH_ASCII_MASK = 0xFF00L;
    public final static long NATIVE_TOUCH_SCANCODE_MASK = 0x00FFL;

    private native void nativeOnCreate(String dataDir, int sampleRate, int monoBufferSize, int stereoBufferSize);

    private native void nativeOnKeyDown(int keyCode, int metaState);

    private native void nativeOnKeyUp(int keyCode, int metaState);

    public native void nativeEmulationResume();

    public native void nativeEmulationPause();

    public native void nativeOnQuit();

    public native long nativeOnTouch(int action, int pointerCount, int pointerIndex, float[] xCoords, float[] yCoords);

    public native void nativeReboot();

    public native void nativeChooseDisk(String path, boolean driveA, boolean readOnly);

    public native void nativeEjectDisk(boolean driveA);


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

        String dataDir = Apple2DisksMenu.getDataDir(this);
        nativeOnCreate(dataDir, sampleRate, monoBufferSize, stereoBufferSize);

        showSplashScreen();
        Apple2CrashHandler.getInstance().checkForCrashes(Apple2Activity.this);
        final boolean firstTime = !Apple2Preferences.FIRST_TIME_CONFIGURED.booleanValue(this);
        Apple2Preferences.FIRST_TIME_CONFIGURED.saveBoolean(this, true);

        mGraphicsInitializedRunnable = new Runnable() {
            @Override
            public void run() {
                if (firstTime) {
                    Apple2Preferences.KeypadPreset.IJKM_SPACE.apply(Apple2Activity.this);
                }
                Apple2Preferences.loadPreferences(Apple2Activity.this);
            }
        };

        // first-time initializations
        if (firstTime) {
            Apple2DisksMenu.firstTime(this);
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
    protected void onResume() {
        super.onResume();
        if (sNativeBarfed) {
            Apple2CrashHandler.getInstance().abandonAllHope(this, sNativeBarfedThrowable);
            return;
        }

        Log.d(TAG, "onResume()");
        showSplashScreen();
        Apple2CrashHandler.getInstance().checkForCrashes(Apple2Activity.this); // NOTE : needs to be called again to clean-up
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

        Log.d(TAG, "onPause()");
        if (mView != null) {
            mView.onPause();
        }

        // Apparently not good to leave popup/dialog windows showing when backgrounding.
        // Dismiss these popups to avoid android.view.WindowLeaked issues
        synchronized (this) {
            dismissAllMenus();
            nativeEmulationPause();
        }

        mPausing.set(false);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (sNativeBarfed) {
            return true;
        }
        if ((keyCode == KeyEvent.KEYCODE_VOLUME_DOWN) || (keyCode == KeyEvent.KEYCODE_VOLUME_MUTE) || (keyCode == KeyEvent.KEYCODE_VOLUME_UP)) {
            return false;
        }
        nativeOnKeyDown(keyCode, event.getMetaState());
        return true;
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        if (sNativeBarfed) {
            return true;
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
            return false;
        } else {
            nativeOnKeyUp(keyCode, event.getMetaState());
            return true;
        }
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        do {

            if (sNativeBarfed) {
                break;
            }
            if (mMainMenu == null) {
                break;
            }

            Apple2MenuView apple2MenuView = peekApple2View();
            if ((apple2MenuView != null) && (!apple2MenuView.isCalibrating())) {
                break;
            }

            //printSamples(event);
            int action = event.getActionMasked();
            int pointerIndex = event.getActionIndex();
            int pointerCount = event.getPointerCount();
            for (int i = 0; i < pointerCount/* && i < MAX_FINGERS */; i++) {
                mXCoords[i] = event.getX(i);
                mYCoords[i] = event.getY(i);
            }

            long nativeFlags = nativeOnTouch(action, pointerCount, pointerIndex, mXCoords, mYCoords);

            if ((nativeFlags & NATIVE_TOUCH_HANDLED) == 0) {
                break;
            }

            if ((nativeFlags & NATIVE_TOUCH_REQUEST_SHOW_MENU) != 0) {
                mMainMenu.show();
            }

            if ((nativeFlags & NATIVE_TOUCH_KEY_TAP) != 0) {
                if (Apple2Preferences.KEYBOARD_CLICK_ENABLED.booleanValue(this)) {
                    AudioManager am = (AudioManager) getSystemService(AUDIO_SERVICE);
                    if (am != null) {
                        am.playSoundEffect(AudioManager.FX_KEY_CLICK);
                    }
                }

                if ((apple2MenuView != null) && apple2MenuView.isCalibrating()) {
                    long asciiScancodeLong = nativeFlags & (NATIVE_TOUCH_ASCII_SCANCODE_MASK << NATIVE_TOUCH_ASCII_SCANCODE_SHIFT);
                    int asciiInt = (int) (asciiScancodeLong >> (NATIVE_TOUCH_ASCII_SCANCODE_SHIFT + 8));
                    int scancode = (int) ((asciiScancodeLong >> NATIVE_TOUCH_ASCII_SCANCODE_SHIFT) & 0xFFL);
                    char ascii = (char) asciiInt;
                    apple2MenuView.onKeyTapCalibrationEvent(ascii, scancode);
                }
            }

            if ((nativeFlags & NATIVE_TOUCH_MENU) == 0) {
                break;
            }

            // handle menu-specific actions

            if ((nativeFlags & NATIVE_TOUCH_INPUT_DEVICE_CHANGED) != 0) {
                Apple2Preferences.TouchDeviceVariant nextVariant;
                if ((nativeFlags & NATIVE_TOUCH_KBD) != 0) {
                    nextVariant = Apple2Preferences.TouchDeviceVariant.KEYBOARD;
                } else if ((nativeFlags & NATIVE_TOUCH_JOY) != 0) {
                    nextVariant = Apple2Preferences.TouchDeviceVariant.JOYSTICK;
                } else if ((nativeFlags & NATIVE_TOUCH_JOY_KPAD) != 0) {
                    nextVariant = Apple2Preferences.TouchDeviceVariant.JOYSTICK_KEYPAD;
                } else {
                    int touchDevice = Apple2Preferences.nativeGetCurrentTouchDevice();
                    nextVariant = Apple2Preferences.TouchDeviceVariant.next(touchDevice);
                }
                Apple2Preferences.CURRENT_TOUCH_DEVICE.saveTouchDevice(this, nextVariant);
            } else if ((nativeFlags & NATIVE_TOUCH_CPU_SPEED_DEC) != 0) {
                int percentSpeed = Apple2Preferences.nativeGetCPUSpeed();
                if (percentSpeed > 400) { // HACK: max value from native side
                    percentSpeed = 375;
                } else if (percentSpeed > 100) {
                    percentSpeed -= 25;
                } else {
                    percentSpeed -= 5;
                }
                Apple2Preferences.CPU_SPEED_PERCENT.saveInt(this, percentSpeed);
            } else if ((nativeFlags & NATIVE_TOUCH_CPU_SPEED_INC) != 0) {
                int percentSpeed = Apple2Preferences.nativeGetCPUSpeed();
                if (percentSpeed >= 100) {
                    percentSpeed += 25;
                } else {
                    percentSpeed += 5;
                }
                Apple2Preferences.CPU_SPEED_PERCENT.saveInt(this, percentSpeed);
            }
        } while (false);

        return super.onTouchEvent(event);
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

                    Apple2Preferences.CURRENT_DISK_A_RO.saveBoolean(Apple2Activity.this, true);
                    final int len = diskPath.length();
                    final String suffix = diskPath.substring(len - 3, len);
                    if (suffix.equalsIgnoreCase(".gz")) { // HACK FIXME TODO : small amount of code duplication of Apple2DisksMenu
                        diskPath = diskPath.substring(0, len - 3);
                    }
                    Apple2Preferences.CURRENT_DISK_A.saveString(Apple2Activity.this, diskPath);

                    while (mDisksMenu.popPathStack() != null) {
                        /* ... */
                    }

                    File storageDir = Apple2DisksMenu.getExternalStorageDirectory();
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

    private void showSplashScreen() {
        if (mSplashScreen != null) {
            return;
        }
        mSplashScreen = new Apple2SplashScreen(this, /*dismissable:*/true);
        mSplashScreen.show();
    }

    private void setupGLView() {

        boolean glViewFirstTime = false;
        if (mView == null) {
            glViewFirstTime = true;
            mView = new Apple2View(this, mGraphicsInitializedRunnable);
            mGraphicsInitializedRunnable = null;
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
        mMenuStack.add(apple2MenuView);
        View menuView = apple2MenuView.getView();
        nativeEmulationPause();
        addContentView(menuView, new FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT));
    }

    public synchronized Apple2MenuView popApple2View() {
        int lastIndex = mMenuStack.size() - 1;
        if (lastIndex < 0) {
            return null;
        }

        Apple2MenuView apple2MenuView = mMenuStack.remove(lastIndex);
        _disposeApple2View(apple2MenuView);
        return apple2MenuView;
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
                    nativeEmulationResume();
                }
            }
        }
    }

    public void maybeResumeCPU() {
        if (mMenuStack.size() == 0 && !mPausing.get()) {
            nativeEmulationResume();
        }
    }

    public void maybeQuitApp() {
        nativeEmulationPause();
        AlertDialog quitDialog = new AlertDialog.Builder(this).setIcon(R.drawable.ic_launcher).setCancelable(true).setTitle(R.string.quit_really).setMessage(R.string.quit_warning).setPositiveButton(R.string.ok, new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                nativeOnQuit();
                Apple2Activity.this.finish();
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
        }).setNegativeButton(R.string.no, null).create();
        registerAndShowDialog(quitDialog);
    }

    public void maybeReboot() {
        nativeEmulationPause();
        AlertDialog rebootDialog = new AlertDialog.Builder(this).setIcon(R.drawable.ic_launcher).setCancelable(true).setTitle(R.string.reboot_really).setMessage(R.string.reboot_warning).setPositiveButton(R.string.ok, new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                nativeReboot();
                Apple2Activity.this.mMainMenu.dismiss();
            }
        }).setNegativeButton(R.string.no, null).create();
        registerAndShowDialog(rebootDialog);
    }
}
