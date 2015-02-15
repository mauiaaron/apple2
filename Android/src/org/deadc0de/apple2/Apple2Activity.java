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

package org.deadc0de.apple2;

import android.app.Activity;
import android.os.Bundle;
import android.widget.TextView;

public class Apple2Activity extends Activity {

    static {
        System.loadLibrary("apple2ix");
    }   

    private native void nativeOnCreate();
    private native void nativeOnResume();
    private native void nativeOnPause();

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        nativeOnCreate();
        TextView tv = new TextView(this);
        tv.setText("Hello Apple2!");
        setContentView(tv);
    }

    @Override
    protected void onResume() {
        super.onResume();
        nativeOnResume();
    }

    @Override
    protected void onPause() {
        super.onPause();
        nativeOnPause();
    }
}
