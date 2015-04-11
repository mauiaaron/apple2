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
import android.os.Bundle;
import android.util.Log;
import android.view.GestureDetector;
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
    private final static String PREFS_CONFIGURED = "prefs_configured";
    private final static int SOFTKEYBOARD_THRESHOLD = 50;
    private final static int MAX_FINGERS = 32;// HACK ...

    private String mDataDir = null;

    private Apple2View mView = null;
    private AlertDialog mQuitDialog = null;
    private AlertDialog mRebootDialog = null;
    private GestureDetector mDetector = null;
    private boolean mSwipeTogglesSpeed = true;
    private boolean mDoubleTapShowsKeyboard = true;
    private boolean mSingleTapShowsMainMenu = true;

    private int mWidth = 0;
    private int mHeight = 0;
    private boolean mSoftKeyboardShowing = false;

    private float[] mXCoords = new float[MAX_FINGERS];
    private float[] mYCoords = new float[MAX_FINGERS];

    static {
        System.loadLibrary("apple2ix");
    }

    private native void nativeOnCreate(String dataDir);
    private native void nativeGraphicsInitialized(int width, int height);
    private native void nativeGraphicsChanged(int width, int height);
    private native void nativeOnKeyDown(int keyCode, int metaState);
    private native void nativeOnKeyUp(int keyCode, int metaState);
    private native void nativeIncreaseCPUSpeed();
    private native void nativeDecreaseCPUSpeed();

    public native void nativeOnResume();
    public native void nativeOnPause();
    public native void nativeOnQuit();

    public native boolean nativeOnTouch(int action, int pointerCount, int pointerIndex, float[] xCoords, float[] yCoords);

    public native void nativeReboot();
    public native void nativeRender();

    public native void nativeEnableTouchJoystick(boolean visibility);
    public native void nativeSetColor(int color);
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

        SharedPreferences settings = getPreferences(Context.MODE_PRIVATE);
        if (settings.getBoolean(PREFS_CONFIGURED, false)) {
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
        SharedPreferences.Editor editor = settings.edit();
        editor.putBoolean(PREFS_CONFIGURED, true);
        editor.apply();

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
        super.onCreate(savedInstanceState);
        Log.e(TAG, "onCreate()");

        mDataDir = firstTimeInitialization();
        nativeOnCreate(mDataDir);

        mView = new Apple2View(this);
        setContentView(mView);

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
                if (mView.getHeight() - h > SOFTKEYBOARD_THRESHOLD) {
                    Log.d(TAG, "Soft keyboard appears to be occupying screen real estate ...");
                    Apple2Activity.this.mSoftKeyboardShowing = true;
                } else {
                    Log.d(TAG, "Soft keyboard appears to be gone ...");
                    Apple2Activity.this.mSoftKeyboardShowing = false;
                }
                nativeGraphicsChanged(w, h);
            }
        });

        mDetector = new GestureDetector(this, new Apple2GestureListener());
    }

    @Override
    protected void onResume() {
        super.onResume();
        mView.onResume();

        Apple2MainMenu mainMenu = mView.getMainMenu();

        boolean noMenusShowing = !(
                (mainMenu != null && mainMenu.isShowing()) ||
                (mQuitDialog != null && mQuitDialog.isShowing()) ||
                (mRebootDialog != null && mRebootDialog.isShowing())
        );

        if (noMenusShowing) {
            nativeOnResume();
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        mView.onPause();

        Apple2MainMenu mainMenu = mView.getMainMenu();
        if (mainMenu == null) {
            // wow, early onPause, just quit and restart later
            nativeOnQuit();
            return;
        }

        nativeOnPause();

        if (isSoftKeyboardShowing()) {
            mView.toggleKeyboard();
        }

        Apple2SettingsMenu settingsMenu = mView.getSettingsMenu();
        Apple2DisksMenu disksMenu = mView.getDisksMenu();
        if (settingsMenu != null) {
            settingsMenu.dismissWithoutResume();
        }
        if (disksMenu != null) {
            disksMenu.dismissWithoutResume();
        }

        boolean someMenuShowing = mainMenu.isShowing() ||
                (mQuitDialog != null && mQuitDialog.isShowing()) ||
                (mRebootDialog != null && mRebootDialog.isShowing());
        if (!someMenuShowing) {
            // show main menu on pause
            mView.showMainMenu();
        }
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
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
                    settingsMenu.dismiss();
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
            return super.onKeyUp(keyCode, event);
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

            this.mDetector.onTouchEvent(event);
        } while (false);

        return super.onTouchEvent(event);
    }

    private class Apple2GestureListener extends GestureDetector.SimpleOnGestureListener {
        private static final String TAG = "Gestures";

        @Override
        public boolean onDown(MotionEvent event) {
            Log.d(TAG,"onDown: " + event.toString());
            return true;
        }

        @Override
        public boolean onFling(MotionEvent event1, MotionEvent event2, float velocityX, float velocityY) {
            if ((event1 == null) || (event2 == null)) {
                return false;
            }
            if (mSwipeTogglesSpeed) {
                float ev1X = event1.getX();
                float ev2X = event2.getX();
                if (ev1X < ev2X) {
                    nativeIncreaseCPUSpeed();
                } else {
                    nativeDecreaseCPUSpeed();
                }
            }
            return true;
        }

        @Override
        public boolean onSingleTapConfirmed(MotionEvent event) {
            if (event == null) {
                return false;
            }
            Log.d(TAG, "onSingleTapConfirmed: " + event.toString());
            Apple2MainMenu mainMenu = Apple2Activity.this.mView.getMainMenu();
            if (mainMenu.isShowing()) {
                Log.d(TAG, "dismissing main menu");
                mainMenu.dismiss();
            } else if (Apple2Activity.this.isSoftKeyboardShowing()) {
                Log.d(TAG, "hiding keyboard");
                Apple2Activity.this.mView.toggleKeyboard();
            } else if (mSingleTapShowsMainMenu) {
                Log.d(TAG, "showing main menu");
                Apple2Activity.this.mView.showMainMenu();
            }
            return true;
        }

        @Override
        public boolean onDoubleTap(MotionEvent event) {
            if (event == null) {
                return false;
            }
            if (mDoubleTapShowsKeyboard) {
                Log.d(TAG, "onDoubleTap: " + event.toString());
                if (!Apple2Activity.this.isSoftKeyboardShowing()) {
                    Log.d(TAG, "showing keyboard");
                    Apple2Activity.this.mView.toggleKeyboard();
                }
            }
            return true;
        }
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

    public boolean isSoftKeyboardShowing() {
        return mSoftKeyboardShowing;
    }

    public void maybeQuitApp() {
        nativeOnPause();
        if (mQuitDialog == null) {
            mQuitDialog = new AlertDialog.Builder(this).setIcon(R.drawable.ic_launcher).setCancelable(true).setTitle(R.string.quit_really).setPositiveButton(R.string.ok, new DialogInterface.OnClickListener() {
                @Override
                public void onClick(DialogInterface dialog, int which) {
                    nativeOnQuit();
                }
            }).setNegativeButton(R.string.no, null).create();
            /*mQuitDialog.setOnCancelListener(new DialogInterface.OnCancelListener() {
                @Override
                public void onCancel(DialogInterface dialog) {
                    nativeOnResume();
                }
            });
            mQuitDialog.setOnDismissListener(new DialogInterface.OnDismissListener() {
                @Override
                public void onDismiss(DialogInterface dialog) {
                    nativeOnResume();
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
                    nativeOnResume();
                }
            });
            mRebootDialog.setOnDismissListener(new DialogInterface.OnDismissListener() {
                @Override
                public void onDismiss(DialogInterface dialog) {
                    nativeOnResume();
                }
            });*/
        }
        mRebootDialog.show();
    }

    public void setSwipeTogglesSpeed(boolean swipeTogglesSpeed) {
        mSwipeTogglesSpeed = swipeTogglesSpeed;
    }

    public void setSingleTapShowsMainMenu(boolean singleTapShowsMainMenu) {
        mSingleTapShowsMainMenu = singleTapShowsMainMenu;
    }

    public void setDoubleTapShowsKeyboard(boolean doubleTapShowsKeyboard) {
        mDoubleTapShowsKeyboard = doubleTapShowsKeyboard;
    }
}
