/*
 * Apple // emulator for *nix
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 2019 Aaron Culliney
 *
 */

package org.deadc0de.apple2ix;

import android.content.Intent;
import android.media.MediaScannerConnection;
import android.net.Uri;
import android.os.Bundle;

import androidx.appcompat.app.AppCompatActivity;

import java.util.concurrent.atomic.AtomicBoolean;

public class Apple2EmailerActivity extends AppCompatActivity {

    public static final String EXTRA_STREAM_PATH = Intent.EXTRA_STREAM + "-PATH";

    public static final int SEND_REQUEST_CODE = 46;

    public static final AtomicBoolean sEmailerIsEmailing = new AtomicBoolean(false);
    public static Callback sEmailerCallback;

    public interface Callback {
        void onEmailerFinished();
    }

    @Override
    protected void onRestoreInstanceState(Bundle inState) {
        super.onRestoreInstanceState(inState);
        mExtraText = inState.getString(Intent.EXTRA_TEXT);
        mExtraStreamPath = inState.getString(EXTRA_STREAM_PATH);
        mExtraStream = inState.getParcelable(Intent.EXTRA_STREAM);
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        outState.putBoolean("ran", true);
        outState.putString(Intent.EXTRA_TEXT, mExtraText);
        outState.putString(EXTRA_STREAM_PATH, mExtraStreamPath);
        outState.putParcelable(Intent.EXTRA_STREAM, mExtraStream);
        super.onSaveInstanceState(outState);
    }

    @Override
    protected void onCreate(Bundle savedState) {
        super.onCreate(savedState);

        Bundle b;
        {
            Intent intent = getIntent();
            Bundle extras = null;
            if (intent != null) {
                extras = intent.getExtras();
            }

            if (savedState != null) {
                b = savedState;
            } else if (extras != null) {
                b = extras;
            } else {
                b = new Bundle();
            }
        }

        final boolean ran = b.getBoolean("ran");
        mExtraText = b.getString(Intent.EXTRA_TEXT);
        mExtraStreamPath = b.getString(EXTRA_STREAM_PATH);

        MediaScannerConnection.scanFile(this,
                /*paths:*/new String[] { mExtraStreamPath },
                /*mimeTypes:*/new String[] { "application/zip" },
                new MediaScannerConnection.OnScanCompletedListener() {
                    public void onScanCompleted(String path, Uri uri) {

                        mExtraStream = uri;
                        Intent emailIntent = new Intent(Intent.ACTION_SEND);

                        try {
                            emailIntent.putExtra(Intent.EXTRA_EMAIL, new String[] { "apple2ix_crash@deadcode.org" });
                            emailIntent.putExtra(Intent.EXTRA_SUBJECT, "A2IX Report To apple2ix_crash@deadcode.org");
                            emailIntent.putExtra(Intent.EXTRA_TEXT, mExtraText);
                            emailIntent.putExtra(Intent.EXTRA_STREAM, mExtraStream);
                            emailIntent.setType("message/rfc822");

                            if (!ran) {
                                Apple2Activity.logMessage(Apple2Activity.LogType.DEBUG, TAG, "STARTING EMAIL ACTIVITY ...");
                                startActivityForResult(emailIntent, SEND_REQUEST_CODE);
                            }
                        } catch (Throwable t) {
                            Apple2Activity.logMessage(Apple2Activity.LogType.ERROR, TAG, "OOPS : " + t);
                            setResult(RESULT_CANCELED);
                            finish();
                        }
                    }
                });
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);

        if (resultCode != RESULT_CANCELED) {
            // Do something?
        }

        if (mExtraStream != null) {
            int deleted = getContentResolver().delete(mExtraStream, null, null);
        }

        sEmailerCallback.onEmailerFinished();

        setResult(RESULT_OK);
        finish();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
    }

    @Override
    public void finish() {
        sEmailerIsEmailing.set(false);
        super.finish();
    }

    private String mExtraText;

    private String mExtraStreamPath;

    private Uri mExtraStream;

    private static final String TAG = "A2EmailerActivity";
}
