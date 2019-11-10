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
import android.widget.Toast;

import java.util.ArrayList;

import org.deadc0de.apple2ix.basic.R;
import org.json.JSONArray;
import org.json.JSONObject;

public class Apple2KeypadSettingsMenu extends Apple2AbstractMenu {

    private final static String TAG = "Apple2KeypadSettingsMenu";

    public final static String PREF_KPAD_AXIS_ROSETTE = "kpAxisRosette";
    public final static String PREF_KPAD_BUTT_ROSETTE = "kpButtRosette";

    public final static int ROSETTE_SIZE = 9;

    private Apple2SettingsMenu.TouchDeviceVariant mVariant;

    public Apple2KeypadSettingsMenu(Apple2Activity activity, Apple2SettingsMenu.TouchDeviceVariant variant) {
        super(activity);

        if (!(variant == Apple2SettingsMenu.TouchDeviceVariant.JOYSTICK || variant == Apple2SettingsMenu.TouchDeviceVariant.JOYSTICK_KEYPAD)) {
            throw new RuntimeException("You're doing it wrong");
        }

        mVariant = variant;
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

    public static class KeyTuple {
        public long ch;
        public long scan;
        public boolean isShifted;

        public KeyTuple(long ch, long scan) {
            this(ch, scan, false);
        }

        public KeyTuple(long ch, long scan, boolean isShifted) {
            this.ch = ch;
            this.scan = scan;
            this.isShifted = isShifted;
        }
    }

    public enum KeypadPreset {
        ARROWS_SPACE {
            @Override
            public String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_preset_arrows_space);
            }

            @Override
            public void apply(Apple2Activity activity) {
                {
                    ArrayList<KeyTuple> axisRosette = new ArrayList<KeyTuple>();
                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));
                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.MOUSETEXT_UP, Apple2KeyboardSettingsMenu.SCANCODE_UP));
                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));

                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.MOUSETEXT_LEFT, Apple2KeyboardSettingsMenu.SCANCODE_LEFT));
                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));
                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.MOUSETEXT_RIGHT, Apple2KeyboardSettingsMenu.SCANCODE_RIGHT));

                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));
                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.MOUSETEXT_DOWN, Apple2KeyboardSettingsMenu.SCANCODE_DOWN));
                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));
                    saveAxisRosette(axisRosette);
                }

                {
                    ArrayList<KeyTuple> buttRosette = new ArrayList<KeyTuple>();
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));

                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_VISUAL_SPACE, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));

                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));
                    saveButtRosette(buttRosette);
                }
            }
        },
        AZ_LEFT_RIGHT_SPACE {
            @Override
            public String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_preset_az_left_right_space);
            }

            @Override
            public void apply(Apple2Activity activity) {
                {
                    ArrayList<KeyTuple> axisRosette = new ArrayList<KeyTuple>();

                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));
                    axisRosette.add(new KeyTuple('A', Apple2KeyboardSettingsMenu.SCANCODE_A));
                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));

                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.MOUSETEXT_LEFT, Apple2KeyboardSettingsMenu.SCANCODE_LEFT));
                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));
                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.MOUSETEXT_RIGHT, Apple2KeyboardSettingsMenu.SCANCODE_RIGHT));

                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));
                    axisRosette.add(new KeyTuple('Z', Apple2KeyboardSettingsMenu.SCANCODE_Z));
                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));
                    saveAxisRosette(axisRosette);
                }

                {
                    ArrayList<KeyTuple> buttRosette = new ArrayList<KeyTuple>();
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));

                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_VISUAL_SPACE, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));

                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));
                    saveButtRosette(buttRosette);
                }
            }
        },
        LEFT_RIGHT_SPACE {
            @Override
            public String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_preset_left_right_space);
            }

            @Override
            public void apply(Apple2Activity activity) {
                {
                    ArrayList<KeyTuple> axisRosette = new ArrayList<KeyTuple>();
                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_LEFT));
                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));
                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_RIGHT));

                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.MOUSETEXT_LEFT, Apple2KeyboardSettingsMenu.SCANCODE_LEFT));
                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));
                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.MOUSETEXT_RIGHT, Apple2KeyboardSettingsMenu.SCANCODE_RIGHT));

                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_LEFT));
                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));
                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_RIGHT));
                    saveAxisRosette(axisRosette);
                }

                {
                    ArrayList<KeyTuple> buttRosette = new ArrayList<KeyTuple>();
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));

                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_VISUAL_SPACE, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));

                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));
                    saveButtRosette(buttRosette);
                }
            }
        },
        QAZ_LEFT_RIGHT_SPACE {
            @Override
            public String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_preset_qaz_left_right_space);
            }

            @Override
            public void apply(Apple2Activity activity) {
                {
                    ArrayList<KeyTuple> axisRosette = new ArrayList<KeyTuple>();
                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));
                    axisRosette.add(new KeyTuple('A', Apple2KeyboardSettingsMenu.SCANCODE_A));
                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));

                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.MOUSETEXT_LEFT, Apple2KeyboardSettingsMenu.SCANCODE_LEFT));
                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));
                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.MOUSETEXT_RIGHT, Apple2KeyboardSettingsMenu.SCANCODE_RIGHT));

                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));
                    axisRosette.add(new KeyTuple('Z', Apple2KeyboardSettingsMenu.SCANCODE_Z));
                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));
                    saveAxisRosette(axisRosette);
                }

                {
                    ArrayList<KeyTuple> buttRosette = new ArrayList<KeyTuple>();
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_Q));
                    buttRosette.add(new KeyTuple('Q', Apple2KeyboardSettingsMenu.SCANCODE_Q));
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_Q));

                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_VISUAL_SPACE, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));

                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));
                    saveButtRosette(buttRosette);
                }
            }
        },
        IJKM_SPACE {
            @Override
            public String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_preset_ijkm_space);
            }

            @Override
            public void apply(Apple2Activity activity) {
                {
                    ArrayList<KeyTuple> axisRosette = new ArrayList<KeyTuple>();
                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));
                    axisRosette.add(new KeyTuple('I', Apple2KeyboardSettingsMenu.SCANCODE_I));
                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));

                    axisRosette.add(new KeyTuple('J', Apple2KeyboardSettingsMenu.SCANCODE_J));
                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));
                    axisRosette.add(new KeyTuple('K', Apple2KeyboardSettingsMenu.SCANCODE_K));

                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));
                    axisRosette.add(new KeyTuple('M', Apple2KeyboardSettingsMenu.SCANCODE_M));
                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));
                    saveAxisRosette(axisRosette);
                }

                {
                    ArrayList<KeyTuple> buttRosette = new ArrayList<KeyTuple>();
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));

                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_VISUAL_SPACE, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));

                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));
                    saveButtRosette(buttRosette);
                }
            }
        },
        WADX_SPACE {
            @Override
            public String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_preset_wadx_space);
            }

            @Override
            public void apply(Apple2Activity activity) {
                {
                    ArrayList<KeyTuple> axisRosette = new ArrayList<KeyTuple>();
                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));
                    axisRosette.add(new KeyTuple('W', Apple2KeyboardSettingsMenu.SCANCODE_W));
                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));

                    axisRosette.add(new KeyTuple('A', Apple2KeyboardSettingsMenu.SCANCODE_A));
                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));
                    axisRosette.add(new KeyTuple('D', Apple2KeyboardSettingsMenu.SCANCODE_D));

                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));
                    axisRosette.add(new KeyTuple('X', Apple2KeyboardSettingsMenu.SCANCODE_X));
                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));
                    saveAxisRosette(axisRosette);
                }

                {
                    ArrayList<KeyTuple> buttRosette = new ArrayList<KeyTuple>();
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));

                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_VISUAL_SPACE, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));

                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));
                    saveButtRosette(buttRosette);
                }
            }
        },
        LODERUNNER_KEYS {
            @Override
            public String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_preset_loderunner);
            }

            @Override
            public String getToast(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_preset_loderunner_toast);
            }

            @Override
            public void apply(Apple2Activity activity) {
                {
                    ArrayList<KeyTuple> axisRosette = new ArrayList<KeyTuple>();
                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));
                    axisRosette.add(new KeyTuple('I', Apple2KeyboardSettingsMenu.SCANCODE_I));
                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));

                    axisRosette.add(new KeyTuple('J', Apple2KeyboardSettingsMenu.SCANCODE_J));
                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_VISUAL_SPACE, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));
                    axisRosette.add(new KeyTuple('L', Apple2KeyboardSettingsMenu.SCANCODE_L));

                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));
                    axisRosette.add(new KeyTuple('K', Apple2KeyboardSettingsMenu.SCANCODE_K));
                    axisRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));
                    saveAxisRosette(axisRosette);
                }

                {
                    ArrayList<KeyTuple> buttRosette = new ArrayList<KeyTuple>();
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_U));
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_O));

                    buttRosette.add(new KeyTuple('U', Apple2KeyboardSettingsMenu.SCANCODE_U));
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));
                    buttRosette.add(new KeyTuple('O', Apple2KeyboardSettingsMenu.SCANCODE_O));

                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_U));
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_O));
                    saveButtRosette(buttRosette);
                }
            }
        },
        ROBOTRON_KEYS {
            @Override
            public String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_preset_robotron);
            }

            @Override
            public void apply(Apple2Activity activity) {
                {
                    ArrayList<KeyTuple> axisRosette = new ArrayList<KeyTuple>();
                    axisRosette.add(new KeyTuple('Q', Apple2KeyboardSettingsMenu.SCANCODE_Q));
                    axisRosette.add(new KeyTuple('W', Apple2KeyboardSettingsMenu.SCANCODE_W));
                    axisRosette.add(new KeyTuple('E', Apple2KeyboardSettingsMenu.SCANCODE_E));

                    axisRosette.add(new KeyTuple('A', Apple2KeyboardSettingsMenu.SCANCODE_A));
                    axisRosette.add(new KeyTuple('S', Apple2KeyboardSettingsMenu.SCANCODE_S));
                    axisRosette.add(new KeyTuple('D', Apple2KeyboardSettingsMenu.SCANCODE_D));

                    axisRosette.add(new KeyTuple('Z', Apple2KeyboardSettingsMenu.SCANCODE_Z));
                    axisRosette.add(new KeyTuple('X', Apple2KeyboardSettingsMenu.SCANCODE_X));
                    axisRosette.add(new KeyTuple('C', Apple2KeyboardSettingsMenu.SCANCODE_C));
                    saveAxisRosette(axisRosette);
                }

                {
                    ArrayList<KeyTuple> buttRosette = new ArrayList<KeyTuple>();
                    buttRosette.add(new KeyTuple('I', Apple2KeyboardSettingsMenu.SCANCODE_I));
                    buttRosette.add(new KeyTuple('O', Apple2KeyboardSettingsMenu.SCANCODE_O));
                    buttRosette.add(new KeyTuple('P', Apple2KeyboardSettingsMenu.SCANCODE_P));

                    buttRosette.add(new KeyTuple('K', Apple2KeyboardSettingsMenu.SCANCODE_K));
                    buttRosette.add(new KeyTuple('L', Apple2KeyboardSettingsMenu.SCANCODE_L));
                    buttRosette.add(new KeyTuple(';', Apple2KeyboardSettingsMenu.SCANCODE_SEMICOLON));

                    buttRosette.add(new KeyTuple(',', Apple2KeyboardSettingsMenu.SCANCODE_COMMA));
                    buttRosette.add(new KeyTuple('.', Apple2KeyboardSettingsMenu.SCANCODE_PERIOD));
                    buttRosette.add(new KeyTuple('/', Apple2KeyboardSettingsMenu.SCANCODE_SLASH));
                    saveButtRosette(buttRosette);
                }
            }
        },
        SEAFOX_KEYS {
            @Override
            public String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_preset_seafox);
            }

            @Override
            public void apply(Apple2Activity activity) {
                {
                    ArrayList<KeyTuple> axisRosette = new ArrayList<KeyTuple>();
                    axisRosette.add(new KeyTuple('Y', Apple2KeyboardSettingsMenu.SCANCODE_Y));
                    axisRosette.add(new KeyTuple('U', Apple2KeyboardSettingsMenu.SCANCODE_U));
                    axisRosette.add(new KeyTuple('I', Apple2KeyboardSettingsMenu.SCANCODE_I));

                    axisRosette.add(new KeyTuple('H', Apple2KeyboardSettingsMenu.SCANCODE_H));
                    axisRosette.add(new KeyTuple('J', Apple2KeyboardSettingsMenu.SCANCODE_J));
                    axisRosette.add(new KeyTuple('K', Apple2KeyboardSettingsMenu.SCANCODE_K));

                    axisRosette.add(new KeyTuple('N', Apple2KeyboardSettingsMenu.SCANCODE_N));
                    axisRosette.add(new KeyTuple('M', Apple2KeyboardSettingsMenu.SCANCODE_M));
                    axisRosette.add(new KeyTuple(',', Apple2KeyboardSettingsMenu.SCANCODE_COMMA));
                    saveAxisRosette(axisRosette);
                }

                {
                    ArrayList<KeyTuple> buttRosette = new ArrayList<KeyTuple>();
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_D));
                    buttRosette.add(new KeyTuple('D', Apple2KeyboardSettingsMenu.SCANCODE_D));
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, Apple2KeyboardSettingsMenu.SCANCODE_F));

                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_VISUAL_SPACE, Apple2KeyboardSettingsMenu.SCANCODE_SPACE));
                    buttRosette.add(new KeyTuple('F', Apple2KeyboardSettingsMenu.SCANCODE_F));

                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));
                    buttRosette.add(new KeyTuple(Apple2KeyboardSettingsMenu.ICONTEXT_NONACTION, -1));
                    saveButtRosette(buttRosette);
                }
            }
        };

        public static void saveAxisRosette(ArrayList<KeyTuple> axisRosette) {
            if (axisRosette.size() != 9) {
                throw new RuntimeException("axis rosette is not correct size");
            }
            Apple2Preferences.setJSONPref(Apple2Preferences.PREF_DOMAIN_JOYSTICK, PREF_KPAD_AXIS_ROSETTE, toJSONArray(axisRosette));
        }

        public static void saveButtRosette(ArrayList<KeyTuple> buttRosette) {
            if (buttRosette.size() != 9) {
                throw new RuntimeException("butt rosette is not correct size");
            }
            Apple2Preferences.setJSONPref(Apple2Preferences.PREF_DOMAIN_JOYSTICK, PREF_KPAD_BUTT_ROSETTE, toJSONArray(buttRosette));
        }

        private static JSONArray toJSONArray(ArrayList<KeyTuple> rosette) {
            JSONArray jsonArray = new JSONArray();
            try {
                for (KeyTuple tuple : rosette) {
                    JSONObject obj = new JSONObject();
                    obj.put("ch", tuple.ch);
                    obj.put("scan", tuple.scan);
                    obj.put("isShifted", tuple.isShifted);
                    jsonArray.put(obj);
                }
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
            return jsonArray;
        }

        public abstract String getTitle(Apple2Activity activity);

        public abstract void apply(Apple2Activity activity);

        public String getToast(Apple2Activity activity) {
            return null;
        }

        public static final int size = KeypadPreset.values().length;

        public static String[] titles(Apple2Activity activity) {
            String[] titles = new String[size];
            int i = 0;
            for (KeypadPreset preset : values()) {
                titles[i++] = preset.getTitle(activity);
            }
            return titles;
        }
    }

    enum SETTINGS implements Apple2AbstractMenu.IMenuEnum {
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
            public String getPrefKey() {
                return null;
            }

            @Override
            public Object getPrefDefault() {
                return null;
            }

            @Override
            public void handleSelection(Apple2Activity activity, Apple2AbstractMenu settingsMenu, boolean isChecked) {
                Apple2KeypadSettingsMenu thisMenu = (Apple2KeypadSettingsMenu)settingsMenu;
                Apple2JoystickCalibration.startCalibration(activity, thisMenu.mVariant);
            }
        },
        KEYPAD_CHOOSE_KEYS {
            @Override
            public final String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_choose);
            }

            @Override
            public final String getSummary(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_choose_summary);
            }

            @Override
            public String getPrefKey() {
                return "kpPresetChoice";
            }

            @Override
            public Object getPrefDefault() {
                return KeypadPreset.IJKM_SPACE.ordinal() + 1;
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
                String[] titles = new String[KeypadPreset.size + 1];
                titles[0] = activity.getResources().getString(R.string.keypad_preset_custom);
                System.arraycopy(KeypadPreset.titles(activity), 0, titles, 1, KeypadPreset.size);

                _alertDialogHandleSelection(activity, R.string.keypad_choose_title, titles, new IPreferenceLoadSave() {
                    @Override
                    public int intValue() {
                        return (int) Apple2Preferences.getJSONPref(self);
                    }

                    @Override
                    public void saveInt(int value) {
                        Apple2Preferences.setJSONPref(self, value);
                        if (value == 0) {
                            Apple2KeypadSettingsMenu keypadSettingsMenu = (Apple2KeypadSettingsMenu) settingsMenu;
                            keypadSettingsMenu.chooseKeys(activity);
                        } else {
                            KeypadPreset.values()[value - 1].apply(activity);

                            String toast = KeypadPreset.values()[value - 1].getToast(activity);
                            if (toast != null) {
                                Toast.makeText(activity, toast, Toast.LENGTH_SHORT).show();
                            }
                        }
                    }
                });
            }
        },
        FAST_AUTOREPEAT {
            @Override
            public final String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_autorepeat_fast);
            }

            @Override
            public final String getSummary(Apple2Activity activity) {
                return activity.getResources().getString(R.string.keypad_autorepeat_fast_summary);
            }

            @Override
            public String getPrefKey() {
                return "kpFastAutoRepeat";
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
        JOYSTICK_ADVANCED {
            @Override
            public final String getTitle(Apple2Activity activity) {
                return activity.getResources().getString(R.string.settings_advanced_joystick);
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

    // ------------------------------------------------------------------------
    // internals

    private void chooseKeys(Apple2Activity activity) {
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

        Apple2KeypadChooser chooser = new Apple2KeypadChooser(activity, viewStack);

        // show this new view...
        chooser.show();

        // ...with nothing else underneath 'cept the emulator OpenGL layer
        for (Apple2MenuView apple2MenuView : viewStack) {
            activity.popApple2View(apple2MenuView);
        }
    }
}
