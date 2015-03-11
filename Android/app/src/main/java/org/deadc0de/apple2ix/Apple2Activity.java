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
import android.content.Context;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.graphics.Rect;
import android.os.Bundle;
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
    private final static String PREFS_CONFIGURED = "prefs_configured";
    private final static int SOFTKEYBOARD_THRESHOLD = 10;

    private Apple2View mView = null;
    private int mWidth = 0;
    private int mHeight = 0;
    private boolean mSoftKeyboardShowing = false;

    static {
        System.loadLibrary("apple2ix");
    }

    private native void nativeOnCreate(String dataDir);
    public native void nativeOnResume();
    public native void nativeOnPause();
    private native void nativeGraphicsInitialized(int width, int height);
    private native void nativeGraphicsChanged(int width, int height);
    public native void nativeRender();
    private native void nativeOnKeyDown(int keyCode, int metaState);
    private native void nativeOnKeyUp(int keyCode, int metaState);

    // HACK NOTE 2015/02/22 : Apparently native code cannot easily access stuff in the APK ... so copy various resources
    // out of the APK and into the /data/data/... for ease of access.  Because this is FOSS software we don't care about
    // these assets
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

        String dataDir = firstTimeInitialization();
        nativeOnCreate(dataDir);

        mView = new Apple2View(this);
        setContentView(mView);

        mView.getViewTreeObserver().addOnGlobalLayoutListener(new ViewTreeObserver.OnGlobalLayoutListener() {
            public void onGlobalLayout() {
                Rect rect = new Rect();
                mView.getWindowVisibleDisplayFrame(rect);
                int h = rect.height();
                if (mView.getHeight() - h > SOFTKEYBOARD_THRESHOLD) {
                    Log.d(TAG, "Soft keyboard appears to be occupying screen real estate ...");
                    Apple2Activity.this.mSoftKeyboardShowing = true;
                } else {
                    Apple2Activity.this.mSoftKeyboardShowing = false;
                }
                nativeGraphicsChanged(rect.width(), h);
            }
        });
    }

    @Override
    protected void onResume() {
        super.onResume();
        mView.onResume();
        nativeOnResume();
    }

    @Override
    protected void onPause() {
        super.onPause();
        mView.onPause();
        nativeOnPause();
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        nativeOnKeyDown(keyCode, event.getMetaState());
        return true;
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        nativeOnKeyUp(keyCode, event.getMetaState());
        return true;
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        Log.d(TAG, "onTouchEvent...");
        return true;
    }

    public void graphicsInitialized(int width, int height) {
        mWidth = width;
        mHeight = height;
        nativeGraphicsInitialized(width, height);
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
}
