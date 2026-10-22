package com.supremo.launcher.launcher.util;

import android.content.Context;
import android.os.Build;

import java.io.File;
import java.io.FileInputStream;
import java.io.RandomAccessFile;
import java.io.IOException;
import java.security.MessageDigest;
import java.util.Locale;

public final class GameDataVerifier {
    private static final String[] CORE_DIRECTORIES = new String[] {
            "data",
            "models",
            "texdb"
    };

    private static final String[][] CORE_FILE_GROUPS = new String[][] {
            {"stream.ini"},
            {"TEXT/AMERICAN.GXT", "Text/american.gxt", "text/american.gxt"},
            {"SAMP/main.scm", "data/script/mainV1.scm", "data/mainV1.scm"},
            {"SAMP/handling.cfg", "data/handling.cfg"},
            {"models/effects.fxp"},
            {"models/coll/peds.col"},
            {"models/coll/weapons.col"}
    };

    private static final long CORE_TEXT_BYTES = 1339196L;
    private static final String CORE_TEXT_SHA256 =
            "7d9134def219df21ffd8535643f4c77c1706cd50c66f5613a9671ae49a1971e1";

    private static volatile VerificationCache verificationCache;

    private static final String[] REQUIRED_TEXTURE_PACKAGES = new String[] {
            "gta3",
            "samp",
            "player",
            "txd",
            "gta_int",
            "mobile"
    };

    private static final String[] ARM64_DXT_TEXTURE_PACKAGES = new String[] {
            "txd",
            "gta3",
            "gta_int",
            "mobile"
    };

    private GameDataVerifier() {
    }

    public static boolean hasRequiredGameData(Context context) {
        return getMissingRequirement(context, DataVariantPreferences.DATA_VARIANT_FULL).isEmpty();
    }

    public static boolean hasRequiredGameDataForVariant(Context context, String ignoredVariantId) {
        return hasRequiredGameData(context);
    }

    /**
     * Returns a cached result from the most recent complete verification. The cache is only
     * reused while the data root has the same modification marker, so an update or replacement
     * invalidates it without moving verification back onto the launcher UI thread.
     */
    public static String getCachedMissingRequirement(Context context) {
        if (context == null) {
            return "contexto indisponivel";
        }
        File externalFilesDir = context.getApplicationContext().getExternalFilesDir(null);
        if (externalFilesDir == null) {
            return "diretorio de dados indisponivel";
        }
        VerificationCache cached = verificationCache;
        long marker = dataRootMarker(externalFilesDir);
        if (cached != null && cached.rootPath.equals(externalFilesDir.getAbsolutePath())
                && cached.marker == marker) {
            return cached.missingRequirement;
        }
        return null;
    }

    public static void invalidateCache() {
        verificationCache = null;
    }

    public static String getMissingRequirement(Context context, String ignoredVariantId) {
        if (context == null) {
            return "contexto indisponivel";
        }

        File externalFilesDir = context.getExternalFilesDir(null);
        if (externalFilesDir == null) {
            return "diretorio de dados indisponivel";
        }

        VerificationCache cached = verificationCache;
        long marker = dataRootMarker(externalFilesDir);
        if (cached != null && cached.rootPath.equals(externalFilesDir.getAbsolutePath())
                && cached.marker == marker) {
            return cached.missingRequirement;
        }

        for (String relativePath : CORE_DIRECTORIES) {
            File directory = resolvePath(externalFilesDir, relativePath);
            if (!directory.exists() || !directory.isDirectory()) {
                return cacheResult(externalFilesDir, "diretorio ausente: " + relativePath);
            }
        }

        for (String[] fileGroup : CORE_FILE_GROUPS) {
            if (!hasAnyNonEmptyFile(externalFilesDir, fileGroup)) {
                return cacheResult(externalFilesDir, "arquivo obrigatorio ausente: " + fileGroup[0]);
            }
        }

        for (String texturePackage : REQUIRED_TEXTURE_PACKAGES) {
            if (!hasTexturePackage(externalFilesDir, texturePackage)) {
                return cacheResult(externalFilesDir, "banco de textura ausente ou invalido: " + texturePackage);
            }
        }

        if (isArm64Runtime()) {
            for (String texturePackage : ARM64_DXT_TEXTURE_PACKAGES) {
                String texturePath = "texdb/" + texturePackage + "/" + texturePackage;
                if (!hasTextureSet(externalFilesDir, texturePath, "dxt")) {
                    return cacheResult(externalFilesDir, "banco DXT obrigatorio ausente: " + texturePackage);
                }
            }
        }

        return cacheResult(externalFilesDir, "");
    }

    private static String cacheResult(File externalFilesDir, String missingRequirement) {
        verificationCache = new VerificationCache(
                externalFilesDir.getAbsolutePath(),
                dataRootMarker(externalFilesDir),
                missingRequirement
        );
        return missingRequirement;
    }

    private static long dataRootMarker(File externalFilesDir) {
        long marker = externalFilesDir.lastModified();
        for (String relativePath : CORE_DIRECTORIES) {
            marker = appendFileTreeMarker(externalFilesDir, relativePath, marker);
        }
        return marker;
    }

    private static long appendFileTreeMarker(File baseDirectory, String relativePath, long marker) {
        File file = resolvePath(baseDirectory, relativePath);
        marker = 31L * marker + relativePath.hashCode();
        marker = 31L * marker + file.lastModified();
        marker = 31L * marker + file.length();
        if (file.isDirectory()) {
            File[] children = file.listFiles();
            if (children != null) {
                java.util.Arrays.sort(children,
                        (left, right) -> left.getName().compareTo(right.getName()));
                for (File child : children) {
                    marker = appendFileTreeMarker(
                            baseDirectory,
                            relativePath + "/" + child.getName(),
                            marker
                    );
                }
            }
        }
        return marker;
    }

