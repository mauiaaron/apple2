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

import android.view.View;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.TextView;

import org.deadc0de.apple2ix.basic.R;
import org.json.JSONArray;

public class Apple2AudioSettingsMenu extends Apple2AbstractMenu {

    private final static String TAG = "Apple2AudioSettingsMenu";

    private final static int AUDIO_LATENCY_NUM_CHOICES = Apple2Preferences.DECENT_AMOUNT_OF_CHOICES;

    private static int sSampleRateCanary = 0;

    public Apple2AudioSettingsMenu(Apple2Activity activity) {
        super(activity);
        sSampleRateCanary = DevicePropertyCalculator.getRecommendedSampleRate(activity);
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

    public enum Volume {
        OFF(0),
        ONE(1),
        TWO(2),
        THREE(3),
        FOUR(4),
        MEDIUM(5),
        FIVE(5),
        SIX(6),
        SEVEN(7),
        EIGHT(8),
        NINE(9),
        MAX(10),
        ELEVEN(11); // Ours goes to eleven...
        private int vol;

        Volume(int vol) {
            this.vol = vol;
        }
    }

    enum SETTINGS implements Apple2AbstractMenu.IMenuEnum {
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
            public String getPrefKey() {
                return "speakerVolume";
            }

            @Override
            public Object getPrefDefault() {
                return Volume.MEDIUM.ordinal();
            }

            @Override
            public View getView(final Apple2Activity activity, View convertView) {
                final IMenuEnum self = this;
                return _sliderView(activity, this, Volume.MAX.ordinal() - 1, new IPreferenceSlider() {
                    @Override
                    public void saveInt(int progress) {
                        Apple2Preferences.setJSONPref(self, Volume.values()[progress].ordinal());
                    }

                    @Override
                    public int intValue() {
                        return (int) Apple2Preferences.getJSONPref(self);
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
            public String getPrefKey() {
                return "mbEnabled";
            }

            @Override
            public Object getPrefDefault() {
                return true;
            }

            @Override
            public View getView(final Apple2Activity activity, View convertView) {
                final IMenuEnum self = this;
                convertView = _basicView(activity, this, convertView);
                CheckBox cb = _addCheckbox(activity, this, convertView, (boolean) Apple2Preferences.getJSONPref(this));
                cb.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
                    @Override
                    public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                        Apple2Preferences.setJSONPref(self, isChecked);
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
            public String getPrefKey() {
                return "mbVolume";
            }

            @Override
            public Object getPrefDefault() {
                return Volume.MEDIUM.ordinal();
            }

            @Override
            public View getView(final Apple2Activity activity, View convertView) {
                final IMenuEnum self = this;
                return _sliderView(activity, this, Volume.MAX.ordinal() - 1, new IPreferenceSlider() {
                    @Override
                    public void saveInt(int progress) {
                        Apple2Preferences.setJSONPref(self, Volume.values()[progress].ordinal());
                    }

                    @Override
                    public int intValue() {
                        return (int) Apple2Preferences.getJSONPref(self);
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
            public String getPrefKey() {
                return "audioLatency";
            }

            @Override
            public Object getPrefDefault() {
                int defaultLatency;
                if (Apple2AudioSettingsMenu.sSampleRateCanary == DevicePropertyCalculator.defaultSampleRate) {
                    // quite possibly an audio-challenged device
                    defaultLatency = 8; // /AUDIO_LATENCY_NUM_CHOICES ->  0.4f
                } else {
                    // reasonable default for high-end devices
                    defaultLatency = 5; // /AUDIO_LATENCY_NUM_CHOICES -> 0.25f
                }
                return ((float) defaultLatency) / AUDIO_LATENCY_NUM_CHOICES;
            }

            @Override
            public View getView(final Apple2Activity activity, View convertView) {
                final IMenuEnum self = this;
                return _sliderView(activity, this, AUDIO_LATENCY_NUM_CHOICES, new IPreferenceSlider() {
                    @Override
                    public void saveInt(int progress) {
                        if (progress == 0) {
                            // disallow 0-length buffer ...
                            progress = 1;
                        }
                        Apple2Preferences.setJSONPref(self, ((float) progress) / AUDIO_LATENCY_NUM_CHOICES);
                    }

                    @Override
                    public int intValue() {
                        float pref = Apple2Preferences.getFloatJSONPref(self);
                        return (int) (pref * AUDIO_LATENCY_NUM_CHOICES);
                    }

                    @Override
                    public void showValue(int progress, final TextView seekBarValue) {
                        seekBarValue.setText("" + ((float) progress / AUDIO_LATENCY_NUM_CHOICES));
                    }
                });
            }
        };

        public static final int size = SETTINGS.values().length;

        @Override
        public String getPrefDomain() {
            return Apple2Preferences.PREF_DOMAIN_AUDIO;
        }

        @Override
        public String getPrefKey() {
            return null;
        }

        @Override
        public Object getPrefDefault() {
            return null;
        }

        @Override
        public void handleSelection(Apple2Activity activity, Apple2AbstractMenu settingsMenu, boolean isChecked) {
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
