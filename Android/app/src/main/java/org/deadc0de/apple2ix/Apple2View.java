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
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.ViewTreeObserver;
import android.view.inputmethod.InputMethodManager;

import com.example.inputmanagercompat.InputManagerCompat;

import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;
import javax.microedition.khronos.opengles.GL10;

class Apple2View extends GLSurfaceView implements InputManagerCompat.InputDeviceListener {
    private final static String TAG = "Apple2View";
    private final static boolean DEBUG = false;
    private final static int MAX_FINGERS = 32;// HACK ...

    public final static long NATIVE_TOUCH_HANDLED = (1 << 0);
    public final static long NATIVE_TOUCH_REQUEST_SHOW_MENU = (1 << 1);
    public final static long NATIVE_TOUCH_REQUEST_SHOW_SYSTEM_KBD = (1 << 2);

    public final static long NATIVE_TOUCH_KEY_TAP = (1 << 4);
    public final static long NATIVE_TOUCH_KBD = (1 << 5);
    public final static long NATIVE_TOUCH_JOY = (1 << 6);
    public final static long NATIVE_TOUCH_MENU = (1 << 7);
    public final static long NATIVE_TOUCH_JOY_KPAD = (1 << 8);

    public final static long NATIVE_TOUCH_INPUT_DEVICE_CHANGED = (1 << 16);

    public final static long NATIVE_TOUCH_ASCII_SCANCODE_SHIFT = 32;
    public final static long NATIVE_TOUCH_ASCII_SCANCODE_MASK = 0xFFFFL;
    public final static long NATIVE_TOUCH_ASCII_MASK = 0xFF00L;
    public final static long NATIVE_TOUCH_SCANCODE_MASK = 0x00FFL;


    private Apple2Activity mActivity;
    private final InputManagerCompat mInputManager;

    private float[] mXCoords = new float[MAX_FINGERS];
    private float[] mYCoords = new float[MAX_FINGERS];

    private int mWidth = 0;
    private int mHeight = 0;

    private static native void nativeGraphicsInitialized();

    private static native void nativeRender();

    private static native void nativeOnJoystickMove(int x, int y);

    public static native long nativeOnTouch(int action, int pointerCount, int pointerIndex, float[] xCoords, float[] yCoords);


