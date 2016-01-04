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

import java.util.ArrayList;

import org.deadc0de.apple2ix.basic.R;

public class Apple2JoystickSettingsMenu extends Apple2AbstractMenu {

    private final static String TAG = "Apple2JoystickSettingsMenu";

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
            public View getView(final Apple2Activity activity, View convertView) {
                convertView = _basicView(activity, this, convertView);
                _addPopupIcon(activity, this, convertView);
                return convertView;
            }

            @Override
            public void handleSelection(final Apple2Activity activity, final Apple2AbstractMenu settingsMenu, boolean isChecked) {
                _alertDialogHandleSelection(activity, R.string.joystick_button_tap_button, new String[]{
                        activity.getResources().getString(R.string.joystick_button_button_none),
                        activity.getResources().getString(R.string.joystick_button_button1),
                        activity.getResources().getString(R.string.joystick_button_button2),
                        activity.getResources().getString(R.string.joystick_button_button_both),
                }, new IPreferenceLoadSave() {
                    @Override
                    public int intValue() {
                        return Apple2Preferences.JOYSTICK_TAP_BUTTON.intValue(activity);
                    }

                    @Override
                    public void saveInt(int value) {
                        Apple2Preferences.JOYSTICK_TAP_BUTTON.saveTouchJoystickButtons(activity, Apple2Preferences.TouchJoystickButtons.values()[value]);
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
            public View getView(final Apple2Activity activity, View convertView) {
                convertView = _basicView(activity, this, convertView);
                _addPopupIcon(activity, this, convertView);
                return convertView;
            }

            @Override
            public void handleSelection(final Apple2Activity activity, final Apple2AbstractMenu settingsMenu, boolean isChecked) {
                _alertDialogHandleSelection(activity, R.string.joystick_button_swipe_up_button, new String[]{
                        activity.getResources().getString(R.string.joystick_button_button_none),
                        activity.getResources().getString(R.string.joystick_button_button1),
                        activity.getResources().getString(R.string.joystick_button_button2),
                        activity.getResources().getString(R.string.joystick_button_button_both),
                }, new IPreferenceLoadSave() {
                    @Override
                    public int intValue() {
                        return Apple2Preferences.JOYSTICK_SWIPEUP_BUTTON.intValue(activity);
                    }

                    @Override
                    public void saveInt(int value) {
                        Apple2Preferences.JOYSTICK_SWIPEUP_BUTTON.saveTouchJoystickButtons(activity, Apple2Preferences.TouchJoystickButtons.values()[value]);
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
            public View getView(final Apple2Activity activity, View convertView) {
                convertView = _basicView(activity, this, convertView);
                _addPopupIcon(activity, this, convertView);
                return convertView;
            }

            @Override
            public void handleSelection(final Apple2Activity activity, final Apple2AbstractMenu settingsMenu, boolean isChecked) {
                _alertDialogHandleSelection(activity, R.string.joystick_button_swipe_down_button, new String[]{
                        activity.getResources().getString(R.string.joystick_button_button_none),
                        activity.getResources().getString(R.string.joystick_button_button1),
                        activity.getResources().getString(R.string.joystick_button_button2),
                        activity.getResources().getString(R.string.joystick_button_button_both),
                }, new IPreferenceLoadSave() {
                    @Override
                    public int intValue() {
                        return Apple2Preferences.JOYSTICK_SWIPEDOWN_BUTTON.intValue(activity);
                    }

                    @Override
                    public void saveInt(int value) {
                        Apple2Preferences.JOYSTICK_SWIPEDOWN_BUTTON.saveTouchJoystickButtons(activity, Apple2Preferences.TouchJoystickButtons.values()[value]);
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

                Apple2JoystickCalibration calibration = new Apple2JoystickCalibration(activity, viewStack, Apple2Preferences.TouchDeviceVariant.JOYSTICK);

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
            public View getView(final Apple2Activity activity, View convertView) {
                return _sliderView(activity, this, Apple2Preferences.TAPDELAY_NUM_CHOICES, new IPreferenceSlider() {
                    @Override
                    public void saveInt(int progress) {
                        Apple2Preferences.JOYSTICK_TAPDELAY.saveInt(activity, progress);
                    }

                    @Override
                    public int intValue() {
                        return Apple2Preferences.JOYSTICK_TAPDELAY.intValue(activity);
                    }

                    @Override
                    public void showValue(int progress, final TextView seekBarValue) {
                        seekBarValue.setText("" + (((float) progress / Apple2Preferences.TAPDELAY_NUM_CHOICES) * Apple2Preferences.TAPDELAY_SCALE));
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
                public View getView(final Apple2Activity activity, View convertView) {
                    convertView = _basicView(activity, this, convertView);
                    CheckBox cb = _addCheckbox(activity, this, convertView, Apple2Preferences.JOYSTICK_VISIBILITY.booleanValue(activity));
                    cb.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
                        @Override
                        public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                            Apple2Preferences.JOYSTICK_VISIBILITY.saveBoolean(activity, isChecked);
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
                public View getView(final Apple2Activity activity, View convertView) {
                    convertView = _basicView(activity, this, convertView);
                    CheckBox cb = _addCheckbox(activity, this, convertView, Apple2Preferences.JOYSTICK_AZIMUTH_VISIBILITY.booleanValue(activity));
                    cb.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
                        @Override
                        public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                            Apple2Preferences.JOYSTICK_AZIMUTH_VISIBILITY.saveBoolean(activity, isChecked);
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
                public View getView(final Apple2Activity activity, View convertView) {
                    convertView = _basicView(activity, this, convertView);
                    CheckBox cb = _addCheckbox(activity, this, convertView, Apple2Preferences.JOYSTICK_AXIS_ON_LEFT.booleanValue(activity));
                    cb.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
                        @Override
                        public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                            Apple2Preferences.JOYSTICK_AXIS_ON_LEFT.saveBoolean(activity, isChecked);
                        }
                    });
                    return convertView;
                }
            },
            JOYSTICK_AXIS_SENSITIVIY {
                @Override
                public final String getTitle(Apple2Activity activity) {
                    return "";
                }

                @Override
                public final String getSummary(Apple2Activity activity) {
                    return activity.getResources().getString(R.string.joystick_axis_sensitivity_summary);
                }

                @Override
                public View getView(final Apple2Activity activity, View convertView) {
                    return _sliderView(activity, this, Apple2Preferences.JOYSTICK_AXIS_SENSITIVITY_NUM_CHOICES, new IPreferenceSlider() {
                        @Override
                        public void saveInt(int progress) {
                            Apple2Preferences.JOYSTICK_AXIS_SENSITIVIY.saveInt(activity, progress);
                        }

                        @Override
                        public int intValue() {
                            return Apple2Preferences.JOYSTICK_AXIS_SENSITIVIY.intValue(activity);
                        }

                        @Override
                        public void showValue(int progress, final TextView seekBarValue) {
                            saveInt(progress);
                            int percent = (int) (Apple2Preferences.JOYSTICK_AXIS_SENSITIVIY.floatValue(activity) * 100.f);
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
                public View getView(final Apple2Activity activity, View convertView) {
                    return _sliderView(activity, this, Apple2Preferences.JOYSTICK_BUTTON_THRESHOLD_NUM_CHOICES, new IPreferenceSlider() {
                        @Override
                        public void saveInt(int progress) {
                            if (progress == 0) {
                                progress = 1;
                            }
                            Apple2Preferences.JOYSTICK_BUTTON_THRESHOLD.saveInt(activity, progress);
                        }

                        @Override
                        public int intValue() {
                            return Apple2Preferences.JOYSTICK_BUTTON_THRESHOLD.intValue(activity);
                        }

                        @Override
                        public void showValue(int progress, final TextView seekBarValue) {
                            int threshold = progress * Apple2Preferences.JOYSTICK_BUTTON_THRESHOLD_STEP;
                            seekBarValue.setText("" + threshold + " pts");
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
}
