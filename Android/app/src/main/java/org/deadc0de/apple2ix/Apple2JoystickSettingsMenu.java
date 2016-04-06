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

import android.util.DisplayMetrics;
import android.view.View;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.TextView;

import java.util.ArrayList;

import org.deadc0de.apple2ix.basic.R;

public class Apple2JoystickSettingsMenu extends Apple2AbstractMenu {

    private final static String TAG = "Apple2JoystickSettingsMenu";

    public final static int JOYSTICK_BUTTON_THRESHOLD_NUM_CHOICES = Apple2Preferences.DECENT_AMOUNT_OF_CHOICES;

    public final static float JOYSTICK_AXIS_SENSITIVITY_MIN = 0.25f;
    public final static float JOYSTICK_AXIS_SENSITIVITY_DEFAULT = 1.f;
    public final static float JOYSTICK_AXIS_SENSITIVITY_MAX = 4.f;
    public final static float JOYSTICK_AXIS_SENSITIVITY_DEC_STEP = 0.05f;
    public final static float JOYSTICK_AXIS_SENSITIVITY_INC_STEP = 0.25f;
    public final static int JOYSTICK_AXIS_SENSITIVITY_DEC_NUMCHOICES = (int) ((JOYSTICK_AXIS_SENSITIVITY_DEFAULT - JOYSTICK_AXIS_SENSITIVITY_MIN) / JOYSTICK_AXIS_SENSITIVITY_DEC_STEP); // 15
    public final static int JOYSTICK_AXIS_SENSITIVITY_INC_NUMCHOICES = (int) ((JOYSTICK_AXIS_SENSITIVITY_MAX - JOYSTICK_AXIS_SENSITIVITY_DEFAULT) / JOYSTICK_AXIS_SENSITIVITY_INC_STEP); // 12
    public final static int JOYSTICK_AXIS_SENSITIVITY_NUM_CHOICES = JOYSTICK_AXIS_SENSITIVITY_DEC_NUMCHOICES + JOYSTICK_AXIS_SENSITIVITY_INC_NUMCHOICES; // 15 + 12

    public final static int TAPDELAY_NUM_CHOICES = Apple2Preferences.DECENT_AMOUNT_OF_CHOICES;
    public final static float TAPDELAY_SCALE = 0.5f;


    public Apple2JoystickSettingsMenu(Apple2Activity activity) {
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
        return true;
    }

    @Override
    public final boolean isEnabled(int position) {
        if (position < 0 || position >= SETTINGS.size) {
            throw new ArrayIndexOutOfBoundsException();
        }
        return true;
    }

    public enum TouchJoystickButtons {
        NONE(0),
        BUTTON1(1),
        BUTTON2(2),
        BOTH(3);
        private int butt;

        TouchJoystickButtons(int butt) {
            this.butt = butt;
        }
    }

    protected enum SETTINGS implements Apple2AbstractMenu.IMenuEnum {
        JOYSTICK_TAP_BUTTON {
            @Override
            public final String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.joystick_button_tap_button);
            }

            @Override
            public final String getSummary(Apple2Activity activity) {
                return activity.getResources().getString(R.string.joystick_button_tap_button_summary);
            }

            @Override
            public String getPrefKey() {
                return "jsTouchDownChar";
            }

            @Override
            public Object getPrefDefault() {
                return TouchJoystickButtons.BUTTON1.ordinal();
            }

            @Override
            public View getView(final Apple2Activity activity, View convertView) {
                convertView = _basicView(activity, this, convertView);
                _addPopupIcon(activity, this, convertView);
                return convertView;
            }