    public Apple2View(Apple2Activity activity) {
        super(activity.getApplication());
        mActivity = activity;

        setFocusable(true);
        setFocusableInTouchMode(true);

        mInputManager = InputManagerCompat.Factory.getInputManager(this.getContext());
        if (mInputManager != null) {
            mInputManager.registerInputDeviceListener(this, null);
        }

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
                if (w != mWidth || h != mHeight) {
                    mWidth = w;
                    mHeight = h;
                    Apple2Preferences.setJSONPref(Apple2Preferences.PREF_DOMAIN_INTERFACE, Apple2Preferences.PREF_DEVICE_WIDTH, mWidth);
                    Apple2Preferences.setJSONPref(Apple2Preferences.PREF_DOMAIN_INTERFACE, Apple2Preferences.PREF_DEVICE_HEIGHT, mHeight);
                }
            }
        });

    }

    private static class ContextFactory implements GLSurfaceView.EGLContextFactory {
        private static int EGL_CONTEXT_CLIENT_VERSION = 0x3098;

        public EGLContext createContext(EGL10 egl, EGLDisplay display, EGLConfig eglConfig) {
            Apple2Activity.logMessage(Apple2Activity.LogType.WARN, TAG, "creating OpenGL ES 2.0 context");
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
            Apple2Activity.logMessage(Apple2Activity.LogType.ERROR, TAG, String.format("%s: EGL error: 0x%x", prompt, error));
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
                Apple2Activity.logMessage(Apple2Activity.LogType.ERROR, TAG, "OOPS! Did not pick an EGLConfig.  What device are you using?!  Android will now crash this app...");
            } else {
                Apple2Activity.logMessage(Apple2Activity.LogType.WARN, TAG, "Using EGL CONFIG : ");
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
            Apple2Activity.logMessage(Apple2Activity.LogType.WARN, TAG, String.format("%d configurations", numConfigs));
            for (int i = 0; i < numConfigs; i++) {
                Apple2Activity.logMessage(Apple2Activity.LogType.WARN, TAG, String.format("Configuration %d:\n", i));
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
                    Apple2Activity.logMessage(Apple2Activity.LogType.WARN, TAG, String.format("  %s: %d\n", name, value[0]));
                } else {
                    // Apple2Activity.logMessage(Apple2Activity.LogType.WARN, TAG, String.format("  %s: failed\n", name));
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
            Apple2Preferences.setJSONPref(Apple2CrashHandler.SETTINGS.GL_VENDOR, GLES20.glGetString(GLES20.GL_VENDOR));
            Apple2Preferences.setJSONPref(Apple2CrashHandler.SETTINGS.GL_RENDERER, GLES20.glGetString(GLES20.GL_RENDERER));
            Apple2Preferences.setJSONPref(Apple2CrashHandler.SETTINGS.GL_VERSION, GLES20.glGetString(GLES20.GL_VERSION));

            Log.v(TAG, "graphicsInitialized(" + width + ", " + height + ")");

            Apple2View.this.mWidth = width;
            Apple2View.this.mHeight = height;
            Apple2Preferences.setJSONPref(Apple2Preferences.PREF_DOMAIN_INTERFACE, Apple2Preferences.PREF_DEVICE_WIDTH, mWidth);
            Apple2Preferences.setJSONPref(Apple2Preferences.PREF_DOMAIN_INTERFACE, Apple2Preferences.PREF_DEVICE_HEIGHT, mHeight);
            nativeGraphicsInitialized();

            Apple2View.this.mActivity.maybeResumeEmulation();
        }

        @Override
        public void onSurfaceCreated(GL10 gl, EGLConfig config) {
            // Do nothing.
        }
    }

    // --------------------------------------------------------------------------
    // Event handling, touch, keyboard, gamepad

    @Override
    public void onInputDeviceAdded(int deviceId) {
    }

    @Override
    public void onInputDeviceChanged(int deviceId) {
    }

    @Override
    public void onInputDeviceRemoved(int deviceId) {
    }

    @Override
    public boolean onGenericMotionEvent(MotionEvent event) {
        if (android.os.Build.VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN_MR2) {
            return super.onGenericMotionEvent(event);
        }

        if (mActivity.isEmulationPaused()) {
            return super.onGenericMotionEvent(event);
        }

        // Check that the event came from a joystick or gamepad since a generic
        // motion event could be almost anything.
        int eventSource = event.getSource();
        if ((event.getAction() == MotionEvent.ACTION_MOVE) && (((eventSource & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD) || ((eventSource & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK))) {
            int id = event.getDeviceId();
            if (id != -1) {

                InputDevice device = event.getDevice();

                float x = getCenteredAxis(event, device, MotionEvent.AXIS_X);
                if (x == 0) {
                    x = getCenteredAxis(event, device, MotionEvent.AXIS_HAT_X);
                }
                if (x == 0) {
                    x = getCenteredAxis(event, device, MotionEvent.AXIS_Z);
                }

                float y = getCenteredAxis(event, device, MotionEvent.AXIS_Y);
                if (y == 0) {
                    y = getCenteredAxis(event, device, MotionEvent.AXIS_HAT_Y);
                }
                if (y == 0) {
                    y = getCenteredAxis(event, device, MotionEvent.AXIS_RZ);
                }

                int normal_x = (int) ((x + 1.f) * 128.f);
                if (normal_x < 0) {
                    normal_x = 0;
                }
                if (normal_x > 255) {
                    normal_x = 255;
                }
                int normal_y = (int) ((y + 1.f) * 128.f);
                if (normal_y < 0) {
                    normal_y = 0;
                }
                if (normal_y > 255) {
                    normal_y = 255;
                }

                nativeOnJoystickMove(normal_x, normal_y);

                return true;
            }
        }

        return super.onGenericMotionEvent(event);
    }

    private static float getCenteredAxis(MotionEvent event, InputDevice device, int axis) {
        if (android.os.Build.VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN_MR2) {
            return 0;
        }

        final InputDevice.MotionRange range = device.getMotionRange(axis, event.getSource());
        if (range != null) {
            final float flat = range.getFlat();
            final float value = event.getAxisValue(axis);

            // Ignore axis values that are within the 'flat' region of the joystick axis center.
            // A joystick at rest does not always report an absolute position of (0,0).
            if (Math.abs(value) > flat) {
                return value;
            }
        }

        return 0;
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
                if ((boolean) Apple2Preferences.getJSONPref(Apple2KeyboardSettingsMenu.SETTINGS.KEYBOARD_ENABLE_CLICK)) {
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

            if ((nativeFlags & NATIVE_TOUCH_REQUEST_SHOW_SYSTEM_KBD) != 0) {
                clearFocus();
                requestFocus();
                InputMethodManager inputMethodManager = (InputMethodManager) mActivity.getSystemService(Context.INPUT_METHOD_SERVICE);
                if (inputMethodManager != null) {
                    inputMethodManager.showSoftInput(this, InputMethodManager.SHOW_FORCED);
                }
            }

        } while (false);

        return true;
    }
}
