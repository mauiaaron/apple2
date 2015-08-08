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

import java.util.ArrayList;

public class Apple2KeypadSettingsMenu extends Apple2AbstractMenu {

    private final static String TAG = "Apple2KeypadSettingsMenu";

    public Apple2KeypadSettingsMenu(Apple2Activity activity) {
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

    enum SETTINGS implements Apple2AbstractMenu.IMenuEnum {
        KEYPAD_CHOOSE_BUTTONS {
            @Override
            public final String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_choose);
            }

            @Override
            public final String getSummary(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_choose_summary);
            }

            @Override
            public void handleSelection(final Apple2Activity activity, final Apple2AbstractMenu settingsMenu, boolean isChecked) {
                ArrayList<Apple2MenuView> viewStack = new ArrayList<>();
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

                Apple2KeypadChooser chooser = new Apple2KeypadChooser(activity, viewStack);

                // show this new view...
                chooser.show();

                // ...with nothing else underneath 'cept the emulator OpenGL layer
                for (Apple2MenuView apple2MenuView : viewStack) {
                    activity.popApple2View(apple2MenuView);
                }
            }
        },
        KEYPAD_CALIBRATE {
            @Override
            public final String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_calibrate);
            }

            @Override
            public final String getSummary(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_calibrate_summary);
            }

            @Override
            public void handleSelection(Apple2Activity activity, Apple2AbstractMenu settingsMenu, boolean isChecked) {
                ArrayList<Apple2MenuView> viewStack = new ArrayList<>();
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

                Apple2JoystickCalibration calibration = new Apple2JoystickCalibration(activity, viewStack, Apple2Preferences.TouchDeviceVariant.JOYSTICK_KEYPAD);

                // show this new view...
                calibration.show();

                // ...with nothing else underneath 'cept the emulator OpenGL layer
                for (Apple2MenuView apple2MenuView : viewStack) {
                    activity.popApple2View(apple2MenuView);
                }
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
