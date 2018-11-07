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

import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.net.Uri;
import android.util.Log;
import android.view.View;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.TextView;

import org.deadc0de.apple2ix.basic.BuildConfig;
import org.deadc0de.apple2ix.basic.R;

public class Apple2SettingsMenu extends Apple2AbstractMenu {

    private final static String TAG = "Apple2SettingsMenu";

    public Apple2SettingsMenu(Apple2Activity activity) {
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
        return true;
    }

    public enum TouchDeviceVariant {
        NONE(0),
        JOYSTICK(1),
        JOYSTICK_KEYPAD(2),
        KEYBOARD(3),
        TOPMENU(4),
        ALERT(5);
        private int dev;

        public static final TouchDeviceVariant FRAMEBUFFER = NONE;

        public static final int size = TouchDeviceVariant.values().length;

        TouchDeviceVariant(int dev) {
            this.dev = dev;
        }

        static TouchDeviceVariant next(int ord) {
            ord = (ord + 1) % size;
            return TouchDeviceVariant.values()[ord];
        }
    }

    enum SETTINGS implements Apple2AbstractMenu.IMenuEnum {
        CURRENT_INPUT {
            @Override
            public final String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.input_current);
            }

            @Override
            public final String getSummary(Apple2Activity activity) {
                return activity.getResources().getString(R.string.input_current_summary);
            }

            @Override
            public String getPrefDomain() {
                return Apple2Preferences.PREF_DOMAIN_TOUCHSCREEN;
            }

            @Override
            public String getPrefKey() {
                return "screenOwner";
            }

            @Override
            public Object getPrefDefault() {
                return TouchDeviceVariant.KEYBOARD.ordinal();
            }

            @Override
            public final View getView(final Apple2Activity activity, View convertView) {
                convertView = _basicView(activity, this, convertView);
                _addPopupIcon(activity, this, convertView);
                return convertView;
            }

