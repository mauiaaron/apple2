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

import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Build;
import androidx.appcompat.app.AlertDialog;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.RadioButton;
import android.widget.TextView;
import android.widget.Toast;

import org.deadc0de.apple2ix.basic.R;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.io.FilenameFilter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;

public class Apple2DisksMenu implements Apple2MenuView {

    private final static String TAG = "Apple2DisksMenu";

    public final static String EXTERNAL_CHOOSER_SENTINEL = "apple2ix://";

    private Apple2Activity mActivity = null;
    private View mDisksView = null;

    private final ArrayList<String> mPathStack = new ArrayList<String>();

    private static boolean sInitializedPath = false;

    private static native String nativeChooseDisk(String jsonData);

    private static native void nativeEjectDisk(boolean isDriveA);

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
                return "driveRO";
            }

            @Override
            public Object getPrefDefault() {
                return false;
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
                return false;
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
                return false;
            }
        },
        USE_NEWSCHOOL_DISK_SELECTION {
            @Override
            public String getPrefKey() {
                return "useNewSchoolDiskSelection";
            }

            @Override
            public Object getPrefDefault() {
                // 2017/06/30 NOTE : keep this default false to accommodate initial installs that only have access to shipped public domain images
                return false;
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

        for (int i = 0; i < 2; i++) {
            final Button ejectButton = (Button) mDisksView.findViewById(i == 0 ? R.id.ejectButton1 : R.id.ejectButton2);
            final int idx = i;
            ejectButton.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    ejectDisk(/*isDriveA:*/idx == 0);
                    dynamicSetup();
                }
            });
        }

        final CheckBox newschoolSelection = (CheckBox) mDisksView.findViewById(R.id.newschoolDiskSelectionButton);
        newschoolSelection.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Apple2Preferences.setJSONPref(SETTINGS.USE_NEWSCHOOL_DISK_SELECTION, newschoolSelection.isChecked());
                dynamicSetup();
            }
        });

        final View newschoolChooser = mDisksView.findViewById(R.id.disk_selection_newschool_chooser);
        newschoolChooser.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                final boolean alreadyChoosing = Apple2DiskChooserActivity.sDiskChooserIsChoosing.getAndSet(true);
                if (alreadyChoosing) {
                    return;
                }

                Intent chooserIntent = new Intent(mActivity, Apple2DiskChooserActivity.class);
                chooserIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION | Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION/* | Intent.FLAG_ACTIVITY_CLEAR_TOP */);

                Apple2DiskChooserActivity.sDisksCallback = mActivity;
                mActivity.startActivity(chooserIntent);
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
        String path = null;
        if (!(boolean) Apple2Preferences.getJSONPref(SETTINGS.USE_NEWSCHOOL_DISK_SELECTION)) {
            path = popPathStack();
        }

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

    public static void insertDisk(Apple2Activity activity, DiskArgs diskArgs, boolean isDriveA, boolean isReadOnly, boolean onLaunch) {
        try {
            JSONObject map = new JSONObject();

            ejectDisk(isDriveA);

            String imageName = diskArgs.path;

            if (imageName == null) {
                imageName = EXTERNAL_CHOOSER_SENTINEL + diskArgs.uri.toString();
            }

            if (imageName.startsWith(EXTERNAL_CHOOSER_SENTINEL)) {
                if (!Apple2Utils.isExternalStorageAccessible(activity)) {
                    // disallow access if we cannot access external storage
                    throw new Exception("External storage not accessible");
                }
                if (diskArgs.pfd == null) {
                    if (diskArgs.uri == null) {
                        String uriString = imageName.substring(EXTERNAL_CHOOSER_SENTINEL.length());
                        diskArgs.uri = Uri.parse(uriString);
                    }
                    diskArgs.pfd = Apple2DiskChooserActivity.openFileDescriptorFromUri(activity, diskArgs.uri);
                }

                int fd = diskArgs.pfd.getFd(); // NPE thrown if diskArgs.pfd is null
                map.put("fd", fd);
            } else {
                File file = new File(imageName);
                if (!file.exists()) {
                    throw new RuntimeException("cannot insert : " + imageName);
                }
            }

            if (isDriveA) {
                Apple2Preferences.setJSONPref(SETTINGS.CURRENT_DISK_PATH_A_RO, isReadOnly);
                Apple2Preferences.setJSONPref(SETTINGS.CURRENT_DISK_PATH_A, imageName);
            } else {
                Apple2Preferences.setJSONPref(SETTINGS.CURRENT_DISK_PATH_B_RO, isReadOnly);
                Apple2Preferences.setJSONPref(SETTINGS.CURRENT_DISK_PATH_B, imageName);
            }

            map.put("disk", imageName);
            map.put("drive", isDriveA ? "0" : "1");
            map.put("readOnly", isReadOnly ? "true" : "false");

            String jsonString = nativeChooseDisk(map.toString());

            if (diskArgs.pfd != null) {
                try {
                    diskArgs.pfd.close();
                } catch (IOException ioe) {
                    Log.e(TAG, "Error attempting to close PFD : " + ioe);
                }
            }
            diskArgs.pfd = null;

            map = new JSONObject(jsonString);
            boolean inserted = map.getBoolean("inserted");
            if (!inserted) {
                ejectDisk(isDriveA);
            }

        } catch (Throwable t) {
            Log.d(TAG, "OOPS: " + t);
        }
    }

    public static void ejectDisk(boolean isDriveA) {
        if (isDriveA) {
            Apple2Preferences.setJSONPref(SETTINGS.CURRENT_DISK_PATH_A, "");
        } else {
            Apple2Preferences.setJSONPref(SETTINGS.CURRENT_DISK_PATH_B, "");
        }
        nativeEjectDisk(isDriveA);
    }

    public void showDiskInsertionAlertDialog(String title, final DiskArgs diskArgs) {

        title = mActivity.getResources().getString(R.string.header_disks) + " " + title;

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

                insertDisk(mActivity, diskArgs, isDriveA, diskReadOnly, /*onLaunch:*/false);

                dialog.dismiss();
                mActivity.dismissAllMenus();
            }
        });

        AlertDialog dialog = builder.create();
        mActivity.registerAndShowDialog(dialog);
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

    public static boolean hasStateExtension(String name) {

        // check file extensions ... sigh ... no String.endsWithIgnoreCase() ?

        final int extLen = Apple2MainMenu.SAVE_FILE_EXTENSION.length();
        final int len = name.length();
        if (len <= extLen) {
            return false;
        }

        final String suffix = name.substring(len - extLen, len);
        return suffix.equalsIgnoreCase(Apple2MainMenu.SAVE_FILE_EXTENSION);
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

        final boolean useNewschoolSelection = (boolean) Apple2Preferences.getJSONPref(SETTINGS.USE_NEWSCHOOL_DISK_SELECTION);

        final CheckBox newschoolSelection = (CheckBox) mDisksView.findViewById(R.id.newschoolDiskSelectionButton);
        newschoolSelection.setChecked(useNewschoolSelection);

        final boolean isKitKatOrBetter = (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT);

        final boolean includeExternalFileChooser = Apple2Utils.isExternalStorageAccessible(mActivity) && isKitKatOrBetter;

        final View newschoolChooser = mDisksView.findViewById(R.id.disk_selection_newschool_chooser);

        if (!includeExternalFileChooser || !useNewschoolSelection) {
            disksList.setEnabled(true);
            disksList.setVisibility(View.VISIBLE);
            newschoolChooser.setVisibility(View.INVISIBLE);

            for (int i = 0; i < 2; i++) {
                LinearLayout layout = (LinearLayout) mDisksView.findViewById((i == 0) ? R.id.a2_newschool_driveA_layout : R.id.a2_newschool_driveB_layout);
                layout.setVisibility(View.INVISIBLE);
            }

            if (!includeExternalFileChooser) {
                newschoolSelection.setVisibility(View.INVISIBLE);
            }
            Apple2Preferences.setJSONPref(SETTINGS.USE_NEWSCHOOL_DISK_SELECTION, false);
            oldschoolDynamicSetup();
            return;
        }

        disksList.setEnabled(false);
        disksList.setVisibility(View.INVISIBLE);
        newschoolChooser.setVisibility(View.VISIBLE);

        // new external file chooser activity can allow navigation to restricted external SD Card(s) ...
        newschoolSelection.setVisibility(View.VISIBLE);

        for (int i = 0; i < 2; i++) {
            String imageName = null;
            do {
                String diskPath = (String) Apple2Preferences.getJSONPref((i == 0) ? SETTINGS.CURRENT_DISK_PATH_A : SETTINGS.CURRENT_DISK_PATH_B);

                if (diskPath == null || diskPath.equals("")) {
                    break;
                }

                Uri uri = Uri.parse(diskPath);
                if (uri == null) {
                    break;
                }

                diskPath = uri.getPath();
                int idx = diskPath.lastIndexOf("/");
                if (idx < 0) {
                    break;
                }

                imageName = diskPath.substring(idx + 1);
            } while (false);

            LinearLayout layout = (LinearLayout) mDisksView.findViewById((i == 0) ? R.id.a2_newschool_driveA_layout : R.id.a2_newschool_driveB_layout);

            if (imageName == null || imageName.equals("")) {
                layout.setVisibility(View.INVISIBLE);
            } else {
                layout.setVisibility(View.VISIBLE);

                boolean readOnly = (boolean) Apple2Preferences.getJSONPref((i == 0) ? SETTINGS.CURRENT_DISK_PATH_A_RO : SETTINGS.CURRENT_DISK_PATH_B_RO);
                imageName = "(" + mActivity.getResources().getString((readOnly ? R.string.disk_read_only : R.string.disk_read_write)) + "): " + imageName;
                TextView textView = (TextView) mDisksView.findViewById((i == 0) ? R.id.a2_newschool_diskA : R.id.a2_newschool_diskB);
                textView.setText(imageName);
            }
        }
    }

    private void oldschoolDynamicSetup() {

        final ListView disksList = (ListView) mDisksView.findViewById(R.id.listView_settings);

        String disksDir = pathStackAsDirectory();
        final boolean isRootPath = (disksDir == null);
        if (isRootPath) {
            disksDir = Apple2Utils.getDataDir(mActivity) + File.separator + "disks"; // default path
        }

        while (disksDir.charAt(disksDir.length() - 1) == File.separatorChar) {
            disksDir = disksDir.substring(0, disksDir.length() - 1);
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
                return file.isDirectory() || hasDiskExtension(name) || hasStateExtension(name);
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

        final File realExtStorageDir = Apple2Utils.getRealExternalStorageDirectory(mActivity);
        final boolean isStoragePath = !isRootPath && (realExtStorageDir != null) && disksDir.equalsIgnoreCase(realExtStorageDir.getAbsolutePath());
        if (isStoragePath) {
            // promote apple2ix directory to top of list
            for (int i = 0; i < files.length; i++) {
                if (files[i].getName().equals("apple2ix")) {
                    if (i > 0) {
                        System.arraycopy(/*src:*/files, /*srcPos:*/0, /*dst:*/files, /*dstPos:*/1, /*length:*/i);
                        files[0] = new File(realExtStorageDir, "apple2ix");
                    }
                    break;
                }
            }
        }

        final boolean includeExternalStoragePath = (Apple2Utils.isExternalStorageAccessible(mActivity) && isRootPath);

        final int offset = includeExternalStoragePath ? 1 : 0;
        final String[] fileNames = new String[files.length + offset];
        final String[] filePaths = new String[files.length + offset];
        final boolean[] isDirectory = new boolean[files.length + offset];

        int idx = 0;

        if (includeExternalStoragePath) {
            filePaths[idx] = Apple2Utils.getRealExternalStorageDirectory(mActivity).getAbsolutePath();
            fileNames[idx] = mActivity.getResources().getString(R.string.storage);
            isDirectory[idx] = true;
            ++idx;
        }

        for (File file : files) {
            isDirectory[idx] = file.isDirectory();
            filePaths[idx] = file.getName();
            fileNames[idx] = filePaths[idx];
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
                    String eject = mActivity.getResources().getString(R.string.disk_eject);
                    if (imageName.equals((String) Apple2Preferences.getJSONPref(SETTINGS.CURRENT_DISK_PATH_A))) {
                        Button ejectButton = new Button(mActivity);
                        ejectButton.setText(eject + " 1");
                        ejectButton.setOnClickListener(new View.OnClickListener() {
                            @Override
                            public void onClick(View v) {
                                ejectDisk(/*driveA:*/true);
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
                                ejectDisk(/*driveA:*/false);
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
                    Log.d(TAG, "Descending to path : " + filePaths[position]);
                    if (parentIsRootPath && !new File(filePaths[position]).isAbsolute()) {
                        pushPathStack(parentDisksDir + File.separator + filePaths[position]);
                    } else {
                        pushPathStack(filePaths[position]);
                    }
                    dynamicSetup();
                    ListView disksList = (ListView) mDisksView.findViewById(R.id.listView_settings);
                    disksList.postInvalidate();
                    return;
                }

                final String imageName = files[position - offset].getAbsolutePath();
                if (imageName.equals((String) Apple2Preferences.getJSONPref(SETTINGS.CURRENT_DISK_PATH_A))) {
                    ejectDisk(/*isDriveA:*/true);
                    dynamicSetup();
                    return;
                }
                if (imageName.equals((String) Apple2Preferences.getJSONPref(SETTINGS.CURRENT_DISK_PATH_B))) {
                    ejectDisk(/*isDriveA:*/false);
                    dynamicSetup();
                    return;
                }

                if (hasStateExtension(imageName)) {
                    final String jsonString = "{ \"stateFile\" : \"" + imageName + "\" }";
                    final boolean restored = Apple2MainMenu.restoreEmulatorState(mActivity, jsonString);
                    mActivity.dismissAllMenus();
                    if (!restored) {
                        Toast.makeText(mActivity, R.string.state_not_restored, Toast.LENGTH_SHORT).show();
                    }
                    return;
                }

                showDiskInsertionAlertDialog(/*title:*/fileNames[position], /*diskPath:*/new DiskArgs(imageName));
            }
        });
    }
}
