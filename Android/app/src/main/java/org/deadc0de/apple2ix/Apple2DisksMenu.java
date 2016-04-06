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
import android.graphics.drawable.Drawable;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.RadioButton;

import org.json.JSONArray;
import org.json.JSONException;

import java.io.File;
import java.io.FilenameFilter;
import java.util.ArrayList;
import java.util.Arrays;

import org.deadc0de.apple2ix.basic.R;

public class Apple2DisksMenu implements Apple2MenuView {

    private final static String TAG = "Apple2DisksMenu";

    private Apple2Activity mActivity = null;
    private View mDisksView = null;

    private final ArrayList<String> mPathStack = new ArrayList<String>();

    private static boolean sInitializedPath = false;

    private static native void nativeChooseDisk(String path, boolean driveA, boolean readOnly);

    private static native void nativeEjectDisk(boolean driveA);

    protected enum SETTINGS implements Apple2AbstractMenu.IMenuEnum {
        CURRENT_DISK_SEARCH_PATH {
            @Override
            public String getPrefKey() {
                return "diskPathStack";
            }

            @Override
            public Object getPrefDefault() {
                return new JSONArray();
            }
        },
        CURRENT_DRIVE_A {
            @Override
            public String getPrefKey() {
                return "driveACurrent";
            }

            @Override
            public Object getPrefDefault() {
                return true;
            }
        },
        CURRENT_DISK_RO_BUTTON {
            @Override
            public String getPrefKey() {
                return "driveACurrent";
            }

            @Override
            public Object getPrefDefault() {
                return true;
            }
        },
        CURRENT_DISK_PATH_A {
            @Override
            public String getPrefKey() {
                return "driveAInsertedDisk";
            }

            @Override
            public Object getPrefDefault() {
                return "";
            }
        },
        CURRENT_DISK_PATH_A_RO {
            @Override
            public String getPrefKey() {
                return "driveAInsertedDiskRO";
            }

            @Override
            public Object getPrefDefault() {
                return true;
            }
        },
        CURRENT_DISK_PATH_B {
            @Override
            public String getPrefKey() {
                return "driveBInsertedDisk";
            }

            @Override
            public Object getPrefDefault() {
                return "";
            }
        },
        CURRENT_DISK_PATH_B_RO {
            @Override
            public String getPrefKey() {
                return "driveBInsertedDiskRO";
            }

            @Override
            public Object getPrefDefault() {
                return true;
            }
        };

        @Override
        public final String getTitle(Apple2Activity activity) {
            throw new RuntimeException();
        }

        @Override
        public final String getSummary(Apple2Activity activity) {
            throw new RuntimeException();
        }

        @Override
        public String getPrefDomain() {
            return Apple2Preferences.PREF_DOMAIN_VM;
        }

        @Override
        public View getView(final Apple2Activity activity, View convertView) {
            throw new RuntimeException();
        }

        @Override
        public void handleSelection(final Apple2Activity activity, final Apple2AbstractMenu settingsMenu, boolean isChecked) {
            throw new RuntimeException();
        }
    }

