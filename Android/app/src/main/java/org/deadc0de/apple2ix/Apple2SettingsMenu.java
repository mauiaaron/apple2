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
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.FrameLayout;
import android.widget.ListView;
import android.widget.TabHost;

public class Apple2SettingsMenu {

    private final static String TAG = "Apple2SettingsMenu";

    private Apple2Activity mActivity = null;
    private Apple2View mParentView = null;
    private View mSettingsView = null;

    public Apple2SettingsMenu(Apple2Activity activity, Apple2View parent) {
        mActivity = activity;
        mParentView = parent;
        setup();
    }

    private void setup() {

        LayoutInflater inflater = (LayoutInflater)mActivity.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        mSettingsView = inflater.inflate(R.layout.activity_settings, null, false);

        // General Settings

        final CheckBox swipeToChangeSpeeds = (CheckBox)mSettingsView.findViewById(R.id.checkBox_speedswipe);
        swipeToChangeSpeeds.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                Apple2SettingsMenu.this.mActivity.setSwipeTogglesSpeed(isChecked);
            }
        });

        final CheckBox doubleTapShowsKeyboard = (CheckBox)mSettingsView.findViewById(R.id.checkBox_doubletapkeyboard);
        doubleTapShowsKeyboard.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                Apple2SettingsMenu.this.mActivity.setDoubleTapShowsKeyboard(isChecked);
            }
        });

        // Joystick Settings

        ListView settingsMenuView = (ListView)mSettingsView.findViewById(R.id.joystick_settings_listview);
        String[] values = new String[] {
                mActivity.getResources().getString(R.string.joystick_configure),
        };
        ArrayAdapter<?> adapter = new ArrayAdapter<String>(mActivity, android.R.layout.simple_list_item_1, android.R.id.text1, values);
        settingsMenuView.setAdapter(adapter);
        settingsMenuView.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                switch (position) {
                    case 0:
                        Apple2SettingsMenu.this.showJoystickConfiguration();
                        break;
                    default:
                        break;
                }
            }
        });

        // Tab management

        TabHost tabHost = (TabHost)mSettingsView.findViewById(R.id.tabHost_settings);
        tabHost.setup();
        TabHost.TabSpec spec = tabHost.newTabSpec("tab_general");
        spec.setIndicator(mActivity.getResources().getString(R.string.tab_general), mActivity.getResources().getDrawable(android.R.drawable.ic_menu_edit));
        spec.setContent(R.id.tab_general);
        tabHost.addTab(spec);

        spec = tabHost.newTabSpec("tab_joystick");
        spec.setIndicator(mActivity.getResources().getString(R.string.tab_joystick), mActivity.getResources().getDrawable(android.R.drawable.ic_menu_compass));
        spec.setContent(R.id.tab_joystick);
        tabHost.addTab(spec);
    }

    public void showJoystickConfiguration() {
        Log.d(TAG, "showJoystickConfiguration...");
    }

    public void show() {
        if (isShowing()) {
            return;
        }
        mActivity.nativeOnPause();
        mActivity.addContentView(mSettingsView, new FrameLayout.LayoutParams(mActivity.getWidth(), mActivity.getHeight()));
    }

    public void dismiss() {
        if (isShowing()) {
            dismissWithoutResume();
            mActivity.nativeOnResume();
        }
    }

    public void dismissWithoutResume() {
        if (isShowing()) {
            ((ViewGroup)mSettingsView.getParent()).removeView(mSettingsView);
            // HACK FIXME TODO ... we seem to lose ability to toggle/show soft keyboard upon dismissal of mSettingsView after use.
            // This hack appears to get the Android UI unwedged ... =P
            Apple2MainMenu androidUIFTW = mParentView.getMainMenu();
            androidUIFTW.show();
            androidUIFTW.dismiss();
        }
    }

    public boolean isShowing() {
        return mSettingsView.isShown();
    }
}
