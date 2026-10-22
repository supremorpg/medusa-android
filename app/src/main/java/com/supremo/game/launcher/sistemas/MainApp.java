package com.supremo.launcher.launcher.sistemas;

import android.app.Application;

public class MainApp extends Application {

    @Override
    public void onCreate() {
        super.onCreate();

        System.out.println("MAIN APP INICIADO");

        UIManager.init(getApplicationContext());
        // Disabled to avoid external runtime commands interfering with gameplay diagnostics.
        // CommandReceiver.startListening();
    }
}
