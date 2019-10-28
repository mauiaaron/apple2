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

import android.content.Context;
import android.content.DialogInterface;
import android.support.v7.app.AlertDialog;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;

import org.deadc0de.apple2ix.basic.R;

public class Apple2SplashScreen implements Apple2MenuView {

    private final static String TAG = "Apple2SplashScreen";

    private Apple2Activity mActivity = null;
    private boolean mDismissable = true;
    private View mSettingsView = null;

    public Apple2SplashScreen(Apple2Activity activity, boolean dismissable) {
        mActivity = activity;
        setup();
        setDismissable(dismissable);
    }

    private void setup() {
        LayoutInflater inflater = (LayoutInflater) mActivity.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        mSettingsView = inflater.inflate(R.layout.activity_splash_screen, null, false);

        Button startButton = (Button) mSettingsView.findViewById(R.id.startButton);
        startButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Apple2SplashScreen.this.dismiss();
            }
        });

        Button prefsButton = (Button) mSettingsView.findViewById(R.id.resetButton);
        prefsButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                AlertDialog.Builder builder = new AlertDialog.Builder(mActivity).setIcon(R.drawable.ic_launcher).setCancelable(true).setTitle(R.string.preferences_reset_really).setMessage(R.string.preferences_reset_warning).setPositiveButton(R.string.ok, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        dialog.dismiss();
                        Apple2Preferences.reset(mActivity);
                    }
                }).setNegativeButton(R.string.no, null);
                AlertDialog dialog = builder.create();
                mActivity.registerAndShowDialog(dialog);
            }
        });

        Button disksButton = (Button) mSettingsView.findViewById(R.id.disksButton);
        disksButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Apple2DisksMenu disksMenu = mActivity.getDisksMenu();
                disksMenu.show();
            }
        });
    }

    public void setDismissable(boolean dismissable) {
        mDismissable = dismissable;
        mActivity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                Button startButton = (Button) mSettingsView.findViewById(R.id.startButton);
                startButton.setEnabled(mDismissable);
                Button prefsButton = (Button) mSettingsView.findViewById(R.id.resetButton);
                prefsButton.setEnabled(mDismissable);
                Button disksButton = (Button) mSettingsView.findViewById(R.id.disksButton);
                disksButton.setEnabled(mDismissable);
            }
        });
    }

    public final boolean isCalibrating() {
        return false;
    }

    public void onKeyTapCalibrationEvent(char ascii, int scancode) {
        /* ... */
    }

    public void show() {
        if (isShowing()) {
            return;
        }
        mActivity.pushApple2View(this);
    }

    public void dismiss() {
        if (mDismissable) {
            mActivity.popApple2View(this);
        }
    }

    public void dismissAll() {
        dismiss();
    }

    public boolean isShowing() {
        return mSettingsView.getParent() != null;
    }

    public View getView() {
        return mSettingsView;
    }
}
