package com.supremo.launcher.launcher.util;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.AssetManager;
import android.os.Environment;
import android.util.Log;

import com.supremo.launcher.BuildConfig;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;

public class ConfigValidator {
    private static final String TAG = "ConfigValidator";
    private static volatile String lastValidatedRoot;
    private static volatile long lastValidatedMarker = Long.MIN_VALUE;
    private static final String PREFS_NAME = "supremo_config_validator";
    private static final String KEY_CORE_GAME_FILES_VERSION = "core_game_files_version";
    private static final String KEY_TOUCH_LAYOUT_VERSION = "touch_layout_version";
    private static final int CORE_GAME_FILES_VERSION = 20260518;
    private static final int TOUCH_LAYOUT_VERSION = 20260509;
    private static final long MIN_CORE_TEXT_BYTES = 1024L;
    private static final long CORE_TEXT_BYTES = 1339196L;
    private static final String CORE_TEXT_SHA256 =
            "7d9134def219df21ffd8535643f4c77c1706cd50c66f5613a9671ae49a1971e1";
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

    public static synchronized void validateConfigFiles(Context context) {
        Context appContext = context.getApplicationContext();
        File externalFilesDir = appContext.getExternalFilesDir(null);
        if (externalFilesDir == null) {
            return;
        }

        boolean lightweightRole = isLauncherRole() || isToolsRole();
        long validationMarker = externalFilesDir.lastModified();
        if (externalFilesDir.getAbsolutePath().equals(lastValidatedRoot)
                && validationMarker == lastValidatedMarker) {
            return;
        }
        SharedPreferences preferences = appContext.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);

        if (!lightweightRole) {
            boolean refreshCoreFiles = preferences.getInt(KEY_CORE_GAME_FILES_VERSION, 0) != CORE_GAME_FILES_VERSION
                    || !coreGameFilesExist(externalFilesDir);
            // Seed only missing APK-owned files.  Never replace a valid external
            // GTA/SAMP file while validating a downloaded data set.
            syncCoreGameFiles(appContext.getAssets(), externalFilesDir, false);
            syncSharedSampFilesIfAvailable(appContext, externalFilesDir);
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
            lastValidatedRoot = externalFilesDir.getAbsolutePath();
            lastValidatedMarker = externalFilesDir.lastModified();
            return;
        }

        int savedVersion = preferences.getInt(KEY_TOUCH_LAYOUT_VERSION, 0);
        if (savedVersion != TOUCH_LAYOUT_VERSION || !touchLayoutAssetsExist(externalFilesDir)) {
            syncTouchLayoutAssets(appContext.getAssets(), externalFilesDir);
        }
        invalidateTouchLayoutCacheIfNeeded(preferences, savedVersion, externalFilesDir);
        GameDataVerifier.invalidateCache();
        lastValidatedRoot = externalFilesDir.getAbsolutePath();
        lastValidatedMarker = externalFilesDir.lastModified();