    public Apple2DisksMenu(Apple2Activity activity) {
        mActivity = activity;

        LayoutInflater inflater = (LayoutInflater) mActivity.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        mDisksView = inflater.inflate(R.layout.activity_disks, null, false);

        final Button cancelButton = (Button) mDisksView.findViewById(R.id.cancelButton);
        cancelButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Apple2DisksMenu.this.mActivity.dismissAllMenus();
            }
        });

        Apple2Utils.getExternalStorageDirectory(activity);
    }

    // ------------------------------------------------------------------------
    // Apple2MenuView interface methods

    public final boolean isCalibrating() {
        return false;
    }

    public void onKeyTapCalibrationEvent(char ascii, int scancode) {
        /* ... */
    }

    public void show() {
        if (isShowing()) {
            return;
        }
        if (!sInitializedPath) {
            sInitializedPath = true;
            setPathStackJSON((JSONArray) Apple2Preferences.getJSONPref(SETTINGS.CURRENT_DISK_SEARCH_PATH));
        }
        dynamicSetup();
        mActivity.pushApple2View(this);
    }

    public void dismiss() {
        String path = popPathStack();
        if (path == null) {
            mActivity.popApple2View(this);
        } else {
            dynamicSetup();
            ListView disksList = (ListView) mDisksView.findViewById(R.id.listView_settings);
            disksList.postInvalidate();
        }
    }

    public void dismissAll() {
        mActivity.popApple2View(this);
    }

    public boolean isShowing() {
        return mDisksView.getParent() != null;
    }

    public View getView() {
        return mDisksView;
    }

    // ------------------------------------------------------------------------
    // path stack methods

    public JSONArray getPathStackJSON() {
        return new JSONArray(Arrays.asList(mPathStack.toArray()));
    }

    public void setPathStackJSON(JSONArray jsonArray) {
        mPathStack.clear();
        try {
            for (int i = 0, count = jsonArray.length(); i < count; i++) {
                String pathComponent = jsonArray.getString(i);
                mPathStack.add(pathComponent);
            }
        } catch (JSONException e) {
            e.printStackTrace();
        }
    }

    public void pushPathStack(String path) {
        mPathStack.add(path);
        Apple2Preferences.setJSONPref(SETTINGS.CURRENT_DISK_SEARCH_PATH, getPathStackJSON());
    }

    public String popPathStack() {
        if (mPathStack.size() == 0) {
            return null;
        }
        String path = mPathStack.remove(mPathStack.size() - 1);
        Apple2Preferences.setJSONPref(SETTINGS.CURRENT_DISK_SEARCH_PATH, getPathStackJSON());
        return path;
    }

    public static void insertDisk(String fullPath, boolean isDriveA, boolean isReadOnly) {
        File file = new File(fullPath);
        final String imageName = fullPath;
        if (!file.exists()) {
            fullPath = fullPath + ".gz";
            file = new File(fullPath);
        }
        if (file.exists()) {
            if (isDriveA) {
                Apple2Preferences.setJSONPref(SETTINGS.CURRENT_DISK_PATH_A_RO, isReadOnly);
                Apple2Preferences.setJSONPref(SETTINGS.CURRENT_DISK_PATH_A, imageName);
            } else {
                Apple2Preferences.setJSONPref(SETTINGS.CURRENT_DISK_PATH_B_RO, isReadOnly);
                Apple2Preferences.setJSONPref(SETTINGS.CURRENT_DISK_PATH_B, imageName);
            }
            nativeChooseDisk(fullPath, isDriveA, isReadOnly);
        } else {
            Log.d(TAG, "Cannot insert: " + fullPath);
        }
    }


    public static boolean hasDiskExtension(String name) {

        // check file extensions ... sigh ... no String.endsWithIgnoreCase() ?

        final int len = name.length();
        if (len <= 3) {
            return false;
        }

        String suffix;
        suffix = name.substring(len - 3, len);
        if (suffix.equalsIgnoreCase(".do") || suffix.equalsIgnoreCase(".po")) {
            return true;
        }

        if (len <= 4) {
            return false;
        }

        suffix = name.substring(len - 4, len);
        if (suffix.equalsIgnoreCase(".dsk") || suffix.equalsIgnoreCase(".nib")) {
            return true;
        }

        if (len <= 6) {
            return false;
        }

        suffix = name.substring(len - 6, len);
        if (suffix.equalsIgnoreCase(".do.gz") || suffix.equalsIgnoreCase(".po.gz")) {
            return true;
        }

        if (len <= 7) {
            return false;
        }

        suffix = name.substring(len - 7, len);
        return (suffix.equalsIgnoreCase(".dsk.gz") || suffix.equalsIgnoreCase(".nib.gz"));
    }

    // ------------------------------------------------------------------------
    // internals ...

    private String pathStackAsDirectory() {
        if (mPathStack.size() == 0) {
            return null;
        }
        StringBuilder pathBuffer = new StringBuilder();
        for (String component : mPathStack) {
            pathBuffer.append(component);
            pathBuffer.append(File.separator);
        }
        return pathBuffer.toString();
    }

    private void dynamicSetup() {

        final ListView disksList = (ListView) mDisksView.findViewById(R.id.listView_settings);
        disksList.setEnabled(true);

        String disksDir = pathStackAsDirectory();
        boolean isRootPath = false;
        if (disksDir == null) {
            isRootPath = true;
            disksDir = Apple2Utils.getDataDir(mActivity) + File.separator + "disks"; // default path
        }

        File dir = new File(disksDir);

        final File[] files = dir.listFiles(new FilenameFilter() {
            public boolean accept(File dir, String name) {
                name = name.toLowerCase();
                if (name.equals(".")) {
                    return false;
                }
                if (name.equals("..")) {
                    return false;
                }
                File file = new File(dir, name);
                return file.isDirectory() || hasDiskExtension(name);
            }
        });

        // This appears to happen in cases where the external files directory String is valid, but is not actually mounted
        // We could probably check for more media "states" in the setup above ... but this defensive coding probably should
        // remain here after any refactoring =)
        if (files == null) {
            dismiss();
            return;
        }

        Arrays.sort(files);

        File extStorageDir = Apple2Utils.getExternalStorageDirectory(mActivity);
        File downloadsDir = Apple2Utils.getDownloadsDirectory(mActivity);
        final boolean includeExternalStoragePath = (extStorageDir != null && isRootPath);
        final boolean includeDownloadsPath = (downloadsDir != null && isRootPath);
        final int offset = includeExternalStoragePath ? (includeDownloadsPath ? 2 : 1) : (includeDownloadsPath ? 1 : 0);
        final String[] fileNames = new String[files.length + offset];
        final boolean[] isDirectory = new boolean[files.length + offset];

        int idx = 0;
        if (includeExternalStoragePath) {
            fileNames[idx] = Apple2Utils.getExternalStorageDirectory(mActivity).getAbsolutePath();
            isDirectory[idx] = true;
            ++idx;
        }
        if (includeDownloadsPath) {
            fileNames[idx] = downloadsDir.getAbsolutePath();
            isDirectory[idx] = true;
            ++idx;
        }

        for (File file : files) {
            isDirectory[idx] = file.isDirectory();
            fileNames[idx] = file.getName();
            if (isDirectory[idx]) {
                fileNames[idx] += File.separator;
            }
            ++idx;
        }

        ArrayAdapter<?> adapter = new ArrayAdapter<String>(mActivity, R.layout.a2disk, R.id.a2disk_title, fileNames) {
            @Override
            public boolean areAllItemsEnabled() {
                return true;
            }

            @Override
            public View getView(int position, View convertView, ViewGroup parent) {
                View view = super.getView(position, convertView, parent);

                LinearLayout layout = (LinearLayout) view.findViewById(R.id.a2disk_widget_frame);
                if (layout.getChildCount() > 0) {
                    // layout cells appear to be reused when scrolling into view ... make sure we start with clear hierarchy
                    layout.removeAllViews();
                }

                if (isDirectory[position]) {
                    ImageView imageView = new ImageView(mActivity);
                    Drawable drawable = mActivity.getResources().getDrawable(android.R.drawable.ic_menu_more);
                    imageView.setImageDrawable(drawable);
                    layout.addView(imageView);
                } else {

                    String imageName = files[position - offset].getAbsolutePath();
                    final int len = imageName.length();
                    final String suffix = imageName.substring(len - 3, len);
                    if (suffix.equalsIgnoreCase(".gz")) {
                        imageName = files[position - offset].getAbsolutePath().substring(0, len - 3);
                    }

                    String eject = mActivity.getResources().getString(R.string.disk_eject);
                    if (imageName.equals((String) Apple2Preferences.getJSONPref(SETTINGS.CURRENT_DISK_PATH_A))) {
                        Button ejectButton = new Button(mActivity);
                        ejectButton.setText(eject + " 1");
                        ejectButton.setOnClickListener(new View.OnClickListener() {
                            @Override
                            public void onClick(View v) {
                                nativeEjectDisk(/*driveA:*/true);
                                Apple2Preferences.setJSONPref(SETTINGS.CURRENT_DISK_PATH_A, "");
                                dynamicSetup();
                            }
                        });
                        layout.addView(ejectButton);
                    } else if (imageName.equals((String) Apple2Preferences.getJSONPref(SETTINGS.CURRENT_DISK_PATH_B))) {
                        Button ejectButton = new Button(mActivity);
                        ejectButton.setText(eject + " 2");
                        ejectButton.setOnClickListener(new View.OnClickListener() {
                            @Override
                            public void onClick(View v) {
                                nativeEjectDisk(/*driveA:*/false);
                                Apple2Preferences.setJSONPref(SETTINGS.CURRENT_DISK_PATH_B, "");
                                dynamicSetup();
                            }
                        });
                        layout.addView(ejectButton);
                    }

                }
                return view;
            }
        };

        final String parentDisksDir = disksDir;
        final boolean parentIsRootPath = isRootPath;

        disksList.setAdapter(adapter);
        disksList.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, final int position, long id) {
                if (isDirectory[position]) {
                    Log.d(TAG, "Descending to path : " + fileNames[position]);
                    if (parentIsRootPath && !new File(fileNames[position]).isAbsolute()) {
                        pushPathStack(parentDisksDir + File.separator + fileNames[position]);
                    } else {
                        pushPathStack(fileNames[position]);
                    }
                    dynamicSetup();
                    ListView disksList = (ListView) mDisksView.findViewById(R.id.listView_settings);
                    disksList.postInvalidate();
                    return;
                }

                String str = files[position - offset].getAbsolutePath();
                final int len = str.length();
                final String suffix = str.substring(len - 3, len);
                if (suffix.equalsIgnoreCase(".gz")) {
                    str = files[position - offset].getAbsolutePath().substring(0, len - 3);
                }
                final String imageName = str;

                if (imageName.equals((String) Apple2Preferences.getJSONPref(SETTINGS.CURRENT_DISK_PATH_A))) {
                    nativeEjectDisk(/*driveA:*/true);
                    Apple2Preferences.setJSONPref(SETTINGS.CURRENT_DISK_PATH_A, "");
                    dynamicSetup();
                    return;
                }
                if (imageName.equals((String) Apple2Preferences.getJSONPref(SETTINGS.CURRENT_DISK_PATH_B))) {
                    nativeEjectDisk(/*driveA:*/false);
                    Apple2Preferences.setJSONPref(SETTINGS.CURRENT_DISK_PATH_B, "");
                    dynamicSetup();
                    return;
                }

                String title = mActivity.getResources().getString(R.string.header_disks);
                title = title + " " + fileNames[position];

                AlertDialog.Builder builder = new AlertDialog.Builder(mActivity).setCancelable(true).setMessage(title);
                LayoutInflater inflater = (LayoutInflater) mActivity.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
                final View diskConfirmationView = inflater.inflate(R.layout.a2disk_confirmation, null, false);
                builder.setView(diskConfirmationView);

                final RadioButton driveA = (RadioButton) diskConfirmationView.findViewById(R.id.radioButton_diskA);
                boolean driveAChecked = (boolean) Apple2Preferences.getJSONPref(SETTINGS.CURRENT_DRIVE_A);
                driveA.setChecked(driveAChecked);
                driveA.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
                    @Override
                    public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                        Apple2Preferences.setJSONPref(SETTINGS.CURRENT_DRIVE_A, isChecked);
                    }
                });
                final RadioButton driveB = (RadioButton) diskConfirmationView.findViewById(R.id.radioButton_diskB);
                driveB.setChecked(!driveAChecked);


                final RadioButton readOnly = (RadioButton) diskConfirmationView.findViewById(R.id.radioButton_readOnly);
                boolean roChecked = (boolean) Apple2Preferences.getJSONPref(SETTINGS.CURRENT_DISK_RO_BUTTON);
                readOnly.setChecked(roChecked);
                readOnly.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
                    @Override
                    public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                        Apple2Preferences.setJSONPref(SETTINGS.CURRENT_DISK_RO_BUTTON, isChecked);
                    }
                });

                final RadioButton readWrite = (RadioButton) diskConfirmationView.findViewById(R.id.radioButton_readWrite);
                readWrite.setChecked(!roChecked);

                builder.setNegativeButton(R.string.cancel, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        dialog.dismiss();
                    }
                });
                builder.setPositiveButton(R.string.ok, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        boolean isDriveA = driveA.isChecked();
                        boolean diskReadOnly = readOnly.isChecked();
                        if (isDriveA) {
                            insertDisk(imageName, /*driveA:*/true, diskReadOnly);
                        } else {
                            insertDisk(imageName, /*driveA:*/false, diskReadOnly);
                        }
                        dialog.dismiss();
                        mActivity.dismissAllMenus();
                    }
                });

                AlertDialog dialog = builder.create();
                mActivity.registerAndShowDialog(dialog);
            }
        });
    }
}
