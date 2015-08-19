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
import org.json.JSONObject;

import java.io.File;
import java.io.FileOutputStream;
import java.io.FilenameFilter;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Arrays;

public class Apple2DisksMenu implements Apple2MenuView {

    private final static String TAG = "Apple2DisksMenu";
    private static String sDataDir = null;

    private Apple2Activity mActivity = null;
    private View mDisksView = null;

    private final ArrayList<String> mPathStack = new ArrayList<>();

    private static File sExternalFilesDir = null;
    private static boolean sExternalStorageAvailable = false;
    private static boolean sInitializedPath = false;

    public Apple2DisksMenu(Apple2Activity activity) {
        mActivity = activity;

        if (!sInitializedPath) {
            sInitializedPath = true;
            Apple2Preferences.CURRENT_DISK_PATH.load(mActivity);
        }

        LayoutInflater inflater = (LayoutInflater) mActivity.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        mDisksView = inflater.inflate(R.layout.activity_disks, null, false);

        final Button cancelButton = (Button) mDisksView.findViewById(R.id.cancelButton);
        cancelButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Apple2DisksMenu.this.mActivity.dismissAllMenus();
            }
        });

        if (!sExternalStorageAvailable) {
            String storageState = Environment.getExternalStorageState();
            File externalDir = new File(Environment.getExternalStorageDirectory().getPath() + File.separator + "apple2ix"); // /sdcard/apple2ix
            sExternalFilesDir = null;
            sExternalStorageAvailable = storageState.equals(Environment.MEDIA_MOUNTED);
            if (sExternalStorageAvailable) {
                sExternalFilesDir = externalDir;
                boolean made = sExternalFilesDir.mkdirs();
                if (!made) {
                    Log.d(TAG, "WARNING: could not make directory : " + sExternalFilesDir);
                }
            } else {
                sExternalStorageAvailable = storageState.equals(Environment.MEDIA_MOUNTED_READ_ONLY) && externalDir.exists();
                sExternalFilesDir = externalDir;
            }
        }
    }

    // HACK NOTE 2015/02/22 : Apparently native code cannot easily access stuff in the APK ... so copy various resources
    // out of the APK and into the /data/data/... for ease of access.  Because this is FOSS software we don't care about
    // security or DRM for these assets =)
    public static String firstTimeAssetsInitialization(Apple2Activity activity) {

        try {
            PackageManager pm = activity.getPackageManager();
            PackageInfo pi = pm.getPackageInfo(activity.getPackageName(), 0);
            sDataDir = pi.applicationInfo.dataDir;
        } catch (PackageManager.NameNotFoundException e) {
            Log.e(TAG, "" + e);
            System.exit(1);
        }

        if (Apple2Preferences.ASSETS_CONFIGURED.booleanValue(activity)) {
            return sDataDir;
        }

        Log.d(TAG, "First time copying stuff-n-things out of APK for ease-of-NDK access...");

        try {
            String[] shaders = activity.getAssets().list("shaders");
            for (String shader : shaders) {
                Apple2DisksMenu.copyFile(activity, "shaders", shader);
            }
            String[] disks = activity.getAssets().list("disks");
            for (String disk : disks) {
                Apple2DisksMenu.copyFile(activity, "disks", disk);
            }
        } catch (IOException e) {
            Log.e(TAG, "problem copying resources : " + e);
            System.exit(1);
        }

        Log.d(TAG, "Saving default preferences");
        Apple2Preferences.ASSETS_CONFIGURED.saveBoolean(activity, true);

        return sDataDir;
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
                JSONObject jsonObject = jsonArray.getJSONObject(i);
                mPathStack.add(jsonObject.toString());
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

    private static void copyFile(Apple2Activity activity, String subdir, String assetName)
            throws IOException {
        String outputPath = sDataDir + File.separator + subdir;
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
                if (name.equalsIgnoreCase(".")) {
                    return false;
                }
                if (name.equalsIgnoreCase("..")) {
                    return false;
                }

                if (name.endsWith(".dsk") || name.endsWith(".do") || name.endsWith(".po") || name.endsWith(".nib") || name.endsWith(".dsk.gz") || name.endsWith(".do.gz") || name.endsWith(".po.gz") || name.endsWith(".nib.gz")) {
                    return true;
                }

                File file = new File(dir, name);
                return file.isDirectory();
            }
        });

        Arrays.sort(files);

        final boolean includeExternalStoragePath = (sExternalStorageAvailable && isRootPath);
        int idx = includeExternalStoragePath ? 1 : 0;
        final String[] fileNames = new String[files.length + idx];
        final boolean[] isDirectory = new boolean[files.length + idx];

        if (includeExternalStoragePath) {
            fileNames[0] = sExternalFilesDir.toString();
            isDirectory[0] = true;
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
                }
                return view;
            }
        };

        final int offset = includeExternalStoragePath ? 1 : 0;
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
                } else {
                    String title = mActivity.getResources().getString(R.string.header_disks);
                    title = title + " " + fileNames[position];

                    AlertDialog.Builder builder = new AlertDialog.Builder(mActivity).setCancelable(true).setMessage(title);
                    LayoutInflater inflater = (LayoutInflater) mActivity.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
                    final View diskConfirmationView = inflater.inflate(R.layout.a2disk_confirmation, null, false);
                    builder.setView(diskConfirmationView);

                    final RadioButton diskA = (RadioButton) diskConfirmationView.findViewById(R.id.radioButton_diskA);
                    diskA.setChecked(Apple2Preferences.CURRENT_DISK_A.booleanValue(mActivity));
                    diskA.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
                        @Override
                        public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                            Apple2Preferences.CURRENT_DISK_A.saveBoolean(mActivity, isChecked);
                        }
                    });
                    final RadioButton diskB = (RadioButton) diskConfirmationView.findViewById(R.id.radioButton_diskB);
                    diskB.setChecked(!Apple2Preferences.CURRENT_DISK_A.booleanValue(mActivity));


                    final RadioButton readOnly = (RadioButton) diskConfirmationView.findViewById(R.id.radioButton_readOnly);
                    readOnly.setChecked(Apple2Preferences.CURRENT_DISK_RO.booleanValue(mActivity));
                    readOnly.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
                        @Override
                        public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                            Apple2Preferences.CURRENT_DISK_RO.saveBoolean(mActivity, isChecked);
                        }
                    });

                    final RadioButton readWrite = (RadioButton) diskConfirmationView.findViewById(R.id.radioButton_readWrite);
                    readWrite.setChecked(!Apple2Preferences.CURRENT_DISK_RO.booleanValue(mActivity));

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
                            mActivity.nativeChooseDisk(files[position - offset].getAbsolutePath(), diskA.isChecked(), readOnly.isChecked());
                        }
                    });

                    builder.show();
                }
            }
        });
    }
}
