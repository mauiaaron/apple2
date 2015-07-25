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
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.SeekBar;
import android.widget.TextView;

public class Apple2AudioSettingsMenu implements Apple2MenuView {

    private final static String TAG = "Apple2AudioSettingsMenu";

    private Apple2Activity mActivity = null;
    private View mSettingsView = null;

    public Apple2AudioSettingsMenu(Apple2Activity activity) {
        mActivity = activity;
        setup();
    }

    enum SETTINGS {
        SPEAKER_ENABLED {
            @Override public String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.speaker_enable);
            }
            @Override public String getSummary(Apple2Activity activity) {
                return activity.getResources().getString(R.string.speaker_enable_summary);
            }
            @Override public View getView(final Apple2Activity activity, View convertView) {
                convertView = _basicView(activity, this, convertView);
                CheckBox cb = _addCheckbox(activity, this, convertView, true);
                cb.setEnabled(false);
                return convertView;
            }
        },
        SPEAKER_VOLUME {
            @Override public String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.speaker_volume);
            }
            @Override public String getSummary(Apple2Activity activity) {
                return activity.getResources().getString(R.string.speaker_volume_summary);
            }
            @Override public View getView(final Apple2Activity activity, View convertView) {
                LayoutInflater inflater = (LayoutInflater)activity.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
                convertView = inflater.inflate(R.layout.a2preference_slider, null, false);

                TextView tv = (TextView)convertView.findViewById(R.id.a2preference_slider_summary);
                tv.setText(getSummary(activity));

                final TextView seekBarValue = (TextView)convertView.findViewById(R.id.a2preference_slider_seekBarValue);

                SeekBar sb = (SeekBar)convertView.findViewById(R.id.a2preference_slider_seekBar);
                sb.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
                    @Override
                    public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                        if (!fromUser) {
                            return;
                        }
                        seekBarValue.setText("" + progress);
                        Apple2Preferences.SPEAKER_VOLUME.saveVolume(activity, Apple2Preferences.Volume.values()[progress]);
                    }

                    @Override
                    public void onStartTrackingTouch(SeekBar seekBar) {
                    }

                    @Override
                    public void onStopTrackingTouch(SeekBar seekBar) {
                    }
                });

                sb.setMax(0); // http://stackoverflow.com/questions/10278467/seekbar-not-setting-actual-progress-setprogress-not-working-on-early-android
                sb.setMax(Apple2Preferences.Volume.MAX.ordinal() - 1);
                int vol = Apple2Preferences.SPEAKER_VOLUME.intValue(activity);
                sb.setProgress(vol);
                seekBarValue.setText("" + vol);
                return convertView;
            }
        },
        MOCKINGBOARD_ENABLED {
            @Override
            public String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.mockingboard_enable);
            }

            @Override
            public String getSummary(Apple2Activity activity) {
                return activity.getResources().getString(R.string.mockingboard_enable_summary);
            }

            @Override
            public View getView(final Apple2Activity activity, View convertView) {
                convertView = _basicView(activity, this, convertView);
                CheckBox cb = _addCheckbox(activity, this, convertView, Apple2Preferences.MOCKINGBOARD_ENABLED.booleanValue(activity));
                cb.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
                    @Override
                    public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                        Apple2Preferences.MOCKINGBOARD_ENABLED.saveBoolean(activity, isChecked);
                    }
                });
                return convertView;
            }
        },
        MOCKINGBOARD_VOLUME {
            @Override public String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.mockingboard_volume);
            }
            @Override public String getSummary(Apple2Activity activity) {
                return activity.getResources().getString(R.string.mockingboard_volume_summary);
            }
            @Override public View getView(final Apple2Activity activity, View convertView) {
                LayoutInflater inflater = (LayoutInflater)activity.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
                convertView = inflater.inflate(R.layout.a2preference_slider, null, false);

                TextView tv = (TextView)convertView.findViewById(R.id.a2preference_slider_summary);
                tv.setText(getSummary(activity));

                final TextView seekBarValue = (TextView)convertView.findViewById(R.id.a2preference_slider_seekBarValue);

                SeekBar sb = (SeekBar)convertView.findViewById(R.id.a2preference_slider_seekBar);
                sb.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
                    @Override
                    public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                        if (!fromUser) {
                            return;
                        }
                        seekBarValue.setText("" + progress);
                        Apple2Preferences.MOCKINGBOARD_VOLUME.saveVolume(activity, Apple2Preferences.Volume.values()[progress]);
                    }

                    @Override
                    public void onStartTrackingTouch(SeekBar seekBar) {
                    }

                    @Override
                    public void onStopTrackingTouch(SeekBar seekBar) {
                    }
                });

                sb.setMax(0); // http://stackoverflow.com/questions/10278467/seekbar-not-setting-actual-progress-setprogress-not-working-on-early-android
                sb.setMax(Apple2Preferences.Volume.MAX.ordinal() - 1);
                int vol = Apple2Preferences.MOCKINGBOARD_VOLUME.intValue(activity);
                sb.setProgress(vol);
                seekBarValue.setText("" + vol);
                return convertView;
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

        public void handleSelection(Apple2AudioSettingsMenu settingsMenu, boolean isChecked) {
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
        LayoutInflater inflater = (LayoutInflater)mActivity.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        mSettingsView = inflater.inflate(R.layout.activity_settings, null, false);

        TextView title = (TextView)mSettingsView.findViewById(R.id.header_settings);
        title.setText(R.string.settings_audio);

        ListView settingsList = (ListView)mSettingsView.findViewById(R.id.listView_settings);
        settingsList.setEnabled(true);

        // This is a bit of a hack ... we're not using the ArrayAdapter as intended here, simply piggy-backing off its call to getView() to supply a completely custom view.  The arguably better way would be to create a custom Apple2SettingsAdapter or something akin to that...
        ArrayAdapter<?> adapter = new ArrayAdapter<String>(mActivity, R.layout.a2preference, R.id.a2preference_title, SETTINGS.titles(mActivity)) {
            @Override
            public boolean areAllItemsEnabled() {
                return false;
            }

            @Override
            public boolean isEnabled(int position) {
                if (position < 0 || position >= SETTINGS.size) {
                    throw new ArrayIndexOutOfBoundsException();
                }
                return position == SETTINGS.MOCKINGBOARD_ENABLED.ordinal();
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
                setting.handleSelection(Apple2AudioSettingsMenu.this, selected);
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
