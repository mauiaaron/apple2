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
import android.os.Build;
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
import android.widget.TextView;

import org.deadc0de.apple2ix.basic.R;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.util.concurrent.atomic.AtomicBoolean;

public class Apple2MainMenu {

    private final static String SAVE_FILE = "emulator.state";
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
                    Log.v(TAG, "OMG, avoiding nasty UI race around save/restore");
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

        AlertDialog rebootQuitDialog = new AlertDialog.Builder(mActivity).setIcon(R.drawable.ic_launcher).setCancelable(true).setTitle(R.string.quit_reboot).setMessage(R.string.quit_reboot_choice).setPositiveButton(R.string.reboot, new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                if (!selectionAlreadyHandled.compareAndSet(false, true)) {
                    Log.v(TAG, "OMG, avoiding nasty UI race in reboot/quit onClick()");
                    return;
                }
                mActivity.rebootEmulation();
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
        }).setNegativeButton(R.string.cancel, null).create();

        mActivity.registerAndShowDialog(rebootQuitDialog);
    }


    public void maybeSaveRestore() {
        mActivity.pauseEmulation();

        final String quickSavePath = Apple2DisksMenu.getDataDir(mActivity) + File.separator + SAVE_FILE;

        final AtomicBoolean selectionAlreadyHandled = new AtomicBoolean(false);

        AlertDialog saveRestoreDialog = new AlertDialog.Builder(mActivity).setIcon(R.drawable.ic_launcher).setCancelable(true).setTitle(R.string.saverestore).setMessage(R.string.saverestore_choice).setPositiveButton(R.string.save, new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                if (!selectionAlreadyHandled.compareAndSet(false, true)) {
                    Log.v(TAG, "OMG, avoiding nasty UI race in save/restore onClick()");
                    return;
                }
                mActivity.saveState(quickSavePath);
                Apple2MainMenu.this.dismiss();
            }
        }).setNeutralButton(R.string.restore, new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                if (!selectionAlreadyHandled.compareAndSet(false, true)) {
                    Log.v(TAG, "OMG, avoiding nasty UI race in save/restore onClick()");
                    return;
                }

                String jsonData = mActivity.loadState(quickSavePath);
                try {
                    JSONObject map = new JSONObject(jsonData);
                    String diskPath1 = map.getString("disk1");
                    boolean readOnly1 = map.getBoolean("readOnly1");
                    Apple2Preferences.CURRENT_DISK_A.setPath(mActivity, diskPath1);
                    Apple2Preferences.CURRENT_DISK_A_RO.saveBoolean(mActivity, readOnly1);

                    String diskPath2 = map.getString("disk2");
                    boolean readOnly2 = map.getBoolean("readOnly2");
                    Apple2Preferences.CURRENT_DISK_B.setPath(mActivity, diskPath2);
                    Apple2Preferences.CURRENT_DISK_B_RO.saveBoolean(mActivity, readOnly2);
                } catch (JSONException je) {
                    Log.v(TAG, "OOPS : " + je);
                }
                Apple2MainMenu.this.dismiss();
            }
        }).setNegativeButton(R.string.cancel, null).create();

        mActivity.registerAndShowDialog(saveRestoreDialog);
    }
}
