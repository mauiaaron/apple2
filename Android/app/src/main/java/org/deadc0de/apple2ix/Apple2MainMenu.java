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

import android.content.Context;
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

public class Apple2MainMenu {

    private final static int MENU_INSET = 20;
    private final static String TAG = "Apple2MainMenu";

    private Apple2Activity mActivity = null;
    private Apple2View mParentView = null;
    private PopupWindow mMainMenuPopup = null;
    private Apple2SettingsMenu mSettingsMenu = null;
    private Apple2DisksMenu mDisksMenu = null;

    public Apple2MainMenu(Apple2Activity activity, Apple2View parent) {
        mActivity = activity;
        mParentView = parent;
        setup();
    }

    enum SETTINGS {
        SHOW_SETTINGS {
            @Override public String getTitle(Context ctx) {
                return ctx.getResources().getString(R.string.menu_settings);
            }
            @Override public String getSummary(Context ctx) {
                return ctx.getResources().getString(R.string.menu_settings_summary);
            }
            @Override public void handleSelection(Apple2MainMenu mainMenu) {
                mainMenu.showSettings();
            }
        },
        LOAD_DISK {
            @Override public String getTitle(Context ctx) {
                return ctx.getResources().getString(R.string.menu_disks);
            }
            @Override public String getSummary(Context ctx) {
                return ctx.getResources().getString(R.string.menu_disks_summary);
            }
            @Override public void handleSelection(Apple2MainMenu mainMenu) {
                mainMenu.showDisksMenu();
            }
        },
        REBOOT_EMULATOR {
            @Override public String getTitle(Context ctx) {
                return ctx.getResources().getString(R.string.reboot);
            }
            @Override public String getSummary(Context ctx) {
                return ctx.getResources().getString(R.string.reboot_summary);
            }
            @Override public void handleSelection(Apple2MainMenu mainMenu) {
                mainMenu.mActivity.maybeReboot();
            }
        },
        QUIT_EMULATOR {
            @Override public String getTitle(Context ctx) {
                return ctx.getResources().getString(R.string.quit);
            }
            @Override public String getSummary(Context ctx) {
                return ctx.getResources().getString(R.string.quit_summary);
            }
            @Override public void handleSelection(Apple2MainMenu mainMenu) {
                mainMenu.mActivity.maybeQuitApp();
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

        LayoutInflater inflater = (LayoutInflater)mActivity.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        View listLayout=inflater.inflate(R.layout.activity_main_menu, null, false);
        ListView mainMenuView = (ListView)listLayout.findViewById(R.id.main_popup_menu);
        mainMenuView.setEnabled(true);
        LinearLayout mainPopupContainer = (LinearLayout)listLayout.findViewById(R.id.main_popup_container);

        final String[] values = SETTINGS.titles(mActivity);

        ArrayAdapter<?> adapter = new ArrayAdapter<String>(mActivity, android.R.layout.simple_list_item_2, android.R.id.text1, values) {
            @Override
            public boolean areAllItemsEnabled() {
                return true;
            }
            @Override
            public View getView(int position, View convertView, ViewGroup parent) {
                View view = super.getView(position, convertView, parent);
                TextView tv = (TextView)view.findViewById(android.R.id.text2);
                SETTINGS setting = SETTINGS.values()[position];
                tv.setText(setting.getSummary(mActivity));
                return view;
            }
        };
        mainMenuView.setAdapter(adapter);
        mainMenuView.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                Log.d(TAG, "position:"+position+" tapped...");
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
            mMainMenuPopup = new PopupWindow(mainPopupContainer, maxWidth+TOTAL_MARGINS, totalHeight, true);
        }

        // This kludgery allows touching the outside or back-buttoning to dismiss
        mMainMenuPopup.setBackgroundDrawable(new BitmapDrawable());
        mMainMenuPopup.setOutsideTouchable(true);
        mMainMenuPopup.setOnDismissListener(new PopupWindow.OnDismissListener() {
            @Override
            public void onDismiss() {
                boolean otherMenusShowing = (getSettingsMenu().isShowing() || getDisksMenu().isShowing());
                if (!otherMenusShowing) {
                    Apple2MainMenu.this.mActivity.nativeOnResume(/*isSystemResume:*/false);
                }
            }
        });
    }

    public void showDisksMenu() {
        Apple2DisksMenu disksMenu = getDisksMenu();
        disksMenu.show();
        mMainMenuPopup.dismiss();
    }

    public void showSettings() {
        Apple2SettingsMenu settings = getSettingsMenu();
        settings.show();
        mMainMenuPopup.dismiss();
    }

    public synchronized Apple2DisksMenu getDisksMenu() {
        if (mDisksMenu == null) {
            mDisksMenu = new Apple2DisksMenu(mActivity, mParentView);
        }
        return mDisksMenu;
    }

    public synchronized Apple2SettingsMenu getSettingsMenu() {
        if (mSettingsMenu == null) {
            mSettingsMenu = new Apple2SettingsMenu(mActivity, mParentView);
        }
        return mSettingsMenu;
    }

    public void show() {
        if (mMainMenuPopup.isShowing()) {
            return;
        }

        mActivity.nativeOnPause();

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
}
