package com.supremo.launcher.launcher.util;

import android.content.Context;
import android.content.res.AssetManager;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

public final class UpdateSourceResolver {
    private static final String CONFIG_ASSET_PATH = "update_sources.json";
    private static final String DEFAULT_REPO_ID = "Soiza/gta-data-full";
    private static final String DEFAULT_REPO_TYPE = "dataset";
    private static final String DEFAULT_REVISION = "main";
    private static final String DEFAULT_CLIENT_CONFIG_PATH = "client_config.json";

    private UpdateSourceResolver() {
    }

    public static UpdateSourceConfig resolve(Context context) {
        return resolve(context, DataVariantPreferences.DATA_VARIANT_FULL);
    }

    /**
     * The second argument remains only for wire compatibility with old callers.
     * There is no longer a Lite/Full source selection.
     */
    public static UpdateSourceConfig resolve(Context context, String ignoredVariantId) {
        JSONObject config = loadJsonAsset(
                context == null ? null : context.getAssets(),
                CONFIG_ASSET_PATH
        );

        JSONObject huggingFace = config == null
                ? null
                : config.optJSONObject("huggingface");
        if (huggingFace == null) {
            huggingFace = new JSONObject();
        }

        String repoId = sanitize(huggingFace.optString("repo_id"));
        if (!isValidRepoId(repoId)) {
            repoId = DEFAULT_REPO_ID;
        }
        String repoType = sanitize(huggingFace.optString("repo_type"));
        if (repoType.isEmpty()) {
            repoType = DEFAULT_REPO_TYPE;
        }
        String revision = sanitize(huggingFace.optString("revision"));
        if (revision.isEmpty()) {
            revision = DEFAULT_REVISION;
        }
        String clientConfigPath = sanitize(huggingFace.optString("client_config_path"));
        if (clientConfigPath.isEmpty()) {
            clientConfigPath = DEFAULT_CLIENT_CONFIG_PATH;
        }

        String treeApiUrl = buildHuggingFaceTreeApiUrl(repoId, repoType, revision);
        String resolveBaseUrl = buildHuggingFaceResolveBaseUrl(repoId, repoType, revision);
        LinkedHashSet<String> clientConfigUrls = new LinkedHashSet<>();
        LinkedHashSet<String> fallbackFileBaseUrls = new LinkedHashSet<>();

        String clientConfigUrl = buildHuggingFaceUrl(
                repoId,
                repoType,
                revision,
                clientConfigPath
        );
        if (isHttpUrl(clientConfigUrl)) {
            clientConfigUrls.add(clientConfigUrl);
        }

        // Root paths in gta-data-full are resolved directly below this base.
        if (isHttpUrl(resolveBaseUrl)) {
            fallbackFileBaseUrls.add(resolveBaseUrl);
        }

        // Keep explicitly configured non-variant sources usable, but never read
        // data_variants or reintroduce the old Lite repository.
        if (config != null) {
            addHttpValues(clientConfigUrls, config.optJSONArray("client_config_urls"));
            addHttpValues(fallbackFileBaseUrls, config.optJSONArray("fallback_file_base_urls"));
        }

        return new UpdateSourceConfig(
                new ArrayList<>(clientConfigUrls),
                new ArrayList<>(fallbackFileBaseUrls),
                treeApiUrl,
                resolveBaseUrl,
                "",
                ""
        );
    }

    private static JSONObject loadJsonAsset(AssetManager assetManager, String assetPath) {
        if (assetManager == null) {
            return null;
        }
        try (InputStream inputStream = assetManager.open(assetPath);
             ByteArrayOutputStream outputStream = new ByteArrayOutputStream()) {
            byte[] buffer = new byte[4096];
            int read;
            while ((read = inputStream.read(buffer)) != -1) {
                outputStream.write(buffer, 0, read);
            }
            String raw = outputStream.toString(StandardCharsets.UTF_8.name());
            return new JSONObject(raw);
        } catch (Exception ignored) {
            return null;
        }
    }

