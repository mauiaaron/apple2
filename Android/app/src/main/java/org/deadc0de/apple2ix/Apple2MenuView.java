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

import android.view.View;

public interface Apple2MenuView {

    public void show();

    public boolean isShowing();

    public void dismiss();

    public void dismissAll();

    public View getView();

    public boolean isCalibrating();

    public void onKeyTapCalibrationEvent(char ascii, int scancode);
}
