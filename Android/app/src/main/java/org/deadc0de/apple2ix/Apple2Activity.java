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

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.graphics.Rect;
import android.media.AudioManager;
import android.os.Build;
import android.os.Bundle;
import android.os.StrictMode;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.widget.FrameLayout;

import java.util.ArrayList;

public class Apple2Activity extends Activity {

    private final static String TAG = "Apple2Activity";
    private final static int MAX_FINGERS = 32;// HACK ...

    private boolean mSetUncaughtExceptionHandler = false;

    private Apple2View mView = null;
    private Apple2SplashScreen mSplashScreen = null;
    private Apple2MainMenu mMainMenu = null;
    private ArrayList<Apple2MenuView> mMenuStack = new ArrayList<Apple2MenuView>();
    private AlertDialog mQuitDialog = null;
    private AlertDialog mRebootDialog = null;

    private int mWidth = 0;
    private int mHeight = 0;

    private int mSampleRate = 0;
    private int mMonoBufferSize = 0;
    private int mStereoBufferSize = 0;

    private float[] mXCoords = new float[MAX_FINGERS];
    private float[] mYCoords = new float[MAX_FINGERS];

    static {
        System.loadLibrary("apple2ix");
    }

    public final static long NATIVE_TOUCH_HANDLED = (1 << 0);
    public final static long NATIVE_TOUCH_REQUEST_SHOW_MENU = (1 << 1);

    public final static long NATIVE_TOUCH_KEY_TAP = (1 << 4);
    public final static long NATIVE_TOUCH_KBD = (1 << 5);
    public final static long NATIVE_TOUCH_JOY = (1 << 6);
    public final static long NATIVE_TOUCH_MENU = (1 << 7);

    public final static long NATIVE_TOUCH_INPUT_DEVICE_CHANGED = (1 << 16);
    public final static long NATIVE_TOUCH_CPU_SPEED_DEC = (1 << 17);
    public final static long NATIVE_TOUCH_CPU_SPEED_INC = (1 << 18);

    public final static long NATIVE_TOUCH_ASCII_SCANCODE_SHIFT = 32;
    public final static long NATIVE_TOUCH_ASCII_SCANCODE_MASK = 0xFFFFL;
    public final static long NATIVE_TOUCH_ASCII_MASK = 0xFF00L;
    public final static long NATIVE_TOUCH_SCANCODE_MASK = 0x00FFL;

    private native void nativeOnCreate(String dataDir, int sampleRate, int monoBufferSize, int stereoBufferSize);

    private native void nativeGraphicsInitialized(int width, int height);

    private native void nativeGraphicsChanged(int width, int height);

    private native void nativeOnKeyDown(int keyCode, int metaState);

    private native void nativeOnKeyUp(int keyCode, int metaState);

    private native void nativeOnUncaughtException(String home, String trace);

    public native void nativeOnResume(boolean isSystemResume);

    public native void nativeOnPause(boolean isSystemPause);

    public native void nativeOnQuit();

    public native long nativeOnTouch(int action, int pointerCount, int pointerIndex, float[] xCoords, float[] yCoords);

    public native void nativeReboot();

    public native void nativeRender();

    public native void nativeChooseDisk(String path, boolean driveA, boolean readOnly);

    public native void nativeEjectDisk(boolean driveA);


