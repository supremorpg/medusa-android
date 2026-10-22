package com.supremo.launcher.launcher.util;

import android.content.Context;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;

import java.util.Locale;

/**
 * Compatibility storage for the former variant preferences.
 *
 * The updater now has one data source only.  The old keys are deliberately
 * read only as a migration boundary; an old "lite" receipt is never accepted
 * as proof that the current Full data source was installed.
 */
public final class DataVariantPreferences {
    public static final String DATA_VARIANT_FULL = "full";

    private static final String PREF_SELECTED_DATA_VARIANT = "selected_data_variant";
    private static final String PREF_INSTALLED_DATA_VARIANT = "installed_data_variant";
    private static final String PREF_PENDING_DATA_VARIANT = "pending_data_variant";

    private DataVariantPreferences() {
    }

    public static String getSelectedVariantId(Context context) {
        return context == null ? "" : DATA_VARIANT_FULL;
    }

    public static String getSelectedVariantIdOrDefault(Context context) {
        return DATA_VARIANT_FULL;
    }

    public static void saveSelectedVariantId(Context context, String ignoredVariantId) {
        if (context == null) {
            return;
        }
        PreferenceManager.getDefaultSharedPreferences(context)
                .edit()
                .putString(PREF_SELECTED_DATA_VARIANT, DATA_VARIANT_FULL)
                .apply();
    }

    public static String getInstalledVariantId(Context context) {
        if (context == null) {
            return "";
        }

        SharedPreferences preferences = PreferenceManager.getDefaultSharedPreferences(context);
        String value = normalizeVariantId(preferences.getString(PREF_INSTALLED_DATA_VARIANT, ""));
        return DATA_VARIANT_FULL.equals(value) ? DATA_VARIANT_FULL : "";
    }

    public static void saveInstalledVariantId(Context context, String ignoredVariantId) {
        commitInstalledVariant(context, DATA_VARIANT_FULL);
    }

    public static String getPendingVariantId(Context context) {
        if (context == null) {
            return "";
        }
        String value = normalizeVariantId(PreferenceManager.getDefaultSharedPreferences(context)
                .getString(PREF_PENDING_DATA_VARIANT, ""));
        return DATA_VARIANT_FULL.equals(value) ? DATA_VARIANT_FULL : "";
    }

    public static void beginInstall(Context context, String ignoredVariantId) {
        if (context == null) {
            return;
        }
        PreferenceManager.getDefaultSharedPreferences(context)
                .edit()
                .putString(PREF_PENDING_DATA_VARIANT, DATA_VARIANT_FULL)
                .commit();
    }

    public static void rollbackInstall(Context context) {
        if (context == null) {
            return;
        }
        PreferenceManager.getDefaultSharedPreferences(context)
                .edit()
                .remove(PREF_PENDING_DATA_VARIANT)
                .commit();
    }

    public static boolean commitInstalledVariant(Context context, String ignoredVariantId) {
        if (context == null) {
            return false;
        }
        return PreferenceManager.getDefaultSharedPreferences(context)
                .edit()
                .putString(PREF_INSTALLED_DATA_VARIANT, DATA_VARIANT_FULL)
                .putString(PREF_SELECTED_DATA_VARIANT, DATA_VARIANT_FULL)
                .remove(PREF_PENDING_DATA_VARIANT)
                .commit();
    }

    public static boolean hasCommittedVariant(Context context, String ignoredVariantId) {
        return DATA_VARIANT_FULL.equals(getInstalledVariantId(context));
    }

    public static boolean shouldForceRefreshForVariant(Context context, String ignoredVariantId) {
        // A manually copied, structurally valid data set is valid even without
        // a receipt.  The service decides whether a refresh is needed from the
        // manifest and file integrity, not from the legacy variant marker.
        return false;
    }

    public static boolean isSupportedVariantId(String variantId) {
        return DATA_VARIANT_FULL.equals(normalizeVariantId(variantId));
    }

    public static String normalizeVariantId(String variantId) {
        if (variantId == null || variantId.trim().isEmpty()) {
            return DATA_VARIANT_FULL;
        }
        String normalized = variantId.trim().toLowerCase(Locale.US);
        return "lite".equals(normalized) || "full".equals(normalized)
                ? DATA_VARIANT_FULL
                : normalized;
    }
}
