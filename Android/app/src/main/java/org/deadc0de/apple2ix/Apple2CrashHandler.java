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

import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.support.v7.app.AlertDialog;
import android.util.Log;
import android.view.View;
import android.widget.ProgressBar;

import org.deadc0de.apple2ix.basic.BuildConfig;
import org.deadc0de.apple2ix.basic.R;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.BufferedWriter;
import java.io.File;
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

    public enum SETTINGS implements Apple2AbstractMenu.IMenuEnum {
        GL_VENDOR {
            @Override
            public String getPrefKey() {
                return "glVendor";
            }

            @Override
            public Object getPrefDefault() {
                return "unknown";
            }
        },
        GL_RENDERER {
            @Override
            public String getPrefKey() {
                return "glRenderer";
            }

            @Override
            public Object getPrefDefault() {
                return "unknown";
            }
        },
        GL_VERSION {
            @Override
            public String getPrefKey() {
                return "glVersion";
            }

            @Override
            public Object getPrefDefault() {
                return "unknown";
            }
        };

        @Override
        public final String getTitle(Apple2Activity activity) {
            throw new RuntimeException();
        }

        @Override
        public final String getSummary(Apple2Activity activity) {
            throw new RuntimeException();
        }

        @Override
        public String getPrefDomain() {
            return Apple2Preferences.PREF_DOMAIN_INTERFACE;
        }

        @Override
        public View getView(final Apple2Activity activity, View convertView) {
            throw new RuntimeException();
        }

        @Override
        public void handleSelection(final Apple2Activity activity, final Apple2AbstractMenu settingsMenu, boolean isChecked) {
            throw new RuntimeException();
        }
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
        },
        SIGABRT {
            @Override
            public final String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.crash_sigabrt);
            }
        },
        SIGFPE {
            @Override
            public final String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.crash_sigfpe);
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
                homeDir = Apple2Utils.getDataDir(activity);
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

        Apple2Preferences.load(activity);
        if (!(boolean) Apple2Preferences.getJSONPref(Apple2SettingsMenu.SETTINGS.CRASH)) {
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
            File allCrashFile = _getCrashFile(activity);
            Apple2Utils.writeFile(new StringBuilder(), allCrashFile);
            allCrashFile.delete();
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

                final Apple2SplashScreen splashScreen = activity.getSplashScreen();
                if (splashScreen != null) {
                    splashScreen.setDismissable(false);
                }
                final ProgressBar bar = (ProgressBar) activity.findViewById(R.id.crash_progressBar);
                try {
                    bar.setVisibility(View.VISIBLE);
                } catch (NullPointerException npe) {
                    /* could happen on early lifecycle crashes */
                }

                new Thread(new Runnable() {
                    @Override
                    public void run() {

                        android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_BACKGROUND);

                        final int sampleRate = DevicePropertyCalculator.getRecommendedSampleRate(activity);
                        final int monoBufferSize = DevicePropertyCalculator.getRecommendedBufferSize(activity, /*isStereo:*/false);
                        final int stereoBufferSize = DevicePropertyCalculator.getRecommendedBufferSize(activity, /*isStereo:*/true);

                        StringBuilder summary = new StringBuilder();
                        StringBuilder allCrashData = new StringBuilder();

                        // prepend information about this device
                        summary.append("BRAND: ").append(Build.BRAND).append("\n");
                        summary.append("MODEL: ").append(Build.MODEL).append("\n");
                        summary.append("MANUFACTURER: ").append(Build.MANUFACTURER).append("\n");
                        summary.append("DEVICE: ").append(Build.DEVICE).append("\n");
                        summary.append("SDK: ").append(Build.VERSION.SDK_INT).append("\n");
                        summary.append("SAMPLE RATE: ").append(sampleRate).append("\n");
                        summary.append("MONO BUFSIZE: ").append(monoBufferSize).append("\n");
                        summary.append("STEREO BUFSIZE: ").append(stereoBufferSize).append("\n");
                        summary.append("GPU VENDOR: ").append(Apple2Preferences.getJSONPref(SETTINGS.GL_VENDOR)).append("\n");
                        summary.append("GPU RENDERER: ").append(Apple2Preferences.getJSONPref(SETTINGS.GL_RENDERER)).append("\n");
                        summary.append("GPU VERSION: ").append(Apple2Preferences.getJSONPref(SETTINGS.GL_VERSION)).append("\n");

                        try {
                            PackageInfo pInfo = activity.getPackageManager().getPackageInfo(activity.getPackageName(), 0);
                            summary.append("APP VERSION: ").append(pInfo.versionName).append("\n");
                        } catch (PackageManager.NameNotFoundException e) {
                            // ...
                        }

                        allCrashData.append(summary);

                        File[] nativeCrashes = _nativeCrashFiles(activity);
                        if (nativeCrashes == null) {
                            nativeCrashes = new File[0];
                        }

                        final int len = nativeCrashes.length + 1/* maybe Java crash */ + 1/* exposeSymbols */;

                        activity.runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                if (bar != null) {
                                    bar.setMax(len);
                                }
                            }
                        });

                        if (len > 0) {
                            Apple2Utils.exposeSymbols(activity);
                        }

                        activity.runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                if (bar != null) {
                                    bar.setProgress(1);
                                }
                            }
                        });

                        boolean summarizedHeader = false;

                        // iteratively process native crashes
                        for (File crash : nativeCrashes) {

                            String crashPath = crash.getAbsolutePath();
                            Log.d(TAG, "Processing crash : " + crashPath);

                            String processedPath = _dumpPath2ProcessedPath(crashPath);
                            try {
                                nativeProcessCrash(crashPath, processedPath); // Run Breakpad minidump_stackwalk
                            } catch (UnsatisfiedLinkError ule) {
                                /* could happen on early lifecycle crashes */
                            }

                            StringBuilder crashData = new StringBuilder();
                            if (!Apple2Utils.readEntireFile(new File(processedPath), crashData)) {
                                Log.e(TAG, "Error processing crash : " + crashPath);
                            }
                            allCrashData.append(">>>>>>> NATIVE CRASH [").append(crashPath).append("]\n");
                            allCrashData.append(crashData);
                            summary.append("NATIVE CRASH:\n");

                            // append succinct information about crashing thread
                            String[] lines = crashData.toString().split("[\\n\\r][\\n\\r]*");
                            for (int i = 0, j = 0; i < lines.length; i++) {

                                // 2 lines of minidump summary
                                if (i < 2) {
                                    if (!summarizedHeader) {
                                        summary.append(lines[i]);
                                        summary.append("\n");
                                    }
                                    continue;
                                }

                                // 1 line of crashing thread and reason
                                if (i == 2) {
                                    summarizedHeader = true;
                                    summary.append(lines[i]);
                                    summary.append("\n");
                                    continue;
                                }

                                // whole lotta modules
                                if (lines[i].startsWith("Module")) {
                                    continue;
                                }

                                // one apparently empty line
                                if (lines[i].matches("^[ \\t]*$")) {
                                    continue;
                                }

                                // append crashing thread backtrace

                                summary.append(lines[i]);
                                summary.append("\n");
                                final int maxSummaryBacktrace = 8;
                                if (j++ >= maxSummaryBacktrace) {
                                    break;
                                }
                            }

                            activity.runOnUiThread(new Runnable() {
                                @Override
                                public void run() {
                                    if (bar != null) {
                                        bar.setProgress(bar.getProgress() + 1);
                                    }
                                }
                            });
                        }

                        StringBuilder javaCrashData = new StringBuilder();
                        File javaCrashFile = _javaCrashFile(activity);
                        if (javaCrashFile.exists()) {
                            Log.d(TAG, "Reading java crashes file");
                            if (!Apple2Utils.readEntireFile(javaCrashFile, javaCrashData)) {
                                Log.e(TAG, "Error processing java crash : " + javaCrashFileName);
                            }
                        }

                        allCrashData.append(">>>>>>> JAVA CRASH DATA\n");
                        allCrashData.append(javaCrashData);

                        summary.append("JAVA CRASH:\n");
                        summary.append(javaCrashData);

                        activity.runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                if (bar != null) {
                                    bar.setProgress(bar.getProgress() + 1);
                                }
                            }
                        });

                        StringBuilder jsonData = new StringBuilder();
                        if (Apple2Utils.readEntireFile(new File(homeDir, Apple2Preferences.PREFS_FILE), jsonData)) {
                            JSONObject obj = null;
                            try {
                                obj = new JSONObject(jsonData.toString());
                            } catch (JSONException e) {
                                Log.e(TAG, "Error reading preferences : " + e);
                            }
                            if (obj != null) {
                                summary.append("PREFS:\n");
                                summary.append(obj.toString());
                                allCrashData.append(">>>>>>> PREFS\n");
                                allCrashData.append(obj.toString());
                            }
                        }

                        Apple2Utils.unexposeSymbols(activity);
                        activity.runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                try {
                                    bar.setVisibility(View.INVISIBLE);
                                    splashScreen.setDismissable(true);
                                } catch (NullPointerException npe) {
                                    /* could happen on early lifecycle crashes */
                                }
                            }
                        });

                        // send report with all the data
                        _sendEmailToDeveloperWithCrashData(activity, summary, allCrashData);
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

    private File _getCrashFile(Apple2Activity activity) {
        File allCrashFile;
        String storageState = Environment.getExternalStorageState();
        if (storageState.equals(Environment.MEDIA_MOUNTED)) {
            allCrashFile = new File(Environment.getExternalStorageDirectory(), "apple2ix_crash.txt");
        } else {
            allCrashFile = new File(Apple2Utils.getDataDir(activity), "apple2ix_crash.txt");
        }
        Log.d(TAG, "Writing all crashes to temp file : " + allCrashFile.getAbsolutePath());
        return allCrashFile;
    }

    private void _sendEmailToDeveloperWithCrashData(Apple2Activity activity, StringBuilder summary, StringBuilder allCrashData) {
        mAlreadySentReport.set(true);

        // <sigh> ... the disaster that is early Android ... there does not appear to be a reliable way to start an
        // email Intent to send both text and an attachment, but we make a valiant (if futile) effort to do so here.
        // And the reason to send an attachment is that you trigger an android.os.TransactionTooLargeException with too
        // much text data in the EXTRA_TEXT ... </sigh>

        Intent emailIntent = new Intent(Intent.ACTION_SENDTO, Uri.fromParts("mailto", "apple2ix_crash@deadcode.org"/*non-zero variant is correct endpoint at the moment*/, null));
        emailIntent.putExtra(Intent.EXTRA_SUBJECT, "Crasher");

        final int maxCharsEmail = 4096;
        int len = summary.length();
        len = len < maxCharsEmail ? len : maxCharsEmail;
        String summaryData = summary.substring(0, len);
        emailIntent.putExtra(Intent.EXTRA_TEXT, "The app crashed, please help!\n\n" + summaryData);

        File allCrashFile = _getCrashFile(activity);
        Apple2Utils.writeFile(allCrashData, allCrashFile);
        if (!allCrashFile.setReadable(true, /*ownerOnly:*/false)) {
            Log.d(TAG, "Oops, could not set file data readable!");
        }

        emailIntent.putExtra(Intent.EXTRA_STREAM, Uri.fromFile(allCrashFile));

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