            @Override
            public void handleSelection(final Apple2Activity activity, final Apple2AbstractMenu settingsMenu, boolean isChecked) {
                final IMenuEnum self = this;
                _alertDialogHandleSelection(activity, R.string.input_current, new String[]{
                        activity.getResources().getString(R.string.joystick),
                        activity.getResources().getString(R.string.keypad),
                        activity.getResources().getString(R.string.keyboard),
                }, new IPreferenceLoadSave() {
                    @Override
                    public int intValue() {
                        int val = (int) Apple2Preferences.getJSONPref(self);
                        return val - 1;
                    }

                    @Override
                    public void saveInt(int value) {
                        Apple2Preferences.setJSONPref(self, TouchDeviceVariant.values()[value].ordinal() + 1);
                    }
                });
            }
        },
        VIDEO_CONFIGURE {
            @Override
            public final String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.video_configure);
            }

            @Override
            public final String getSummary(Apple2Activity activity) {
                return activity.getResources().getString(R.string.video_configure_summary);
            }

            @Override
            public void handleSelection(final Apple2Activity activity, final Apple2AbstractMenu settingsMenu, boolean isChecked) {
                new Apple2VideoSettingsMenu(activity).show();
            }
        },
        JOYSTICK_CONFIGURE {
            @Override
            public final String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.joystick_configure);
            }

            @Override
            public final String getSummary(Apple2Activity activity) {
                return activity.getResources().getString(R.string.joystick_configure_summary);
            }

            @Override
            public void handleSelection(final Apple2Activity activity, final Apple2AbstractMenu settingsMenu, boolean isChecked) {
                new Apple2JoystickSettingsMenu(activity).show();
            }
        },
        KEYPAD_CONFIGURE {
            @Override
            public final String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_configure);
            }

            @Override
            public final String getSummary(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_configure_summary);
            }

            @Override
            public void handleSelection(final Apple2Activity activity, final Apple2AbstractMenu settingsMenu, boolean isChecked) {
                new Apple2KeypadSettingsMenu(activity).show();
            }
        },
        KEYBOARD_CONFIGURE {
            @Override
            public final String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keyboard_configure);
            }

            @Override
            public final String getSummary(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keyboard_configure_summary);
            }

            @Override
            public void handleSelection(final Apple2Activity activity, final Apple2AbstractMenu settingsMenu, boolean isChecked) {
                new Apple2KeyboardSettingsMenu(activity).show();
            }
        },
        AUDIO_CONFIGURE {
            @Override
            public final String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.audio_configure);
            }

            @Override
            public final String getSummary(Apple2Activity activity) {
                return activity.getResources().getString(R.string.audio_configure_summary);
            }

            @Override
            public void handleSelection(Apple2Activity activity, Apple2AbstractMenu settingsMenu, boolean isChecked) {
                new Apple2AudioSettingsMenu(activity).show();
            }
        },
        SHOW_DISK_OPERATIONS {
            @Override
            public final String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.disk_show_operation);
            }

            @Override
            public final String getSummary(Apple2Activity activity) {
                return activity.getResources().getString(R.string.disk_show_operation_summary);
            }

            @Override
            public String getPrefKey() {
                return "diskAnimationsEnabled";
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
        FAST_DISK_OPERATIONS {
            @Override
            public final String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.disk_fast_operation);
            }

            @Override
            public final String getSummary(Apple2Activity activity) {
                return activity.getResources().getString(R.string.disk_fast_operation_summary);
            }

            @Override
            public String getPrefKey() {
                return "diskFastLoading";
            }

            @Override
            public Object getPrefDefault() {
                return false;
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
        ABOUT {
            @Override
            public final String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.about_apple2ix);
            }

            @Override
            public final String getSummary(Apple2Activity activity) {
                return activity.getResources().getString(R.string.about_apple2ix_summary);
            }

            @Override
            public void handleSelection(Apple2Activity activity, final Apple2AbstractMenu settingsMenu, boolean isChecked) {
                String url = "https://deadc0de.org/apple2ix/android/";
                Intent i = new Intent(Intent.ACTION_VIEW);
                i.setData(Uri.parse(url));
                activity.startActivity(i);
            }
        },
        LICENSES {
            @Override
            public final String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.about_apple2ix_licenses);
            }

            @Override
            public final String getSummary(Apple2Activity activity) {
                return activity.getResources().getString(R.string.about_apple2ix_licenses_summary);
            }

            @Override
            public void handleSelection(Apple2Activity activity, final Apple2AbstractMenu settingsMenu, boolean isChecked) {
                String url = "https://deadc0de.org/apple2ix/licenses/";
                Intent i = new Intent(Intent.ACTION_VIEW);
                i.setData(Uri.parse(url));
                activity.startActivity(i);
            }
        },
        RESET_PREFERENCES {
            @Override
            public final String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.preferences_reset_title);
            }

            @Override
            public final String getSummary(Apple2Activity activity) {
                return activity.getResources().getString(R.string.preferences_reset_summary);
            }

            @Override
            public void handleSelection(final Apple2Activity activity, final Apple2AbstractMenu settingsMenu, boolean isChecked) {
                AlertDialog.Builder builder = new AlertDialog.Builder(activity).setIcon(R.drawable.ic_launcher).setCancelable(true).setTitle(R.string.preferences_reset_really).setMessage(R.string.preferences_reset_warning).setPositiveButton(R.string.ok, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        dialog.dismiss();
                        Apple2Preferences.reset(activity);
                    }
                }).setNegativeButton(R.string.no, null);
                AlertDialog dialog = builder.create();
                activity.registerAndShowDialog(dialog);
            }
        },
        CRASH {
            @Override
            public final String getTitle(Apple2Activity activity) {
                if (BuildConfig.DEBUG) {
                    // in debug mode we actually exercise the crash reporter ...
                    return activity.getResources().getString(R.string.crasher_title);
                } else {
                    return activity.getResources().getString(R.string.crasher_check_title);
                }
            }

            @Override
            public final String getSummary(Apple2Activity activity) {
                if (BuildConfig.DEBUG) {
                    return activity.getResources().getString(R.string.crasher_summary);
                } else {
                    return activity.getResources().getString(R.string.crasher_check_summary);
                }
            }

            @Override
            public String getPrefKey() {
                return "sendCrashReports";
            }

            @Override
            public Object getPrefDefault() {
                return true;
            }

            @Override
            public View getView(final Apple2Activity activity, View convertView) {
                convertView = _basicView(activity, this, convertView);
                if (!BuildConfig.DEBUG) {
                    CheckBox cb = _addCheckbox(activity, this, convertView, (boolean) Apple2Preferences.getJSONPref(this));
                    final IMenuEnum self = this;
                    cb.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
                        @Override
                        public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                            Apple2Preferences.setJSONPref(self, isChecked);
                        }
                    });
                }
                return convertView;
            }

            @Override
            public void handleSelection(final Apple2Activity activity, final Apple2AbstractMenu settingsMenu, boolean isChecked) {

                if (BuildConfig.DEBUG) {
                    _alertDialogHandleSelection(activity, R.string.crasher, Apple2CrashHandler.CrashType.titles(activity), new IPreferenceLoadSave() {
                        @Override
                        public int intValue() {
                            return -1;
                        }

                        @Override
                        public void saveInt(int value) {
                            switch (value) {
                                case 0: {
                                    final String[] str = new String[1];
                                    str[0] = null;
                                    activity.runOnUiThread(new Runnable() {
                                        @Override
                                        public void run() {
                                            Log.d(TAG, "About to NPE : " + str[0].length());
                                        }
                                    });
                                }
                                break;

                                default:
                                    Apple2CrashHandler.getInstance().performCrash(value);
                                    break;
                            }
                        }
                    });
                }
            }
        };

        public static final int size = SETTINGS.values().length;

        @Override
        public String getPrefDomain() {
            return Apple2Preferences.PREF_DOMAIN_INTERFACE;
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
