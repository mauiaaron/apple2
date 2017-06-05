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
import android.content.Context;
import android.content.DialogInterface;
import android.graphics.drawable.BitmapDrawable;
import android.net.Uri;
import android.os.Build;
import android.os.ParcelFileDescriptor;
import android.util.Log;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.PopupWindow;
import android.widget.RadioButton;
import android.widget.TextView;

import org.deadc0de.apple2ix.basic.R;
import org.json.JSONObject;

import java.io.File;
import java.io.IOException;
import java.util.concurrent.atomic.AtomicBoolean;

public class Apple2MainMenu {

    public final static String SAVE_FILE = "emulator.state";
    private final static String TAG = "Apple2MainMenu";

    private Apple2Activity mActivity = null;
    private Apple2View mParentView = null;
    private PopupWindow mMainMenuPopup = null;

    private AtomicBoolean mShowingRebootQuit = new AtomicBoolean(false);
    private AtomicBoolean mShowingSaveRestore = new AtomicBoolean(false);

    public Apple2MainMenu(Apple2Activity activity, Apple2View parent) {
        mActivity = activity;
        mParentView = parent;
        setup();
    }

    enum SETTINGS {
        SHOW_SETTINGS {
            @Override
            public String getTitle(Context ctx) {
                return ctx.getResources().getString(R.string.menu_settings);
            }

            @Override
            public String getSummary(Context ctx) {
                return ctx.getResources().getString(R.string.menu_settings_summary);
            }

            @Override
            public void handleSelection(Apple2MainMenu mainMenu) {
                mainMenu.showSettings();
            }
        },
        LOAD_DISK {
            @Override
            public String getTitle(Context ctx) {
                return ctx.getResources().getString(R.string.menu_disks);
            }

            @Override
            public String getSummary(Context ctx) {
                return ctx.getResources().getString(R.string.menu_disks_summary);
            }

            @Override
            public void handleSelection(Apple2MainMenu mainMenu) {
                mainMenu.showDisksMenu();
            }
        },
        SAVE_RESTORE {
            @Override
            public String getTitle(Context ctx) {
                return ctx.getResources().getString(R.string.saverestore);
            }

            @Override
            public String getSummary(Context ctx) {
                return ctx.getResources().getString(R.string.saverestore_summary);
            }

            @Override
            public void handleSelection(Apple2MainMenu mainMenu) {
                if (!mainMenu.mShowingSaveRestore.compareAndSet(false, true)) {
                    Log.v(TAG, "OMG, avoiding nasty UI race around sync/restore");
                    return;
                }
                mainMenu.maybeSaveRestore();
            }
        },
        REBOOT_QUIT_EMULATOR {
            @Override
            public String getTitle(Context ctx) {
                return ctx.getResources().getString(R.string.quit_reboot);
            }

            @Override
            public String getSummary(Context ctx) {
                return "";
            }

            @Override
            public void handleSelection(Apple2MainMenu mainMenu) {
                if (!mainMenu.mShowingRebootQuit.compareAndSet(false, true)) {
                    Log.v(TAG, "OMG, avoiding nasty UI race around quit/reboot");
                    return;
                }
                mainMenu.maybeRebootQuit();
            }
        };

        public abstract String getTitle(Context ctx);

        public abstract String getSummary(Context ctx);

        public abstract void handleSelection(Apple2MainMenu mainMenu);

        public static String[] titles(Context ctx) {
            String[] titles = new String[values().length];
            int i = 0;
            for (SETTINGS setting : values()) {
                titles[i++] = setting.getTitle(ctx);
            }
            return titles;
        }
    }

