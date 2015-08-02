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

import android.view.View;

public interface Apple2MenuView {

    public void show();

    public boolean isShowing();

    public void dismiss();

    public View getView();

    public boolean isCalibrating();
}
