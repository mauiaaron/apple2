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

import android.view.View;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.TextView;

public class Apple2AudioSettingsMenu extends Apple2AbstractMenu {

    private final static String TAG = "Apple2AudioSettingsMenu";

    public Apple2AudioSettingsMenu(Apple2Activity activity) {
        super(activity);
    }

    @Override
    public final String[] allTitles() {
        return SETTINGS.titles(mActivity);
    }

    @Override
    public final IMenuEnum[] allValues() {
        return SETTINGS.values();
    }

    @Override
    public final boolean areAllItemsEnabled() {
        return false;
    }

    @Override
    public final boolean isEnabled(int position) {
        if (position < 0 || position >= SETTINGS.size) {
            throw new ArrayIndexOutOfBoundsException();
        }
        return position == SETTINGS.MOCKINGBOARD_ENABLED.ordinal();
    }

    enum SETTINGS implements Apple2AbstractMenu.IMenuEnum {
        SPEAKER_ENABLED {
            @Override
            public final String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.speaker_enable);
            }

            @Override
            public final String getSummary(Apple2Activity activity) {
                return activity.getResources().getString(R.string.speaker_enable_summary);
            }

            @Override
            public View getView(final Apple2Activity activity, View convertView) {
                convertView = _basicView(activity, this, convertView);
                CheckBox cb = _addCheckbox(activity, this, convertView, true);
                cb.setEnabled(false);
                return convertView;
            }
        },
        SPEAKER_VOLUME {
            @Override
            public final String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.speaker_volume);
            }

            @Override
            public final String getSummary(Apple2Activity activity) {
                return activity.getResources().getString(R.string.speaker_volume_summary);
            }

            @Override
            public View getView(final Apple2Activity activity, View convertView) {
                return _sliderView(activity, this, Apple2Preferences.Volume.MAX.ordinal() - 1, new IPreferenceSlider() {
                    @Override
                    public void saveInt(int progress) {
                        Apple2Preferences.SPEAKER_VOLUME.saveVolume(activity, Apple2Preferences.Volume.values()[progress]);
                    }

                    @Override
                    public int intValue() {
                        return Apple2Preferences.SPEAKER_VOLUME.intValue(activity);
                    }

                    @Override
                    public void showValue(int progress, final TextView seekBarValue) {
                        seekBarValue.setText("" + progress);
                    }
                });
            }
        },
        MOCKINGBOARD_ENABLED {
            @Override
            public final String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.mockingboard_enable);
            }

            @Override
            public final String getSummary(Apple2Activity activity) {
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
            @Override
            public final String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.mockingboard_volume);
            }

            @Override
            public final String getSummary(Apple2Activity activity) {
                return activity.getResources().getString(R.string.mockingboard_volume_summary);
            }

            @Override
            public View getView(final Apple2Activity activity, View convertView) {
                return _sliderView(activity, this, Apple2Preferences.Volume.MAX.ordinal() - 1, new IPreferenceSlider() {
                    @Override
                    public void saveInt(int progress) {
                        Apple2Preferences.MOCKINGBOARD_VOLUME.saveVolume(activity, Apple2Preferences.Volume.values()[progress]);
                    }

                    @Override
                    public int intValue() {
                        return Apple2Preferences.MOCKINGBOARD_VOLUME.intValue(activity);
                    }

                    @Override
                    public void showValue(int progress, final TextView seekBarValue) {
                        seekBarValue.setText("" + progress);
                    }
                });
            }
        },
        ADVANCED_SEPARATOR {
            @Override
            public final String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.settings_advanced);
            }

            @Override
            public final String getSummary(Apple2Activity activity) {
                return activity.getResources().getString(R.string.settings_advanced_summary);
            }
        },
        AUDIO_LATENCY {
            @Override
            public final String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.audio_latency);
            }

            @Override
            public final String getSummary(Apple2Activity activity) {
                return activity.getResources().getString(R.string.audio_latency_summary);
            }

            @Override
            public View getView(final Apple2Activity activity, View convertView) {
                return _sliderView(activity, this, Apple2Preferences.AUDIO_LATENCY_NUM_CHOICES, new IPreferenceSlider() {
                    @Override
                    public void saveInt(int progress) {
                        if (progress == 0) {
                            // disallow 0-length buffer ...
                            progress = 1;
                        }
                        Apple2Preferences.AUDIO_LATENCY.saveInt(activity, progress);
                    }

                    @Override
                    public int intValue() {
                        return Apple2Preferences.AUDIO_LATENCY.intValue(activity);
                    }

                    @Override
                    public void showValue(int progress, final TextView seekBarValue) {
                        seekBarValue.setText("" + ((float) progress / Apple2Preferences.AUDIO_LATENCY_NUM_CHOICES));
                    }
                });
            }
        };

        public static final int size = SETTINGS.values().length;

        @Override
        public void handleSelection(Apple2Activity activity, Apple2AbstractMenu settingsMenu, boolean isChecked) {
            /* ... */
        }

        @Override
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
}
