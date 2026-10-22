package com.supremo.launcher.main.ui;

import android.app.Activity;
import android.view.View;

import androidx.constraintlayout.widget.ConstraintLayout;

import com.supremo.launcher.R;
import com.supremo.launcher.launcher.util.Util;

import java.nio.charset.StandardCharsets;

public class RadialVehicles {
    private ConstraintLayout radialLayout;
    private Activity activity;
    public static boolean menuVisibleV;
    private RadialMenu mRadialMenu;

    native void sendCommandV(byte[] str);

    public void show() {
        sendCommandV("/mv".getBytes(StandardCharsets.UTF_8));
    }
}