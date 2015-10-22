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
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.CheckedTextView;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.SeekBar;
import android.widget.TextView;

import org.deadc0de.apple2ix.basic.R;

public abstract class Apple2AbstractMenu implements Apple2MenuView {

    private final static String TAG = "Apple2AbstractMenu";

    protected Apple2Activity mActivity = null;
    private View mSettingsView = null;

    public Apple2AbstractMenu(Apple2Activity activity) {
        mActivity = activity;
        setup();
    }

    public synchronized void show() {
        if (isShowing()) {
            return;
        }
        mActivity.pushApple2View(this);
    }

    public void dismiss() {
        mActivity.popApple2View(this);
    }

    public void dismissAll() {
        this.dismiss();
    }

    public boolean isShowing() {
        return mSettingsView.isShown();
    }

    public View getView() {
        return mSettingsView;
    }

    public boolean isCalibrating() {
        return false;
    }

    public void onKeyTapCalibrationEvent(char ascii, int scancode) {
        /* ... */
    }

    // ------------------------------------------------------------------------
    // required overrides ...

    public interface IMenuEnum {
        public String getTitle(final Apple2Activity activity);

        public String getSummary(final Apple2Activity activity);

        public View getView(final Apple2Activity activity, View convertView);

        public void handleSelection(final Apple2Activity activity, final Apple2AbstractMenu settingsMenu, boolean isChecked);
    }

    public abstract IMenuEnum[] allValues();

    public abstract String[] allTitles();

    public abstract boolean areAllItemsEnabled();

    public abstract boolean isEnabled(int position);

    // ------------------------------------------------------------------------
    // boilerplate menu view code

    protected static View _basicView(Apple2Activity activity, IMenuEnum setting, View convertView) {
        TextView tv = (TextView) convertView.findViewById(R.id.a2preference_title);
        if (tv == null) {
            // attemping to recycle different layout ...
            LayoutInflater inflater = (LayoutInflater) activity.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
            convertView = inflater.inflate(R.layout.a2preference, null, false);
            tv = (TextView) convertView.findViewById(R.id.a2preference_title);
        }
        tv.setText(setting.getTitle(activity));

        tv = (TextView) convertView.findViewById(R.id.a2preference_summary);
        tv.setText(setting.getSummary(activity));

        LinearLayout layout = (LinearLayout) convertView.findViewById(R.id.a2preference_widget_frame);
        if (layout.getChildCount() > 0) {
            // layout cells appear to be reused when scrolling into view ... make sure we start with clear hierarchy
            layout.removeAllViews();
        }

        return convertView;
    }

    public interface IPreferenceLoadSave {
        public int intValue();

        public void saveInt(int value);
    }

    public interface IPreferenceSlider extends IPreferenceLoadSave {
        public void showValue(int value, final TextView seekBarValue);
    }

