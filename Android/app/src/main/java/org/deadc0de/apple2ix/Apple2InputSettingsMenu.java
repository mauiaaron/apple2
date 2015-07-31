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
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.CheckBox;
import android.widget.CheckedTextView;
import android.widget.CompoundButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.SeekBar;
import android.widget.TextView;

public class Apple2InputSettingsMenu implements Apple2MenuView {

    private final static String TAG = "Apple2InputSettingsMenu";

    private Apple2Activity mActivity = null;
    private View mSettingsView = null;

    public Apple2InputSettingsMenu(Apple2Activity activity) {
        mActivity = activity;
        setup();
    }

    enum SETTINGS {
        KEYBOARD_CONFIGURE {
            @Override
            public String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keyboard_configure);
            }

            @Override
            public String getSummary(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keyboard_configure_summary);
            }

            @Override
            public void handleSelection(final Apple2Activity activity, final Apple2InputSettingsMenu settingsMenu, boolean isChecked) {
                //new Apple2KeyboardSettingsMenu().show();
            }
        },
        JOYSTICK_CONFIGURE {
            @Override
            public String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.joystick_configure);
            }

            @Override
            public String getSummary(Apple2Activity activity) {
                return activity.getResources().getString(R.string.joystick_configure_summary);
            }

            @Override
            public void handleSelection(final Apple2Activity activity, final Apple2InputSettingsMenu settingsMenu, boolean isChecked) {
                //new Apple2JoystickSettingsMenu().show();
            }
        },
        TOUCH_MENU_ENABLED {
            @Override
            public String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.touch_menu_enable);
            }

            @Override
            public String getSummary(Apple2Activity activity) {
                return activity.getResources().getString(R.string.touch_menu_enable_summary);
            }

            @Override
            public View getView(final Apple2Activity activity, View convertView) {
                convertView = _basicView(activity, this, convertView);
                CheckBox cb = _addCheckbox(activity, this, convertView, Apple2Preferences.TOUCH_MENU_ENABLED.booleanValue(activity));
                cb.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
                    @Override
                    public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                        Apple2Preferences.TOUCH_MENU_ENABLED.saveBoolean(activity, isChecked);
                    }
                });
                return convertView;
            }
        },
        TOUCH_MENU_VISIBILITY {
            @Override
            public String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.touch_menu_visibility);
            }

            @Override
            public String getSummary(Apple2Activity activity) {
                return activity.getResources().getString(R.string.touch_menu_visibility_summary);
            }

            @Override
            public View getView(final Apple2Activity activity, View convertView) {
                LayoutInflater inflater = (LayoutInflater) activity.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
                convertView = inflater.inflate(R.layout.a2preference_slider, null, false);

                TextView tv = (TextView) convertView.findViewById(R.id.a2preference_slider_summary);
                tv.setText(getSummary(activity));

                final TextView seekBarValue = (TextView) convertView.findViewById(R.id.a2preference_slider_seekBarValue);

                SeekBar sb = (SeekBar) convertView.findViewById(R.id.a2preference_slider_seekBar);
                sb.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
                    @Override
                    public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                        if (!fromUser) {
                            return;
                        }
                        float alpha = (float) progress / Apple2Preferences.ALPHA_SLIDER_NUM_CHOICES;
                        seekBarValue.setText("" + alpha);
                        Apple2Preferences.TOUCH_MENU_VISIBILITY.saveInt(activity, progress);
                    }

                    @Override
                    public void onStartTrackingTouch(SeekBar seekBar) {
                    }

                    @Override
                    public void onStopTrackingTouch(SeekBar seekBar) {
                    }
                });

                sb.setMax(0); // http://stackoverflow.com/questions/10278467/seekbar-not-setting-actual-progress-setprogress-not-working-on-early-android
                sb.setMax(Apple2Preferences.ALPHA_SLIDER_NUM_CHOICES);
                int progress = Apple2Preferences.TOUCH_MENU_VISIBILITY.intValue(activity);
                sb.setProgress(progress);
                seekBarValue.setText("" + ((float) progress / Apple2Preferences.ALPHA_SLIDER_NUM_CHOICES));
                return convertView;
            }
        },
        CURRENT_INPUT {
            @Override
            public String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.input_current);
            }

            @Override
            public String getSummary(Apple2Activity activity) {
                return activity.getResources().getString(R.string.input_current_summary);
            }

            @Override
            public View getView(final Apple2Activity activity, View convertView) {
                convertView = _basicView(activity, this, convertView);
                _addPopupIcon(activity, this, convertView);
                return convertView;
            }

            @Override
            public void handleSelection(final Apple2Activity activity, final Apple2InputSettingsMenu settingsMenu, boolean isChecked) {
                AlertDialog.Builder builder = new AlertDialog.Builder(activity).setIcon(R.drawable.ic_launcher).setCancelable(true).setTitle(R.string.input_current);
                builder.setNegativeButton(R.string.cancel, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        dialog.dismiss();
                    }
                });

                String[] touch_choices = new String[]{
                        activity.getResources().getString(R.string.joystick),
                        activity.getResources().getString(R.string.keyboard),
                };
                final int checkedPosition = Apple2Preferences.CURRENT_TOUCH_DEVICE.intValue(activity) - 1;
                final ArrayAdapter<String> adapter = new ArrayAdapter<String>(activity, android.R.layout.select_dialog_singlechoice, touch_choices) {
                    @Override
                    public View getView(int position, View convertView, ViewGroup parent) {
                        View view = super.getView(position, convertView, parent);
                        CheckedTextView ctv = (CheckedTextView) view.findViewById(android.R.id.text1);
                        ctv.setChecked(position == checkedPosition);
                        return view;
                    }
                };

                builder.setAdapter(adapter, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int color) {
                        Apple2Preferences.CURRENT_TOUCH_DEVICE.saveTouchDevice(activity, Apple2Preferences.TouchDevice.values()[color + 1]);
                        dialog.dismiss();
                    }
                });
                builder.show();
            }
        };

        private static View _basicView(Apple2Activity activity, SETTINGS setting, View convertView) {
            TextView tv = (TextView) convertView.findViewById(R.id.a2preference_title);
            tv.setText(setting.getTitle(activity));

            tv = (TextView) convertView.findViewById(R.id.a2preference_summary);
            tv.setText(setting.getSummary(activity));

            LinearLayout layout = (LinearLayout) convertView.findViewById(R.id.a2preference_widget_frame);
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
            LinearLayout layout = (LinearLayout) convertView.findViewById(R.id.a2preference_widget_frame);
            layout.addView(imageView);
            return imageView;
        }

        private static CheckBox _addCheckbox(Apple2Activity activity, SETTINGS setting, View convertView, boolean isChecked) {
            CheckBox checkBox = new CheckBox(activity);
            checkBox.setChecked(isChecked);
            LinearLayout layout = (LinearLayout) convertView.findViewById(R.id.a2preference_widget_frame);
            layout.addView(checkBox);
            return checkBox;
        }

        public static final int size = SETTINGS.values().length;

        public abstract String getTitle(Apple2Activity activity);

        public abstract String getSummary(Apple2Activity activity);

        public void handleSelection(Apple2Activity activity, Apple2InputSettingsMenu settingsMenu, boolean isChecked) {
            /* ... */
        }

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
        LayoutInflater inflater = (LayoutInflater) mActivity.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        mSettingsView = inflater.inflate(R.layout.activity_settings, null, false);

        TextView title = (TextView) mSettingsView.findViewById(R.id.header_settings);
        title.setText(R.string.settings_audio);

        ListView settingsList = (ListView) mSettingsView.findViewById(R.id.listView_settings);
        settingsList.setEnabled(true);

        // This is a bit of a hack ... we're not using the ArrayAdapter as intended here, simply piggy-backing off its call to getView() to supply a completely custom view.  The arguably better way would be to create a custom Apple2SettingsAdapter or something akin to that...
        ArrayAdapter<?> adapter = new ArrayAdapter<String>(mActivity, R.layout.a2preference, R.id.a2preference_title, SETTINGS.titles(mActivity)) {
            @Override
            public boolean areAllItemsEnabled() {
                return true;
            }

            @Override
            public boolean isEnabled(int position) {
                if (position < 0 || position >= SETTINGS.size) {
                    throw new ArrayIndexOutOfBoundsException();
                }
                return true;
            }

            @Override
            public View getView(int position, View convertView, ViewGroup parent) {
                //View view = super.getView(position, convertView, parent);
                // ^^^ WHOA ... this is catching an NPE deep in AOSP code on the second time loading the audio menu, WTF?
                // Methinks it is related to the hack of loading a completely different R.layout.something for certain views...
                View view = convertView != null ? convertView : super.getView(position, null, parent);
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
                if (layout == null) {
                    return;
                }

                View childView = layout.getChildAt(0);
                boolean selected = false;
                if (childView != null && childView instanceof CheckBox) {
                    CheckBox checkBox = (CheckBox) childView;
                    checkBox.setChecked(!checkBox.isChecked());
                    selected = checkBox.isChecked();
                }
                setting.handleSelection(mActivity, Apple2InputSettingsMenu.this, selected);
            }
        });
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
