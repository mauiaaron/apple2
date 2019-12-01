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
import android.os.Build;
import android.os.Environment;
import androidx.appcompat.app.AlertDialog;
import android.view.View;
import android.widget.ProgressBar;

import org.deadc0de.apple2ix.basic.BuildConfig;
import org.deadc0de.apple2ix.basic.R;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.io.FilenameFilter;
import java.io.IOException;
import java.io.InterruptedIOException;
import java.util.ArrayList;
import java.util.concurrent.atomic.AtomicBoolean;

public class Apple2CrashHandler {

    public final static String ALL_CRASH_FILE = "apple2ix_data.zip";

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

        File oldCrashFile = _getCrashLogFile(activity);
        if (oldCrashFile.exists()) {
            oldCrashFile.delete();
        }

        if (!areCrashesPresent(activity)) {
            return;
        }

        Apple2Preferences.load(activity);
        if (!(boolean) Apple2Preferences.getJSONPref(Apple2SettingsMenu.SETTINGS.CRASH)) {
            cleanCrashData(activity);
            return;
        }

        boolean previouslyRanCrashCheck = mAlreadyRanCrashCheck.getAndSet(true);
        if (previouslyRanCrashCheck) {
            // don't keep asking on return from backgrounding
            return;
        }

        final AlertDialog crashDialog = new AlertDialog.Builder(activity).setIcon(R.drawable.ic_launcher).setCancelable(true).setTitle(R.string.crasher_send).setMessage(R.string.crasher_send_message).setPositiveButton(R.string.ok, new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                dialog.dismiss();
                emailCrashesAndLogs(activity);
            }
        }).create();
        activity.registerAndShowDialog(crashDialog);
    }

    public void emailCrashesAndLogs(final Apple2Activity activity) {
        final Apple2SplashScreen splashScreen = activity.getSplashScreen();
        if (splashScreen != null) {
            splashScreen.setDismissable(false);
        }
        final ProgressBar bar = (ProgressBar) activity.findViewById(R.id.crash_progressBar);
        try {
            bar.setVisibility(View.VISIBLE);
        } catch (NullPointerException npe) {
            /* email logs doesn't show the splash screen */
        }

        new Thread(new Runnable() {
            @Override
            public void run() {

                android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_BACKGROUND);

                final int sampleRate = DevicePropertyCalculator.getRecommendedSampleRate(activity);
                final int monoBufferSize = DevicePropertyCalculator.getRecommendedBufferSize(activity, /*isStereo:*/false);
                final int stereoBufferSize = DevicePropertyCalculator.getRecommendedBufferSize(activity, /*isStereo:*/true);

                StringBuilder summary = new StringBuilder();

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

                if (nativeCrashes.length > 0) {
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

                // iteratively process native crashes
                ArrayList<File> allCrashFiles = new ArrayList<File>();
                if (nativeCrashes.length > 0) {
                    for (File crash : nativeCrashes) {

                        String crashPath = crash.getAbsolutePath();
                        Apple2Activity.logMessage(Apple2Activity.LogType.DEBUG, TAG, "Processing crash : " + crashPath);

                        String processedPath = _dumpPath2ProcessedPath(crashPath);
                        try {
                            nativeProcessCrash(crashPath, processedPath); // Run Breakpad minidump_stackwalk
                        } catch (UnsatisfiedLinkError ule) {
                            /* could happen on early lifecycle crashes */
                        }

                        allCrashFiles.add(new File(processedPath));

                        activity.runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                if (bar != null) {
                                    bar.setProgress(bar.getProgress() + 1);
                                }
                            }
                        });
                    }
                    summary.append("" + nativeCrashes.length + " Native dumps\n");
                }

                File javaCrashFile = _javaCrashFile(activity);
                if (javaCrashFile.exists()) {
                    summary.append("Java crash log\n");
                    allCrashFiles.add(javaCrashFile);
                }

                activity.runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        if (bar != null) {
                            bar.setProgress(bar.getProgress() + 1);
                        }
                    }
                });

                allCrashFiles.add(new File(homeDir, Apple2Preferences.PREFS_FILE));

                if (nativeCrashes.length > 0) {
                    Apple2Utils.unexposeSymbols(activity);
                }

                activity.runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        try {
                            bar.setVisibility(View.INVISIBLE);
                            splashScreen.setDismissable(true);
                        } catch (NullPointerException npe) {
                            /* email logs doesn't show the splash screen */
                        }
                    }
                });

                // Add all the log files ...
                {
                    File[] nativeLogs = _nativeLogs(activity);
                    for (File logFile : nativeLogs) {
                        allCrashFiles.add(logFile);
                    }
                }

                File[] allCrashesAry = new File[allCrashFiles.size()];
                File nativeCrashesZip = Apple2Utils.zipFiles(allCrashFiles.toArray(allCrashesAry), _getCrashLogFile(activity));
                // send report with all the data
                _sendEmailToDeveloperWithCrashData(activity, summary, nativeCrashesZip);
            }
        }).start();
    }

    public void cleanCrashData(Apple2Activity activity) {

        Apple2Activity.logMessage(Apple2Activity.LogType.DEBUG, TAG, "Cleaning up crash data ...");
        File[] nativeCrashes = _nativeCrashFiles(activity);
        for (File crash : nativeCrashes) {

            if (!crash.delete()) {
                Apple2Activity.logMessage(Apple2Activity.LogType.DEBUG, TAG, "Could not unlink crash : " + crash);
            }

            File processed = new File(_dumpPath2ProcessedPath(crash.getAbsolutePath()));
            if (!processed.delete()) {
                Apple2Activity.logMessage(Apple2Activity.LogType.DEBUG, TAG, "Could not unlink processed : " + processed);
            }
        }

        File javaCrashFile = _javaCrashFile(activity);
        if (!javaCrashFile.delete()) {
            Apple2Activity.logMessage(Apple2Activity.LogType.DEBUG, TAG, "Could not unlink java crash : " + javaCrashFile);
        }

        /* HACK NOTE -- don't delete the crash log file here ... possibly the email sender program still needs the link!
        File allCrashFile = _getCrashLogFile(activity);
        Apple2Utils.writeFile(new StringBuilder(), allCrashFile);
        allCrashFile.delete();
        */
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
                Apple2Activity.logMessage(Apple2Activity.LogType.ERROR, TAG, "Exception attempting to write data : " + e);
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

    private File[] _nativeLogs(Apple2Activity activity) {
        FilenameFilter logFilter = new FilenameFilter() {
            public boolean accept(File dir, String name) {
                File file = new File(dir, name);
                if (file.isDirectory()) {
                    return false;
                }

                return name.startsWith("apple2ix_log");
            }
        };

        return new File(homeDir).listFiles(logFilter);
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

    private File _getCrashLogFile(Apple2Activity activity) {
        File file;
        String storageState = Environment.getExternalStorageState();
        if (storageState.equals(Environment.MEDIA_MOUNTED)) {
            file = new File(Environment.getExternalStorageDirectory(), ALL_CRASH_FILE);
        } else {
            file = new File(Apple2Utils.getDataDir(activity), ALL_CRASH_FILE);
        }
        return file;
    }

    private void _sendEmailToDeveloperWithCrashData(Apple2Activity activity, StringBuilder summary, File nativeCrashesZip) {
        final boolean alreadyChoosing = Apple2EmailerActivity.sEmailerIsEmailing.getAndSet(true);
        if (alreadyChoosing) {
            return;
        }

        // <sigh> ... the disaster that is early Android ... there does not appear to be a reliable way to start an
        // email Intent to send both text and an attachment, but we make a valiant (if futile) effort to do so here.
        // And the reason to send an attachment is that you trigger an android.os.TransactionTooLargeException with too
        // much text data in the EXTRA_TEXT ... </sigh>

        Intent emailIntent = new Intent(activity, Apple2EmailerActivity.class);
        emailIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK/* | Intent.FLAG_ACTIVITY_CLEAR_TOP */);

        final int maxCharsEmail = 4096;
        int len = summary.length();
        len = len < maxCharsEmail ? len : maxCharsEmail;
        String summaryData = summary.substring(0, len);
        emailIntent.putExtra(Intent.EXTRA_TEXT, "A2IX app logs and crash data\n\n" + summaryData);

        if (!nativeCrashesZip.setReadable(true, /*ownerOnly:*/false)) {
            Apple2Activity.logMessage(Apple2Activity.LogType.DEBUG, TAG, "Oops, could not set crash file data readable!");
        }

        emailIntent.putExtra(Apple2EmailerActivity.EXTRA_STREAM_PATH, nativeCrashesZip.getAbsolutePath());

        Apple2Activity.logMessage(Apple2Activity.LogType.DEBUG, TAG, "STARTING EMAILER ACTIVITY ...");
        Apple2EmailerActivity.sEmailerCallback = activity;

        activity.startActivityForResult(emailIntent, Apple2EmailerActivity.SEND_REQUEST_CODE);
    }

    private final static String TAG = "Apple2CrashHandler";
    private final static Apple2CrashHandler sCrashHandler = new Apple2CrashHandler();

    private String homeDir;
    private Thread.UncaughtExceptionHandler mDefaultExceptionHandler;
    private AtomicBoolean mAlreadyRanCrashCheck = new AtomicBoolean(false);

    private static native void nativePerformCrash(int crashType); // testing

    private static native void nativeProcessCrash(String crashFilePath, String crashProcessedPath);
}
