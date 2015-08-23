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

public class Apple2KeyboardSettingsMenu extends Apple2AbstractMenu {

    private final static String TAG = "Apple2KeyboardSettingsMenu";

    // These settings must match native side
    public final static int MOUSETEXT_BEGIN = 0x80;
    public final static int MOUSETEXT_CLOSEDAPPLE = MOUSETEXT_BEGIN/*+0x00*/;
    public final static int MOUSETEXT_OPENAPPLE = MOUSETEXT_BEGIN+0x01;
    public final static int MOUSETEXT_LEFT = MOUSETEXT_BEGIN+0x08;
    public final static int MOUSETEXT_UP = MOUSETEXT_BEGIN+0x0b;
    public final static int MOUSETEXT_DOWN = MOUSETEXT_BEGIN+0x0a;
    public final static int MOUSETEXT_RIGHT = MOUSETEXT_BEGIN+0x15;

    public final static int ICONTEXT_BEGIN = 0xA0;
    public final static int ICONTEXT_VISUAL_SPACE = ICONTEXT_BEGIN + 0x11;
    public final static int ICONTEXT_KBD_BEGIN = ICONTEXT_BEGIN + 0x13;
    public final static int ICONTEXT_CTRL = ICONTEXT_KBD_BEGIN/* + 0x00*/;
    public final static int ICONTEXT_ESC = ICONTEXT_KBD_BEGIN + 0x09;
    public final static int ICONTEXT_RETURN = ICONTEXT_KBD_BEGIN + 0x0A;
    public final static int ICONTEXT_NONACTION = ICONTEXT_KBD_BEGIN + 0x0C;

    public final static int SCANCODE_A = 30;
    public final static int SCANCODE_D = 32;
    public final static int SCANCODE_F = 33;
    public final static int SCANCODE_H = 35;
    public final static int SCANCODE_I = 23;
    public final static int SCANCODE_J = 36;
    public final static int SCANCODE_K = 37;
    public final static int SCANCODE_L = 38;
    public final static int SCANCODE_M = 50;
    public final static int SCANCODE_N = 49;
    public final static int SCANCODE_O = 24;
    public final static int SCANCODE_U = 22;
    public final static int SCANCODE_W = 17;
    public final static int SCANCODE_X = 45;
    public final static int SCANCODE_Y = 21;
    public final static int SCANCODE_Z = 44;
    public final static int SCANCODE_SPACE = 57;
    public final static int SCANCODE_UP = 103;
    public final static int SCANCODE_LEFT = 105;
    public final static int SCANCODE_RIGHT = 106;
    public final static int SCANCODE_DOWN = 108;
    public final static int SCANCODE_COMMA = 51;

    public Apple2KeyboardSettingsMenu(Apple2Activity activity) {
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
        KEYBOARD_VISIBILITY_INACTIVE {
            @Override
            public final String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keyboard_visibility_inactive);
            }

            @Override
            public final String getSummary(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keyboard_visibility_inactive_summary);
            }

            @Override
            public View getView(final Apple2Activity activity, View convertView) {
                return _sliderView(activity, this, Apple2Preferences.ALPHA_SLIDER_NUM_CHOICES, new IPreferenceSlider() {
                    @Override
                    public void saveInt(int progress) {
                        Apple2Preferences.KEYBOARD_VISIBILITY_INACTIVE.saveInt(activity, progress);
                    }

                    @Override
                    public int intValue() {
                        return Apple2Preferences.KEYBOARD_VISIBILITY_INACTIVE.intValue(activity);
                    }

                    @Override
                    public void showValue(int progress, final TextView seekBarValue) {
                        seekBarValue.setText("" + ((float) progress / Apple2Preferences.ALPHA_SLIDER_NUM_CHOICES));
                    }
                });
            }
        },
        KEYBOARD_VISIBILITY_ACTIVE {
            @Override
            public final String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keyboard_visibility_active);
            }

            @Override
            public final String getSummary(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keyboard_visibility_active_summary);
            }

            @Override
            public View getView(final Apple2Activity activity, View convertView) {
                return _sliderView(activity, this, Apple2Preferences.ALPHA_SLIDER_NUM_CHOICES, new IPreferenceSlider() {
                    @Override
                    public void saveInt(int progress) {
                        Apple2Preferences.KEYBOARD_VISIBILITY_ACTIVE.saveInt(activity, progress);
                    }

                    @Override
                    public int intValue() {
                        return Apple2Preferences.KEYBOARD_VISIBILITY_ACTIVE.intValue(activity);
                    }

                    @Override
                    public void showValue(int progress, final TextView seekBarValue) {
                        seekBarValue.setText("" + ((float) progress / Apple2Preferences.ALPHA_SLIDER_NUM_CHOICES));
                    }
                });
            }
        },
        KEYBOARD_ENABLE_LOWERCASE {
            @Override
            public final String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keyboard_lowercase_enabled);
            }

            @Override
            public final String getSummary(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keyboard_lowercase_enabled_summary);
            }

            @Override
            public View getView(final Apple2Activity activity, View convertView) {
                convertView = _basicView(activity, this, convertView);
                CheckBox cb = _addCheckbox(activity, this, convertView, Apple2Preferences.KEYBOARD_LOWERCASE_ENABLED.booleanValue(activity));
                cb.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
                    @Override
                    public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                        Apple2Preferences.KEYBOARD_LOWERCASE_ENABLED.saveBoolean(activity, isChecked);
                    }
                });
                return convertView;
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
