/*
 * Apple // emulator for *nix
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 2016 Aaron Culliney
 *
 */

package org.deadc0de.apple2ix;

import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.os.Environment;
import android.util.Log;
import android.view.View;
import android.widget.ProgressBar;

import org.deadc0de.apple2ix.basic.BuildConfig;
import org.deadc0de.apple2ix.basic.R;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.FilenameFilter;
import java.io.IOException;
import java.io.InputStream;
import java.io.InterruptedIOException;
import java.io.OutputStream;
import java.util.zip.GZIPOutputStream;

public class Apple2Utils {

    public final static String TAG = "Apple2Utils";

    private static String sDataDir = null;
    private static File sExternalFilesDir = null;
    private static File sRealExternalFilesDir = null;

    public static boolean readEntireFile(File file, StringBuilder fileData) {
        final int maxAttempts = 5;
        int attempts = 0;
        do {
            try {
                BufferedReader reader = new BufferedReader(new FileReader(file));
                char[] buf = new char[1024];
                int numRead = 0;
                while ((numRead = reader.read(buf)) != -1) {
                    String readData = String.valueOf(buf, 0, numRead);
                    fileData.append(readData);
                }
                reader.close();
                break;
            } catch (InterruptedIOException ie) {
                /* EINTR, EAGAIN ... */
            } catch (IOException e) {
                Apple2Activity.logMessage(Apple2Activity.LogType.DEBUG, TAG, "Error reading file at path : " + file.toString());
            }

            try {
                Thread.sleep(100, 0);
            } catch (InterruptedException e) {
                /* ... */
            }
            ++attempts;
        } while (attempts < maxAttempts);

        return attempts < maxAttempts;
    }

    public static boolean writeFile(final StringBuilder data, File file) {

        final int maxAttempts = 5;
        int attempts = 0;
        do {
            try {
                BufferedWriter writer = new BufferedWriter(new FileWriter(file));
                writer.append(data);
                writer.flush();
                writer.close();
                break;
            } catch (InterruptedIOException ie) {
                /* EINTR, EAGAIN ... */
            } catch (IOException e) {
                Apple2Activity.logMessage(Apple2Activity.LogType.ERROR, TAG, "Exception attempting to write data : " + e);
            }

            try {
                Thread.sleep(100, 0);
            } catch (InterruptedException e) {
                /* ... */
            }
            ++attempts;
        } while (attempts < maxAttempts);

        return attempts < maxAttempts;
    }

    public static void migrateToExternalStorage(Apple2Activity activity) {

        do {
            if (BuildConfig.VERSION_CODE >= 18) {

                // Rename old emulator state file
                // TODO FIXME : Remove this migration code when all/most users are on version >= 18

                final File srcFile = new File(getDataDir(activity) + File.separator + Apple2MainMenu.OLD_SAVE_FILE);
                if (!srcFile.exists()) {
                    break;
                }

                final File dstFile = new File(getDataDir(activity) + File.separator + Apple2MainMenu.SAVE_FILE);
                final boolean success = copyFile(srcFile, dstFile);
                if (success) {
                    srcFile.delete();
                }
            }
        } while (false);


        final File extStorage = Apple2Utils.getExternalStorageDirectory(activity);
        if (extStorage == null) {
            return;
        }

        do {
            if (BuildConfig.VERSION_CODE >= 18) {

                // Migrate old emulator state file from internal path to external storage to allow user manipulation
                // TODO FIXME : Remove this migration code when all/most users are on version >= 18

                final File srcFile = new File(getDataDir(activity) + File.separator + Apple2MainMenu.SAVE_FILE);
                if (!srcFile.exists()) {
                    break;
                }

                final File dstFile = new File(extStorage + File.separator + Apple2MainMenu.SAVE_FILE);
                final boolean success = copyFile(srcFile, dstFile);
                if (success) {
                    srcFile.delete();
                }
            }
        } while (false);

        do {
            if (BuildConfig.VERSION_CODE >= 20) {

                // Recursively rename all *.state files found in /sdcard/apple2ix
                // TODO FIXME : Remove this migration code when all/most users are on version >= 20

                recursivelyRenameEmulatorStateFiles(extStorage);
            }
        } while (false);
    }

