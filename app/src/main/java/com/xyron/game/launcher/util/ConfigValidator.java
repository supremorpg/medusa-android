package com.xyron.game.launcher.util;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.AssetManager;
import android.os.Environment;
import android.util.Log;

import com.xyron.game.BuildConfig;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;

public class ConfigValidator {
    private static final String TAG = "ConfigValidator";
    private static final String PREFS_NAME = "xyron_config_validator";
    private static final String KEY_CORE_GAME_FILES_VERSION = "core_game_files_version";
    private static final String KEY_TOUCH_LAYOUT_VERSION = "touch_layout_version";
    private static final int CORE_GAME_FILES_VERSION = 20260518;
    private static final int TOUCH_LAYOUT_VERSION = 20260509;
    private static final long MIN_CORE_TEXT_BYTES = 1024L;
    private static final String CORE_TEXT_ASSET_PATH = "Text/american.gxt";
    private static final String CORE_TEXT_OUTPUT_PATH = "TEXT/AMERICAN.GXT";
    private static final String[][] CORE_GAME_FILES = new String[][] {
            {"Text/american.gxt", "Text/american.gxt"},
            {"Text/american.gxt", "text/american.gxt"},
            {"Text/american.gxt", "TEXT/AMERICAN.GXT"},
            {"data/fonts.dat", "data/fonts.dat"},
            {"data/fonts.dat", "DATA/FONTS.DAT"},
            {"data/handling.cfg", "data/handling.cfg"},
            {"data/handling.cfg", "DATA/HANDLING.CFG"},
            {"data/handling.cfg", "SAMP/handling.cfg"},
            {"data/ShopUVs.dat", "data/ShopUVs.dat"},
            {"data/ShopUVs.dat", "DATA/SHOPUVS.DAT"},
            {"data/mainV1.scm", "data/mainV1.scm"},
            {"data/mainV1.scm", "data/script/mainV1.scm"},
            {"data/mainV1.scm", "SAMP/main.scm"},
            {"data/scriptv1.img", "data/scriptv1.img"},
            {"data/scriptv1.img", "data/script/scriptv1.img"}
    };
    private static final String[] TOUCH_LAYOUT_FILES = new String[] {
            "data/TouchDefaultPhoneWidescreen.cfg",
            "data/TouchDefaultPhone3x2.cfg",
            "data/TouchDefaultTabletWidescreen.cfg",
            "data/TouchDefaultTablet4x3.cfg",
            "data/360Default1280x720.cfg",
            "data/360Default960x720.cfg"
    };
    private static final String[] SHARED_GAME_DIRECTORIES = new String[] {
            "anim",
            "audio",
            "data",
            "models",
            "texdb",
            "TEXT",
            "fonts"
    };
    private static final String[] SHARED_ROOT_FILES = new String[] {
            "Adjustable.cfg",
            "CINFO.BIN",
            "KeyboardMappings.cfg",
            "client_config.json",
            "files.json",
            "gta_sa.set",
            "gtasatelem.set",
            "stream.ini"
    };
    private static final String[][] SHARED_SAMP_FILES = new String[][] {
            {"SAMP/SAMP.ide", "SAMP/SAMP.ide"},
            {"SAMP/WEAPON.dat", "SAMP/WEAPON.dat"},
            {"SAMP/carcols.dat", "SAMP/carcols.dat"},
            {"SAMP/colorcycle.dat", "SAMP/colorcycle.dat"},
            {"SAMP/gta.dat", "SAMP/gta.dat"},
            {"SAMP/handling.cfg", "SAMP/handling.cfg"},
            {"SAMP/main.scm", "SAMP/main.scm"},
            {"SAMP/peds.ide", "SAMP/peds.ide"},
            {"SAMP/script.img", "SAMP/script.img"},
            {"SAMP/timecyc.dat", "SAMP/timecyc.dat"},
            {"SAMP/timecycp.dat", "SAMP/timecycp.dat"},
            {"SAMP/tracks2.dat", "SAMP/tracks2.dat"},
            {"SAMP/tracks4.dat", "SAMP/tracks4.dat"},
            {"SAMP/vehicles.ide", "SAMP/vehicles.ide"},
            {"SAMP/water.dat", "SAMP/water.dat"},
            {"SAMP/water1.dat", "SAMP/water1.dat"}
    };
    private static final String[][] SHARED_DIRECTORY_ALIASES = new String[][] {
            {"data/Decision", "data/decision"},
            {"data/Decision/Allowed", "data/decision/allowed"}
    };

