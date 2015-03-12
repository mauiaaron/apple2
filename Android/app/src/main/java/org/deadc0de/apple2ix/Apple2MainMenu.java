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
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.PopupWindow;

public class Apple2MainMenu {

    private final static int MENU_INSET = 20;
    private final static String TAG = "Apple2MainMenu";

    private Apple2Activity mActivity = null;
    private Apple2View mParentView = null;
    private PopupWindow mMainMenuPopup = null;
    private Apple2SettingsMenu mSettingsMenu = null;

    public Apple2MainMenu(Apple2Activity activity, Apple2View parent) {
        mActivity = activity;
        mParentView = parent;
        setup();
    }

    private void setup() {

        LayoutInflater inflater = (LayoutInflater)mActivity.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        View listLayout=inflater.inflate(R.layout.activity_main_menu, null, false);
        ListView mainMenuView = (ListView)listLayout.findViewById(R.id.main_popup_menu);
        mainMenuView.setEnabled(true);
        LinearLayout mainPopupContainer = (LinearLayout)listLayout.findViewById(R.id.main_popup_container);

        String[] values = new String[] {
                mActivity.getResources().getString(R.string.menu_settings),
                mActivity.getResources().getString(R.string.menu_disks),
                mActivity.getResources().getString(R.string.spacer),
                mActivity.getResources().getString(R.string.reboot),
        };

        ArrayAdapter<?> adapter = new ArrayAdapter<String>(mActivity, android.R.layout.simple_list_item_1, android.R.id.text1, values) {
            @Override
            public boolean areAllItemsEnabled() {
                return false;
            }
            @Override
            public boolean isEnabled(int position) {
                if (position < 0 || position > 3) {
                    throw new ArrayIndexOutOfBoundsException();
                }
                return position != 2;
            }
        };
        mainMenuView.setAdapter(adapter);
        mainMenuView.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                switch (position) {
                    case 0:
                        Apple2MainMenu.this.showSettings();
                        break;
                    case 1:
                        Apple2MainMenu.this.showDisksMenu();
                        break;
                    case 3:
                        Apple2MainMenu.this.reboot();
                        break;
                    default:
                        Apple2MainMenu.this.dismiss();
                        break;
                }
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
                if ( !(getSettingsMenu().isShowing() /*|| getDisksMenu().isShowing()*/) ) {
                    Apple2MainMenu.this.mActivity.nativeOnResume();
                }
            }
        });
    }

    public void showDisksMenu() {
        Log.d(TAG, "showDisksMenu...");
    }

    public void showSettings() {
        Apple2SettingsMenu settings = getSettingsMenu();
        settings.show();
        mMainMenuPopup.dismiss();
    }

    public void reboot() {
        mActivity.nativeReboot();
        mMainMenuPopup.dismiss();
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