    public static File getExternalStorageDirectory(Apple2Activity activity) {

        do {
            if (sExternalFilesDir != null) {
                break;
            }

            String storageState = Environment.getExternalStorageState();
            if (!Environment.MEDIA_MOUNTED.equals(storageState)) {
                // 2015/10/28 : do not expose sExternalFilesDir unless it is writable
                break;
            }

            File realExternalStorageDir = Environment.getExternalStorageDirectory();
            if (realExternalStorageDir == null) {
                break;
            }

            File externalDir = new File(realExternalStorageDir, "apple2ix"); // /sdcard/apple2ix
            if (!externalDir.exists()) {
                boolean made = externalDir.mkdirs();
                if (!made) {
                    Apple2Activity.logMessage(Apple2Activity.LogType.DEBUG, TAG, "WARNING: could not make directory : " + sExternalFilesDir);
                    break;
                }
            }

            sExternalFilesDir = externalDir;
            sRealExternalFilesDir = realExternalStorageDir;
        } while (false);

        return sExternalFilesDir;
    }

    public static File getRealExternalStorageDirectory(Apple2Activity activity) {
        getExternalStorageDirectory(activity);
        return sRealExternalFilesDir;
    }

    public static boolean isExternalStorageAccessible(Apple2Activity activity) {
        getExternalStorageDirectory(activity);
        return (sRealExternalFilesDir != null) && (new File(sRealExternalFilesDir.getAbsolutePath()).listFiles() != null);
    }

    // HACK NOTE 2015/02/22 : Apparently native code cannot easily access stuff in the APK ... so copy various resources
    // out of the APK and into the /data/data/... for ease of access.  Because this is FOSS software we don't care about
    // security or DRM for these assets =)
    public static String getDataDir(Apple2Activity activity) {

        if (sDataDir != null) {
            return sDataDir;
        }

        try {
            PackageManager pm = activity.getPackageManager();
            PackageInfo pi = pm.getPackageInfo(activity.getPackageName(), 0);
            sDataDir = pi.applicationInfo.dataDir;
        } catch (PackageManager.NameNotFoundException e) {
            Apple2Activity.logMessage(Apple2Activity.LogType.ERROR, TAG, "" + e);
            if (sDataDir == null) {
                sDataDir = "/data/local/tmp";
            }
        }

        return sDataDir;
    }