    private static void addHttpValues(Set<String> target, JSONArray values) {
        if (values == null) {
            return;
        }
        for (int i = 0; i < values.length(); i++) {
            String value = sanitize(values.optString(i));
            if (isHttpUrl(value)) {
                target.add(value);
            }
        }
    }

    private static String buildHuggingFaceUrl(
            String repoId,
            String repoType,
            String revision,
            String path
    ) {
        String base = buildHuggingFaceResolveBaseUrl(repoId, repoType, revision);
        String normalizedPath = trimLeadingSlashes(path);
        return normalizedPath.isEmpty() ? base : base + "/" + normalizedPath;
    }

    private static String buildHuggingFaceResolveBaseUrl(
            String repoId,
            String repoType,
            String revision
    ) {
        return "https://huggingface.co/"
                + resolveRepoTypePathSegment(repoType)
                + trimSlashes(repoId)
                + "/resolve/"
                + trimSlashes(revision);
    }

    private static String buildHuggingFaceTreeApiUrl(
            String repoId,
            String repoType,
            String revision
    ) {
        return "https://huggingface.co/api/"
                + resolveRepoTypePathSegment(repoType)
                + trimSlashes(repoId)
                + "/tree/"
                + trimSlashes(revision)
                + "?recursive=true&expand=true&limit=50";
    }

    private static String resolveRepoTypePathSegment(String repoType) {
        if ("dataset".equalsIgnoreCase(repoType) || "datasets".equalsIgnoreCase(repoType)) {
            return "datasets/";
        }
        if ("space".equalsIgnoreCase(repoType) || "spaces".equalsIgnoreCase(repoType)) {
            return "spaces/";
        }
        return "models/";
    }

    private static boolean isValidRepoId(String repoId) {
        return !repoId.isEmpty()
                && !repoId.contains("SEU_USUARIO")
                && !repoId.contains("SEU_REPO")
                && !repoId.contains("<")
                && !repoId.contains(">")
                && !repoId.startsWith("/")
                && !repoId.contains("..")
                && !repoId.contains(" ");
    }

    private static boolean isHttpUrl(String value) {
        return value != null
                && (value.startsWith("http://") || value.startsWith("https://"));
    }

    private static String sanitize(String value) {
        return value == null ? "" : value.trim();
    }

    private static String trimSlashes(String value) {
        String sanitized = sanitize(value);
        while (sanitized.startsWith("/")) {
            sanitized = sanitized.substring(1);
        }
        while (sanitized.endsWith("/")) {
            sanitized = sanitized.substring(0, sanitized.length() - 1);
        }
        return sanitized;
    }

    private static String trimLeadingSlashes(String value) {
        String sanitized = sanitize(value);
        while (sanitized.startsWith("/")) {
            sanitized = sanitized.substring(1);
        }
        return sanitized;
    }

    public static final class UpdateSourceConfig {
        public final List<String> clientConfigUrls;
        public final List<String> fallbackFileBaseUrls;
        public final String huggingFaceTreeApiUrl;
        public final String huggingFaceResolveBaseUrl;
        public final String huggingFaceFilesPathPrefix;
        public final String huggingFaceSupplementalFilesPathPrefix;

        public UpdateSourceConfig(
                List<String> clientConfigUrls,
                List<String> fallbackFileBaseUrls,
                String huggingFaceTreeApiUrl,
                String huggingFaceResolveBaseUrl,
                String huggingFaceFilesPathPrefix,
                String huggingFaceSupplementalFilesPathPrefix
        ) {
            this.clientConfigUrls = clientConfigUrls;
            this.fallbackFileBaseUrls = fallbackFileBaseUrls;
            this.huggingFaceTreeApiUrl = huggingFaceTreeApiUrl;
            this.huggingFaceResolveBaseUrl = huggingFaceResolveBaseUrl;
            this.huggingFaceFilesPathPrefix = huggingFaceFilesPathPrefix;
            this.huggingFaceSupplementalFilesPathPrefix = huggingFaceSupplementalFilesPathPrefix;
        }
    }
}
