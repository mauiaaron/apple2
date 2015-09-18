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

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.graphics.drawable.Drawable;
import android.os.Environment;
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
import java.io.FileOutputStream;
import java.io.FilenameFilter;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Arrays;

import org.deadc0de.apple2ix.basic.R;

public class Apple2DisksMenu implements Apple2MenuView {

    private final static String TAG = "Apple2DisksMenu";
    private static String sDataDir = null;

    private Apple2Activity mActivity = null;
    private View mDisksView = null;

    private final ArrayList<String> mPathStack = new ArrayList<String>();

    private static File sExternalFilesDir = null;
    private static boolean sInitializedPath = false;

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

        getExternalStorageDirectory();
    }

    public static File getExternalStorageDirectory() {
        if (sExternalFilesDir == null) {
            String storageState = Environment.getExternalStorageState();
            File externalDir = new File(Environment.getExternalStorageDirectory().getPath() + File.separator + "apple2ix"); // /sdcard/apple2ix
            sExternalFilesDir = null;
            boolean externalStorageAvailable = storageState.equals(Environment.MEDIA_MOUNTED);
            if (externalStorageAvailable) {
                sExternalFilesDir = externalDir;
                boolean made = sExternalFilesDir.mkdirs();
                if (!made) {
                    Log.d(TAG, "WARNING: could not make directory : " + sExternalFilesDir);
                }
            } else {
                sExternalFilesDir = externalDir;
            }
        }
        return sExternalFilesDir;
    }

    // HACK NOTE 2015/02/22 : Apparently native code cannot easily access stuff in the APK ... so copy various resources
    // out of the APK and into the /data/data/... for ease of access.  Because this is FOSS software we don't care about
    // security or DRM for these assets =)
    public static String getDataDir(Apple2Activity activity) {

        if (sDataDir != null) {
            return sDataDir;
        }

        try {
            PackageManager pm = activity.getPackageManager();
            PackageInfo pi = pm.getPackageInfo(activity.getPackageName(), 0);
            sDataDir = pi.applicationInfo.dataDir;
        } catch (PackageManager.NameNotFoundException e) {
            Log.e(TAG, "" + e);
            if (sDataDir == null) {
                sDataDir = "/data/local/tmp";
            }
        }

        return sDataDir;
    }

    public static void firstTime(Apple2Activity activity) {
        getDataDir(activity);

        Log.d(TAG, "First time copying stuff-n-things out of APK for ease-of-NDK access...");

        getExternalStorageDirectory();

        try {
            String[] shaders = activity.getAssets().list("shaders");
            for (String shader : shaders) {
                Apple2DisksMenu.copyFile(activity, "shaders", shader, sDataDir + File.separator + "shaders");
            }

            String[] disks = activity.getAssets().list("disks");
            for (String disk : disks) {
                Apple2DisksMenu.copyFile(activity, "disks", disk, sDataDir + File.separator + "disks");
            }

            String[] keyboards = activity.getAssets().list("keyboards");
            for (String kbd : keyboards) {
                Apple2DisksMenu.copyFile(activity, "keyboards", kbd, sDataDir + File.separator + "keyboards");
            }

        } catch (IOException e) {
            Log.e(TAG, "problem copying resources : " + e);
            throw new RuntimeException("This should not happen");
        }

        try {
            String[] keyboards = activity.getAssets().list("keyboards");
            for (String kbd : keyboards) {
                Apple2DisksMenu.copyFile(activity, "keyboards", kbd, sDataDir + File.separator + "keyboards");
                if (sExternalFilesDir != null) {
                    Apple2DisksMenu.copyFile(activity, "keyboards", kbd, sExternalFilesDir.getPath());
                }
            }
        } catch (IOException e) {
            Log.e(TAG, "problem copying keyboards to sdcard : " + e);
            // non-fatal
        }
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
            Apple2Preferences.CURRENT_DISK_PATH.load(mActivity);
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
        return mDisksView.isShown();
    }

    public View getView() {
        return mDisksView;
    }

    // ------------------------------------------------------------------------
    // path stack methods

    public String getPathStackJSON() {
        JSONArray jsonArray = new JSONArray(Arrays.asList(mPathStack.toArray()));
        return jsonArray.toString();
    }

    public void setPathStackJSON(String pathStackJSON) {
        mPathStack.clear();
        try {
            JSONArray jsonArray = new JSONArray(pathStackJSON);
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
        Apple2Preferences.CURRENT_DISK_PATH.saveString(mActivity, getPathStackJSON());
    }

    public String popPathStack() {
        if (mPathStack.size() == 0) {
            return null;
        }
        String path = mPathStack.remove(mPathStack.size() - 1);
        Apple2Preferences.CURRENT_DISK_PATH.saveString(mActivity, getPathStackJSON());
        return path;
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

    private static void copyFile(Apple2Activity activity, String subdir, String assetName, String outputPath)
            throws IOException {
        Log.d(TAG, "Copying " + subdir + File.separator + assetName + " to " + outputPath + File.separator + assetName + " ...");
        boolean made = new File(outputPath).mkdirs();
        if (!made) {
            Log.d(TAG, "WARNING, cannot mkdirs on path : " + outputPath);
        }

        InputStream is = activity.getAssets().open(subdir + File.separator + assetName);
        File file = new File(outputPath + File.separator + assetName);
        boolean madeWriteable = file.setWritable(true);
        if (!madeWriteable) {
            Log.d(TAG, "WARNING, cannot make a copied file writeable for asset : " + assetName);
        }
        FileOutputStream os = new FileOutputStream(file);

        final int BUF_SZ = 4096;
        byte[] buf = new byte[BUF_SZ];
        while (true) {
            int len = is.read(buf, 0, BUF_SZ);
            if (len < 0) {
                break;
            }
            os.write(buf, 0, len);
        }
        os.flush();
        os.close();
    }

    private void dynamicSetup() {

        final ListView disksList = (ListView) mDisksView.findViewById(R.id.listView_settings);
        disksList.setEnabled(true);

        String disksDir = pathStackAsDirectory();
        boolean isRootPath = false;
        if (disksDir == null) {
            isRootPath = true;
            disksDir = sDataDir + File.separator + "disks"; // default path
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
                if (file.isDirectory()) {
                    return true;
                }

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
        });

        // This appears to happen in cases where the external files directory String is valid, but is not actually mounted
        // We could probably check for more media "states" in the setup above ... but this defensive coding probably should
        // remain here after any refactoring =)
        if (files == null) {
            dismiss();
            return;
        }

        Arrays.sort(files);

        getExternalStorageDirectory();
        final boolean includeExternalStoragePath = (sExternalFilesDir != null && isRootPath);
        final int offset = includeExternalStoragePath ? 1 : 0;
        final String[] fileNames = new String[files.length + offset];
        final boolean[] isDirectory = new boolean[files.length + offset];

        if (includeExternalStoragePath) {
            fileNames[0] = sExternalFilesDir.getPath();
            isDirectory[0] = true;
        }

        int idx = offset;
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
                    if (imageName.equals(Apple2Preferences.CURRENT_DISK_A.stringValue(mActivity))) {
                        Button ejectButton = new Button(mActivity);
                        ejectButton.setText(eject + " 1");
                        ejectButton.setOnClickListener(new View.OnClickListener() {
                            @Override
                            public void onClick(View v) {
                                mActivity.nativeEjectDisk(/*driveA:*/true);
                                Apple2Preferences.CURRENT_DISK_A.saveString(mActivity, "");
                                dynamicSetup();
                            }
                        });
                        layout.addView(ejectButton);
                    } else if (imageName.equals(Apple2Preferences.CURRENT_DISK_B.stringValue(mActivity))) {
                        Button ejectButton = new Button(mActivity);
                        ejectButton.setText(eject + " 2");
                        ejectButton.setOnClickListener(new View.OnClickListener() {
                            @Override
                            public void onClick(View v) {
                                mActivity.nativeEjectDisk(/*driveA:*/false);
                                Apple2Preferences.CURRENT_DISK_B.saveString(mActivity, "");
                                dynamicSetup();
                            }
                        });
                        layout.addView(ejectButton);
                    }

                }
                return view;
            }
        };

        disksList.setAdapter(adapter);
        disksList.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, final int position, long id) {
                if (isDirectory[position]) {
                    Log.d(TAG, "Descending to path : " + fileNames[position]);
                    pushPathStack(fileNames[position]);
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

                if (imageName.equals(Apple2Preferences.CURRENT_DISK_A.stringValue(mActivity))) {
                    mActivity.nativeEjectDisk(/*driveA:*/true);
                    Apple2Preferences.CURRENT_DISK_A.saveString(mActivity, "");
                    dynamicSetup();
                    return;
                }
                if (imageName.equals(Apple2Preferences.CURRENT_DISK_B.stringValue(mActivity))) {
                    mActivity.nativeEjectDisk(/*driveA:*/false);
                    Apple2Preferences.CURRENT_DISK_B.saveString(mActivity, "");
                    dynamicSetup();
                    return;

                }

                String title = mActivity.getResources().getString(R.string.header_disks);
                title = title + " " + fileNames[position];

                AlertDialog.Builder builder = new AlertDialog.Builder(mActivity).setCancelable(true).setMessage(title);
                LayoutInflater inflater = (LayoutInflater) mActivity.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
                final View diskConfirmationView = inflater.inflate(R.layout.a2disk_confirmation, null, false);
                builder.setView(diskConfirmationView);

                final RadioButton diskA = (RadioButton) diskConfirmationView.findViewById(R.id.radioButton_diskA);
                diskA.setChecked(Apple2Preferences.CURRENT_DRIVE_A_BUTTON.booleanValue(mActivity));
                diskA.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
                    @Override
                    public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                        Apple2Preferences.CURRENT_DRIVE_A_BUTTON.saveBoolean(mActivity, isChecked);
                    }
                });
                final RadioButton diskB = (RadioButton) diskConfirmationView.findViewById(R.id.radioButton_diskB);
                diskB.setChecked(!Apple2Preferences.CURRENT_DRIVE_A_BUTTON.booleanValue(mActivity));


                final RadioButton readOnly = (RadioButton) diskConfirmationView.findViewById(R.id.radioButton_readOnly);
                readOnly.setChecked(Apple2Preferences.CURRENT_DISK_RO_BUTTON.booleanValue(mActivity));
                readOnly.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
                    @Override
                    public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                        Apple2Preferences.CURRENT_DISK_RO_BUTTON.saveBoolean(mActivity, isChecked);
                    }
                });

                final RadioButton readWrite = (RadioButton) diskConfirmationView.findViewById(R.id.radioButton_readWrite);
                readWrite.setChecked(!Apple2Preferences.CURRENT_DISK_RO_BUTTON.booleanValue(mActivity));

                builder.setNegativeButton(R.string.cancel, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        dialog.dismiss();
                    }
                });
                builder.setPositiveButton(R.string.ok, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        dialog.dismiss();
                        mActivity.dismissAllMenus();
                        boolean isDriveA = diskA.isChecked();
                        boolean diskReadOnly = readOnly.isChecked();
                        if (isDriveA) {
                            Apple2Preferences.CURRENT_DISK_A.saveString(mActivity, imageName);
                            Apple2Preferences.CURRENT_DISK_A_RO.saveBoolean(mActivity, diskReadOnly);
                        } else {
                            Apple2Preferences.CURRENT_DISK_B.saveString(mActivity, imageName);
                            Apple2Preferences.CURRENT_DISK_B_RO.saveBoolean(mActivity, diskReadOnly);
                        }

                        mActivity.nativeChooseDisk(imageName, isDriveA, diskReadOnly);
                    }
                });

                AlertDialog dialog = builder.create();
                mActivity.registerAndShowDialog(dialog);
            }
        });
    }
}