    protected static View _sliderView(final Apple2Activity activity, final IMenuEnum setting, final int numChoices, final IPreferenceSlider iLoadSave) {

        LayoutInflater inflater = (LayoutInflater) activity.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        View view = inflater.inflate(R.layout.a2preference_slider, null, false);

        TextView tv = (TextView) view.findViewById(R.id.a2preference_slider_summary);
        tv.setText(setting.getSummary(activity));

        final TextView seekBarValue = (TextView) view.findViewById(R.id.a2preference_slider_seekBarValue);

        SeekBar sb = (SeekBar) view.findViewById(R.id.a2preference_slider_seekBar);
        sb.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                if (!fromUser) {
                    return;
                }
                iLoadSave.showValue(progress, seekBarValue);
                iLoadSave.saveInt(progress);
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
            }
        });

        sb.setMax(0); // http://stackoverflow.com/questions/10278467/seekbar-not-setting-actual-progress-setprogress-not-working-on-early-android
        sb.setMax(numChoices);
        int progress = iLoadSave.intValue();
        sb.setProgress(progress);
        iLoadSave.showValue(progress, seekBarValue);
        return view;
    }

    protected static void _alertDialogHandleSelection(final Apple2Activity activity, final int titleId, final String[] choices, final IPreferenceLoadSave iLoadSave) {
        _alertDialogHandleSelection(activity, activity.getResources().getString(titleId), choices, iLoadSave);
    }

    protected static void _alertDialogHandleSelection(final Apple2Activity activity, final String titleId, final String[] choices, final IPreferenceLoadSave iLoadSave) {
        AlertDialog.Builder builder = new AlertDialog.Builder(activity).setIcon(R.drawable.ic_launcher).setCancelable(true).setTitle(titleId);
        builder.setNegativeButton(R.string.cancel, new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                dialog.dismiss();
            }
        });

        final int checkedPosition = iLoadSave.intValue();
        final ArrayAdapter<String> adapter = new ArrayAdapter<String>(activity, android.R.layout.select_dialog_singlechoice, choices) {
            @Override
            public View getView(int position, View convertView, ViewGroup parent) {
                View view = super.getView(position, convertView, parent);
                CheckedTextView ctv = (CheckedTextView) view.findViewById(android.R.id.text1);
                ctv.setChecked(position == checkedPosition);
                return view;
            }
        };

        builder.setAdapter(adapter, new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int value) {
                iLoadSave.saveInt(value);
                dialog.dismiss();
            }
        });
        AlertDialog dialog = builder.create();
        activity.registerAndShowDialog(dialog);
    }

    protected static ImageView _addPopupIcon(Apple2Activity activity, IMenuEnum setting, View convertView) {
        ImageView imageView = new ImageView(activity);
        Drawable drawable = activity.getResources().getDrawable(android.R.drawable.ic_menu_edit);
        imageView.setImageDrawable(drawable);
        LinearLayout layout = (LinearLayout) convertView.findViewById(R.id.a2preference_widget_frame);
        layout.addView(imageView);
        return imageView;
    }

    protected static CheckBox _addCheckbox(Apple2Activity activity, IMenuEnum setting, View convertView, boolean isChecked) {
        CheckBox checkBox = new CheckBox(activity);
        checkBox.setChecked(isChecked);
        LinearLayout layout = (LinearLayout) convertView.findViewById(R.id.a2preference_widget_frame);
        layout.addView(checkBox);
        return checkBox;
    }

    private void setup() {

        LayoutInflater inflater = (LayoutInflater) mActivity.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        mSettingsView = inflater.inflate(R.layout.activity_settings, null, false);

        final Button cancelButton = (Button) mSettingsView.findViewById(R.id.cancelButton);
        cancelButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                mActivity.dismissAllMenus();
            }
        });

        ListView settingsList = (ListView) mSettingsView.findViewById(R.id.listView_settings);
        settingsList.setEnabled(true);

        ArrayAdapter<?> adapter = new ArrayAdapter<String>(mActivity, R.layout.a2preference, R.id.a2preference_title, allTitles()) {
            @Override
            public boolean areAllItemsEnabled() {
                return Apple2AbstractMenu.this.areAllItemsEnabled();
            }

            @Override
            public boolean isEnabled(int position) {
                return Apple2AbstractMenu.this.isEnabled(position);
            }

            @Override
            public View getView(int position, View convertView, ViewGroup parent) {
                //View view = super.getView(position, convertView, parent);
                // ^^^ WHOA ... this is catching an NPE deep in AOSP code on the second time loading ... WTF?
                // Methinks it is related to the hack of loading a completely different R.layout.something for certain views...
                View view = convertView != null ? convertView : super.getView(position, null, parent);
                IMenuEnum setting = allValues()[position];
                return setting.getView(mActivity, view);
            }
        };

        settingsList.setAdapter(adapter);
        settingsList.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                IMenuEnum setting = allValues()[position];
                LinearLayout layout = (LinearLayout) view.findViewById(R.id.a2preference_widget_frame);
                if (layout == null) {
                    return;
                }

                View childView = layout.getChildAt(0);
                boolean selected = false;
                if (childView != null && childView instanceof CheckBox) {
                    CheckBox checkBox = (CheckBox) childView;
                    checkBox.setChecked(!checkBox.isChecked());
                    selected = checkBox.isChecked();
                }
                setting.handleSelection(mActivity, Apple2AbstractMenu.this, selected);
            }
        });
    }
}