        /*File file2 = new File(externalFilesDir, "gta_sa.set");
        if (!file2.exists()) {
            file2.getParentFile().mkdirs();
            copyAsset(context.getAssets(), "gta_sa.set", file2.toString());
        }*/
    }

    private static void ensureCoreTextDatabase(AssetManager assetManager, File externalFilesDir) {
        // The native text loader requests this file with different casing on
        // different builds.  Android storage is case-sensitive, so keep one
        // valid external copy as the source for all supported spellings.
        String[] outputPaths = new String[] {
                CORE_TEXT_OUTPUT_PATH,
                "text/american.gxt",
                "Text/american.gxt"
        };
        File sourceFile = null;
        for (String outputPath : outputPaths) {
            File candidate = new File(externalFilesDir, outputPath);
            if (isValidCoreTextFile(candidate)) {
                sourceFile = candidate;
                break;
            }
        }

        for (String outputPath : outputPaths) {
            File textDatabase = new File(externalFilesDir, outputPath);
            if (isValidCoreTextFile(textDatabase)) {
                continue;
            }
            if (sourceFile != null) {
                copyFileIfMissing(sourceFile, textDatabase);
            } else {
                copyAssetIfNeeded(
                        assetManager,
                        CORE_TEXT_ASSET_PATH,
                        textDatabase.toString(),
                        false
                );
            }
        }
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
            boolean valid = isCoreTextPath(fileMapping[1])
                    ? isValidCoreTextFile(file)
                    : file.isFile() && file.length() > 0L;
            if (!valid) {
                return false;
            }
        }
        return true;
    }

    private static void syncSharedSampFilesIfAvailable(Context context, File externalFilesDir) {
        if (GameDataVerifier.getCachedMissingRequirement(context) == null
                ? GameDataVerifier.hasRequiredGameData(context)
                : GameDataVerifier.getCachedMissingRequirement(context).isEmpty()) {
            return;
        }

        File sourceRoot = findSharedSampDirectory(externalFilesDir);
        if (sourceRoot == null) {
            return;
        }

        // A shared installation may contain a complete GTA data set for another
        // build.  Import only the explicitly compatible SAMP files; copying
        // texdb/models/fonts/TEXT/data would mix two GTA installations.
        Log.i(TAG, "Importing compatible SAMP files from " + sourceRoot.getAbsolutePath());
        for (String[] mapping : SHARED_SAMP_FILES) {
            copyFileIfMissing(
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

    private static boolean copyFileIfMissing(File sourceFile, File destinationFile) {
        if (destinationFile != null && destinationFile.isFile()
                && (isValidCoreTextFile(destinationFile) || (!isCoreTextFile(sourceFile)
                && destinationFile.length() > 0L))) {
            return true;
        }
        return copyFileIfChanged(sourceFile, destinationFile);
    }

    private static boolean copyFileIfChanged(File sourceFile, File destinationFile) {
        if (sourceFile == null || !sourceFile.isFile() || sourceFile.length() <= 0L
                || destinationFile == null) {
            return false;
        }
        if (destinationFile.isFile() && destinationFile.length() == sourceFile.length()
                && (!isCoreTextFile(sourceFile) || isValidCoreTextFile(destinationFile))) {
            return true;
        }

        ensureParentDirectory(destinationFile);
        File temporaryFile = new File(destinationFile.getPath() + ".tmp-" + System.nanoTime());
        try (FileInputStream inputStream = new FileInputStream(sourceFile);
             FileOutputStream outputStream = new FileOutputStream(temporaryFile, false)) {
            copyFile(inputStream, outputStream);
            outputStream.flush();
            if (!temporaryFile.isFile() || temporaryFile.length() != sourceFile.length()
                    || (isCoreTextFile(sourceFile) && !isValidCoreTextFile(temporaryFile))) {
                return false;
            }
            if (destinationFile.exists() && !destinationFile.delete()) {
                return false;
            }
            return temporaryFile.renameTo(destinationFile);
        } catch (IOException e) {
            Log.w(TAG, "Could not copy shared game data: " + sourceFile.getAbsolutePath(), e);
            return false;
        } finally {
            if (temporaryFile.exists()) {
                temporaryFile.delete();
            }
        }
    }

    private static boolean isCoreTextFile(File file) {
        return file != null && file.isFile() && file.length() == CORE_TEXT_BYTES
                && isValidCoreTextFile(file);
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
        if (!overwrite && outputFile.isFile()
                && (isCoreTextAsset(sourceAsset)
                ? isValidCoreTextFile(outputFile)
                : outputFile.length() > 0L)) {
            return true;
        }
        return copyAsset(assetManager, sourceAsset, destinationPath);
    }

    static boolean copyAsset(AssetManager assetManager, String str, String str2) {
        File outputFile = new File(str2);
        ensureParentDirectory(outputFile);
        File temporaryFile = new File(str2 + ".tmp-" + System.nanoTime());
        try (InputStream open = assetManager.open(str);
             FileOutputStream fileOutputStream = new FileOutputStream(temporaryFile)) {
            copyFile(open, fileOutputStream);
            fileOutputStream.flush();
            if (!temporaryFile.isFile() || temporaryFile.length() <= 0L
                    || (isCoreTextAsset(str) && !isValidCoreTextFile(temporaryFile))) {
                return false;
            }
            if (outputFile.exists() && !outputFile.delete()) {
                return false;
            }
            return temporaryFile.renameTo(outputFile);
        } catch (Exception e) {
            Log.w(TAG, "Could not copy asset: " + str, e);
            return false;
        } finally {
            if (temporaryFile.exists()) {
                temporaryFile.delete();
            }
        }
    }

    private static boolean isCoreTextAsset(String assetPath) {
        return CORE_TEXT_ASSET_PATH.equals(assetPath);
    }

    private static boolean isCoreTextPath(String path) {
        return path != null && ("Text/american.gxt".equals(path)
                || "text/american.gxt".equals(path)
                || "TEXT/AMERICAN.GXT".equals(path));
    }

    private static boolean isValidCoreTextFile(File file) {
        if (file == null || !file.isFile() || file.length() < MIN_CORE_TEXT_BYTES
                || file.length() != CORE_TEXT_BYTES) {
            return false;
        }
        try (FileInputStream inputStream = new FileInputStream(file)) {
            MessageDigest digest = MessageDigest.getInstance("SHA-256");
            byte[] buffer = new byte[8192];
            int read;
            while ((read = inputStream.read(buffer)) != -1) {
                digest.update(buffer, 0, read);
            }
            StringBuilder hash = new StringBuilder();
            for (byte value : digest.digest()) {
                hash.append(String.format(java.util.Locale.US, "%02x", value & 0xff));
            }
            return CORE_TEXT_SHA256.equals(hash.toString());
        } catch (Exception e) {
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
        return "launcher".equals(BuildConfig.SUPREMO_APK_ROLE);
    }

    private static boolean isToolsRole() {
        return "tools".equals(BuildConfig.SUPREMO_APK_ROLE);
    }
}
