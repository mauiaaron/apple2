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

package org.deadc0de.apple2;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.util.Log;
import android.widget.TextView;

import java.io.File;
import java.io.InputStream;
import java.io.IOException;
import java.io.FileOutputStream;

public class Apple2Activity extends Activity {

    private final static String TAG = "Apple2Activity";

    private final static int BUF_SZ = 4096;

    private final static String PREFS_CONFIGURED = "prefs_configured";

    static {
        System.loadLibrary("apple2ix");
    }

    private native void nativeOnCreate(String dataDir);
    private native void nativeOnResume();
    private native void nativeOnPause();

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
            _copyFile(dataDir, "shaders", "Basic.vsh");
            _copyFile(dataDir, "shaders", "Basic.fsh");
            _copyFile(dataDir, "disks", "blank.dsk");
            _copyFile(dataDir, "disks", "blank.nib");
            _copyFile(dataDir, "disks", "blank.po");
            _copyFile(dataDir, "disks", "etc.dsk");
            _copyFile(dataDir, "disks", "flapple140.po");
            _copyFile(dataDir, "disks", "mystery.dsk");
            _copyFile(dataDir, "disks", "ProDOS.dsk");
            _copyFile(dataDir, "disks", "speedtest.dsk");
            _copyFile(dataDir, "disks", "testdisplay1.dsk");
            _copyFile(dataDir, "disks", "testvm1.dsk");
        } catch (IOException e) {
            Log.e(TAG, "problem copying resources : "+e);
            System.exit(1);
        }

        Log.d(TAG, "Saving default preferences");
        SharedPreferences.Editor editor = settings.edit();
        editor.putBoolean(PREFS_CONFIGURED, true);
        editor.commit();

        return dataDir;
    }

    private void _copyFile(String dataDir, String subdir, String assetName)
        throws IOException
    {
        String outputPath = dataDir+File.separator+subdir;
        Log.d(TAG, "Copying "+subdir+File.separator+assetName+" to "+outputPath+File.separator+assetName+" ...");
        new File(outputPath).mkdirs();

        InputStream is = getAssets().open(subdir+File.separator+assetName);
        if (is == null) {
            Log.e(TAG, "inputstream is null");
        }
        File file = new File(outputPath+File.separator+assetName);
        file.setWritable(true);

        FileOutputStream os = new FileOutputStream(file);
        if (os == null) {
            Log.e(TAG, "outputstream is null");
        }

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

        TextView tv = new TextView(this);
        tv.setText("Hello Apple2!");
        setContentView(tv);
    }

    @Override
    protected void onResume() {
        super.onResume();
        nativeOnResume();
    }

    @Override
    protected void onPause() {
        super.onPause();
        nativeOnPause();
    }
}
