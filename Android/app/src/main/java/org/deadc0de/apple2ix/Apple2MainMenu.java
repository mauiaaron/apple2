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

    public final static int MENU_INSET = 20;

    private final static String TAG = "Apple2MainMenu";

    private Apple2Activity mActivity = null;
    private Apple2View mParentView = null;
    private PopupWindow mMainMenuPopup = null;

    public Apple2MainMenu(Apple2Activity activity, Apple2View parent) {
        mActivity = activity;
        mParentView = parent;
        init();
    }

    private void init() {

        LayoutInflater inflater = (LayoutInflater)mActivity.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        View listLayout=inflater.inflate(R.layout.activity_main_menu, null, false);
        ListView mainMenuView = (ListView)listLayout.findViewById(R.id.main_popup_menu);
        mainMenuView.setEnabled(true);
        LinearLayout mainPopupContainer = (LinearLayout)listLayout.findViewById(R.id.main_popup_container);
        String[] values = new String[] {
                "Emulation Settings...",
                "Load Disk Image...",
                "Resume...",
        };

        ArrayAdapter<?> adapter = new ArrayAdapter<String>(mActivity, android.R.layout.simple_list_item_1, android.R.id.text1, values);
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
                    default:
                        Apple2MainMenu.this.dismiss();
                        break;
                }
            }
        });

        // WTF ... is there an easier way to dynamically calculate these dimensions?
        int totalHeight = 0;
        int maxWidth = 0;
        for (int i=0; i<adapter.getCount(); i++) {
            View view = adapter.getView(i, null, mainMenuView);
            view.measure(View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED), View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED));
            totalHeight += view.getMeasuredHeight();
            int width = view.getMeasuredWidth();
            if (width > maxWidth) {
                maxWidth = width;
            }
        }

        mMainMenuPopup = new PopupWindow(mainPopupContainer, maxWidth+MENU_INSET, totalHeight+MENU_INSET, true);
    }

    public void showDisksMenu() {
        Log.d(TAG, "showDisksMenu...");
    }

    public void showSettings() {
        Log.d(TAG, "showSettings...");
    }

    public void quitEmulator() {

    }

    public void show() {
        if (mMainMenuPopup.isShowing()) {
            return;
        }

        mActivity.nativeOnPause();

        int x = (mActivity.getWidth()-mMainMenuPopup.getWidth())/2;
        int y = (mActivity.getHeight()-mMainMenuPopup.getHeight())/2;

        mMainMenuPopup.showAtLocation(mParentView, Gravity.NO_GRAVITY, x, y);
    }

    public void dismiss() {
        if (mMainMenuPopup.isShowing()) {
            mActivity.nativeOnResume();
            mMainMenuPopup.dismiss();
        }
    }

    public void androidBackButton() {
        dismiss();
    }

    public boolean isShowing() {
        return mMainMenuPopup.isShowing();
    }
}