    public static void validateConfigFiles(Context context) {
        Context appContext = context.getApplicationContext();
        File externalFilesDir = appContext.getExternalFilesDir(null);
        if (externalFilesDir == null) {
            return;
        }

        boolean lightweightRole = isLauncherRole() || isToolsRole();
        SharedPreferences preferences = appContext.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);

        if (!lightweightRole) {
            boolean refreshCoreFiles = preferences.getInt(KEY_CORE_GAME_FILES_VERSION, 0) != CORE_GAME_FILES_VERSION
                    || !coreGameFilesExist(externalFilesDir);
            syncCoreGameFiles(appContext.getAssets(), externalFilesDir, refreshCoreFiles);
            syncSharedGameDataIfAvailable(appContext, externalFilesDir);
            if (refreshCoreFiles) {
                preferences.edit()
                        .putInt(KEY_CORE_GAME_FILES_VERSION, CORE_GAME_FILES_VERSION)
                        .apply();
            }
            ensureCoreTextDatabase(appContext.getAssets(), externalFilesDir);
        }

        File file = new File(externalFilesDir, "SAMP/settings.ini");
        if (!file.exists()) {
            ensureParentDirectory(file);
            if (!copyAsset(appContext.getAssets(), "settings.ini", file.toString())) {
                writeDefaultSettings(file);
            }
        }

        if (lightweightRole) {
            return;
        }

        int savedVersion = preferences.getInt(KEY_TOUCH_LAYOUT_VERSION, 0);
        if (savedVersion != TOUCH_LAYOUT_VERSION || !touchLayoutAssetsExist(externalFilesDir)) {
            syncTouchLayoutAssets(appContext.getAssets(), externalFilesDir);
        }
        invalidateTouchLayoutCacheIfNeeded(preferences, savedVersion, externalFilesDir);