            @Override
            public void handleSelection(final Apple2Activity activity, final Apple2AbstractMenu settingsMenu, boolean isChecked) {
                final IMenuEnum self = this;
                _alertDialogHandleSelection(activity, R.string.joystick_button_tap_button, new String[]{
                        activity.getResources().getString(R.string.joystick_button_button_none),
                        activity.getResources().getString(R.string.joystick_button_button1),
                        activity.getResources().getString(R.string.joystick_button_button2),
                        activity.getResources().getString(R.string.joystick_button_button_both),
                }, new IPreferenceLoadSave() {
                    @Override
                    public int intValue() {
                        return (int) Apple2Preferences.getJSONPref(self);
                    }

                    @Override
                    public void saveInt(int value) {
                        Apple2Preferences.setJSONPref(self, TouchJoystickButtons.values()[value].ordinal());
                    }
                });
            }
        },
        JOYSTICK_SWIPEUP_BUTTON {
            @Override
            public final String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.joystick_button_swipe_up_button);
            }

            @Override
            public final String getSummary(Apple2Activity activity) {
                return activity.getResources().getString(R.string.joystick_button_swipe_up_button_summary);
            }

            @Override
            public String getPrefKey() {
                return "jsSwipeNorthChar";
            }

            @Override
            public Object getPrefDefault() {
                return TouchJoystickButtons.BOTH.ordinal();
            }

            @Override
            public View getView(final Apple2Activity activity, View convertView) {
                convertView = _basicView(activity, this, convertView);
                _addPopupIcon(activity, this, convertView);
                return convertView;
            }

            @Override
            public void handleSelection(final Apple2Activity activity, final Apple2AbstractMenu settingsMenu, boolean isChecked) {
                final IMenuEnum self = this;
                _alertDialogHandleSelection(activity, R.string.joystick_button_swipe_up_button, new String[]{
                        activity.getResources().getString(R.string.joystick_button_button_none),
                        activity.getResources().getString(R.string.joystick_button_button1),
                        activity.getResources().getString(R.string.joystick_button_button2),
                        activity.getResources().getString(R.string.joystick_button_button_both),
                }, new IPreferenceLoadSave() {
                    @Override
                    public int intValue() {
                        return (int) Apple2Preferences.getJSONPref(self);
                    }

                    @Override
                    public void saveInt(int value) {
                        Apple2Preferences.setJSONPref(self, TouchJoystickButtons.values()[value].ordinal());
                    }
                });
            }
        },
        JOYSTICK_SWIPEDOWN_BUTTON {
            @Override
            public final String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.joystick_button_swipe_down_button);
            }

            @Override
            public final String getSummary(Apple2Activity activity) {
                return activity.getResources().getString(R.string.joystick_button_swipe_down_button_summary);
            }

            @Override
            public String getPrefKey() {
                return "jsSwipeSouthChar";
            }

            @Override
            public Object getPrefDefault() {
                return TouchJoystickButtons.BUTTON2.ordinal();
            }

            @Override
            public View getView(final Apple2Activity activity, View convertView) {
                convertView = _basicView(activity, this, convertView);
                _addPopupIcon(activity, this, convertView);
                return convertView;
            }

            @Override
            public void handleSelection(final Apple2Activity activity, final Apple2AbstractMenu settingsMenu, boolean isChecked) {
                final IMenuEnum self = this;
                _alertDialogHandleSelection(activity, R.string.joystick_button_swipe_down_button, new String[]{
                        activity.getResources().getString(R.string.joystick_button_button_none),
                        activity.getResources().getString(R.string.joystick_button_button1),
                        activity.getResources().getString(R.string.joystick_button_button2),
                        activity.getResources().getString(R.string.joystick_button_button_both),
                }, new IPreferenceLoadSave() {
                    @Override
                    public int intValue() {
                        return (int) Apple2Preferences.getJSONPref(self);
                    }

                    @Override
                    public void saveInt(int value) {
                        Apple2Preferences.setJSONPref(self, TouchJoystickButtons.values()[value].ordinal());
                    }
                });
            }
        },
        JOYSTICK_CALIBRATE {
            @Override
            public final String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.joystick_calibrate);
            }

            @Override
            public final String getSummary(Apple2Activity activity) {
                return activity.getResources().getString(R.string.joystick_calibrate_summary);
            }

            @Override
            public void handleSelection(Apple2Activity activity, Apple2AbstractMenu settingsMenu, boolean isChecked) {
                ArrayList<Apple2MenuView> viewStack = new ArrayList<Apple2MenuView>();
                {
                    int idx = 0;
                    while (true) {
                        Apple2MenuView apple2MenuView = activity.peekApple2View(idx);
                        if (apple2MenuView == null) {
                            break;
                        }
                        viewStack.add(apple2MenuView);
                        ++idx;
                    }
                }

                Apple2JoystickCalibration calibration = new Apple2JoystickCalibration(activity, viewStack, Apple2SettingsMenu.TouchDeviceVariant.JOYSTICK);

                // show this new view...
                calibration.show();

                // ...with nothing else underneath 'cept the emulator OpenGL layer
                for (Apple2MenuView apple2MenuView : viewStack) {
                    activity.popApple2View(apple2MenuView);
                }
            }
        },
        JOYSTICK_TAPDELAY {
            @Override
            public final String getTitle(Apple2Activity activity) {
                return "";
            }

            @Override
            public final String getSummary(Apple2Activity activity) {
                return activity.getResources().getString(R.string.joystick_button_tapdelay_summary);
            }

            @Override
            public String getPrefKey() {
                return "jsTapDelaySecs";
            }

            @Override
            public Object getPrefDefault() {
                return ((float) 8 / TAPDELAY_NUM_CHOICES * TAPDELAY_SCALE); // -> 0.2f
            }

            @Override
            public View getView(final Apple2Activity activity, View convertView) {
                final IMenuEnum self = this;
                return _sliderView(activity, this, TAPDELAY_NUM_CHOICES, new IPreferenceSlider() {
                    @Override
                    public void saveInt(int progress) {
                        Apple2Preferences.setJSONPref(self, ((float) progress / TAPDELAY_NUM_CHOICES * TAPDELAY_SCALE));
                    }

                    @Override
                    public int intValue() {
                        return (int) (Apple2Preferences.getFloatJSONPref(self) / TAPDELAY_SCALE * TAPDELAY_NUM_CHOICES);
                    }

                    @Override
                    public void showValue(int progress, final TextView seekBarValue) {
                        seekBarValue.setText("" + (((float) progress / TAPDELAY_NUM_CHOICES) * TAPDELAY_SCALE));
                    }
                });
            }
        },
        JOYSTICK_ADVANCED {
            @Override
            public final String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.settings_advanced);
            }

            @Override
            public final String getSummary(Apple2Activity activity) {
                return activity.getResources().getString(R.string.settings_advanced_joystick_summary);
            }

            @Override
            public void handleSelection(Apple2Activity activity, Apple2AbstractMenu settingsMenu, boolean isChecked) {
                new Apple2JoystickSettingsMenu.JoystickAdvanced(activity).show();
            }
        };

        public static final int size = SETTINGS.values().length;

        @Override
        public String getPrefDomain() {
            return Apple2Preferences.PREF_DOMAIN_JOYSTICK;
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

    public static class JoystickAdvanced extends Apple2AbstractMenu {

        private final static String TAG = "JoystickAdvanced";

        public JoystickAdvanced(Apple2Activity activity) {
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
            return position == SETTINGS.JOYSTICK_AXIS_ON_LEFT.ordinal();
        }

        protected enum SETTINGS implements Apple2AbstractMenu.IMenuEnum {
            JOYSTICK_VISIBILITY {
                @Override
                public final String getTitle(Apple2Activity activity) {
                    return activity.getResources().getString(R.string.joystick_visible);
                }

                @Override
                public final String getSummary(Apple2Activity activity) {
                    return activity.getResources().getString(R.string.joystick_visible_summary);
                }

                @Override
                public String getPrefKey() {
                    return "showControls";
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
            JOYSTICK_AZIMUTH_VISIBILITY {
                @Override
                public final String getTitle(Apple2Activity activity) {
                    return activity.getResources().getString(R.string.joystick_azimuth_visible);
                }

                @Override
                public final String getSummary(Apple2Activity activity) {
                    return activity.getResources().getString(R.string.joystick_azimuth_visible_summary);
                }

                @Override
                public String getPrefKey() {
                    return "showAzimuth";
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
            JOYSTICK_AXIS_ON_LEFT {
                @Override
                public final String getTitle(Apple2Activity activity) {
                    return activity.getResources().getString(R.string.joystick_axisleft);
                }

                @Override
                public final String getSummary(Apple2Activity activity) {
                    return activity.getResources().getString(R.string.joystick_axisleft_summary);
                }

                @Override
                public String getPrefKey() {
                    return "axisIsOnLeft";
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
            JOYSTICK_AXIS_SENSITIVITY {
                @Override
                public final String getTitle(Apple2Activity activity) {
                    return "";
                }

                @Override
                public final String getSummary(Apple2Activity activity) {
                    return activity.getResources().getString(R.string.joystick_axis_sensitivity_summary);
                }

                @Override
                public String getPrefKey() {
                    return "axisSensitivity";
                }

                @Override
                public Object getPrefDefault() {
                    return 1.f;
                }

                @Override
                public View getView(final Apple2Activity activity, View convertView) {
                    final IMenuEnum self = this;
                    return _sliderView(activity, this, JOYSTICK_AXIS_SENSITIVITY_NUM_CHOICES, new IPreferenceSlider() {
                        @Override
                        public void saveInt(int progress) {
                            final int pivot = JOYSTICK_AXIS_SENSITIVITY_DEC_NUMCHOICES;
                            float sensitivity = 1.f;
                            if (progress < pivot) {
                                int decAmount = (pivot - progress);
                                sensitivity -= (JOYSTICK_AXIS_SENSITIVITY_DEC_STEP * decAmount);
                            } else if (progress > pivot) {
                                int incAmount = (progress - pivot);
                                sensitivity += (JOYSTICK_AXIS_SENSITIVITY_INC_STEP * incAmount);
                            }
                            Apple2Preferences.setJSONPref(self, sensitivity);
                        }

                        @Override
                        public int intValue() {
                            float sensitivity = Apple2Preferences.getFloatJSONPref(self);
                            int pivot = JOYSTICK_AXIS_SENSITIVITY_DEC_NUMCHOICES;
                            if (sensitivity < 1.f) {
                                pivot = Math.round((sensitivity - JOYSTICK_AXIS_SENSITIVITY_MIN) / JOYSTICK_AXIS_SENSITIVITY_DEC_STEP);
                            } else if (sensitivity > 1.f) {
                                sensitivity -= 1.f;
                                pivot += Math.round(sensitivity / JOYSTICK_AXIS_SENSITIVITY_INC_STEP);
                            }
                            return pivot;
                        }

                        @Override
                        public void showValue(int progress, final TextView seekBarValue) {
                            saveInt(progress);
                            int percent = (int) (Apple2Preferences.getFloatJSONPref(self) * 100.f);
                            seekBarValue.setText("" + percent + "%");
                        }
                    });
                }
            },
            JOYSTICK_BUTTON_THRESHOLD {
                @Override
                public final String getTitle(Apple2Activity activity) {
                    return "";
                }

                @Override
                public final String getSummary(Apple2Activity activity) {
                    return activity.getResources().getString(R.string.joystick_button_threshold_summary);
                }

                @Override
                public String getPrefKey() {
                    return "switchThreshold";
                }

                @Override
                public Object getPrefDefault() {
                    return 128;
                }

                @Override
                public View getView(final Apple2Activity activity, View convertView) {
                    final IMenuEnum self = this;
                    return _sliderView(activity, this, JOYSTICK_BUTTON_THRESHOLD_NUM_CHOICES, new IPreferenceSlider() {
                        @Override
                        public void saveInt(int progress) {
                            if (progress == 0) {
                                progress = 1;
                            }
                            progress *= getJoystickButtonSwitchThresholdScale(activity);
                            Apple2Preferences.setJSONPref(self, progress);
                        }

                        @Override
                        public int intValue() {
                            int progress = (int) Apple2Preferences.getJSONPref(self);
                            return (progress / getJoystickButtonSwitchThresholdScale(activity));
                        }

                        @Override
                        public void showValue(int progress, final TextView seekBarValue) {
                            int threshold = progress * getJoystickButtonSwitchThresholdScale(activity);
                            seekBarValue.setText("" + threshold + " pts");
                        }
                    });
                }
            };

            public static final int size = SETTINGS.values().length;

            @Override
            public String getPrefDomain() {
                return Apple2Preferences.PREF_DOMAIN_JOYSTICK;
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

    public static int getJoystickButtonSwitchThresholdScale(Apple2Activity activity) {

        DisplayMetrics dm = new DisplayMetrics();
        activity.getWindowManager().getDefaultDisplay().getMetrics(dm);

        int smallScreenAxis = dm.widthPixels < dm.heightPixels ? dm.widthPixels : dm.heightPixels;
        int oneThirdScreenAxis = smallScreenAxis / 3;

        // largest switch threshold value is 1/3 small dimension of screen
        return oneThirdScreenAxis / JOYSTICK_BUTTON_THRESHOLD_NUM_CHOICES;
    }

}
