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

import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.ProgressBar;

import org.deadc0de.apple2ix.basic.BuildConfig;
import org.deadc0de.apple2ix.basic.R;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.FilenameFilter;
import java.io.IOException;
import java.io.InterruptedIOException;
import java.util.concurrent.atomic.AtomicBoolean;

public class Apple2CrashHandler {

    public final static String javaCrashFileName = "jcrash.txt";

    public static Apple2CrashHandler getInstance() {
        return sCrashHandler;
    }

    public enum CrashType {
        JAVA_CRASH {
            @Override
            public final String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.crash_java_npe);
            }
        },
        NULL_DEREF {
            @Override
            public final String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.crash_null);
            }
        },
        STACKCALL_OVERFLOW {
            @Override
            public final String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.crash_stackcall_overflow);
            }
        },
        STACKBUF_OVERFLOW {
            @Override
            public final String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.crash_stackbuf_overflow);
            }
        };


        public static final int size = CrashType.values().length;

        public abstract String getTitle(Apple2Activity activity);

        public static String[] titles(Apple2Activity activity) {
            String[] titles = new String[size];
            int i = 0;
            for (CrashType setting : values()) {
                titles[i++] = setting.getTitle(activity);
            }
            return titles;
        }
    }

    public synchronized void initializeAndSetCustomExceptionHandler(Apple2Activity activity) {
        synchronized (this) {
            if (homeDir == null) {
                homeDir = Apple2DisksMenu.getDataDir(activity);
            }
        }
        if (mDefaultExceptionHandler != null) {
            return;
        }
        mDefaultExceptionHandler = Thread.getDefaultUncaughtExceptionHandler();

        final Thread.UncaughtExceptionHandler defaultExceptionHandler = mDefaultExceptionHandler;

        Thread.setDefaultUncaughtExceptionHandler(new Thread.UncaughtExceptionHandler() {
            @Override
            public void uncaughtException(Thread thread, Throwable t) {
                try {
                    Apple2CrashHandler.onUncaughtException(thread, t);
                } catch (Throwable terminator2) {
                    // Yo dawg, I hear you like exceptions in your exception handler! ...
                }
                defaultExceptionHandler.uncaughtException(thread, t);
            }
        });
    }

    public void abandonAllHope(Apple2Activity activity, Throwable nativeBarfed) {
        // write out the early link crash and send this through the main crash processing code
        onUncaughtException(Thread.currentThread(), nativeBarfed);
        checkForCrashes(activity);
    }

    public boolean areJavaCrashesPresent(Apple2Activity activity) {
        File javaCrash = _javaCrashFile(activity);
        return javaCrash.exists();
    }

    public boolean areNativeCrashesPresent(Apple2Activity activity) {
        File[] nativeCrashes = _nativeCrashFiles(activity);
        return nativeCrashes != null && nativeCrashes.length > 0;
    }

    public boolean areCrashesPresent(Apple2Activity activity) {
        return areJavaCrashesPresent(activity) || areNativeCrashesPresent(activity);
    }

    public void checkForCrashes(final Apple2Activity activity) {
        if (!areCrashesPresent(activity)) {
            return;
        }

        if (!Apple2Preferences.CRASH_CHECK.booleanValue(activity)) {
            return;
        }

        boolean previouslyRanCrashCheck = mAlreadyRanCrashCheck.getAndSet(true);

        boolean previouslySentReport = mAlreadySentReport.get();
        if (previouslySentReport) {

            // here we assume that the crash data was previously sent via email ... if not then we lost it =P

            Log.d(TAG, "Cleaning up crash data ...");
            int idx = 0;
            File[] nativeCrashes = _nativeCrashFiles(activity);
            for (File crash : nativeCrashes) {

                if (!crash.delete()) {
                    Log.d(TAG, "Could not unlink crash : " + crash);
                }

                File processed = new File(_dumpPath2ProcessedPath(crash.getAbsolutePath()));
                if (!processed.delete()) {
                    Log.d(TAG, "Could not unlink processed : " + processed);
                }
            }

            File javaCrashFile = _javaCrashFile(activity);
            if (!javaCrashFile.delete()) {
                Log.d(TAG, "Could not unlink java crash : " + javaCrashFile);
            }

            // remove previous log file
            _writeTempLogFile(activity, new StringBuilder());
            return;
        }

        if (previouslyRanCrashCheck) {
            // don't keep asking on return from backgrounding
            return;
        }

        final AlertDialog crashDialog = new AlertDialog.Builder(activity).setIcon(R.drawable.ic_launcher).setCancelable(true).setTitle(R.string.crasher_send).setMessage(R.string.crasher_send_message).setNegativeButton(R.string.no, null).setPositiveButton(R.string.ok, new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                dialog.dismiss();

                final Button startButton = (Button) activity.findViewById(R.id.startButton);
                startButton.setEnabled(false);
                final Button prefsButton = (Button) activity.findViewById(R.id.prefsButton);
                prefsButton.setEnabled(false);
                final Button disksButton = (Button) activity.findViewById(R.id.disksButton);
                disksButton.setEnabled(false);

                final ProgressBar bar = (ProgressBar) activity.findViewById(R.id.crash_progressBar);
                bar.setVisibility(View.VISIBLE);

                new Thread(new Runnable() {
                    @Override
                    public void run() {

                        android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_BACKGROUND);

                        final int sampleRate = DevicePropertyCalculator.getRecommendedSampleRate(activity);
                        final int monoBufferSize = DevicePropertyCalculator.getRecommendedBufferSize(activity, /*isStereo:*/false);
                        final int stereoBufferSize = DevicePropertyCalculator.getRecommendedBufferSize(activity, /*isStereo:*/true);

                        StringBuilder allCrashData = new StringBuilder();

                        // prepend information about this device
                        allCrashData.append("BRAND: ").append(Build.BRAND).append("\n");
                        allCrashData.append("MODEL: ").append(Build.MODEL).append("\n");
                        allCrashData.append("MANUFACTURER: ").append(Build.MANUFACTURER).append("\n");
                        allCrashData.append("DEVICE: ").append(Build.DEVICE).append("\n");
                        allCrashData.append("SAMPLE RATE: ").append(sampleRate).append("\n");
                        allCrashData.append("MONO BUFSIZE: ").append(monoBufferSize).append("\n");
                        allCrashData.append("STEREO BUFSIZE: ").append(stereoBufferSize).append("\n");
                        allCrashData.append("GPU VENDOR: ").append(Apple2Preferences.GL_VENDOR.stringValue(activity)).append("\n");
                        allCrashData.append("GPU RENDERER: ").append(Apple2Preferences.GL_RENDERER.stringValue(activity)).append("\n");
                        allCrashData.append("GPU VERSION: ").append(Apple2Preferences.GL_VERSION.stringValue(activity)).append("\n");

                        File[] nativeCrashes = _nativeCrashFiles(activity);
                        if (nativeCrashes == null) {
                            nativeCrashes = new File[0];
                        }

                        final int len = nativeCrashes.length + 1/* maybe Java crash */ + 1/* exposeSymbols */;

                        activity.runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                bar.setMax(len);
                            }
                        });

                        if (len > 0) {
                            Apple2DisksMenu.exposeSymbols(activity);
                        }

                        activity.runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                bar.setProgress(1);
                            }
                        });

                        // iteratively process native crashes
                        for (File crash : nativeCrashes) {

                            String crashPath = crash.getAbsolutePath();
                            Log.d(TAG, "Processing crash : " + crashPath);

                            String processedPath = _dumpPath2ProcessedPath(crashPath);
                            nativeProcessCrash(crashPath, processedPath); // Run Breakpad minidump_stackwalk

                            StringBuilder crashData = new StringBuilder();
                            if (!_readFile(new File(processedPath), crashData)) {
                                Log.e(TAG, "Error processing crash : " + crashPath);
                            }
                            allCrashData.append(">>>>>>> NATIVE CRASH [").append(crashPath).append("]\n");
                            allCrashData.append(crashData);

                            activity.runOnUiThread(new Runnable() {
                                @Override
                                public void run() {
                                    bar.setProgress(bar.getProgress()+1);
                                }
                            });
                        }

                        StringBuilder javaCrashData = new StringBuilder();
                        File javaCrashFile = _javaCrashFile(activity);
                        if (javaCrashFile.exists()) {
                            Log.d(TAG, "Reading java crashes file");
                            if (!_readFile(javaCrashFile, javaCrashData)) {
                                Log.e(TAG, "Error processing java crash : " + javaCrashFileName);
                            }
                        }

                        allCrashData.append(">>>>>>> JAVA CRASH DATA\n");
                        allCrashData.append(javaCrashData);
                        activity.runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                bar.setProgress(bar.getProgress() + 1);
                            }
                        });

                        Apple2DisksMenu.unexposeSymbols(activity);
                        activity.runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                bar.setVisibility(View.INVISIBLE);
                                startButton.setEnabled(true);
                                prefsButton.setEnabled(true);
                                disksButton.setEnabled(true);
                            }
                        });

                        // send report with all the data
                        _sendEmailToDeveloperWithCrashData(activity, allCrashData);
                    }
                }).start();
            }
        }).create();
        activity.registerAndShowDialog(crashDialog);
    }

    public void performCrash(int crashType) {
        if (BuildConfig.DEBUG) {
            nativePerformCrash(crashType);
        }
    }

    // ------------------------------------------------------------------------
    // privates

    private Apple2CrashHandler() {
        /* ... */
    }

    private static void onUncaughtException(Thread thread, Throwable t) {
        StackTraceElement[] stackTraceElements = t.getStackTrace();
        StringBuffer traceBuffer = new StringBuffer();

        // append the Java stack trace
        traceBuffer.append("NAME: ").append(t.getClass().getName()).append("\n");
        traceBuffer.append("MESSAGE: ").append(t.getMessage()).append("\n");
        final int maxTraceSize = 2048 + 1024 + 512; // probably should keep this less than a standard Linux PAGE_SIZE
        for (StackTraceElement elt : stackTraceElements) {
            traceBuffer.append(elt.toString());
            traceBuffer.append("\n");
            if (traceBuffer.length() >= maxTraceSize) {
                break;
            }
        }
        traceBuffer.append("\n");

        final int maxAttempts = 5;
        int attempts = 0;
        do {
            try {
                BufferedWriter writer = new BufferedWriter(new FileWriter(new File(sCrashHandler.homeDir, javaCrashFileName), /*append:*/true));
                writer.append(traceBuffer);
                writer.flush();
                writer.close();
                break;
            } catch (InterruptedIOException ie) {
                /* EINTR, EAGAIN ... */
            } catch (IOException e) {
                Log.e(TAG, "Exception attempting to write data : " + e);
            }

            try {
                Thread.sleep(100, 0);
            } catch (InterruptedException e) {
                /* ... */
            }
            ++attempts;
        } while (attempts < maxAttempts);
    }

    private File _javaCrashFile(Apple2Activity activity) {
        return new File(homeDir, javaCrashFileName);
    }

    private File[] _nativeCrashFiles(Apple2Activity activity) {
        FilenameFilter dmpFilter = new FilenameFilter() {
            public boolean accept(File dir, String name) {
                File file = new File(dir, name);
                if (file.isDirectory()) {
                    return false;
                }

                // check file extensions ... sigh ... no String.endsWithIgnoreCase() ?

                final String extension = ".dmp";
                final int nameLen = name.length();
                final int extLen = extension.length();
                if (nameLen <= extLen) {
                    return false;
                }

                String suffix = name.substring(nameLen - extLen, nameLen);
                return (suffix.equalsIgnoreCase(extension));
            }
        };

        return new File(homeDir).listFiles(dmpFilter);
    }

    private String _dumpPath2ProcessedPath(String crashPath) {
        return crashPath.substring(0, crashPath.length() - 4) + ".txt";
    }

    private boolean _readFile(File file, StringBuilder fileData) {
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
                Log.d(TAG, "Error reading file at path : " + file.toString());
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

    private File _writeTempLogFile(Apple2Activity activity, StringBuilder allCrashData) {

        File allCrashFile = null;

        String storageState = Environment.getExternalStorageState();
        if (storageState.equals(Environment.MEDIA_MOUNTED)) {
            allCrashFile = new File(Environment.getExternalStorageDirectory(), "apple2ix_crash.txt");
        } else {
            allCrashFile = new File(Apple2DisksMenu.getDataDir(activity), "apple2ix_crash.txt");
        }

        Log.d(TAG, "Writing all crashes to temp file : " + allCrashFile.getAbsolutePath());
        final int maxAttempts = 5;
        int attempts = 0;
        do {
            try {
                BufferedWriter writer = new BufferedWriter(new FileWriter(allCrashFile));
                writer.append(allCrashData);
                writer.flush();
                writer.close();
                break;
            } catch (InterruptedIOException ie) {
                /* EINTR, EAGAIN ... */
            } catch (IOException e) {
                Log.e(TAG, "Exception attempting to write data : " + e);
            }

            try {
                Thread.sleep(100, 0);
            } catch (InterruptedException e) {
                /* ... */
            }
            ++attempts;
        } while (attempts < maxAttempts);

        if (!allCrashFile.setReadable(true, /*ownerOnly:*/false)) {
            Log.d(TAG, "Oops, could not set all crash data readable!");
        }

        return allCrashFile;
    }

    private void _sendEmailToDeveloperWithCrashData(Apple2Activity activity, StringBuilder allCrashData) {
        mAlreadySentReport.set(true);

        Intent emailIntent = new Intent(Intent.ACTION_SENDTO, Uri.fromParts("mailto", "apple2ix_crash@deadcode.org"/*non-zero variant is correct endpoint at the moment*/, null));
        emailIntent.putExtra(Intent.EXTRA_SUBJECT, "Crasher");

        File allCrashFile = _writeTempLogFile(activity, allCrashData);
        // Putting all the text data into the EXTRA_TEXT appears to trigger android.os.TransactionTooLargeException ...
        //emailIntent.putExtra(Intent.EXTRA_TEXT, allCrashData.toString());
        emailIntent.putExtra(Intent.EXTRA_STREAM, Uri.fromFile(allCrashFile));

        // But we can put some text data
        emailIntent.putExtra(Intent.EXTRA_TEXT, "Greeting Apple2ix developers!  The app crashed, please help!");

        Log.d(TAG, "STARTING CHOOSER FOR EMAIL ...");
        activity.startActivity(Intent.createChooser(emailIntent, "Send email"));
        Log.d(TAG, "AFTER START ACTIVITY ...");
    }


    private final static String TAG = "Apple2CrashHandler";
    private final static Apple2CrashHandler sCrashHandler = new Apple2CrashHandler();

    private String homeDir;
    private Thread.UncaughtExceptionHandler mDefaultExceptionHandler;
    private AtomicBoolean mAlreadyRanCrashCheck = new AtomicBoolean(false);
    private AtomicBoolean mAlreadySentReport = new AtomicBoolean(false);

    private static native void nativePerformCrash(int crashType); // testing

    private static native void nativeProcessCrash(String crashFilePath, String crashProcessedPath);

}