    private void _setCustomExceptionHandler() {
        if (mSetUncaughtExceptionHandler) {
            return;
        }
        mSetUncaughtExceptionHandler = true;

        final String homeDir = "/data/data/" + this.getPackageName();
        final Thread.UncaughtExceptionHandler defaultExceptionHandler = Thread.getDefaultUncaughtExceptionHandler();
        Thread.setDefaultUncaughtExceptionHandler(new Thread.UncaughtExceptionHandler() {
            @Override
            public void uncaughtException(Thread thread, Throwable t) {
                try {
                    StackTraceElement[] stackTraceElements = t.getStackTrace();
                    StringBuffer traceBuffer = new StringBuffer();

                    // prepend information about this device
                    traceBuffer.append(Build.BRAND);
                    traceBuffer.append("\n");
                    traceBuffer.append(Build.MODEL);
                    traceBuffer.append("\n");
                    traceBuffer.append(Build.MANUFACTURER);
                    traceBuffer.append("\n");
                    traceBuffer.append(Build.DEVICE);
                    traceBuffer.append("\n");
                    traceBuffer.append("Device sample rate:");
                    traceBuffer.append(mSampleRate);
                    traceBuffer.append("\n");
                    traceBuffer.append("Device mono buffer size:");
                    traceBuffer.append(mMonoBufferSize);
                    traceBuffer.append("\n");
                    traceBuffer.append("Device stereo buffer size:");
                    traceBuffer.append(mStereoBufferSize);
                    traceBuffer.append("\n");

                    // now append the actual stack trace
                    traceBuffer.append(t.getClass().getName());
                    traceBuffer.append("\n");
                    final int maxTraceSize = 2048 + 1024 + 512; // probably should keep this less than a standard Linux PAGE_SIZE
                    for (StackTraceElement elt : stackTraceElements) {
                        traceBuffer.append(elt.toString());
                        traceBuffer.append("\n");
                        if (traceBuffer.length() >= maxTraceSize) {
                            break;
                        }
                    }
                    traceBuffer.append("\n");

                    nativeOnUncaughtException(homeDir, traceBuffer.toString());
                } catch (Throwable terminator2) {
                    // Yo dawg, I hear you like exceptions in your exception handler! ...
                }

                defaultExceptionHandler.uncaughtException(thread, t);
            }
        });
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        if (BuildConfig.DEBUG) {
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

        _setCustomExceptionHandler();

        // run first-time initializations
        if (!Apple2Preferences.FIRST_TIME_CONFIGURED.booleanValue(this)) {
            Apple2DisksMenu.firstTime(this);
            Apple2Preferences.KeypadPreset.IJKM_SPACE.apply(this);
        }
        Apple2Preferences.FIRST_TIME_CONFIGURED.saveBoolean(this, true);

        // get device audio parameters for native OpenSLES
        mSampleRate = DevicePropertyCalculator.getRecommendedSampleRate(this);
        mMonoBufferSize = DevicePropertyCalculator.getRecommendedBufferSize(this, /*isStereo:*/false);
        mStereoBufferSize = DevicePropertyCalculator.getRecommendedBufferSize(this, /*isStereo:*/true);
        Log.d(TAG, "Device sampleRate:" + mSampleRate + " mono bufferSize:" + mMonoBufferSize + " stereo bufferSize:" + mStereoBufferSize);

        String dataDir = Apple2DisksMenu.getDataDir(this);
        nativeOnCreate(dataDir, mSampleRate, mMonoBufferSize, mStereoBufferSize);

        // NOTE: load preferences after nativeOnCreate ... native CPU thread should still be paused
        Apple2Preferences.loadPreferences(this);

        mView = new Apple2View(this);
        setContentView(mView);

        // Another Android Annoyance ...
        // Even though we no longer use the system soft keyboard (which would definitely trigger width/height changes to our OpenGL canvas),
        // we still need to listen to dimension changes, because it seems on some janky devices you have an incorrect width/height set when
        // the initial OpenGL onSurfaceChanged() callback occurs.  For now, include this defensive coding...
        mView.getViewTreeObserver().addOnGlobalLayoutListener(new ViewTreeObserver.OnGlobalLayoutListener() {
            public void onGlobalLayout() {
                Rect rect = new Rect();
                mView.getWindowVisibleDisplayFrame(rect);
                int h = rect.height();
                int w = rect.width();
                if (w < h) {
                    // assure landscape dimensions
                    final int w_ = w;
                    w = h;
                    h = w_;
                }
                nativeGraphicsChanged(w, h);
            }
        });
    }

    @Override
    protected void onResume() {
        super.onResume();
        Log.d(TAG, "onResume()");
        mView.onResume();
        nativeOnResume(/*isSystemResume:*/true);
    }

    @Override
    protected void onPause() {
        super.onPause();
        Log.d(TAG, "onPause()");
        mView.onPause();

        // Apparently not good to leave popup/dialog windows showing when backgrounding.
        // Dismiss these popups to avoid android.view.WindowLeaked issues
        // TODO FIXME : need to test/fix other popups generated in other menus ...
        dismissAllMenus();

        nativeOnPause(true);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if ((keyCode == KeyEvent.KEYCODE_VOLUME_DOWN) || (keyCode == KeyEvent.KEYCODE_VOLUME_MUTE) || (keyCode == KeyEvent.KEYCODE_VOLUME_UP)) {
            return false;
        }
        nativeOnKeyDown(keyCode, event.getMetaState());
        return true;
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
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

    private String actionToString(int action) {
        switch (action) {
            case MotionEvent.ACTION_CANCEL:
                return "CANCEL:" + action;
            case MotionEvent.ACTION_DOWN:
                return "DOWN:" + action;
            case MotionEvent.ACTION_MOVE:
                return "MOVE:" + action;
            case MotionEvent.ACTION_UP:
                return "UP:" + action;
            case MotionEvent.ACTION_POINTER_DOWN:
                return "PDOWN:" + action;
            case MotionEvent.ACTION_POINTER_UP:
                return "PUP:" + action;
            default:
                return "UNK:" + action;
        }
    }

    private void printSamples(MotionEvent ev) {
        final int historySize = ev.getHistorySize();
        final int pointerCount = ev.getPointerCount();

        /*
        for (int h = 0; h < historySize; h++) {
            Log.d(TAG, "Event "+ev.getAction().toString()+" at historical time "+ev.getHistoricalEventTime(h)+" :");
            for (int p = 0; p < pointerCount; p++) {
                Log.d(TAG, "  pointer "+ev.getPointerId(p)+": ("+ev.getHistoricalX(p, h)+","+ev.getHistoricalY(p, h)+")");
            }
        }
        */
        int pointerIndex = ev.getActionIndex();

        Log.d(TAG, "Event " + actionToString(ev.getActionMasked()) + " for " + pointerIndex + " at time " + ev.getEventTime() + " :");
        for (int p = 0; p < pointerCount; p++) {
            Log.d(TAG, "  pointer " + ev.getPointerId(p) + ": (" + ev.getX(p) + "," + ev.getY(p) + ")");
        }
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        do {

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
                AudioManager am = (AudioManager) getSystemService(AUDIO_SERVICE);
                if (am != null) {
                    am.playSoundEffect(AudioManager.FX_KEY_CLICK);
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
                int touchDevice = Apple2Preferences.nativeGetCurrentTouchDevice();
                Apple2Preferences.TouchDeviceVariant nextVariant = Apple2Preferences.TouchDeviceVariant.next(touchDevice);
                Apple2Preferences.CURRENT_TOUCH_DEVICE.saveTouchDevice(this, nextVariant);
            }
            if ((nativeFlags & NATIVE_TOUCH_CPU_SPEED_DEC) != 0) {
                int percentSpeed = Apple2Preferences.nativeGetCPUSpeed();
                if (percentSpeed > 100) {
                    percentSpeed -= 25;
                } else {
                    percentSpeed -= 5;
                }
                Apple2Preferences.CPU_SPEED_PERCENT.saveInt(this, percentSpeed);
            }
            if ((nativeFlags & NATIVE_TOUCH_CPU_SPEED_INC) != 0) {
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

    public void graphicsInitialized(int w, int h) {
        if (mMainMenu == null) {
            mMainMenu = new Apple2MainMenu(this, mView);
        }

        if (w < h) {
            // assure landscape dimensions
            final int w_ = w;
            w = h;
            h = w_;
        }

        mWidth = w;
        mHeight = h;

        // tell native about this...
        nativeGraphicsInitialized(w, h);

        showSplashScreen();
    }

    public void showMainMenu() {
        if (mMainMenu != null) {
            Apple2SettingsMenu settingsMenu = mMainMenu.getSettingsMenu();
            Apple2DisksMenu disksMenu = mMainMenu.getDisksMenu();
            if (!(settingsMenu.isShowing() || disksMenu.isShowing())) {
                mMainMenu.show();
            }
        }
    }

    public Apple2MainMenu getMainMenu() {
        return mMainMenu;
    }

    public synchronized void showSplashScreen() {
        if (mSplashScreen == null) {
            mSplashScreen = new Apple2SplashScreen(Apple2Activity.this);
        }
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mSplashScreen.show();
            }
        });
    }

    public synchronized void pushApple2View(Apple2MenuView apple2MenuView) {
        mMenuStack.add(apple2MenuView);
        View menuView = apple2MenuView.getView();
        nativeOnPause(false);
        addContentView(menuView, new FrameLayout.LayoutParams(getWidth(), getHeight()));
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
        if (mQuitDialog != null && mQuitDialog.isShowing()) {
            mQuitDialog.dismiss();
        }
        if (mRebootDialog != null && mRebootDialog.isShowing()) {
            mRebootDialog.dismiss();
        }

        // Get rid of the menu hierarchy
        Apple2MenuView apple2MenuView;
        do {
            apple2MenuView = popApple2View();
            if (apple2MenuView != null) {
                apple2MenuView.dismissAll();
            }
        } while (apple2MenuView != null);
    }

    public synchronized Apple2MenuView popApple2View(Apple2MenuView apple2MenuView) {
        boolean wasRemoved = mMenuStack.remove(apple2MenuView);
        _disposeApple2View(apple2MenuView);
        return wasRemoved ? apple2MenuView : null;
    }

    private void _disposeApple2View(Apple2MenuView apple2MenuView) {

        // Actually remove View from view hierarchy
        {
            View menuView = apple2MenuView.getView();
            if (menuView.isShown()) {
                ((ViewGroup) menuView.getParent()).removeView(menuView);
            }
        }

        // if no more views on menu stack, resume emulation
        if (mMenuStack.size() == 0) {
            nativeOnResume(/*isSystemResume:*/false);
        }
    }

    public Apple2View getView() {
        return mView;
    }

    public int getWidth() {
        return mWidth;
    }

    public int getHeight() {
        return mHeight;
    }

    public void maybeQuitApp() {
        nativeOnPause(false);
        if (mQuitDialog == null) {
            mQuitDialog = new AlertDialog.Builder(this).setIcon(R.drawable.ic_launcher).setCancelable(true).setTitle(R.string.quit_really).setMessage(R.string.quit_warning).setPositiveButton(R.string.ok, new DialogInterface.OnClickListener() {
                @Override
                public void onClick(DialogInterface dialog, int which) {
                    nativeOnQuit();
                    Apple2Activity.this.finish();
                    try {
                        Thread.sleep(1000);
                    } catch (InterruptedException ex) {
                        // ...
                    }
                    System.exit(0);
                }
            }).setNegativeButton(R.string.no, null).create();
            /*mQuitDialog.setOnCancelListener(new DialogInterface.OnCancelListener() {
                @Override
                public void onCancel(DialogInterface dialog) {
                    nativeOnResume(false);
                }
            });
            mQuitDialog.setOnDismissListener(new DialogInterface.OnDismissListener() {
                @Override
                public void onDismiss(DialogInterface dialog) {
                    nativeOnResume(false);
                }
            });*/
        }
        mQuitDialog.show();
    }

    public void maybeReboot() {
        nativeOnPause(false);
        if (mRebootDialog == null) {
            mRebootDialog = new AlertDialog.Builder(this).setIcon(R.drawable.ic_launcher).setCancelable(true).setTitle(R.string.reboot_really).setMessage(R.string.reboot_warning).setPositiveButton(R.string.ok, new DialogInterface.OnClickListener() {
                @Override
                public void onClick(DialogInterface dialog, int which) {
                    nativeReboot();
                    Apple2Activity.this.mMainMenu.dismiss();
                }
            }).setNegativeButton(R.string.no, null).create();
            /*mRebootDialog.setOnCancelListener(new DialogInterface.OnCancelListener() {
                @Override
                public void onCancel(DialogInterface dialog) {
                    nativeOnResume(false);
                }
            });
            mRebootDialog.setOnDismissListener(new DialogInterface.OnDismissListener() {
                @Override
                public void onDismiss(DialogInterface dialog) {
                    nativeOnResume(false);
                }
            });*/
        }
        mRebootDialog.show();
    }
}
