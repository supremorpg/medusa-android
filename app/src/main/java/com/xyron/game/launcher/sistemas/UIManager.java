package com.xyron.game.launcher.sistemas;

import android.content.Context;
import android.util.Log;

public class UIManager {

    private static final String TAG = "XyronUI";
    public static Context context;

    public static void init(Context ctx) {
        context = ctx;
    }

    public static void openOverlay(String name) {
        Log.i(TAG, "open_ui ignored, browser overlays are disabled: " + name);
    }
}
