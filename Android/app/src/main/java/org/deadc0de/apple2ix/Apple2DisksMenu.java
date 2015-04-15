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
import android.graphics.drawable.Drawable;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.CheckBox;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.RadioButton;

import java.io.File;
import java.io.FilenameFilter;
import java.util.Arrays;

public class Apple2DisksMenu {

    private final static String TAG = "Apple2DisksMenu";

    private Apple2Activity mActivity = null;
    private Apple2View mParentView = null;
    private View mDisksView = null;

    public Apple2DisksMenu(Apple2Activity activity, Apple2View parent) {
        mActivity = activity;
        mParentView = parent;
        setup();
    }

    private void setup() {

        LayoutInflater inflater = (LayoutInflater) mActivity.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        mDisksView = inflater.inflate(R.layout.activity_disks, null, false);

        RadioButton diskA = (RadioButton) mDisksView.findViewById(R.id.radioButton_diskA);
        diskA.setChecked(true);

        CheckBox readWrite = (CheckBox) mDisksView.findViewById(R.id.checkBox_readWrite);
        readWrite.setChecked(false);
    }

    private void dynamicSetup() {

        final ListView disksList = (ListView)mDisksView.findViewById(R.id.listView_settings);
        disksList.setEnabled(true);

        String dataDir = mActivity.getDataDir() + File.separator + "disks";
        File dir = new File(dataDir);

        final File[] files = dir.listFiles(new FilenameFilter() {
            public boolean accept(File dir, String name) {
                name = name.toLowerCase();
                boolean acceptable = name.endsWith(".dsk") || name.endsWith(".do") || name.endsWith(".po") || name.endsWith(".nib") ||
                        name.endsWith(".dsk.gz") || name.endsWith(".do.gz") || name.endsWith(".po.gz") || name.endsWith(".nib.gz");
                return acceptable;
            }
        });

        Arrays.sort(files);
        int len = files.length;
        final String[] fileNames = new String[len];
        final boolean[] isDirectory = new boolean[len];

        for (int i=0; i<files.length; i++) {
            File file = files[i];
            fileNames[i] = file.getName();
            isDirectory[i] = file.isDirectory();
        }

        ArrayAdapter<?> adapter = new ArrayAdapter<String>(mActivity, R.layout.a2disk, R.id.a2disk_title, fileNames) {
            @Override
            public boolean areAllItemsEnabled() {
                return true;
            }
            @Override
            public View getView(int position, View convertView, ViewGroup parent) {
                View view = super.getView(position, convertView, parent);

                LinearLayout layout = (LinearLayout)view.findViewById(R.id.a2disk_widget_frame);
                if (layout.getChildCount() > 0) {
                    // layout cells appear to be reused when scrolling into view ... make sure we start with clear hierarchy
                    layout.removeAllViews();
                }

                if (isDirectory[position]) {
                    ImageView imageView = new ImageView(mActivity);
                    Drawable drawable = mActivity.getResources().getDrawable(android.R.drawable.ic_menu_more);
                    imageView.setImageDrawable(drawable);
                    layout.addView(imageView);
                }
                return view;
            }
        };

        disksList.setAdapter(adapter);
        disksList.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                if (isDirectory[position]) {
                    // TODO FIXME ...
                } else {
                    RadioButton diskA = (RadioButton)mDisksView.findViewById(R.id.radioButton_diskA);
                    CheckBox readWrite = (CheckBox)mDisksView.findViewById(R.id.checkBox_readWrite);
                    mActivity.nativeChooseDisk(files[position].getAbsolutePath(), diskA.isChecked(), !readWrite.isChecked());
                    Apple2DisksMenu.this.dismissWithoutResume();
                    Apple2DisksMenu.this.mActivity.getView().showMainMenu();
                }
            }
        });
    }

    public void show() {
        if (isShowing()) {
            return;
        }
        dynamicSetup();
        mActivity.nativeOnPause();
        mActivity.addContentView(mDisksView, new FrameLayout.LayoutParams(mActivity.getWidth(), mActivity.getHeight()));
    }

    public void dismiss() {
        if (isShowing()) {
            dismissWithoutResume();
            mActivity.nativeOnResume(/*isSystemResume:*/false);
        }
    }

    public void dismissWithoutResume() {
        if (isShowing()) {
            ((ViewGroup)mDisksView.getParent()).removeView(mDisksView);
            // HACK FIXME TODO ... we seem to lose ability to toggle/show soft keyboard upon dismissal of mDisksView after use.
            // This hack appears to get the Android UI unwedged ... =P
            Apple2MainMenu androidUIFTW = mParentView.getMainMenu();
            androidUIFTW.show();
            androidUIFTW.dismiss();
        }
    }

    public boolean isShowing() {
        return mDisksView.isShown();
    }
}
