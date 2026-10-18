package com.bda.controller;

import android.content.Context;
import android.os.Handler;

/**
 * Open no-op replacement for the optional legacy MOGA controller SDK.
 *
 * The public tree must compile without shipping the closed SDK binary. Android
 * gamepads still work through the platform input APIs used elsewhere.
 */
public final class Controller {
    public static final int STATE_CURRENT_PRODUCT_VERSION = 0;

    private static final Controller INSTANCE = new Controller();

    private ControllerListener listener;

    private Controller() {
    }

    public static Controller getInstance(Context context) {
        return INSTANCE;
    }

    public void init() {
    }

    public void setListener(ControllerListener listener, Handler handler) {
        this.listener = listener;
    }

    public int getState(int state) {
        return 0;
    }

    public void onResume() {
    }

    public void onPause() {
    }

    public void exit() {
        listener = null;
    }
}
