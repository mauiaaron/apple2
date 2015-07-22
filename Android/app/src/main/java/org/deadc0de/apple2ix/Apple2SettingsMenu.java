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
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.TextView;

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

    enum SETTINGS {
        JOYSTICK_CONFIGURE {
            @Override public String getTitle(Context ctx) {
                return ctx.getResources().getString(R.string.configure_joystick);
            }
            @Override public String getSummary(Context ctx) {
                return ctx.getResources().getString(R.string.configure_joystick_summary);
            }
            @Override public boolean wantsCheckbox() {
                return false;
            }
            @Override public void handleSelection(Apple2SettingsMenu settingsMenu, boolean selected) {
            }
        },
        AUDIO_CONFIGURE {
            @Override public String getTitle(Context ctx) {
                return ctx.getResources().getString(R.string.audio_enabled);
            }
            @Override public String getSummary(Context ctx) {
                return ctx.getResources().getString(R.string.audio_enabled_summary);
            }
            @Override public void handleSelection(Apple2SettingsMenu settingsMenu, boolean selected) {
            }
        },
        VIDEO_CONFIGURE {
            @Override public String getTitle(Context ctx) {
                return ctx.getResources().getString(R.string.configure_video);
            }
            @Override public String getSummary(Context ctx) {
                return ctx.getResources().getString(R.string.configure_video_summary);
            }
            @Override public boolean wantsCheckbox() {
                return false;
            }
            @Override public boolean wantsPopup() {
                return true;
            }
            @Override
            public void handleSelection(final Apple2SettingsMenu settingsMenu, boolean selected) {
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

        public abstract String getTitle(Context ctx);
        public abstract String getSummary(Context ctx);
        public abstract void handleSelection(Apple2SettingsMenu settingsMenu, boolean selected);
        public boolean wantsCheckbox() {
            return true;
        }
        public boolean wantsPopup() {
            return false;
        }

        public static String[] titles(Context ctx) {
            String[] titles = new String[values().length];
            int i = 0;
            for (SETTINGS setting : values()) {
                titles[i++] = setting.getTitle(ctx);
            }
            return titles;
        }
    }

    private void setup() {

        LayoutInflater inflater = (LayoutInflater)mActivity.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        mSettingsView = inflater.inflate(R.layout.activity_settings, null, false);

        ListView settingsList = (ListView)mSettingsView.findViewById(R.id.listView_settings);
        settingsList.setEnabled(true);

        final String[] titles = SETTINGS.titles(mActivity);

        ArrayAdapter<?> adapter = new ArrayAdapter<String>(mActivity, R.layout.a2preference, R.id.a2preference_title, titles) {
            @Override
            public boolean areAllItemsEnabled() {
                return true;
            }
            /*@Override
            public boolean isEnabled(int position) {
                if (position < 0 || position > 3) {
                    throw new ArrayIndexOutOfBoundsException();
                }
                return position != 2;
            }*/
            @Override
            public View getView(int position, View convertView, ViewGroup parent) {
                View view = super.getView(position, convertView, parent);
                TextView tv = (TextView)view.findViewById(R.id.a2preference_summary);

                SETTINGS setting = SETTINGS.values()[position];
                tv.setText(setting.getSummary(mActivity));

                LinearLayout layout = (LinearLayout)view.findViewById(R.id.a2preference_widget_frame);
                if (layout.getChildCount() > 0) {
                    // layout cells appear to be reused when scrolling into view ... make sure we start with clear hierarchy
                    layout.removeAllViews();
                }

                if (setting.wantsPopup()) {
                    ImageView imageView = new ImageView(mActivity);
                    Drawable drawable = mActivity.getResources().getDrawable(android.R.drawable.ic_menu_edit);
                    imageView.setImageDrawable(drawable);
                    layout.addView(imageView);
                }
                if (setting.wantsCheckbox()) {
                    CheckBox checkBox = new CheckBox(mActivity);
                    layout.addView(checkBox);
                }
                return view;
            }
        };

        settingsList.setAdapter(adapter);
        settingsList.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                SETTINGS setting = SETTINGS.values()[position];

                LinearLayout layout = (LinearLayout)view.findViewById(R.id.a2preference_widget_frame);
                View childView = layout.getChildAt(0);
                boolean selected = false;
                if (childView != null && childView instanceof CheckBox) {
                    CheckBox checkBox = (CheckBox)childView;
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
        mActivity.nativeOnPause();
        mActivity.addContentView(mSettingsView, new FrameLayout.LayoutParams(mActivity.getWidth(), mActivity.getHeight()));
    }

    public void dismiss() {
        if (isShowing()) {
            dismissWithoutResume();
            mActivity.nativeOnResume(/*isSystemResume:*/false);
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
