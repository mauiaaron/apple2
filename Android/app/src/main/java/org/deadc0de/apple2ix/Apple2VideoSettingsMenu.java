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

import android.content.pm.ActivityInfo;
import android.view.View;
import android.widget.CheckBox;
import android.widget.CompoundButton;

import java.util.ArrayList;

import org.deadc0de.apple2ix.basic.R;

public class Apple2VideoSettingsMenu extends Apple2AbstractMenu {

    private final static String TAG = "Apple2VideoSettingsMenu";

    public Apple2VideoSettingsMenu(Apple2Activity activity) {
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
        if (position == SETTINGS.PORTRAIT_CALIBRATE.ordinal()) {
            if ((boolean) Apple2Preferences.getJSONPref(SETTINGS.LANDSCAPE_MODE)) {
                return false;
            }
        }
        return true;
    }

    public enum HiresColor {
        BW,
        COLOR,
        INTERPOLATED
    }

    protected enum SETTINGS implements Apple2AbstractMenu.IMenuEnum {
        LANDSCAPE_MODE {
            @Override
            public final String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.mode_landscape);
            }

            @Override
            public final String getSummary(Apple2Activity activity) {
                return activity.getResources().getString(R.string.mode_landscape_summary);
            }

            @Override
            public String getPrefDomain() {
                return Apple2Preferences.PREF_DOMAIN_INTERFACE;
            }

            @Override
            public String getPrefKey() {
                return "landscapeEnabled";
            }

            @Override
            public Object getPrefDefault() {
                return true;
            }

            @Override
            public final View getView(final Apple2Activity activity, View convertView) {
                final Object self = this;
                convertView = _basicView(activity, this, convertView);
                CheckBox cb = _addCheckbox(activity, this, convertView, (boolean) Apple2Preferences.getJSONPref(this));
                cb.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
                    @Override
                    public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                        Apple2Preferences.setJSONPref((IMenuEnum)self, isChecked);
                        applyLandscapeMode(activity);
                    }
                });
                return convertView;
            }
        },
        PORTRAIT_CALIBRATE {
            @Override
            public final String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.portrait_calibrate);
            }

            @Override
            public final String getSummary(Apple2Activity activity) {
                return activity.getResources().getString(R.string.portrait_calibrate_summary);
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

                // switch to portrait
                Apple2Preferences.setJSONPref(SETTINGS.LANDSCAPE_MODE, false);
                applyLandscapeMode(activity);
                Apple2PortraitCalibration calibration = new Apple2PortraitCalibration(activity, viewStack);

                // show this new view...
                calibration.show();

                // ...with nothing else underneath 'cept the emulator OpenGL layer
                for (Apple2MenuView apple2MenuView : viewStack) {
                    activity.popApple2View(apple2MenuView);
                }
            }
        },
        COLOR_CONFIGURE {
            @Override
            public final String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.color_configure);
            }

            @Override
            public final String getSummary(Apple2Activity activity) {
                return activity.getResources().getString(R.string.color_configure_summary);
            }

            @Override
            public String getPrefDomain() {
                return Apple2Preferences.PREF_DOMAIN_VIDEO;
            }

            @Override
            public String getPrefKey() {
                return "colorMode";
            }

            @Override
            public Object getPrefDefault() {
                return HiresColor.INTERPOLATED.ordinal();
            }

            @Override
            public View getView(Apple2Activity activity, View convertView) {
                convertView = _basicView(activity, this, convertView);
                _addPopupIcon(activity, this, convertView);
                return convertView;
            }

            @Override
            public void handleSelection(final Apple2Activity activity, final Apple2AbstractMenu settingsMenu, boolean isChecked) {
                final Apple2AbstractMenu.IMenuEnum self = this;
                _alertDialogHandleSelection(activity, R.string.video_configure, new String[]{
                        settingsMenu.mActivity.getResources().getString(R.string.color_bw),
                        settingsMenu.mActivity.getResources().getString(R.string.color_color),
                        settingsMenu.mActivity.getResources().getString(R.string.color_interpolated),
                }, new IPreferenceLoadSave() {
                    @Override
                    public int intValue() {
                        return (int) Apple2Preferences.getJSONPref(self);
                    }

                    @Override
                    public void saveInt(int value) {
                        Apple2Preferences.setJSONPref(self, HiresColor.values()[value].ordinal());
                    }
                });
            }
        };

        public static final int size = SETTINGS.values().length;

        @Override
        public String getPrefDomain() {
            return null;
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

        public static boolean applyLandscapeMode(final Apple2Activity activity) {
            if ((boolean) Apple2Preferences.getJSONPref(SETTINGS.LANDSCAPE_MODE)) {
                activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
                return false;
            } else {
                int orientation = activity.getRequestedOrientation();
                activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
                return orientation != ActivityInfo.SCREEN_ORIENTATION_PORTRAIT;
            }
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