    private static final class VerificationCache {
        final String rootPath;
        final long marker;
        final String missingRequirement;

        VerificationCache(String rootPath, long marker, String missingRequirement) {
            this.rootPath = rootPath;
            this.marker = marker;
            this.missingRequirement = missingRequirement;
        }
    }

    public static boolean isArm64Runtime() {
        String[] abis = Build.SUPPORTED_64_BIT_ABIS;
        if (abis == null) {
            return false;
        }
        for (String abi : abis) {
            if ("arm64-v8a".equalsIgnoreCase(abi)) {
                return true;
            }
        }
        return false;
    }

    private static boolean hasAnyNonEmptyFile(File baseDirectory, String[] relativePaths) {
        for (String relativePath : relativePaths) {
            if (isCoreTextPath(relativePath)
                    ? isValidCoreTextFile(resolvePath(baseDirectory, relativePath))
                    : hasNonEmptyFile(baseDirectory, relativePath)) {
                return true;
            }
        }
        return false;
    }

    private static boolean isCoreTextPath(String path) {
        return path != null && ("TEXT/AMERICAN.GXT".equals(path)
                || "Text/american.gxt".equals(path)
                || "text/american.gxt".equals(path));
    }

    private static boolean isValidCoreTextFile(File file) {
        if (file == null || !file.isFile() || file.length() != CORE_TEXT_BYTES) {
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
                hash.append(String.format(Locale.US, "%02x", value & 0xff));
            }
            return CORE_TEXT_SHA256.equals(hash.toString());
        } catch (Exception ignored) {
            return false;
        }
    }

    private static boolean hasTexturePackage(File baseDirectory, String name) {
        String basePath = "texdb/" + name;
        File image = resolvePath(baseDirectory, basePath + ".img");
        if (image.isFile() && isValidImgArchive(image)) {
            return true;
        }
        if (hasTextureSet(baseDirectory, basePath + "/" + name)) {
            return true;
        }
        return false;
    }

    private static boolean hasTextureSet(File baseDirectory, String basePath) {
        return hasTextureSet(baseDirectory, basePath, "dxt")
                || hasTextureSet(baseDirectory, basePath, "etc")
                || hasTextureSet(baseDirectory, basePath, "pvr");
    }

    private static boolean hasTextureSet(File baseDirectory, String basePath, String format) {
        return hasNonEmptyFile(baseDirectory, basePath + "." + format + ".dat")
                && hasNonEmptyFile(baseDirectory, basePath + "." + format + ".tmb")
                && hasNonEmptyFile(baseDirectory, basePath + "." + format + ".toc");
    }

    private static boolean isValidImgArchive(File file) {
        if (file == null || !file.isFile() || file.length() < 8L) {
            return false;
        }
        try (RandomAccessFile input = new RandomAccessFile(file, "r")) {
            byte[] signature = new byte[4];
            input.readFully(signature);
            String version = new String(signature, "US-ASCII");
            if (!"VER2".equals(version)) {
                return false;
            }
            long entryCount = readLittleEndianInt(input);
            long tableBytes = entryCount * 32L;
            if (entryCount <= 0L || tableBytes > input.length() - 8L
                    || tableBytes > 64L * 1024L * 1024L) {
                return false;
            }
            long fileSectors = input.length() / 2048L;
            for (long index = 0; index < entryCount; index++) {
                long offsetSectors = readLittleEndianInt(input) & 0xffffffffL;
                long sizeSectors = readLittleEndianShort(input) & 0xffffL;
                if (offsetSectors > fileSectors
                        || sizeSectors > fileSectors - offsetSectors) {
                    return false;
                }
                input.skipBytes(26);
            }
            return true;
        } catch (IOException | RuntimeException ignored) {
            return false;
        }
    }

    private static int readLittleEndianInt(RandomAccessFile input) throws IOException {
        int b0 = input.readUnsignedByte();
        int b1 = input.readUnsignedByte();
        int b2 = input.readUnsignedByte();
        int b3 = input.readUnsignedByte();
        return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
    }

    private static int readLittleEndianShort(RandomAccessFile input) throws IOException {
        return input.readUnsignedByte() | (input.readUnsignedByte() << 8);
    }

    private static boolean hasNonEmptyFile(File baseDirectory, String relativePath) {
        File file = resolvePath(baseDirectory, relativePath);
        return file.exists() && file.isFile() && file.length() > 0L;
    }

    private static File resolvePath(File baseDirectory, String relativePath) {
        File directFile = new File(baseDirectory, relativePath);
        if (directFile.exists()) {
            return directFile;
        }

        String normalizedPath = relativePath.replace('\\', '/');
        if (normalizedPath.startsWith("/") || normalizedPath.matches("^[A-Za-z]:.*")) {
            return new File(baseDirectory, "__invalid__");
        }
        String[] parts = normalizedPath.split("/", -1);
        File current = baseDirectory;

        for (String part : parts) {
            if (part.length() == 0 || ".".equals(part) || "..".equals(part)
                    || part.indexOf(':') >= 0) {
                return new File(baseDirectory, "__invalid__");
            }
            File[] children = current.listFiles();
            File matchedChild = null;
            if (children != null) {
                for (File child : children) {
                    if (child.getName().equalsIgnoreCase(part)) {
                        matchedChild = child;
                        break;
                    }
                }
            }
            if (matchedChild == null) {
                return new File(current, part);
            }
            current = matchedChild;
        }

        return current;
    }
}
