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

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.graphics.drawable.Drawable;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.CheckBox;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.TextView;

public class Apple2SettingsMenu implements Apple2MenuView {

    private final static String TAG = "Apple2SettingsMenu";

    private Apple2Activity mActivity = null;
    private View mSettingsView = null;

    private Apple2AudioSettingsMenu mAudioSettings = null;

    public Apple2SettingsMenu(Apple2Activity activity) {
        mActivity = activity;
        setup();
        mAudioSettings = new Apple2AudioSettingsMenu(mActivity);
    }

    enum SETTINGS {
        JOYSTICK_CONFIGURE {
            @Override public String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.configure_joystick);
            }
            @Override public String getSummary(Apple2Activity activity) {
                return activity.getResources().getString(R.string.configure_joystick_summary);
            }
            @Override public void handleSelection(Apple2SettingsMenu settingsMenu, boolean isChecked) {
                //settingsMenu.mJoystickSettings.show();
            }
        },
        AUDIO_CONFIGURE {
            @Override public String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.audio_configure);
            }
            @Override public String getSummary(Apple2Activity activity) {
                return activity.getResources().getString(R.string.audio_configure_summary);
            }
            @Override public void handleSelection(Apple2SettingsMenu settingsMenu, boolean isChecked) {
                settingsMenu.mAudioSettings.show();
            }
        },
        VIDEO_CONFIGURE {
            @Override public String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.configure_video);
            }
            @Override public String getSummary(Apple2Activity activity) {
                return activity.getResources().getString(R.string.configure_video_summary);
            }
            @Override public View getView(Apple2Activity activity, View convertView) {
                convertView = _basicView(activity, this, convertView);
                _addPopupIcon(activity, this, convertView);
                return convertView;
            }
            @Override
            public void handleSelection(final Apple2SettingsMenu settingsMenu, boolean isChecked) {
                AlertDialog.Builder builder = new AlertDialog.Builder(settingsMenu.mActivity).setIcon(R.drawable.ic_launcher).setCancelable(true).setTitle(R.string.configure_video);
                builder.setNegativeButton(R.string.cancel, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        dialog.dismiss();
                    }
                });

                String[] color_choices = new String[]{
                        settingsMenu.mActivity.getResources().getString(R.string.color_bw),
                        settingsMenu.mActivity.getResources().getString(R.string.color_color),
                        settingsMenu.mActivity.getResources().getString(R.string.color_interpolated),
                };
                final ArrayAdapter<String> adapter = new ArrayAdapter<String>(settingsMenu.mActivity, android.R.layout.select_dialog_singlechoice, color_choices);

                builder.setAdapter(adapter, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int color) {
                        Apple2Preferences.HIRES_COLOR.saveHiresColor(settingsMenu.mActivity, Apple2Preferences.HiresColor.values()[color]);
                        dialog.dismiss();
                    }
                });
                builder.show();
            }
        };

        private static View _basicView(Apple2Activity activity, SETTINGS setting,  View convertView) {
            TextView tv = (TextView)convertView.findViewById(R.id.a2preference_title);
            tv.setText(setting.getTitle(activity));

            tv = (TextView)convertView.findViewById(R.id.a2preference_summary);
            tv.setText(setting.getSummary(activity));

            LinearLayout layout = (LinearLayout)convertView.findViewById(R.id.a2preference_widget_frame);
            if (layout.getChildCount() > 0) {
                // layout cells appear to be reused when scrolling into view ... make sure we start with clear hierarchy
                layout.removeAllViews();
            }

            return convertView;
        }
        private static ImageView _addPopupIcon(Apple2Activity activity, SETTINGS setting, View convertView) {
            ImageView imageView = new ImageView(activity);
            Drawable drawable = activity.getResources().getDrawable(android.R.drawable.ic_menu_edit);
            imageView.setImageDrawable(drawable);
            LinearLayout layout = (LinearLayout)convertView.findViewById(R.id.a2preference_widget_frame);
            layout.addView(imageView);
            return imageView;
        }
        private static CheckBox _addCheckbox(Apple2Activity activity, SETTINGS setting, View convertView, boolean isChecked) {
            CheckBox checkBox = new CheckBox(activity);
            checkBox.setChecked(isChecked);
            LinearLayout layout = (LinearLayout)convertView.findViewById(R.id.a2preference_widget_frame);
            layout.addView(checkBox);
            return checkBox;
        }

        public static final int size = SETTINGS.values().length;

        public abstract String getTitle(Apple2Activity activity);
        public abstract String getSummary(Apple2Activity activity);
        public abstract void handleSelection(Apple2SettingsMenu settingsMenu, boolean isChecked);

        public View getView(Apple2Activity activity, View convertView) {
            return _basicView(activity, this, convertView);
        }

        public static String[] titles(Apple2Activity activity) {
            String[] titles = new String[size];
            int i = 0;
            for (SETTINGS setting : values()) {
                titles[i++] = setting.getTitle(activity);
            }
            return titles;
        }
    }

    private void setup() {

        LayoutInflater inflater = (LayoutInflater)mActivity.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        mSettingsView = inflater.inflate(R.layout.activity_settings, null, false);

        ListView settingsList = (ListView)mSettingsView.findViewById(R.id.listView_settings);
        settingsList.setEnabled(true);

        ArrayAdapter<?> adapter = new ArrayAdapter<String>(mActivity, R.layout.a2preference, R.id.a2preference_title, SETTINGS.titles(mActivity)) {
            @Override
            public boolean areAllItemsEnabled() {
                return true;
            }

            @Override
            public boolean isEnabled(int position) {
                return super.isEnabled(position);
            }

            @Override
            public View getView(int position, View convertView, ViewGroup parent) {
                View view = super.getView(position, convertView, parent);
                SETTINGS setting = SETTINGS.values()[position];
                return setting.getView(mActivity, view);
            }
        };

        settingsList.setAdapter(adapter);
        settingsList.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                SETTINGS setting = SETTINGS.values()[position];
                LinearLayout layout = (LinearLayout) view.findViewById(R.id.a2preference_widget_frame);
                View childView = layout.getChildAt(0);
                boolean selected = false;
                if (childView != null && childView instanceof CheckBox) {
                    CheckBox checkBox = (CheckBox) childView;
                    checkBox.setChecked(!checkBox.isChecked());
                    selected = checkBox.isChecked();
                }
                setting.handleSelection(Apple2SettingsMenu.this, selected);
            }
        });
    }

    public void showJoystickConfiguration() {
        Log.d(TAG, "showJoystickConfiguration...");
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

    public boolean isShowing() {
        return mSettingsView.isShown();
    }

    public View getView() {
        return mSettingsView;
    }
}
