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

/*
 * Sourced from AOSP "GL2JNI" sample code.
 */

package org.deadc0de.apple2ix;

import android.content.Context;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.media.AudioManager;
import android.opengl.GLES20;
import android.opengl.GLSurfaceView;
import android.os.Build;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.ViewTreeObserver;

import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;
import javax.microedition.khronos.opengles.GL10;

class Apple2View extends GLSurfaceView {
    private final static String TAG = "Apple2View";
    private final static boolean DEBUG = false;
    private final static int MAX_FINGERS = 32;// HACK ...

    public final static long NATIVE_TOUCH_HANDLED = (1 << 0);
    public final static long NATIVE_TOUCH_REQUEST_SHOW_MENU = (1 << 1);

    public final static long NATIVE_TOUCH_KEY_TAP = (1 << 4);
    public final static long NATIVE_TOUCH_KBD = (1 << 5);
    public final static long NATIVE_TOUCH_JOY = (1 << 6);
    public final static long NATIVE_TOUCH_MENU = (1 << 7);
    public final static long NATIVE_TOUCH_JOY_KPAD = (1 << 8);

    public final static long NATIVE_TOUCH_INPUT_DEVICE_CHANGED = (1 << 16);
    public final static long NATIVE_TOUCH_CPU_SPEED_DEC = (1 << 17);
    public final static long NATIVE_TOUCH_CPU_SPEED_INC = (1 << 18);

    public final static long NATIVE_TOUCH_ASCII_SCANCODE_SHIFT = 32;
    public final static long NATIVE_TOUCH_ASCII_SCANCODE_MASK = 0xFFFFL;
    public final static long NATIVE_TOUCH_ASCII_MASK = 0xFF00L;
    public final static long NATIVE_TOUCH_SCANCODE_MASK = 0x00FFL;


    private Apple2Activity mActivity;
    private Runnable mGraphicsInitializedRunnable;

    private float[] mXCoords = new float[MAX_FINGERS];
    private float[] mYCoords = new float[MAX_FINGERS];


    private static native void nativeGraphicsInitialized(int width, int height);

    private static native void nativeGraphicsChanged(int width, int height);

    private static native void nativeRender();

    private static native void nativeOnKeyDown(int keyCode, int metaState);

    private static native void nativeOnKeyUp(int keyCode, int metaState);

    public static native long nativeOnTouch(int action, int pointerCount, int pointerIndex, float[] xCoords, float[] yCoords);


    public Apple2View(Apple2Activity activity, Runnable graphicsInitializedRunnable) {
        super(activity.getApplication());
        mActivity = activity;
        mGraphicsInitializedRunnable = graphicsInitializedRunnable;

        setFocusable(true);
        setFocusableInTouchMode(true);

        /* By default, GLSurfaceView() creates a RGB_565 opaque surface.
         * If we want a translucent one, we should change the surface's
         * format here, using PixelFormat.TRANSLUCENT for GL Surfaces
         * is interpreted as any 32-bit surface with alpha by SurfaceFlinger.
         */
        this.getHolder().setFormat(PixelFormat.TRANSLUCENT);

        /* Setup the context factory for 2.0 rendering.
         * See ContextFactory class definition below
         */
        setEGLContextFactory(new ContextFactory());

        /* We need to choose an EGLConfig that matches the format of
         * our surface exactly. This is going to be done in our
         * custom config chooser. See ConfigChooser class definition
         * below.
         */
        setEGLConfigChooser(new ConfigChooser(8, 8, 8, 8, /*depth:*/0, /*stencil:*/0));

        /* Set the renderer responsible for frame rendering */
        setRenderer(new Renderer());

        // Another Android Annoyance ...
        // Even though we no longer use the system soft keyboard (which would definitely trigger width/height changes to our OpenGL canvas),
        // we still need to listen to dimension changes, because it seems on some janky devices you have an incorrect width/height set when
        // the initial OpenGL onSurfaceChanged() callback occurs.  For now, include this defensive coding...
        getViewTreeObserver().addOnGlobalLayoutListener(new ViewTreeObserver.OnGlobalLayoutListener() {
            public void onGlobalLayout() {
                Rect rect = new Rect();
                Apple2View.this.getWindowVisibleDisplayFrame(rect);
                int h = rect.height();
                int w = rect.width();
                if (w < h) {
                    // assure landscape dimensions
                    final int w_ = w;
                    w = h;
                    h = w_;
                }
                nativeGraphicsChanged(w, h);
            }
        });

    }

