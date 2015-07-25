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
import android.content.Context;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.os.StrictMode;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.ViewTreeObserver;

import java.io.File;
import java.io.InputStream;
import java.io.IOException;
import java.io.FileOutputStream;

public class Apple2Activity extends Activity {

    private final static String TAG = "Apple2Activity";
    private final static int BUF_SZ = 4096;
    private final static int SOFTKEYBOARD_THRESHOLD = 50;
    private final static int MAX_FINGERS = 32;// HACK ...

    private String mDataDir = null;

    private Apple2View mView = null;
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

    private native void nativeOnCreate(String dataDir, int sampleRate, int monoBufferSize, int stereoBufferSize);
    private native void nativeGraphicsInitialized(int width, int height);
    private native void nativeGraphicsChanged(int width, int height);
    private native void nativeOnKeyDown(int keyCode, int metaState);
    private native void nativeOnKeyUp(int keyCode, int metaState);
    private native void nativeOnUncaughtException(String home, String trace);

    public native void nativeOnResume(boolean isSystemResume);
    public native void nativeOnPause();
    public native void nativeOnQuit();

    public native boolean nativeOnTouch(int action, int pointerCount, int pointerIndex, float[] xCoords, float[] yCoords);

    public native void nativeReboot();
    public native void nativeRender();

    public native void nativeChooseDisk(String path, boolean driveA, boolean readOnly);


    // HACK NOTE 2015/02/22 : Apparently native code cannot easily access stuff in the APK ... so copy various resources
    // out of the APK and into the /data/data/... for ease of access.  Because this is FOSS software we don't care about
    // security or DRM for these assets =)
    private String firstTimeInitialization() {

        String dataDir = null;
        try {
            PackageManager pm = getPackageManager();
            PackageInfo pi = pm.getPackageInfo(getPackageName(), 0);
            dataDir = pi.applicationInfo.dataDir;
        } catch (PackageManager.NameNotFoundException e) {
            Log.e(TAG, ""+e);
            System.exit(1);
        }

        if (Apple2Preferences.PREFS_CONFIGURED.booleanValue(this)) {
            return dataDir;
        }

        Log.d(TAG, "First time copying stuff-n-things out of APK for ease-of-NDK access...");

        try {
            String[] shaders = getAssets().list("shaders");
            for (String shader : shaders) {
                _copyFile(dataDir, "shaders", shader);
            }
            String[] disks = getAssets().list("disks");
            for (String disk : disks) {
                _copyFile(dataDir, "disks", disk);
            }
        } catch (IOException e) {
            Log.e(TAG, "problem copying resources : "+e);
            System.exit(1);
        }

        Log.d(TAG, "Saving default preferences");
        Apple2Preferences.PREFS_CONFIGURED.saveBoolean(this, true);

        return dataDir;
    }

