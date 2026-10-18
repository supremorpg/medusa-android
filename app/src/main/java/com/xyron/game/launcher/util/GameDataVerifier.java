package com.xyron.game.launcher.util;

import android.content.Context;
import android.os.Build;

import java.io.File;
import java.util.Locale;

public final class GameDataVerifier {
    private static final String[] CORE_DIRECTORIES = new String[] {
            "data",
            "models",
            "texdb"
    };

    private static final String[][] CORE_FILE_GROUPS = new String[][] {
            {"stream.ini"},
            {"SAMP/main.scm", "data/script/mainV1.scm", "data/mainV1.scm"},
            {"SAMP/handling.cfg", "data/handling.cfg"},
            {"models/effects.fxp"},
            {"models/coll/peds.col"},
            {"models/coll/weapons.col"}
    };

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
        String selectedVariantId = DataVariantPreferences.getSelectedVariantIdOrDefault(context);
        String installedVariantId = DataVariantPreferences.getInstalledVariantId(context);
        if (!installedVariantId.isEmpty() && !installedVariantId.equals(selectedVariantId)) {
            return false;
        }
        return hasRequiredGameDataForVariant(context, selectedVariantId);
    }

    public static boolean hasRequiredGameDataForVariant(Context context, String variantId) {
        return getMissingRequirement(context, variantId).isEmpty();
    }

    public static String getMissingRequirement(Context context, String variantId) {
        if (context == null) {
            return "contexto indisponivel";
        }

        String normalizedVariantId = DataVariantPreferences.normalizeVariantId(variantId);
        if (!DataVariantPreferences.isSupportedVariantId(normalizedVariantId)) {
            return "variante de dados invalida";
        }

        File externalFilesDir = context.getExternalFilesDir(null);
        if (externalFilesDir == null) {
            return "diretorio de dados indisponivel";
        }

        for (String relativePath : CORE_DIRECTORIES) {
            File directory = resolvePath(externalFilesDir, relativePath);
            if (!directory.exists() || !directory.isDirectory()) {
                return "diretorio ausente: " + relativePath;
            }
        }

        for (String[] fileGroup : CORE_FILE_GROUPS) {
            if (!hasAnyNonEmptyFile(externalFilesDir, fileGroup)) {
                return "arquivo obrigatorio ausente: " + fileGroup[0];
            }
        }

        for (String texturePackage : REQUIRED_TEXTURE_PACKAGES) {
            if (!hasTexturePackage(externalFilesDir, texturePackage)) {
                return "banco de textura ausente: " + texturePackage;
            }
        }

        if (isArm64Runtime()) {
            for (String texturePackage : ARM64_DXT_TEXTURE_PACKAGES) {
                String texturePath = "texdb/" + texturePackage + "/" + texturePackage;
                if (!hasTextureSet(externalFilesDir, texturePath, "dxt")) {
                    return "banco DXT obrigatorio ausente: " + texturePackage;
                }
            }
        }

        return "";
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
            if (hasNonEmptyFile(baseDirectory, relativePath)) {
                return true;
            }
        }
        return false;
    }

    private static boolean hasTexturePackage(File baseDirectory, String name) {
        String basePath = "texdb/" + name;
        if (hasNonEmptyFile(baseDirectory, basePath + ".img")) {
            return true;
        }
        if (hasTextureSet(baseDirectory, basePath + "/" + name)) {
            return true;
        }

        File textureDirectory = resolvePath(baseDirectory, basePath);
        if (!textureDirectory.exists() || !textureDirectory.isDirectory()) {
            return false;
        }

        boolean hasDat = false;
        boolean hasTmb = false;
        boolean hasToc = false;
        File[] files = textureDirectory.listFiles();
        if (files == null) {
            return false;
        }

        for (File file : files) {
            if (!file.isFile() || file.length() <= 0L) {
                continue;
            }
            String fileName = file.getName().toLowerCase(Locale.US);
            hasDat |= fileName.endsWith(".dat");
            hasTmb |= fileName.endsWith(".tmb");
            hasToc |= fileName.endsWith(".toc");
        }
        return hasDat && hasTmb && hasToc;
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
        String[] parts = normalizedPath.split("/");
        File current = baseDirectory;

        for (String part : parts) {
            if (part.length() == 0) {
                continue;
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