    public static void exposeAPKAssetsToExternal(Apple2Activity activity) {
        getExternalStorageDirectory(activity);
        if (sExternalFilesDir == null) {
            return;
        }

        final ProgressBar bar = (ProgressBar) activity.findViewById(R.id.crash_progressBar);
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    bar.setVisibility(View.VISIBLE);
                    bar.setIndeterminate(true);
                } catch (NullPointerException npe) {
                    Log.v(TAG, "Avoid NPE in exposeAPKAssetsToExternal #1");
                }
            }
        });

        Log.v(TAG, "Overwriting system files in /sdcard/apple2ix/ (external storage) ...");
        recursivelyCopyAPKAssets(activity, /*from APK directory:*/"keyboards", /*to location:*/sExternalFilesDir.getAbsolutePath(), false);

        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    bar.setVisibility(View.INVISIBLE);
                    bar.setIndeterminate(false);
                } catch (NullPointerException npe) {
                    Log.v(TAG, "Avoid NPE in exposeAPKAssetsToExternal #2");
                }
            }
        });
    }

    public static void exposeAPKAssets(Apple2Activity activity) {
        final ProgressBar bar = (ProgressBar) activity.findViewById(R.id.crash_progressBar);
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    bar.setVisibility(View.VISIBLE);
                    bar.setIndeterminate(true);
                } catch (NullPointerException npe) {
                    Log.v(TAG, "Avoid NPE in exposeAPKAssets #1");
                }
            }
        });

        getDataDir(activity);

        // FIXME TODO : Heavy-handed migration to 1.1.3 ...
        recursivelyDelete(new File(new File(sDataDir, "disks").getAbsolutePath(), "blanks"));
        recursivelyDelete(new File(new File(sDataDir, "disks").getAbsolutePath(), "demo"));
        recursivelyDelete(new File(new File(sDataDir, "disks").getAbsolutePath(), "eamon"));
        recursivelyDelete(new File(new File(sDataDir, "disks").getAbsolutePath(), "logo"));
        recursivelyDelete(new File(new File(sDataDir, "disks").getAbsolutePath(), "miscgame"));

        Apple2Activity.logMessage(Apple2Activity.LogType.DEBUG, TAG, "First time copying stuff-n-things out of APK for ease-of-NDK access...");

        getExternalStorageDirectory(activity);
        recursivelyCopyAPKAssets(activity, /*from APK directory:*/"disks",     /*to location:*/new File(sDataDir, "disks").getAbsolutePath(), true);
        recursivelyCopyAPKAssets(activity, /*from APK directory:*/"keyboards", /*to location:*/new File(sDataDir, "keyboards").getAbsolutePath(), false);
        recursivelyCopyAPKAssets(activity, /*from APK directory:*/"shaders",   /*to location:*/new File(sDataDir, "shaders").getAbsolutePath(), false);

        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    bar.setVisibility(View.INVISIBLE);
                    bar.setIndeterminate(false);
                } catch (NullPointerException npe) {
                    Log.v(TAG, "Avoid NPE in exposeAPKAssets #1");
                }
            }
        });
    }

    public static void exposeSymbols(Apple2Activity activity) {
        recursivelyCopyAPKAssets(activity, /*from APK directory:*/"symbols",   /*to location:*/new File(sDataDir, "symbols").getAbsolutePath(), false);
    }

    public static void unexposeSymbols(Apple2Activity activity) {
        recursivelyDelete(new File(sDataDir, "symbols"));
    }

    // TODO FIXME : WARNING : this is super dangerous if there are symlinks !!!
    private static void recursivelyDelete(File file) {
        if (file.isDirectory()) {
            for (File f : file.listFiles()) {
                recursivelyDelete(f);
            }
        }
        if (!file.delete()) {
            Apple2Activity.logMessage(Apple2Activity.LogType.DEBUG, TAG, "Failed to delete file: " + file);
        }
    }

    private static void recursivelyCopyAPKAssets(Apple2Activity activity, String srcFileOrDir, String dstFileOrDir, boolean shouldGzip) {
        AssetManager assetManager = activity.getAssets();

        final int maxAttempts = 5;
        String[] files = null;
        int attempts = 0;
        do {
            try {
                files = assetManager.list(srcFileOrDir);
                break;
            } catch (InterruptedIOException e) {
                /* EINTR, EAGAIN ... */
            } catch (IOException e) {
                Apple2Activity.logMessage(Apple2Activity.LogType.DEBUG, TAG, "OOPS exception attempting to list APK files at : " + srcFileOrDir + " : " + e);
            }

            try {
                Thread.sleep(100, 0);
            } catch (InterruptedException ie) {
                /* ... */
            }
            ++attempts;
        } while (attempts < maxAttempts);

        if (files == null) {
            Apple2Activity.logMessage(Apple2Activity.LogType.DEBUG, TAG, "OOPS, could not list APK assets at : " + srcFileOrDir);
            return;
        }

        if (files.length > 0) {
            // ensure destination directory exists
            File dstPath = new File(dstFileOrDir);
            if (!dstPath.mkdirs()) {
                if (!dstPath.exists()) {
                    Apple2Activity.logMessage(Apple2Activity.LogType.DEBUG, TAG, "OOPS, could not mkdirs on " + dstPath);
                    return;
                }
            }
            for (String filename : files) {
                // iterate on files and subdirectories
                recursivelyCopyAPKAssets(activity, srcFileOrDir + File.separator + filename, dstFileOrDir + File.separator + filename, shouldGzip);
            }
            return;
        }

        // presumably this is a file, not a subdirectory
        InputStream is = null;
        OutputStream os = null;
        attempts = 0;
        do {
            try {
                is = assetManager.open(srcFileOrDir);
                if (shouldGzip) {
                    os = new GZIPOutputStream(new FileOutputStream(dstFileOrDir + ".gz"));
                } else {
                    os = new FileOutputStream(dstFileOrDir);
                }
                copyFile(is, os);
                break;
            } catch (InterruptedIOException e) {
                /* EINTR, EAGAIN */
            } catch (IOException e) {
                Apple2Activity.logMessage(Apple2Activity.LogType.ERROR, TAG, "Failed to copy asset file: " + srcFileOrDir + " : " + e.getMessage());
            } finally {
                if (is != null) {
                    try {
                        is.close();
                    } catch (IOException e) {
                        // NOOP
                    }
                }
                if (os != null) {
                    try {
                        os.close();
                    } catch (IOException e) {
                        // NOOP
                    }
                }
            }
            try {
                Thread.sleep(100, 0);
            } catch (InterruptedException ie) {
                /* ... */
            }
            ++attempts;
        } while (attempts < maxAttempts);
    }

    private static void recursivelyRenameEmulatorStateFiles(File directory) {
        try {
            if (!directory.isDirectory()) {
                return;
            }

            final int oldSuffixLen = 6;

            File[] files = directory.listFiles(new FilenameFilter() {
                @Override
                public boolean accept(File dir, String name) {

                    if (name.equals(".") || name.equals("..")) {
                        return false;
                    }

                    final File file = new File(dir, name);
                    if (file.isDirectory()) {
                        return true;
                    }

                    final int len = name.length();
                    if (len < oldSuffixLen) {
                        return false;
                    }

                    final String suffix = name.substring(len - oldSuffixLen, len);
                    return suffix.equalsIgnoreCase(".state");
                }
            });

            if (files == null) {
                return;
            }

            for (File file : files) {
                if (file.isDirectory()) {
                    recursivelyRenameEmulatorStateFiles(file);
                } else {
                    final File srcFile = file;
                    final String oldName = file.getName();
                    final String newName = oldName.substring(0, oldName.length() - oldSuffixLen) + Apple2MainMenu.SAVE_FILE_EXTENSION;
                    boolean success = file.renameTo(new File(file.getParentFile(), newName));
                    if (success) {
                        srcFile.delete();
                    }
                }
            }
        } catch (Exception e) {
            Apple2Activity.logMessage(Apple2Activity.LogType.ERROR, TAG, "OOPS : {e}");
        }
    }

    private static boolean copyFile(final File srcFile, final File dstFile) {
        final int maxAttempts = 5;
        int attempts = 0;
        do {
            try {
                FileInputStream is = new FileInputStream(srcFile);
                FileOutputStream os = new FileOutputStream(dstFile);
                copyFile(is, os);
                break;
            } catch (InterruptedIOException e) {
                // EINTR, EAGAIN ...
            } catch (IOException e) {
                Apple2Activity.logMessage(Apple2Activity.LogType.DEBUG, TAG, "OOPS exception attempting to copy emulator state file : " + e);
            }

            try {
                Thread.sleep(100, 0);
            } catch (InterruptedException ie) {
                // ...
            }
            ++attempts;
        } while (attempts < maxAttempts);

        return attempts < maxAttempts;
    }

    private static void copyFile(InputStream is, OutputStream os) throws IOException {
        final int BUF_SZ = 4096;
        byte[] buf = new byte[BUF_SZ];
        while (true) {
            int len = is.read(buf, 0, BUF_SZ);
            if (len < 0) {
                break;
            }
            os.write(buf, 0, len);
        }
        os.flush();
    }
}