    private static class ContextFactory implements GLSurfaceView.EGLContextFactory {
        private static int EGL_CONTEXT_CLIENT_VERSION = 0x3098;

        public EGLContext createContext(EGL10 egl, EGLDisplay display, EGLConfig eglConfig) {
            Log.w(TAG, "creating OpenGL ES 2.0 context");
            checkEglError("Before eglCreateContext", egl);
            int[] attrib_list = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL10.EGL_NONE};
            EGLContext context = egl.eglCreateContext(display, eglConfig, EGL10.EGL_NO_CONTEXT, attrib_list);
            checkEglError("After eglCreateContext", egl);
            return context;
        }

        public void destroyContext(EGL10 egl, EGLDisplay display, EGLContext context) {
            egl.eglDestroyContext(display, context);
        }
    }

    private static void checkEglError(String prompt, EGL10 egl) {
        int error;
        while ((error = egl.eglGetError()) != EGL10.EGL_SUCCESS) {
            Log.e(TAG, String.format("%s: EGL error: 0x%x", prompt, error));
        }
    }

    private static class ConfigChooser implements GLSurfaceView.EGLConfigChooser {

        public ConfigChooser(int r, int g, int b, int a, int depth, int stencil) {
            mRedSize = r;
            mGreenSize = g;
            mBlueSize = b;
            mAlphaSize = a;
            mDepthSize = depth;
            mStencilSize = stencil;
        }

        /* This EGL config specification is used to specify 2.0 rendering.
         * We use a minimum size of 4 bits for red/green/blue, but will
         * perform actual matching in chooseConfig() below.
         */
        private static int EGL_OPENGL_ES2_BIT = 4;
        private static int[] s_configAttribs2 = {
                EGL10.EGL_RED_SIZE, 4,
                EGL10.EGL_GREEN_SIZE, 4,
                EGL10.EGL_BLUE_SIZE, 4,
                EGL10.EGL_ALPHA_SIZE, 4,
                EGL10.EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                EGL10.EGL_NONE
        };

        public EGLConfig chooseConfig(EGL10 egl, EGLDisplay display) {

            // Get the number of minimally matching EGL configurations
            int[] num_config = new int[1];
            egl.eglChooseConfig(display, s_configAttribs2, null, 0, num_config);

            int numConfigs = num_config[0];

            if (numConfigs <= 0) {
                throw new IllegalArgumentException("No configs match configSpec");
            }

            // Allocate then read the array of minimally matching EGL configs
            EGLConfig[] configs = new EGLConfig[numConfigs];
            egl.eglChooseConfig(display, s_configAttribs2, configs, numConfigs, num_config);

            if (DEBUG) {
                printConfigs(egl, display, configs);
            }

            // Now return the "best" one
            EGLConfig best = chooseConfig(egl, display, configs);
            if (best == null) {
                Log.e(TAG, "OOPS! Did not pick an EGLConfig.  What device are you using?!  Android will now crash this app...");
            } else {
                Log.w(TAG, "Using EGL CONFIG : ");
                printConfig(egl, display, best);
            }
            return best;
        }

        public EGLConfig chooseConfig(EGL10 egl, EGLDisplay display, EGLConfig[] configs) {
            for (EGLConfig config : configs) {
                int d = findConfigAttrib(egl, display, config, EGL10.EGL_DEPTH_SIZE, 0);
                int s = findConfigAttrib(egl, display, config, EGL10.EGL_STENCIL_SIZE, 0);

                // We need at least mDepthSize and mStencilSize bits
                if (d < mDepthSize || s < mStencilSize) {
                    continue;
                }

                // We want an *exact* match for red/green/blue/alpha
                int r = findConfigAttrib(egl, display, config, EGL10.EGL_RED_SIZE, 0);
                int g = findConfigAttrib(egl, display, config, EGL10.EGL_GREEN_SIZE, 0);
                int b = findConfigAttrib(egl, display, config, EGL10.EGL_BLUE_SIZE, 0);
                int a = findConfigAttrib(egl, display, config, EGL10.EGL_ALPHA_SIZE, 0);

                if (r == mRedSize && g == mGreenSize && b == mBlueSize && a == mAlphaSize) {
                    return config;
                }
            }
            return null;
        }

        private int findConfigAttrib(EGL10 egl, EGLDisplay display, EGLConfig config, int attribute, int defaultValue) {

            if (egl.eglGetConfigAttrib(display, config, attribute, mValue)) {
                return mValue[0];
            }
            return defaultValue;
        }

        private void printConfigs(EGL10 egl, EGLDisplay display, EGLConfig[] configs) {
            int numConfigs = configs.length;
            Log.w(TAG, String.format("%d configurations", numConfigs));
            for (int i = 0; i < numConfigs; i++) {
                Log.w(TAG, String.format("Configuration %d:\n", i));
                printConfig(egl, display, configs[i]);
            }
        }

        private void printConfig(EGL10 egl, EGLDisplay display, EGLConfig config) {
            int[] attributes = {
                    EGL10.EGL_BUFFER_SIZE,
                    EGL10.EGL_ALPHA_SIZE,
                    EGL10.EGL_BLUE_SIZE,
                    EGL10.EGL_GREEN_SIZE,
                    EGL10.EGL_RED_SIZE,
                    EGL10.EGL_DEPTH_SIZE,
                    EGL10.EGL_STENCIL_SIZE,
                    EGL10.EGL_CONFIG_CAVEAT,
                    EGL10.EGL_CONFIG_ID,
                    EGL10.EGL_LEVEL,
                    EGL10.EGL_MAX_PBUFFER_HEIGHT,
                    EGL10.EGL_MAX_PBUFFER_PIXELS,
                    EGL10.EGL_MAX_PBUFFER_WIDTH,
                    EGL10.EGL_NATIVE_RENDERABLE,
                    EGL10.EGL_NATIVE_VISUAL_ID,
                    EGL10.EGL_NATIVE_VISUAL_TYPE,
                    0x3030, // EGL10.EGL_PRESERVED_RESOURCES,
                    EGL10.EGL_SAMPLES,
                    EGL10.EGL_SAMPLE_BUFFERS,
                    EGL10.EGL_SURFACE_TYPE,
                    EGL10.EGL_TRANSPARENT_TYPE,
                    EGL10.EGL_TRANSPARENT_RED_VALUE,
                    EGL10.EGL_TRANSPARENT_GREEN_VALUE,
                    EGL10.EGL_TRANSPARENT_BLUE_VALUE,
                    0x3039, // EGL10.EGL_BIND_TO_TEXTURE_RGB,
                    0x303A, // EGL10.EGL_BIND_TO_TEXTURE_RGBA,
                    0x303B, // EGL10.EGL_MIN_SWAP_INTERVAL,
                    0x303C, // EGL10.EGL_MAX_SWAP_INTERVAL,
                    EGL10.EGL_LUMINANCE_SIZE,
                    EGL10.EGL_ALPHA_MASK_SIZE,
                    EGL10.EGL_COLOR_BUFFER_TYPE,
                    EGL10.EGL_RENDERABLE_TYPE,
                    0x3042 // EGL10.EGL_CONFORMANT
            };
            String[] names = {
                    "EGL_BUFFER_SIZE",
                    "EGL_ALPHA_SIZE",
                    "EGL_BLUE_SIZE",
                    "EGL_GREEN_SIZE",
                    "EGL_RED_SIZE",
                    "EGL_DEPTH_SIZE",
                    "EGL_STENCIL_SIZE",
                    "EGL_CONFIG_CAVEAT",
                    "EGL_CONFIG_ID",
                    "EGL_LEVEL",
                    "EGL_MAX_PBUFFER_HEIGHT",
                    "EGL_MAX_PBUFFER_PIXELS",
                    "EGL_MAX_PBUFFER_WIDTH",
                    "EGL_NATIVE_RENDERABLE",
                    "EGL_NATIVE_VISUAL_ID",
                    "EGL_NATIVE_VISUAL_TYPE",
                    "EGL_PRESERVED_RESOURCES",
                    "EGL_SAMPLES",
                    "EGL_SAMPLE_BUFFERS",
                    "EGL_SURFACE_TYPE",
                    "EGL_TRANSPARENT_TYPE",
                    "EGL_TRANSPARENT_RED_VALUE",
                    "EGL_TRANSPARENT_GREEN_VALUE",
                    "EGL_TRANSPARENT_BLUE_VALUE",
                    "EGL_BIND_TO_TEXTURE_RGB",
                    "EGL_BIND_TO_TEXTURE_RGBA",
                    "EGL_MIN_SWAP_INTERVAL",
                    "EGL_MAX_SWAP_INTERVAL",
                    "EGL_LUMINANCE_SIZE",
                    "EGL_ALPHA_MASK_SIZE",
                    "EGL_COLOR_BUFFER_TYPE",
                    "EGL_RENDERABLE_TYPE",
                    "EGL_CONFORMANT"
            };
            int[] value = new int[1];
            for (int i = 0; i < attributes.length; i++) {
                int attribute = attributes[i];
                String name = names[i];
                if (egl.eglGetConfigAttrib(display, config, attribute, value)) {
                    Log.w(TAG, String.format("  %s: %d\n", name, value[0]));
                } else {
                    // Log.w(TAG, String.format("  %s: failed\n", name));
                    while (egl.eglGetError() != EGL10.EGL_SUCCESS) ;
                }
            }
        }

        // Subclasses can adjust these values:
        protected int mRedSize;
        protected int mGreenSize;
        protected int mBlueSize;
        protected int mAlphaSize;
        protected int mDepthSize;
        protected int mStencilSize;
        private int[] mValue = new int[1];
    }

    private class Renderer implements GLSurfaceView.Renderer {

        @Override
        public void onDrawFrame(GL10 gl) {
            nativeRender();
        }

        @Override
        public void onSurfaceChanged(GL10 gl, int width, int height) {
            Apple2Preferences.GL_VENDOR.saveString(mActivity, GLES20.glGetString(GLES20.GL_VENDOR));
            Apple2Preferences.GL_RENDERER.saveString(mActivity, GLES20.glGetString(GLES20.GL_RENDERER));
            Apple2Preferences.GL_VERSION.saveString(mActivity, GLES20.glGetString(GLES20.GL_VERSION));

            Log.v(TAG, "graphicsInitialized(" + width + ", " + height + ")");

            if (width < height) {
                // assure landscape dimensions
                final int w_ = width;
                width = height;
                height = w_;
            }

            nativeGraphicsInitialized(width, height);

            if (Apple2View.this.mGraphicsInitializedRunnable != null) {
                Apple2View.this.mGraphicsInitializedRunnable.run();
                Apple2View.this.mGraphicsInitializedRunnable = null;
            }

            Apple2View.this.mActivity.maybeResumeCPU();
        }

        @Override
        public void onSurfaceCreated(GL10 gl, EGLConfig config) {
            // Do nothing.
        }
    }

    // --------------------------------------------------------------------------
    // Event handling, touch, keyboard, gamepad

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (Apple2Activity.isNativeBarfed()) {
            return super.onKeyDown(keyCode, event);
        }
        if ((keyCode == KeyEvent.KEYCODE_VOLUME_DOWN) || (keyCode == KeyEvent.KEYCODE_VOLUME_MUTE) || (keyCode == KeyEvent.KEYCODE_VOLUME_UP)) {
            return super.onKeyDown(keyCode, event);
        }

        nativeOnKeyDown(keyCode, event.getMetaState());
        return true;
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        if (Apple2Activity.isNativeBarfed()) {
            return super.onKeyUp(keyCode, event);
        }

        if (keyCode == KeyEvent.KEYCODE_BACK) {
            Apple2MenuView apple2MenuView = mActivity.peekApple2View();
            if (apple2MenuView == null) {
                mActivity.showMainMenu();
            } else {
                apple2MenuView.dismiss();
            }
            return true;
        } else if (keyCode == KeyEvent.KEYCODE_MENU) {
            mActivity.showMainMenu();
            return true;
        } else if ((keyCode == KeyEvent.KEYCODE_VOLUME_DOWN) || (keyCode == KeyEvent.KEYCODE_VOLUME_MUTE) || (keyCode == KeyEvent.KEYCODE_VOLUME_UP)) {
            return super.onKeyUp(keyCode, event);
        }

        nativeOnKeyUp(keyCode, event.getMetaState());
        return true;
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        do {

            if (Apple2Activity.isNativeBarfed()) {
                break;
            }
            if (mActivity.getMainMenu() == null) {
                break;
            }

            Apple2MenuView apple2MenuView = mActivity.peekApple2View();
            if ((apple2MenuView != null) && (!apple2MenuView.isCalibrating())) {
                break;
            }

            //printSamples(event);
            int action = event.getActionMasked();
            int pointerIndex = event.getActionIndex();
            int pointerCount = event.getPointerCount();
            for (int i = 0; i < pointerCount/* && i < MAX_FINGERS */; i++) {
                mXCoords[i] = event.getX(i);
                mYCoords[i] = event.getY(i);
            }

            long nativeFlags = nativeOnTouch(action, pointerCount, pointerIndex, mXCoords, mYCoords);

            if ((nativeFlags & NATIVE_TOUCH_HANDLED) == 0) {
                break;
            }

            if ((nativeFlags & NATIVE_TOUCH_REQUEST_SHOW_MENU) != 0) {
                mActivity.getMainMenu().show();
            }

            if ((nativeFlags & NATIVE_TOUCH_KEY_TAP) != 0) {
                if (Apple2Preferences.KEYBOARD_CLICK_ENABLED.booleanValue(mActivity)) {
                    AudioManager am = (AudioManager) mActivity.getSystemService(Context.AUDIO_SERVICE);
                    if (am != null) {
                        am.playSoundEffect(AudioManager.FX_KEY_CLICK);
                    }
                }

                if ((apple2MenuView != null) && apple2MenuView.isCalibrating()) {
                    long asciiScancodeLong = nativeFlags & (NATIVE_TOUCH_ASCII_SCANCODE_MASK << NATIVE_TOUCH_ASCII_SCANCODE_SHIFT);
                    int asciiInt = (int) (asciiScancodeLong >> (NATIVE_TOUCH_ASCII_SCANCODE_SHIFT + 8));
                    int scancode = (int) ((asciiScancodeLong >> NATIVE_TOUCH_ASCII_SCANCODE_SHIFT) & 0xFFL);
                    char ascii = (char) asciiInt;
                    apple2MenuView.onKeyTapCalibrationEvent(ascii, scancode);
                }
            }

            if ((nativeFlags & NATIVE_TOUCH_MENU) == 0) {
                break;
            }

            // handle menu-specific actions

            if ((nativeFlags & NATIVE_TOUCH_INPUT_DEVICE_CHANGED) != 0) {
                Apple2Preferences.TouchDeviceVariant nextVariant;
                if ((nativeFlags & NATIVE_TOUCH_KBD) != 0) {
                    nextVariant = Apple2Preferences.TouchDeviceVariant.KEYBOARD;
                } else if ((nativeFlags & NATIVE_TOUCH_JOY) != 0) {
                    nextVariant = Apple2Preferences.TouchDeviceVariant.JOYSTICK;
                } else if ((nativeFlags & NATIVE_TOUCH_JOY_KPAD) != 0) {
                    nextVariant = Apple2Preferences.TouchDeviceVariant.JOYSTICK_KEYPAD;
                } else {
                    int touchDevice = Apple2Preferences.nativeGetCurrentTouchDevice();
                    nextVariant = Apple2Preferences.TouchDeviceVariant.next(touchDevice);
                }
                Apple2Preferences.CURRENT_TOUCH_DEVICE.saveTouchDevice(mActivity, nextVariant);
            } else if ((nativeFlags & NATIVE_TOUCH_CPU_SPEED_DEC) != 0) {
                int percentSpeed = Apple2Preferences.nativeGetCPUSpeed();
                if (percentSpeed > 400) { // HACK: max value from native side
                    percentSpeed = 375;
                } else if (percentSpeed > 100) {
                    percentSpeed -= 25;
                } else {
                    percentSpeed -= 5;
                }
                Apple2Preferences.CPU_SPEED_PERCENT.saveInt(mActivity, percentSpeed);
            } else if ((nativeFlags & NATIVE_TOUCH_CPU_SPEED_INC) != 0) {
                int percentSpeed = Apple2Preferences.nativeGetCPUSpeed();
                if (percentSpeed >= 100) {
                    percentSpeed += 25;
                } else {
                    percentSpeed += 5;
                }
                Apple2Preferences.CPU_SPEED_PERCENT.saveInt(mActivity, percentSpeed);
            }
        } while (false);

        return true;
    }
}