    private void setup() {

        LayoutInflater inflater = (LayoutInflater) mActivity.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        View listLayout = inflater.inflate(R.layout.activity_main_menu, null, false);
        ListView mainMenuView = (ListView) listLayout.findViewById(R.id.main_popup_menu);
        mainMenuView.setEnabled(true);
        LinearLayout mainPopupContainer = (LinearLayout) listLayout.findViewById(R.id.main_popup_container);

        final String[] values = SETTINGS.titles(mActivity);

        ArrayAdapter<?> adapter = new ArrayAdapter<String>(mActivity, android.R.layout.simple_list_item_2, android.R.id.text1, values) {
            @Override
            public boolean areAllItemsEnabled() {
                return true;
            }

            @Override
            public View getView(int position, View convertView, ViewGroup parent) {
                View view = super.getView(position, convertView, parent);
                TextView tv = (TextView) view.findViewById(android.R.id.text2);
                SETTINGS setting = SETTINGS.values()[position];
                tv.setText(setting.getSummary(mActivity));
                return view;
            }
        };
        mainMenuView.setAdapter(adapter);
        mainMenuView.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                Log.d(TAG, "position:" + position + " tapped...");
                SETTINGS setting = SETTINGS.values()[position];
                setting.handleSelection(Apple2MainMenu.this);
            }
        });

        if (Build.VERSION.SDK_INT > Build.VERSION_CODES.GINGERBREAD_MR1) {
            mMainMenuPopup = new PopupWindow(mainPopupContainer, android.app.ActionBar.LayoutParams.WRAP_CONTENT, android.app.ActionBar.LayoutParams.WRAP_CONTENT, true);
        } else {
            // 2015/03/11 ... there may well be a less hackish way to support Gingerbread, but eh ... diminishing returns
            final int TOTAL_MARGINS = 16;
            int totalHeight = TOTAL_MARGINS;
            int maxWidth = 0;
            for (int i = 0; i < adapter.getCount(); i++) {
                View view = adapter.getView(i, null, mainMenuView);
                view.measure(View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED), View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED));
                totalHeight += view.getMeasuredHeight();
                int width = view.getMeasuredWidth();
                if (width > maxWidth) {
                    maxWidth = width;
                }
            }
            mMainMenuPopup = new PopupWindow(mainPopupContainer, maxWidth + TOTAL_MARGINS, totalHeight, true);
        }

        // This kludgery allows touching the outside or back-buttoning to dismiss
        mMainMenuPopup.setBackgroundDrawable(new BitmapDrawable());
        mMainMenuPopup.setOutsideTouchable(true);
        mMainMenuPopup.setOnDismissListener(new PopupWindow.OnDismissListener() {
            @Override
            public void onDismiss() {
                Apple2MainMenu.this.mActivity.maybeResumeEmulation();
            }
        });
    }

    public void showDisksMenu() {
        Apple2DisksMenu disksMenu = mActivity.getDisksMenu();
        disksMenu.show();
        mMainMenuPopup.dismiss();
    }

    public void showSettings() {
        Apple2SettingsMenu settings = mActivity.getSettingsMenu();
        settings.show();
        mMainMenuPopup.dismiss();
    }

    public void show() {
        if (mMainMenuPopup.isShowing()) {
            return;
        }

        mShowingRebootQuit.set(false);
        mShowingSaveRestore.set(false);

        mActivity.pauseEmulation();

        mMainMenuPopup.showAtLocation(mParentView, Gravity.CENTER, 0, 0);
    }

    public void dismiss() {
        if (mMainMenuPopup.isShowing()) {
            mMainMenuPopup.dismiss();
            // listener will resume ...
        }
    }

    public boolean isShowing() {
        return mMainMenuPopup.isShowing();
    }


    public void maybeRebootQuit() {
        mActivity.pauseEmulation();

        final AtomicBoolean selectionAlreadyHandled = new AtomicBoolean(false);

        LayoutInflater inflater = (LayoutInflater) mActivity.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        final View resetConfirmationView = inflater.inflate(R.layout.a2reset_confirmation, null, false);

        final RadioButton openAppleSelected = (RadioButton) resetConfirmationView.findViewById(R.id.radioButton_openApple);
        openAppleSelected.setChecked(true);
        final RadioButton closedAppleSelected = (RadioButton) resetConfirmationView.findViewById(R.id.radioButton_closedApple);
        closedAppleSelected.setChecked(false);
        final RadioButton noAppleSelected = (RadioButton) resetConfirmationView.findViewById(R.id.radioButton_noApple);
        noAppleSelected.setChecked(false);

        AlertDialog.Builder builder = new AlertDialog.Builder(mActivity).setIcon(R.drawable.ic_launcher).setCancelable(true).setTitle(R.string.quit_reboot).setMessage(R.string.quit_reboot_choice).setPositiveButton(R.string.reset, new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                if (!selectionAlreadyHandled.compareAndSet(false, true)) {
                    Log.v(TAG, "OMG, avoiding nasty UI race in reboot/quit onClick()");
                    return;
                }
                int resetState = 0;
                if (openAppleSelected.isChecked()) {
                    resetState = 1;
                } else if (closedAppleSelected.isChecked()) {
                    resetState = 2;
                }
                mActivity.rebootEmulation(resetState);
                Apple2MainMenu.this.dismiss();
            }
        }).setNeutralButton(R.string.quit, new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                if (!selectionAlreadyHandled.compareAndSet(false, true)) {
                    Log.v(TAG, "OMG, avoiding nasty UI race in reboot/quit onClick()");
                    return;
                }
                mActivity.quitEmulator();
            }
        }).setNegativeButton(R.string.cancel, null);

        builder.setView(resetConfirmationView);
        AlertDialog rebootQuitDialog = builder.create();

        mActivity.registerAndShowDialog(rebootQuitDialog);
    }


    public void maybeSaveRestore() {
        mActivity.pauseEmulation();

        final String quickSavePath;
        final File extStorage = Apple2Utils.getExternalStorageDirectory(mActivity);

        if (extStorage != null) {
            quickSavePath = extStorage + File.separator + SAVE_FILE;
        } else {
            quickSavePath = Apple2Utils.getDataDir(mActivity) + File.separator + SAVE_FILE;
        }

        final AtomicBoolean selectionAlreadyHandled = new AtomicBoolean(false);

        AlertDialog saveRestoreDialog = new AlertDialog.Builder(mActivity).setIcon(R.drawable.ic_launcher).setCancelable(true).setTitle(R.string.saverestore).setMessage(R.string.saverestore_choice).setPositiveButton(R.string.save, new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                if (!selectionAlreadyHandled.compareAndSet(false, true)) {
                    Log.v(TAG, "OMG, avoiding nasty UI race in sync/restore onClick()");
                    return;
                }
                mActivity.saveState(quickSavePath);
                Apple2MainMenu.this.dismiss();
            }
        }).setNeutralButton(R.string.restore, new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                if (!selectionAlreadyHandled.compareAndSet(false, true)) {
                    Log.v(TAG, "OMG, avoiding nasty UI race in sync/restore onClick()");
                    return;
                }

                Apple2DisksMenu.ejectDisk(/*isDriveA:*/true);
                Apple2DisksMenu.ejectDisk(/*isDriveA:*/false);

                // First we extract and open the emulator.state disk paths (which could be in a restricted location)
                String jsonString = mActivity.stateExtractDiskPaths(quickSavePath);
                try {

                    JSONObject map = new JSONObject(jsonString);
                    map.put("stateFile", quickSavePath);

                    final String[] diskPathKeys = new String[]{"diskA", "diskB"};
                    final String[] readOnlyKeys = new String[]{"readOnlyA", "readOnlyB"};
                    final String[] fdKeys = new String[]{"fdA", "fdB"};

                    ParcelFileDescriptor[] pfds = { null, null };

                    for (int i = 0; i < 2; i++) {
                        String diskPath = map.getString(diskPathKeys[i]);
                        boolean readOnly = map.getBoolean(readOnlyKeys[i]);

                        Apple2Preferences.setJSONPref(i == 0 ? Apple2DisksMenu.SETTINGS.CURRENT_DISK_PATH_A : Apple2DisksMenu.SETTINGS.CURRENT_DISK_PATH_B, diskPath);
                        Apple2Preferences.setJSONPref(i == 0 ? Apple2DisksMenu.SETTINGS.CURRENT_DISK_PATH_A_RO : Apple2DisksMenu.SETTINGS.CURRENT_DISK_PATH_B_RO, readOnly);

                        if (diskPath.equals("")) {
                            continue;
                        }

                        if (diskPath.startsWith(Apple2DisksMenu.EXTERNAL_CHOOSER_SENTINEL)) {
                            String uriString = diskPath.substring(Apple2DisksMenu.EXTERNAL_CHOOSER_SENTINEL.length());

                            Uri uri = Uri.parse(uriString);

                            pfds[i] = Apple2DiskChooserActivity.openFileDescriptorFromUri(mActivity, uri);
                            int fd = pfds[i].getFd();

                            map.put(fdKeys[i], fd);
                        } else {
                            boolean exists = new File(diskPath).exists();
                            if (!exists) {
                                Log.e(TAG, "Did not find path(s) for drive #" + i + " specified in emulator.state file : " + diskPath);
                            }
                        }
                    }

                    jsonString = mActivity.loadState(map.toString());

                    for (int i = 0; i < 2; i++) {
                        try {
                            if (pfds[i] != null) {
                                pfds[i].close();
                            }
                        } catch (IOException ioe) {
                            Log.e(TAG, "Error attempting to close PFD #" + i + " : " + ioe);
                        }
                    }
                    map = new JSONObject(jsonString);

                    {
                        boolean wasGzippedA = map.getBoolean("wasGzippedA");
                        Apple2Preferences.setJSONPref(Apple2DisksMenu.SETTINGS.CURRENT_DISK_PATH_A_GZ, wasGzippedA);
                    }
                    {
                        boolean wasGzippedB = map.getBoolean("wasGzippedB");
                        Apple2Preferences.setJSONPref(Apple2DisksMenu.SETTINGS.CURRENT_DISK_PATH_B_GZ, wasGzippedB);
                    }

                    // FIXME TODO : what to do if state load failed?

                } catch (Throwable t) {
                    Log.v(TAG, "OOPS : " + t);
                }
                Apple2MainMenu.this.dismiss();
            }
        }).setNegativeButton(R.string.cancel, null).create();

        mActivity.registerAndShowDialog(saveRestoreDialog);
    }
}
