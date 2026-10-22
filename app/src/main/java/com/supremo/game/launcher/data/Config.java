package com.supremo.launcher.launcher.data;

import android.content.Context;
import android.graphics.Bitmap;

public class Config {

    public static final String DEFAULT_SERVER_HOST = "192.168.3.134";
    public static final int DEFAULT_SERVER_PORT = 7777;

    public static Context mainContext;

    public static String responseServer;
    public static int responseInt;

    public static String mNewsDescription[], mNewsTitle[], mNewsImage[];
    public static Bitmap mBitmap[];

    public static int mServersDoubling[], mServersPing[], mServersIsNew[];
    public static String mServersOnline[];
    public static String mServersName[], mServersHost[];
    public static int mServersPort[];
}
