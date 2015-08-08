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

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;

public class Apple2SplashScreen implements Apple2MenuView {

    private final static String TAG = "Apple2SplashScreen";

    private Apple2Activity mActivity = null;
    private View mSettingsView = null;

    public Apple2SplashScreen(Apple2Activity activity) {
        mActivity = activity;
        setup();
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

        Button prefsButton = (Button) mSettingsView.findViewById(R.id.prefsButton);
        prefsButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Apple2SettingsMenu settingsMenu = mActivity.getMainMenu().getSettingsMenu();
                settingsMenu.show();
                Apple2SplashScreen.this.dismiss();
            }
        });
    }

    public final boolean isCalibrating() {
        return false;
    }

    public void show() {
        if (isShowing()) {
            return;
        }
        mActivity.pushApple2View(this);
    }

    public void dismiss() {
        mActivity.popApple2View(this);
    }

    public void dismissAll() {
        dismiss();
    }

    public boolean isShowing() {
        return mSettingsView.isShown();
    }

    public View getView() {
        return mSettingsView;
    }
}
