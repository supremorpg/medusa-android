package com.xyron.game.main;

import android.os.Build;
import android.os.Bundle;
import android.os.Process;
import android.util.Log;

import com.bytedance.shadowhook.ShadowHook;
import com.wardrumstudios.utils.WarMedia;

import java.io.File;

public class GTASA extends WarMedia {
    private static final String TAG = "GTASA";

    public static GTASA gtasaSelf = null;
    static String vmVersion;

    boolean UseExpansionPack = false;

    static {
        vmVersion = null;
        Log.i(TAG, "**** Loading SO's");

        try {
            ShadowHook.init(new ShadowHook.ConfigBuilder()
                    .setMode(ShadowHook.Mode.UNIQUE)
                    .build());

            vmVersion = System.getProperty("java.vm.version");
            Log.i(TAG, "vmVersion " + vmVersion);

            boolean is64BitRuntime = is64BitRuntime();
            Log.i(TAG, "Native runtime " + (is64BitRuntime ? "64-bit" : "32-bit"));

            loadOptionalLibrary("ImmEmulatorJ");
            loadOptionalLibrary(is64BitRuntime ? "OpenAL64" : "OpenAL32");
            loadRequiredLibrary("SCAnd");
            loadRequiredLibrary("GTASA");
            loadRequiredLibrary("bass");
            loadRequiredLibrary(getSampLibraryName());
        }
        catch (ExceptionInInitializerError | UnsatisfiedLinkError e) {
            Log.e(TAG, e.getMessage());
        }
    }

    static String getSampLibraryName() {
        return "samp";
    }

    private static boolean is64BitRuntime() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            return Process.is64Bit();
        }

        String osArch = System.getProperty("os.arch", "");
        return osArch != null && osArch.contains("64");
    }

    private static void loadRequiredLibrary(String libraryName) {
        System.loadLibrary(libraryName);
        Log.i(TAG, "Loaded " + libraryName);
    }

    private static void loadOptionalLibrary(String libraryName) {
        try {
            System.loadLibrary(libraryName);
            Log.i(TAG, "Loaded optional " + libraryName);
        }
        catch (UnsatisfiedLinkError error) {
            Log.w(TAG, "Optional native library not available: " + libraryName);
        }
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        Log.i(TAG, "**** onCreate");
        gtasaSelf = this;

        expansionFileName = "main.8." + getPackageName() + ".obb";
        patchFileName = "patch.8." + getPackageName() + ".obb";
        apkFileName = GetPackageName(getPackageName());
        Log.i(TAG, "apkFileName " + apkFileName);

        baseDirectory = GetGameBaseDirectory();
        AllowLongPressForExit = true;

        String[] checkLitePart = { "anim", "audio", "data", "models", "texdb" };

        for (String part : checkLitePart) {
            File folder = new File(baseDirectory + part);

            if (folder.exists() && folder.isDirectory()) {
                Log.i(TAG, "Using lite data.");
                UseExpansionPack = false;
            }
            else {
                Log.i(TAG, "Using obb.");
                UseExpansionPack = true;
            }
        }

        if (UseExpansionPack) {
            xAPKS = new XAPKFile[2];
            xAPKS[0] = new XAPKFile(true, 8, 1967561852);
            xAPKS[1] = new XAPKFile(false, 8, 625313014);
        }

        wantsMultitouch = true;
        wantsAccelerometer = true;

        RestoreCurrentLanguage();

        super.onCreate(savedInstanceState);
        SetReportPS3As360(false);
    }

    @Override
    public boolean ServiceAppCommand(String cmd, String args) {
        if (cmd.equalsIgnoreCase("SetLocale")) {
            SetLocale(args);
            return false;
        }

        return false;
    }

    @Override
    public int ServiceAppCommandValue(String cmd, String args) {
        if (cmd.equalsIgnoreCase("GetDownloadBytes")) {
            return 0;
        }

        if (cmd.equalsIgnoreCase("GetDownloadState")) {
            return 4;
        }

        return (!cmd.equalsIgnoreCase("GetNetworkState") || !isNetworkAvailable()) ? 0 : 1;
    }

    public native void main();

    @Override
    public void onStart() {
        Log.i(TAG, "**** onStart");
        super.onStart();
    }

    @Override
    public void onRestart() {
        Log.i(TAG, "**** onRestart");
        super.onRestart();
    }

    @Override
    public void onResume() {
        Log.i(TAG, "**** onResume");
        super.onResume();
    }

    @Override
    public void onPause() {
        Log.i(TAG, "**** onPause");
        super.onPause();
    }

    @Override
    public void onStop() {
        Log.i(TAG, "**** onStop");
        super.onStop();
    }

    @Override
    public void onDestroy() {
        Log.i(TAG, "**** onDestroy");
        super.onDestroy();
    }

    @Override
    public boolean CustomLoadFunction() {
        return CheckIfNeedsReadPermission(gtasaSelf);
    }

    public static void staticEnterSocialClub() {
        gtasaSelf.EnterSocialClub();
    }

    public static void staticExitSocialClub() {
        gtasaSelf.ExitSocialClub();
    }

    public void EnterSocialClub() {
        Log.i(TAG, "**** EnterSocialClub");
    }

    public void ExitSocialClub() {
        Log.i(TAG, "**** ExitSocialClub");
    }
}