    private void _copyFile(String dataDir, String subdir, String assetName)
        throws IOException
    {
        String outputPath = dataDir+File.separator+subdir;
        Log.d(TAG, "Copying "+subdir+File.separator+assetName+" to "+outputPath+File.separator+assetName+" ...");
        new File(outputPath).mkdirs();

        InputStream is = getAssets().open(subdir+File.separator+assetName);
        File file = new File(outputPath+File.separator+assetName);
        file.setWritable(true);
        FileOutputStream os = new FileOutputStream(file);

        byte[] buf = new byte[BUF_SZ];
        while (true) {
            int len = is.read(buf, 0, BUF_SZ);
            if (len < 0) {
                break;
            }
            os.write(buf, 0, len);
        }
        os.flush();
        os.close();
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

        // Immediately set up exception handler ...
        final String homeDir = "/data/data/"+this.getPackageName();
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

        Log.e(TAG, "onCreate()");

        mDataDir = firstTimeInitialization();

        // get device audio parameters for native OpenSLES
        mSampleRate = DevicePropertyCalculator.getRecommendedSampleRate(this);
        mMonoBufferSize = DevicePropertyCalculator.getRecommendedBufferSize(this, /*isStereo:*/false);
        mStereoBufferSize = DevicePropertyCalculator.getRecommendedBufferSize(this, /*isStereo:*/true);
        Log.d(TAG, "Device sampleRate:"+mSampleRate+" mono bufferSize:"+mMonoBufferSize+" stereo bufferSize:"+mStereoBufferSize);

        nativeOnCreate(mDataDir, mSampleRate, mMonoBufferSize, mStereoBufferSize);

        // NOTE: load preferences after nativeOnCreate
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
        Apple2MainMenu mainMenu = mView.getMainMenu();
        if (mainMenu != null) {
            mainMenu.dismiss();
        }
        if (mQuitDialog != null  && mQuitDialog.isShowing()) {
            mQuitDialog.dismiss();
        }
        if (mRebootDialog != null && mRebootDialog.isShowing()) {
            mRebootDialog.dismiss();
        }

        // For good measure, get rid of other menus too
        Apple2SettingsMenu settingsMenu = mView.getSettingsMenu();
        if (settingsMenu != null) {
            settingsMenu.dismissWithoutResume();
        }
        Apple2DisksMenu disksMenu = mView.getDisksMenu();
        if (disksMenu != null) {
            disksMenu.dismissWithoutResume();
        }

        nativeOnPause();
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
            Apple2SettingsMenu settingsMenu = mView.getSettingsMenu();
            Apple2DisksMenu disksMenu = mView.getDisksMenu();
            if (settingsMenu != null) {
                if (settingsMenu.isShowing()) {
                    Apple2AudioSettingsMenu audioSubmenu = settingsMenu.getAudioSubmenu();
                    if (audioSubmenu.isShowing()) {
                        audioSubmenu.dismiss();
                    } else {
                        settingsMenu.dismiss();
                    }
                } else if (disksMenu.isShowing()) {
                    disksMenu.dismiss();
                } else {
                    mView.showMainMenu();
                }
            }
            return true;
        } else if (keyCode == KeyEvent.KEYCODE_MENU) {
            mView.showMainMenu();
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
                return "CANCEL:"+action;
            case MotionEvent.ACTION_DOWN:
                return "DOWN:"+action;
            case MotionEvent.ACTION_MOVE:
                return "MOVE:"+action;
            case MotionEvent.ACTION_UP:
                return "UP:"+action;
            case MotionEvent.ACTION_POINTER_DOWN:
                return "PDOWN:"+action;
            case MotionEvent.ACTION_POINTER_UP:
                return "PUP:"+action;
            default:
                return "UNK:"+action;
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

        Log.d(TAG, "Event "+actionToString(ev.getActionMasked())+" for "+pointerIndex+" at time "+ev.getEventTime()+" :");
        for (int p=0; p<pointerCount; p++) {
            Log.d(TAG, "  pointer "+ev.getPointerId(p)+": ("+ev.getX(p)+","+ev.getY(p)+")");
        }
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        do {
            Apple2MainMenu mainMenu = mView.getMainMenu();
            if (mainMenu == null) {
                break;
            }

            Apple2SettingsMenu settingsMenu = mainMenu.getSettingsMenu();
            Apple2DisksMenu disksMenu = mView.getDisksMenu();
            if (settingsMenu != null && settingsMenu.isShowing()) {
                break;
            }
            if (disksMenu != null && disksMenu.isShowing()) {
                break;
            }

            //printSamples(event);
            int action = event.getActionMasked();
            int pointerIndex = event.getActionIndex();
            int pointerCount = event.getPointerCount();
            for (int i=0; i<pointerCount/* && i < MAX_FINGERS */; i++) {
                mXCoords[i] = event.getX(i);
                mYCoords[i] = event.getY(i);
            }

            boolean nativeHandled = nativeOnTouch(action, pointerCount, pointerIndex, mXCoords, mYCoords);
            if (nativeHandled) {
                break;
            }

            mainMenu.show();
        } while (false);

        return super.onTouchEvent(event);
    }

    public void graphicsInitialized(int w, int h) {
        if (w < h) {
            // assure landscape dimensions
            final int w_ = w;
            w = h;
            h = w_;
        }

        mWidth = w;
        mHeight = h;

        nativeGraphicsInitialized(w, h);
    }

    public Apple2View getView() {
        return mView;
    }

    public String getDataDir() {
        return mDataDir;
    }

    public int getWidth() {
        return mWidth;
    }

    public int getHeight() {
        return mHeight;
    }

    public void maybeQuitApp() {
        nativeOnPause();
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
        nativeOnPause();
        if (mRebootDialog == null) {
            mRebootDialog = new AlertDialog.Builder(this).setIcon(R.drawable.ic_launcher).setCancelable(true).setTitle(R.string.reboot_really).setMessage(R.string.reboot_warning).setPositiveButton(R.string.ok, new DialogInterface.OnClickListener() {
                @Override
                public void onClick(DialogInterface dialog, int which) {
                    nativeReboot();
                    Apple2Activity.this.mView.getMainMenu().dismiss();
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