        /*File file2 = new File(externalFilesDir, "gta_sa.set");
        if (!file2.exists()) {
            file2.getParentFile().mkdirs();
            copyAsset(context.getAssets(), "gta_sa.set", file2.toString());
        }*/
    }

    private static void ensureCoreTextDatabase(AssetManager assetManager, File externalFilesDir) {
        File textDatabase = new File(externalFilesDir, CORE_TEXT_OUTPUT_PATH);
        if (textDatabase.exists() && textDatabase.length() >= MIN_CORE_TEXT_BYTES) {
            return;
        }

        copyAsset(assetManager, CORE_TEXT_ASSET_PATH, textDatabase.toString());
    }

    private static void syncCoreGameFiles(AssetManager assetManager,
                                          File externalFilesDir,
                                          boolean overwrite) {
        for (String[] fileMapping : CORE_GAME_FILES) {
            if (fileMapping.length < 2) {
                continue;
            }
            copyAssetIfNeeded(
                    assetManager,
                    fileMapping[0],
                    new File(externalFilesDir, fileMapping[1]).toString(),
                    overwrite
            );
        }
    }

    private static boolean coreGameFilesExist(File externalFilesDir) {
        for (String[] fileMapping : CORE_GAME_FILES) {
            if (fileMapping.length < 2) {
                continue;
            }
            File file = new File(externalFilesDir, fileMapping[1]);
            if (!file.exists() || !file.isFile() || file.length() <= 0L) {
                return false;
            }
        }
        return true;
    }

    private static void syncSharedGameDataIfAvailable(Context context, File externalFilesDir) {
        if (GameDataVerifier.hasRequiredGameData(context)) {
            return;
        }

        File sourceRoot = findSharedSampDirectory(externalFilesDir);
        if (sourceRoot == null) {
            return;
        }

        Log.i(TAG, "Importing shared SAMP data from " + sourceRoot.getAbsolutePath());
        for (String relativePath : SHARED_GAME_DIRECTORIES) {
            copyDirectoryIfAvailable(
                    new File(sourceRoot, relativePath),
                    new File(externalFilesDir, relativePath)
            );
        }
        for (String relativePath : SHARED_ROOT_FILES) {
            copyFileIfChanged(
                    new File(sourceRoot, relativePath),
                    new File(externalFilesDir, relativePath)
            );
        }
        for (String[] mapping : SHARED_SAMP_FILES) {
            copyFileIfChanged(
                    new File(sourceRoot, mapping[0]),
                    new File(externalFilesDir, mapping[1])
            );
        }
        for (String[] mapping : SHARED_DIRECTORY_ALIASES) {
            copyDirectoryIfAvailable(
                    new File(sourceRoot, mapping[0]),
                    new File(externalFilesDir, mapping[1])
            );
        }
    }

    private static File findSharedSampDirectory(File externalFilesDir) {
        File externalStorage = Environment.getExternalStorageDirectory();
        File[] candidates = new File[] {
                new File(externalStorage, "SAMP"),
                new File(getStorageRoot(externalFilesDir), "SAMP")
        };

        for (File candidate : candidates) {
            if (looksLikeSharedGameData(candidate) && !isSameDirectory(candidate, externalFilesDir)) {
                return candidate;
            }
        }
        return null;
    }

    private static File getStorageRoot(File externalFilesDir) {
        String absolutePath = externalFilesDir.getAbsolutePath();
        String androidSegment = File.separator + "Android";
        int androidIndex = absolutePath.indexOf(androidSegment);
        if (androidIndex > 0) {
            return new File(absolutePath.substring(0, androidIndex));
        }
        return Environment.getExternalStorageDirectory();
    }

    private static boolean looksLikeSharedGameData(File directory) {
        return directory != null
                && directory.isDirectory()
                && new File(directory, "data").isDirectory()
                && new File(directory, "texdb").isDirectory();
    }

    private static boolean isSameDirectory(File first, File second) {
        try {
            return first.getCanonicalFile().equals(second.getCanonicalFile());
        } catch (IOException e) {
            return first.getAbsolutePath().equals(second.getAbsolutePath());
        }
    }

    private static void copyDirectoryIfAvailable(File sourceDirectory, File destinationDirectory) {
        if (sourceDirectory == null || !sourceDirectory.isDirectory()) {
            return;
        }

        if (!destinationDirectory.exists() && !destinationDirectory.mkdirs()) {
            return;
        }

        File[] children = sourceDirectory.listFiles();
        if (children == null) {
            return;
        }

        for (File child : children) {
            File destination = new File(destinationDirectory, child.getName());
            if (child.isDirectory()) {
                copyDirectoryIfAvailable(child, destination);
            } else {
                copyFileIfChanged(child, destination);
            }
        }
    }

    private static boolean copyFileIfChanged(File sourceFile, File destinationFile) {
        if (sourceFile == null || !sourceFile.isFile() || sourceFile.length() <= 0L) {
            return false;
        }
        if (destinationFile.exists()
                && destinationFile.isFile()
                && destinationFile.length() == sourceFile.length()) {
            return true;
        }

        ensureParentDirectory(destinationFile);
        try (FileInputStream inputStream = new FileInputStream(sourceFile);
             FileOutputStream outputStream = new FileOutputStream(destinationFile, false)) {
            copyFile(inputStream, outputStream);
            outputStream.flush();
            return true;
        } catch (IOException e) {
            Log.w(TAG, "Could not copy shared game data: " + sourceFile.getAbsolutePath(), e);
            return false;
        }
    }

    private static void syncTouchLayoutAssets(AssetManager assetManager, File externalFilesDir) {
        for (String relativePath : TOUCH_LAYOUT_FILES) {
            copyAsset(assetManager, relativePath, new File(externalFilesDir, relativePath).toString());
            copyAsset(assetManager, relativePath, new File(externalFilesDir, "files/" + relativePath).toString());
        }
    }

    private static boolean touchLayoutAssetsExist(File externalFilesDir) {
        for (String relativePath : TOUCH_LAYOUT_FILES) {
            if (!new File(externalFilesDir, relativePath).exists()
                    || !new File(externalFilesDir, "files/" + relativePath).exists()) {
                return false;
            }
        }
        return true;
    }

    private static void invalidateTouchLayoutCacheIfNeeded(SharedPreferences preferences,
                                                           int savedVersion,
                                                           File externalFilesDir) {
        if (savedVersion == TOUCH_LAYOUT_VERSION) {
            return;
        }

        deleteIfExists(new File(externalFilesDir, "gtasatelem.set"));
        deleteIfExists(new File(externalFilesDir, "files/gtasatelem.set"));

        preferences.edit()
                .putInt(KEY_TOUCH_LAYOUT_VERSION, TOUCH_LAYOUT_VERSION)
                .apply();
    }

    static boolean copyAssetIfNeeded(AssetManager assetManager,
                                     String sourceAsset,
                                     String destinationPath,
                                     boolean overwrite) {
        File outputFile = new File(destinationPath);
        if (!overwrite && outputFile.exists() && outputFile.length() > 0L) {
            return true;
        }
        return copyAsset(assetManager, sourceAsset, destinationPath);
    }

    static boolean copyAsset(AssetManager assetManager, String str, String str2) {
        File outputFile = new File(str2);
        ensureParentDirectory(outputFile);
        if (outputFile.exists() && !outputFile.delete()) {
            return false;
        }

        try (InputStream open = assetManager.open(str);
             FileOutputStream fileOutputStream = new FileOutputStream(outputFile)) {
            copyFile(open, fileOutputStream);
            fileOutputStream.flush();
            return true;
        } catch (Exception e) {
            e.printStackTrace();
            return false;
        }
    }

    static void copyFile(InputStream inputStream, OutputStream outputStream) throws IOException {
        byte[] bArr = new byte[8192];
        while (true) {
            int read = inputStream.read(bArr);
            if (read != -1) {
                outputStream.write(bArr, 0, read);
            } else {
                return;
            }
        }
    }

    private static void ensureParentDirectory(File file) {
        File parent = file.getParentFile();
        if (parent != null && !parent.exists()) {
            parent.mkdirs();
        }
    }

    private static void deleteIfExists(File file) {
        if (file.exists()) {
            file.delete();
        }
    }

    private static void writeDefaultSettings(File file) {
        ensureParentDirectory(file);
        String content = "[client]\n"
                + "name = Player\n"
                + "host = \n"
                + "port = \n"
                + "\n"
                + "[gui]\n"
                + "ChatMaxMessages = 5\n"
                + "VoiceChatEnable = true\n"
                + "androidkeyboard = 0\n"
                + "androidkeyboard23123 = false\n";
        try (FileOutputStream outputStream = new FileOutputStream(file, false)) {
            outputStream.write(content.getBytes(StandardCharsets.UTF_8));
            outputStream.flush();
        } catch (Exception ignored) {
        }
    }

    private static boolean isLauncherRole() {
        return "launcher".equals(BuildConfig.XYRON_APK_ROLE);
    }

    private static boolean isToolsRole() {
        return "tools".equals(BuildConfig.XYRON_APK_ROLE);
    }
}
