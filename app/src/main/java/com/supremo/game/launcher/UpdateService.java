package com.supremo.launcher.launcher;

import android.app.Service;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.Looper;
import android.os.Message;
import android.os.Messenger;
import android.os.RemoteException;
import android.util.Log;

import com.android.volley.Response;
import com.android.volley.VolleyError;
import com.android.volley.toolbox.StringRequest;
import com.android.volley.toolbox.Volley;
import com.downloader.Error;
import com.downloader.OnDownloadListener;
import com.downloader.OnProgressListener;
import com.downloader.PRDownloader;
import com.downloader.PRDownloaderConfig;
import com.downloader.Progress;
import com.supremo.launcher.launcher.data.FilesData;
import com.supremo.launcher.launcher.util.DataVariantPreferences;
import com.supremo.launcher.launcher.util.GameDataVerifier;
import com.supremo.launcher.launcher.util.UpdateSourceResolver;
import com.supremo.launcher.launcher.util.Util;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.file.AtomicMoveNotSupportedException;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.URL;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.Locale;
import java.util.Set;

public class UpdateService extends Service {
    private static final String DEFAULT_DATA_VARIANT_ID = DataVariantPreferences.DATA_VARIANT_FULL;

    Messenger mMessenger;
    Messenger mActivityMessenger;
    IncomingHandler mInHandler;

    public UpdateActivity.GameStatus mGameStatus = UpdateActivity.GameStatus.Unknown;
    public UpdateActivity.UpdateStatus mUpdateStatus = UpdateActivity.UpdateStatus.Undefined;

    public boolean mDownloadingStatus = false;

    public long mUpdateGameDataSize = 0;
    public long mUpdateGameDataSizeUpdated = 0;
    public String mUpdateGameURL = "";
    public int mUpdateVersion;

    public ArrayList<String> mUpdateFiles;
    public ArrayList<String> mUpdateFilesName;
    public ArrayList<Long> mUpdateFilesSize;
    public ArrayList<String> mUpdateFilesUrl;
    public ArrayList<String> mUpdateFilesHash;
    public ArrayList<String> mClientConfigUrls;
    public ArrayList<String> mFallbackFileBaseUrls;
    public String mHuggingFaceTreeApiUrl = "";
    public String mHuggingFaceResolveBaseUrl = "";
    public String mHuggingFaceFilesPathPrefix = "";
    public String mHuggingFaceSupplementalFilesPathPrefix = "";
    public String mDataVariantId = DEFAULT_DATA_VARIANT_ID;
    public boolean mForceReinstallData = false;
    private String mLastDataDownloadFailure = "";
    private boolean mDataTerminalOutcome = false;
    private final Set<String> mQueuedDataPaths = new HashSet<>();

    public int mGpuType = 0;

    @Override
    public void onCreate() {
        super.onCreate();
        HandlerThread handlerThread = new HandlerThread("ServiceStartArguments", 10);
        handlerThread.start();
        PRDownloader.initialize(getApplicationContext(), PRDownloaderConfig.newBuilder().setDatabaseEnabled(true).setReadTimeout(30000).setConnectTimeout(30000).build());
        mInHandler = new IncomingHandler(handlerThread.getLooper());
        mMessenger = new Messenger(mInHandler);
    }

    @Override
    public IBinder onBind(Intent intent) {
        return mMessenger.getBinder();
    }

    class IncomingHandler extends Handler {
        public IncomingHandler(Looper looper) {
            super(looper);
        }

        @Override
        public void handleMessage(Message msg) {
            mActivityMessenger = msg.replyTo;
            Message obtain = null;
            Messenger messenger = null;
            if (msg.what == 0) {
                mGpuType = msg.getData().getInt("gputype");
                if(mGpuType == 0)
                {
                    Log.e("x1y2z", "GPU not found");
                    return;
                }
                updateDataVariantFromMessage(msg);
                startUpdating();
            } else if (msg.what == 1) {
                startGameUpdateChecking();
            } else if(msg.what == 2) {
                updateGame();
            } else if (msg.what == 4) {
                obtain = Message.obtain(mInHandler, 4);
                obtain.getData().putString("status", mUpdateStatus.name());
                UpdateService updateService = UpdateService.this;
                obtain.replyTo = updateService.mMessenger;
                messenger = updateService.mActivityMessenger;
                try {
                    messenger.send(obtain);
                } catch (RemoteException e5) {
                    e5.printStackTrace();
                }
            } else if (msg.what == 5) {
                obtain = Message.obtain(mInHandler, 5);
                obtain.getData().putString("status", mGameStatus.name());
                obtain.replyTo = mMessenger;
                try {
                    mActivityMessenger.send(obtain);
                } catch (RemoteException e5) {
                    e5.printStackTrace();
                }

            } else if (msg.what == 7) {
                mGpuType = msg.getData().getInt("gputype");
                if(mGpuType == 0)
                {
                    Log.e("x1y2z", "GPU not found");
                    return;
                }
                updateDataVariantFromMessage(msg);
                startUpdating();
            }

        }
    }

    void startUpdating()
    {
        resetUpdateState();
        loadUpdateSources();
        setUpdateStatus(UpdateActivity.UpdateStatus.CheckUpdate);
        requestClientConfig(0);
    }

    private void loadUpdateSources() {
        UpdateSourceResolver.UpdateSourceConfig sourceConfig = UpdateSourceResolver.resolve(this, mDataVariantId);
        mClientConfigUrls = new ArrayList<>(sourceConfig.clientConfigUrls);
        mFallbackFileBaseUrls = new ArrayList<>(sourceConfig.fallbackFileBaseUrls);
        mHuggingFaceTreeApiUrl = sourceConfig.huggingFaceTreeApiUrl;
        mHuggingFaceResolveBaseUrl = sourceConfig.huggingFaceResolveBaseUrl;
        mHuggingFaceFilesPathPrefix = sourceConfig.huggingFaceFilesPathPrefix;
        mHuggingFaceSupplementalFilesPathPrefix = sourceConfig.huggingFaceSupplementalFilesPathPrefix;
    }

    private void updateDataVariantFromMessage(Message msg) {
        mForceReinstallData = msg.getData().getBoolean(UpdateActivity.EXTRA_FORCE_REINSTALL_DATA, false);
        String requestedMode = msg.getData().getString("data_mode");
        if (requestedMode == null || requestedMode.trim().isEmpty()) {
            requestedMode = msg.getData().getString("data_variant");
        }
        String normalizedVariantId = DataVariantPreferences.normalizeVariantId(requestedMode);
        if (!DataVariantPreferences.isSupportedVariantId(normalizedVariantId)) {
            normalizedVariantId = DEFAULT_DATA_VARIANT_ID;
        }

        mDataVariantId = normalizedVariantId;
        DataVariantPreferences.saveSelectedVariantId(this, mDataVariantId);
        DataVariantPreferences.beginInstall(this, mDataVariantId);
        if (!mForceReinstallData && DataVariantPreferences.shouldForceRefreshForVariant(this, mDataVariantId)) {
            Log.d("UpdateService", "Forcing refresh for game data");
            mForceReinstallData = true;
        }
    }

    private void requestClientConfig(int index) {
        if (mClientConfigUrls == null || index >= mClientConfigUrls.size()) {
            handleClientConfigFailure();
            return;
        }

        String clientConfigUrl = mClientConfigUrls.get(index);
        Volley.newRequestQueue(getApplicationContext()).add(new StringRequest(clientConfigUrl, new Response.Listener<String>() {
            @Override
            public void onResponse(String response) {
                try {
                    JSONObject jSONObject = new JSONObject(response).getJSONObject("client_config");
                    mUpdateVersion = jSONObject.getInt("version_code");
                    mUpdateGameURL = resolvePossiblyRelativeUrl(
                            clientConfigUrl,
                            jSONObject.optString("url_launcher")
                    );

                    String string = resolvePossiblyRelativeUrl(
                            clientConfigUrl,
                            jSONObject.optString("url_cache_files")
                    );
                    if (string.isEmpty()) {
                        requestClientConfig(index + 1);
                        return;
                    }

                    getFilesInfo(string);

                    if (!isGameUpdateExists()) {
                        if (mUpdateFiles.isEmpty()) {
                            mGameStatus = UpdateActivity.GameStatus.Updated;
                        }
                        else {
                            mGameStatus = UpdateActivity.GameStatus.UpdateRequired;
                        }
                    }
                    else {
                        mGameStatus = UpdateActivity.GameStatus.GameUpdateRequired;
                    }

                    setUpdateStatus(UpdateActivity.UpdateStatus.Undefined);

                } catch (Exception e) {
                    e.printStackTrace();
                    requestClientConfig(index + 1);
                }
            }
        }, new Response.ErrorListener() {
            @Override
            public void onErrorResponse(VolleyError error) {
                Log.d("x1y2z", "error " + error.toString());
                requestClientConfig(index + 1);
            }
        }));
    }

    private void handleClientConfigFailure() {
        if (canUseHuggingFaceTreeFallback()) {
            requestHuggingFaceTreeManifest();
            return;
        }
        handleSourceUnavailable();
    }

    private void handleSourceUnavailable() {
        mGameStatus = UpdateActivity.GameStatus.Unknown;
        DataVariantPreferences.rollbackInstall(this);
        setUpdateStatus(UpdateActivity.UpdateStatus.SourceUnavailable);
        notifyGameStatus();
    }

    private void setDataDownloadFailed(String reason) {
        mUpdateStatus = UpdateActivity.UpdateStatus.DataDownloadFailed;
        Message obtain = Message.obtain(mInHandler, 4);
        obtain.getData().putString("status", mUpdateStatus.name());
        obtain.getData().putString("reason", sanitize(reason).isEmpty()
                ? "Um ou mais arquivos obrigatorios nao puderam ser baixados."
                : sanitize(reason));
        obtain.replyTo = mMessenger;
        if (mActivityMessenger != null) {
            try {
                mActivityMessenger.send(obtain);
            } catch (RemoteException e) {
                e.printStackTrace();
            }
        }
    }

    private boolean canUseHuggingFaceTreeFallback() {
        return !sanitize(mHuggingFaceTreeApiUrl).isEmpty()
                && !sanitize(mHuggingFaceResolveBaseUrl).isEmpty();
    }

    private void requestHuggingFaceTreeManifest() {
        new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    boolean loaded = getFilesInfoFromHuggingFaceTree();
                    if (!loaded) {
                        handleSourceUnavailable();
                        return;
                    }

                    mUpdateVersion = getInstalledVersionCode();
                    mUpdateGameURL = "";
                    finalizeManifestLoad();
                } catch (Exception e) {
                    Log.e("x1y2z", "hugging face tree fallback failed", e);
                    handleSourceUnavailable();
                }
            }
        }).start();
    }

    private void finalizeManifestLoad() {
        if (!isGameUpdateExists()) {
            if (mUpdateFiles.isEmpty()) {
                mGameStatus = UpdateActivity.GameStatus.Updated;
            } else {
                mGameStatus = UpdateActivity.GameStatus.UpdateRequired;
            }
        } else {
            mGameStatus = UpdateActivity.GameStatus.GameUpdateRequired;
        }

        setUpdateStatus(UpdateActivity.UpdateStatus.Undefined);
        notifyGameStatus();
    }

    private void notifyGameStatus() {
        Message obtain = Message.obtain(mInHandler, 5);
        obtain.getData().putString("status", mGameStatus.name());
        obtain.replyTo = mMessenger;
        if (mActivityMessenger != null) {
            try {
                mActivityMessenger.send(obtain);
            } catch (RemoteException e5) {
                e5.printStackTrace();
            }
        }
    }

    public void getFilesInfo(String response) throws IOException, JSONException {
        Util.responseFiles = "";
        Util.responseFilesInt = 0;
        new Thread(new Runnable() {
            @Override
            public void run() {
                HttpURLConnection connection = null;
                BufferedReader reader = null;

                try {
                    URL url = new URL(response);
                    connection = (HttpURLConnection) url.openConnection();
                    connection.connect();


                    InputStream stream = connection.getInputStream();

                    reader = new BufferedReader(new InputStreamReader(stream));

                    StringBuffer buffer = new StringBuffer();
                    String line = "";

                    while ((line = reader.readLine()) != null) {
                        buffer.append(line+"\n");

                    }

                    Util.responseFiles = buffer.toString();
                    Util.responseFilesInt = 1;


                } catch (MalformedURLException e) {
                    e.printStackTrace();
                    Util.responseFilesInt = 2;
                } catch (IOException e) {
                    e.printStackTrace();
                    Util.responseFilesInt = 2;
                }
            }
        }).start();
        int i5;
        while (true) {
            i5 = Util.responseFilesInt;
            if (i5 != 0) {
                break;
            }
            try {
                Thread.sleep(30);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }

        if(i5 == 2) {
            throw new IOException("Unable to fetch files manifest: " + response);
        }

        Log.d("x1y2z", "Info: " + Util.responseFiles);
        JSONObject jsonObject = new JSONObject(Util.responseFiles);
        JSONArray jsonArray = jsonObject.getJSONArray("files");
        Log.d("x1y2z", "Length: " + jsonArray.length());
        for(int i = 0; i<jsonArray.length(); i++) {
            JSONObject fileObject = jsonArray.getJSONObject(i);
            FilesData fileData = new FilesData(
                    fileObject.getString("name"),
                    fileObject.optLong("size", 0L),
                    fileObject.getString("path"),
                    fileObject.optString("url"),
                    fileObject.optString("hash", fileObject.optString("sha256", ""))
            );
            enqueueMissingFile(fileData, response);
        }
    }

    private boolean getFilesInfoFromHuggingFaceTree() throws IOException, JSONException {
        String nextUrl = mHuggingFaceTreeApiUrl;
        ArrayList<JSONObject> fileEntries = new ArrayList<>();
        Set<String> remotePaths = new HashSet<>();

        while (!sanitize(nextUrl).isEmpty()) {
            HttpURLConnection connection = null;
            BufferedReader reader = null;

            try {
                connection = (HttpURLConnection) new URL(nextUrl).openConnection();
                connection.setConnectTimeout(30000);
                connection.setReadTimeout(30000);
                connection.connect();

                int responseCode = connection.getResponseCode();
                if (responseCode < 200 || responseCode >= 300) {
                    throw new IOException("Unexpected Hugging Face response code: " + responseCode);
                }

                reader = new BufferedReader(new InputStreamReader(connection.getInputStream()));
                StringBuilder buffer = new StringBuilder();
                String line;

                while ((line = reader.readLine()) != null) {
                    buffer.append(line);
                }

                JSONArray entries = new JSONArray(buffer.toString());
                for (int i = 0; i < entries.length(); i++) {
                    JSONObject item = entries.optJSONObject(i);
                    if (item == null || !"file".equalsIgnoreCase(item.optString("type"))) {
                        continue;
                    }

                    String remotePath = sanitize(item.optString("path"));
                    if (remotePath.isEmpty()) {
                        continue;
                    }

                    fileEntries.add(item);
                    remotePaths.add(remotePath);
                }

                nextUrl = parseNextLink(connection.getHeaderField("Link"));
            } finally {
                if (reader != null) {
                    try {
                        reader.close();
                    } catch (IOException ignored) {
                    }
                }
                if (connection != null) {
                    connection.disconnect();
                }
            }
        }

        if (fileEntries.isEmpty()) {
            return false;
        }

        boolean hasUsableRemoteFile = false;
        for (JSONObject entry : fileEntries) {
            FilesData fileData = buildHuggingFaceFileData(entry, remotePaths);
            if (fileData != null) {
                hasUsableRemoteFile = true;
                enqueueMissingFile(fileData, mHuggingFaceResolveBaseUrl);
            }
        }

        return hasUsableRemoteFile;
    }

    private FilesData buildHuggingFaceFileData(JSONObject entry, Set<String> remotePaths) {
        String remotePath = sanitize(entry.optString("path")).replace('\\', '/');
        if (remotePath.isEmpty() || remotePath.startsWith(".") || remotePath.startsWith("/")) {
            return null;
        }
        String[] remoteSegments = remotePath.split("/", -1);
        for (String segment : remoteSegments) {
            if (segment.isEmpty() || ".".equals(segment) || "..".equals(segment)
                    || segment.indexOf(':') >= 0) {
                return null;
            }
        }

        String localPath = resolveHuggingFaceLocalPath(remotePath, remotePaths);
        if (localPath.isEmpty()) {
            return null;
        }

        String fileName = new File(localPath).getName();
        long size = Math.max(0L, entry.optLong("size", 0L));
        String hash = "";
        String hashAlgorithm = "";
        JSONObject lfs = entry.optJSONObject("lfs");
        if (lfs != null) {
            hash = sanitize(lfs.optString("oid"));
            hashAlgorithm = sanitize(lfs.optString("algorithm"));
            if (hashAlgorithm.isEmpty()) {
                hashAlgorithm = "sha256";
            }
        } else {
            hash = sanitize(entry.optString("sha256"));
            hashAlgorithm = hash.isEmpty() ? "" : "sha256";
            if (hash.isEmpty()) {
                hash = sanitize(entry.optString("sha1"));
                hashAlgorithm = hash.isEmpty() ? "" : "sha1";
            }
            if (hash.isEmpty()) {
                hash = sanitize(entry.optString("oid"));
            }
        }
        String downloadUrl = joinUrl(mHuggingFaceResolveBaseUrl, remotePath);
        if (!isHttpUrl(downloadUrl) || (size <= 0L && hash.isEmpty())) {
            return null;
        }
        return new FilesData(fileName, size, localPath, downloadUrl, hash, hashAlgorithm);
    }

    private String resolveHuggingFaceLocalPath(String remotePath, Set<String> remotePaths) {
        String selectedPrefix = trimTrailingSlash(sanitize(mHuggingFaceFilesPathPrefix));
        if (selectedPrefix.isEmpty()) {
            // Repositories such as Soiza/gta-data-full keep the game files at
            // the repository root, so the remote path is already the local path.
            if (remotePath.startsWith("files/")) {
                String strippedPath = remotePath.substring("files/".length());
                return remotePaths.contains(strippedPath) ? "" : strippedPath;
            }
            return remotePath;
        }

        String selectedLocalPath = stripHuggingFacePrefix(remotePath, selectedPrefix);
        if (!selectedLocalPath.isEmpty()) {
            return selectedLocalPath;
        }

        String supplementalPrefix = trimTrailingSlash(sanitize(mHuggingFaceSupplementalFilesPathPrefix));
        String supplementalLocalPath = stripHuggingFacePrefix(remotePath, supplementalPrefix);
        if (!supplementalLocalPath.isEmpty()) {
            String selectedRemotePath = selectedPrefix + "/" + supplementalLocalPath;
            return remotePaths.contains(selectedRemotePath) ? "" : supplementalLocalPath;
        }

        return "";
    }

    private String stripHuggingFacePrefix(String remotePath, String prefix) {
        if (prefix.isEmpty() || remotePath.equals(prefix)) {
            return "";
        }
        String expectedPrefix = prefix + "/";
        return remotePath.startsWith(expectedPrefix)
                ? remotePath.substring(expectedPrefix.length())
                : "";
    }

    private String trimTrailingSlash(String value) {
        String result = value;
        while (result.endsWith("/")) {
            result = result.substring(0, result.length() - 1);
        }
        return result;
    }

    private String parseNextLink(String linkHeader) {
        String sanitizedHeader = sanitize(linkHeader);
        if (sanitizedHeader.isEmpty()) {
            return "";
        }

        String[] parts = sanitizedHeader.split(",");
        for (String part : parts) {
            String trimmed = sanitize(part);
            if (!trimmed.contains("rel=\"next\"")) {
                continue;
            }

            int start = trimmed.indexOf('<');
            int end = trimmed.indexOf('>');
            if (start >= 0 && end > start) {
                return trimmed.substring(start + 1, end);
            }
        }

        return "";
    }

    private void enqueueMissingFile(FilesData fileData, String manifestBaseUrl) {
        if (fileData == null) {
            return;
        }

        String localPath = normalizeLocalDataPath(fileData.getPath());
        if (shouldIgnoreFile(localPath)) {
            return;
        }
        File file = resolveSafeDataFile(localPath);
        if (file == null) {
            Log.w("UpdateService", "Ignoring unsafe data manifest path: " + fileData.getPath());
            return;
        }
        if (!mForceReinstallData && isCompleteFile(file, fileData.getSize(), fileData.getHash(), fileData.getHashAlgorithm())) {
            return;
        }

        if (!shouldDownloadForCurrentGpu(localPath)) {
            return;
        }

        String downloadUrl = resolveDownloadUrl(manifestBaseUrl, fileData);
        if (downloadUrl.isEmpty()) {
            Log.w("UpdateService", "Missing download URL for: " + localPath);
            return;
        }

        String queueKey = localPath.toLowerCase(Locale.US);
        if (!mQueuedDataPaths.add(queueKey)) {
            Log.w("UpdateService", "Ignoring duplicate data destination: " + localPath);
            return;
        }
        mUpdateFiles.add(localPath);
        String fileName = file.getName();
        Log.d("x1y2z", "File name: " + fileName);
        mUpdateFilesName.add(fileName);
        Log.d("x1y2z", "File path: " + localPath);
        mUpdateFilesSize.add(fileData.getSize());
        if (mUpdateFilesHash == null) {
            mUpdateFilesHash = new ArrayList<>();
        }
        mUpdateFilesHash.add(fileData.getHash());
        mUpdateFilesUrl.add(downloadUrl);
        mUpdateGameDataSize = mUpdateGameDataSize + Math.max(0L, fileData.getSize());
        Log.d("x1y2z", "File size: " + fileData.getSize());
        Log.d("x1y2z", "Data size: " + mUpdateGameDataSize);
    }

    private String normalizeLocalDataPath(String remotePath) {
        String normalizedPath = sanitize(remotePath).replace('\\', '/');
        if (normalizedPath.isEmpty() || normalizedPath.startsWith("/") || normalizedPath.matches("^[A-Za-z]:.*")) {
            return "";
        }

        String configuredPrefix = sanitize(mHuggingFaceFilesPathPrefix).replace('\\', '/');
        while (configuredPrefix.startsWith("/")) {
            configuredPrefix = configuredPrefix.substring(1);
        }
        while (configuredPrefix.endsWith("/")) {
            configuredPrefix = configuredPrefix.substring(0, configuredPrefix.length() - 1);
        }
        if (!configuredPrefix.isEmpty() && normalizedPath.startsWith(configuredPrefix + "/")) {
            normalizedPath = normalizedPath.substring(configuredPrefix.length() + 1);
        }

        String supplementalPrefix = sanitize(mHuggingFaceSupplementalFilesPathPrefix).replace('\\', '/');
        while (supplementalPrefix.startsWith("/")) {
            supplementalPrefix = supplementalPrefix.substring(1);
        }
        while (supplementalPrefix.endsWith("/")) {
            supplementalPrefix = supplementalPrefix.substring(0, supplementalPrefix.length() - 1);
        }
        if (!supplementalPrefix.isEmpty() && normalizedPath.startsWith(supplementalPrefix + "/")) {
            normalizedPath = normalizedPath.substring(supplementalPrefix.length() + 1);
        }
        if (normalizedPath.startsWith("files/")) {
            normalizedPath = normalizedPath.substring("files/".length());
        }

        if (normalizedPath.isEmpty()) {
            return "";
        }

        String[] segments = normalizedPath.split("/", -1);
        StringBuilder result = new StringBuilder();
        for (String segment : segments) {
            if (segment.isEmpty() || ".".equals(segment) || "..".equals(segment) || segment.indexOf(':') >= 0) {
                return "";
            }
            if (result.length() > 0) {
                result.append('/');
            }
            result.append(segment);
        }
        return result.toString();
    }

    private File resolveSafeDataFile(String relativePath) {
        if (relativePath == null || relativePath.isEmpty()) {
            return null;
        }

        File root = getExternalFilesDir(null);
        if (root == null) {
            return null;
        }

        try {
            File canonicalRoot = root.getCanonicalFile();
            File canonicalFile = new File(canonicalRoot, relativePath).getCanonicalFile();
            String rootPath = canonicalRoot.getPath();
            String filePath = canonicalFile.getPath();
            if (!filePath.startsWith(rootPath + File.separator)) {
                return null;
            }
            return canonicalFile;
        } catch (IOException e) {
            Log.e("UpdateService", "Unable to resolve data file path", e);
            return null;
        }
    }

    private boolean isCompleteFile(
            File file,
            long expectedSize,
            String expectedHash,
            String hashAlgorithm
    ) {
        if (file == null || !file.isFile() || file.length() <= 0L || expectedSize <= 0L
                || file.length() != expectedSize) {
            return false;
        }
        String normalizedHash = normalizeHash(expectedHash);
        if (normalizedHash.isEmpty()) {
            return true;
        }
        String actualHash = calculateFileHash(file, hashAlgorithm, normalizedHash);
        return normalizedHash.equalsIgnoreCase(actualHash);
    }

    private String normalizeHash(String value) {
        String normalized = sanitize(value).toLowerCase(Locale.US);
        int separator = normalized.indexOf(':');
        if (separator > 0 && separator + 1 < normalized.length()) {
            normalized = normalized.substring(separator + 1);
        }
        return normalized;
    }

    private String calculateFileHash(File file, String hashAlgorithm, String expectedHash) {
        String algorithm = sanitize(hashAlgorithm).toLowerCase(Locale.US);
        if (algorithm.isEmpty()) {
            if (expectedHash.length() == 64) {
                algorithm = "sha256";
            } else if (expectedHash.length() == 40) {
                algorithm = "sha1";
            } else if (expectedHash.length() == 32) {
                algorithm = "md5";
            }
        }
        String jcaAlgorithm;
        if ("sha256".equals(algorithm)) {
            jcaAlgorithm = "SHA-256";
        } else if ("sha1".equals(algorithm)) {
            jcaAlgorithm = "SHA-1";
        } else if ("md5".equals(algorithm)) {
            jcaAlgorithm = "MD5";
        } else {
            return "";
        }
        try (FileInputStream input = new FileInputStream(file)) {
            MessageDigest digest = MessageDigest.getInstance(jcaAlgorithm);
            byte[] buffer = new byte[8192];
            int read;
            while ((read = input.read(buffer)) != -1) {
                digest.update(buffer, 0, read);
            }
            StringBuilder result = new StringBuilder(digest.getDigestLength() * 2);
            for (byte value : digest.digest()) {
                result.append(String.format(Locale.US, "%02x", value & 0xff));
            }
            return result.toString();
        } catch (Exception e) {
            Log.w("UpdateService", "Unable to calculate file hash: " + file, e);
            return "";
        }
    }

    private boolean shouldIgnoreFile(String localPath) {
        String fileName = new File(localPath == null ? "" : localPath).getName();
        return fileName.equals("samp_log.txt")
                || fileName.equals("svlog.txt")
                || fileName.equals("gtasatelem.set")
                || fileName.equals("GTASAMP10.b")
                || fileName.equals("gta_sa.set")
                || fileName.equals("settings.ini");
    }

    private boolean shouldDownloadForCurrentGpu(String filePath) {
        String normalizedPath = filePath == null ? "" : filePath.toLowerCase(Locale.US);
        if (isMandatoryArm64DxtTexture(normalizedPath)) {
            return true;
        }
        if (normalizedPath.contains("player")
                || normalizedPath.contains("playerhi")
                || normalizedPath.contains("menu")
                || normalizedPath.contains("samp")) {
            return true;
        }

        if (normalizedPath.contains(".dxt.") && mGpuType != 1) {
            return false;
        }
        if (normalizedPath.contains(".etc.") && mGpuType != 2) {
            return false;
        }
        return !normalizedPath.contains(".pvr.") || mGpuType == 3;
    }

    private boolean isMandatoryArm64DxtTexture(String normalizedPath) {
        if (!GameDataVerifier.isArm64Runtime() || !normalizedPath.endsWith(".dxt.dat")
                && !normalizedPath.endsWith(".dxt.tmb")
                && !normalizedPath.endsWith(".dxt.toc")) {
            return false;
        }
        return normalizedPath.startsWith("texdb/txd/txd.dxt.")
                || normalizedPath.startsWith("texdb/gta3/gta3.dxt.")
                || normalizedPath.startsWith("texdb/gta_int/gta_int.dxt.")
                || normalizedPath.startsWith("texdb/mobile/mobile.dxt.");
    }

    private String resolveDownloadUrl(String filesManifestUrl, FilesData fileData) {
        String explicitUrl = sanitize(fileData.getUrl());
        if (!explicitUrl.isEmpty()) {
            String resolvedExplicitUrl = resolvePossiblyRelativeUrl(filesManifestUrl, explicitUrl);
            return isHttpUrl(resolvedExplicitUrl) ? resolvedExplicitUrl : "";
        }

        String relativePath = trimLeadingSlash(fileData.getPath());
        if (!sanitize(mHuggingFaceFilesPathPrefix).isEmpty()) {
            relativePath = normalizeLocalDataPath(fileData.getPath());
        } else if (relativePath.startsWith("files/")) {
            relativePath = relativePath.substring("files/".length());
        }
        if (relativePath.isEmpty()) {
            return "";
        }
        for (String baseUrl : mFallbackFileBaseUrls) {
            String resolved = joinUrl(baseUrl, relativePath);
            if (isHttpUrl(resolved)) {
                return resolved;
            }
        }

        return "";
    }

    private boolean isHttpUrl(String value) {
        return value != null
                && (value.startsWith("http://") || value.startsWith("https://"));
    }

    private String joinUrl(String baseUrl, String relativePath) {
        String sanitizedBaseUrl = sanitize(baseUrl);
        if (sanitizedBaseUrl.isEmpty()) {
            return "";
        }
        if (!sanitizedBaseUrl.endsWith("/")) {
            sanitizedBaseUrl = sanitizedBaseUrl + "/";
        }
        return resolvePossiblyRelativeUrl(sanitizedBaseUrl, relativePath);
    }

    private String resolvePossiblyRelativeUrl(String baseUrl, String candidate) {
        String sanitizedCandidate = sanitize(candidate);
        if (sanitizedCandidate.isEmpty()) {
            return "";
        }
        if (sanitizedCandidate.startsWith("http://") || sanitizedCandidate.startsWith("https://")) {
            return sanitizedCandidate;
        }
        String sanitizedBaseUrl = sanitize(baseUrl);
        if (sanitizedBaseUrl.isEmpty()) {
            return sanitizedCandidate;
        }
        try {
            return new URL(new URL(sanitizedBaseUrl), sanitizedCandidate).toString();
        } catch (MalformedURLException ignored) {
            return sanitizedCandidate;
        }
    }

    private String sanitize(String value) {
        return value == null ? "" : value.trim();
    }

    private int getInstalledVersionCode() {
        try {
            PackageInfo packageInfo = getPackageManager().getPackageInfo(getPackageName(), PackageManager.GET_ACTIVITIES);
            return packageInfo.versionCode;
        } catch (Exception e) {
            Log.e("x1y2z", "unable to read installed version", e);
            return 1;
        }
    }

    private String trimLeadingSlash(String value) {
        String sanitizedValue = sanitize(value);
        while (sanitizedValue.startsWith("/")) {
            sanitizedValue = sanitizedValue.substring(1);
        }
        return sanitizedValue;
    }

    public void updateGame() {
        if (isGameUpdateExists()) {
            Log.d("UpdateService", "updateGame exists");
            setUpdateStatus(UpdateActivity.UpdateStatus.DownloadGame);
            downloadGame();
            return;
        }
        Log.d("UpdateService", "updateGame done");
        setUpdateStatus(UpdateActivity.UpdateStatus.Undefined);
        File file = new File(getExternalFilesDir(null) + "/download/update.apk");
        Message obtain = Message.obtain(mInHandler, 2);
        obtain.getData().putBoolean("status", true);
        obtain.getData().putString("apkPath", file.getAbsolutePath());
        obtain.replyTo = mMessenger;
        if (mActivityMessenger != null) {
            try {
                mActivityMessenger.send(obtain);
            } catch (RemoteException e5) {
                e5.printStackTrace();
            }
        }
        setUpdateStatus(UpdateActivity.UpdateStatus.Undefined);
    }


    public void startGameUpdateChecking()
    {
        if (!mUpdateFiles.isEmpty()) {
            setUpdateStatus(UpdateActivity.UpdateStatus.DownloadGameData);
            startDataUpdating();
            return;
        }

        completeGameDataInstall();
    }

    private void completeGameDataInstall() {
        if (mDataTerminalOutcome) {
            return;
        }
        String missingRequirement = GameDataVerifier.getMissingRequirement(this, mDataVariantId);
        if (!missingRequirement.isEmpty()) {
            Log.e("UpdateService", "Game data validation failed: " + missingRequirement);
            DataVariantPreferences.rollbackInstall(this);
            mDataTerminalOutcome = true;
            setDataValidationFailed(missingRequirement);
            return;
        }

        if (!DataVariantPreferences.commitInstalledVariant(this, mDataVariantId)) {
            DataVariantPreferences.rollbackInstall(this);
            mDataTerminalOutcome = true;
            setDataValidationFailed("Nao foi possivel registrar os arquivos instalados.");
            return;
        }
        mDataTerminalOutcome = true;
        Message obtain = Message.obtain(this.mInHandler, 2);
        obtain.getData().putBoolean("status", true);
        obtain.getData().putBoolean("dataReady", true);
        obtain.replyTo = mMessenger;
        if (mActivityMessenger != null) {
            try {
                mActivityMessenger.send(obtain);
            } catch (RemoteException e5) {
                e5.printStackTrace();
            }
        }
    }

    private void setDataValidationFailed(String reason) {
        mUpdateStatus = UpdateActivity.UpdateStatus.DataValidationFailed;
        Message obtain = Message.obtain(mInHandler, 4);
        obtain.getData().putString("status", mUpdateStatus.name());
        obtain.getData().putString("reason", reason);
        obtain.replyTo = mMessenger;
        if (mActivityMessenger != null) {
            try {
                mActivityMessenger.send(obtain);
            } catch (RemoteException e) {
                e.printStackTrace();
            }
        }
    }

    public void setUpdateStatus(UpdateActivity.UpdateStatus updateStatus) {
        if (updateStatus.name().length() != 0 && mUpdateStatus != updateStatus) {
            mUpdateStatus = updateStatus;
            Message obtain = Message.obtain(mInHandler, 4);
            obtain.getData().putString("status", mUpdateStatus.name());
            obtain.replyTo = mMessenger;
            Messenger messenger = mActivityMessenger;
            if (messenger != null) {
                try {
                    messenger.send(obtain);
                } catch (RemoteException e5) {
                    e5.printStackTrace();
                }
            }
        }
    }


    public void startDataUpdating()
    {
        ArrayList<String> arrayList = new ArrayList<>(mUpdateFiles);
        ArrayList<String> arrayList1 = new ArrayList<>(mUpdateFilesName);
        ArrayList<Long> arrayList2 = new ArrayList<>(mUpdateFilesSize);
        ArrayList<String> arrayList3 = new ArrayList<>(mUpdateFilesUrl);
        ArrayList<String> arrayList4 = mUpdateFilesHash == null
                ? new ArrayList<>()
                : new ArrayList<>(mUpdateFilesHash);
        mUpdateFiles.clear();
        mUpdateFilesName.clear();
        mUpdateFilesSize.clear();
        mUpdateFilesUrl.clear();
        if (mUpdateFilesHash == null) {
            mUpdateFilesHash = new ArrayList<>();
        }
        mUpdateFilesHash.clear();
        Ref.IntRef intRef = new Ref.IntRef();
        intRef.element = 0;
        Ref.LongRef longRef1 = new Ref.LongRef();
        longRef1.element = 0;
        while(intRef.element<arrayList.size()) {
            String localPath = arrayList.get(intRef.element);
            File file = resolveSafeDataFile(localPath);
            File parentFile = file == null ? null : file.getParentFile();
            String downloadUrl = arrayList3.get(intRef.element);
            if (file == null || parentFile == null || downloadUrl == null || downloadUrl.trim().isEmpty()) {
                mDownloadingStatus = false;
                mUpdateFiles.add(localPath);
                mUpdateFilesName.add(arrayList1.get(intRef.element));
                mUpdateFilesSize.add(arrayList2.get(intRef.element));
                mUpdateFilesUrl.add(downloadUrl == null ? "" : downloadUrl);
                mUpdateFilesHash.add(intRef.element < arrayList4.size() ? arrayList4.get(intRef.element) : "");
                Log.e("UpdateService", "Invalid queued data download: " + localPath);
                intRef.element++;
                continue;
            }

            if (!parentFile.exists() && !parentFile.mkdirs()) {
                mDownloadingStatus = false;
                mUpdateFiles.add(localPath);
                mUpdateFilesName.add(arrayList1.get(intRef.element));
                mUpdateFilesSize.add(arrayList2.get(intRef.element));
                mUpdateFilesUrl.add(downloadUrl);
                mUpdateFilesHash.add(intRef.element < arrayList4.size() ? arrayList4.get(intRef.element) : "");
                Log.e("UpdateService", "Unable to create data directory: " + parentFile);
                intRef.element++;
                continue;
            }
            final int fileIndex = intRef.element;
            final String queuedPath = localPath;
            final long expectedSize = arrayList2.get(fileIndex);
            final String expectedHash = fileIndex < arrayList4.size() ? arrayList4.get(fileIndex) : "";
            final File partFile = new File(parentFile, file.getName() + ".part");
            if (partFile.exists()) {
                Log.w("UpdateService", "Replacing stale partial data file: " + partFile);
            }

            Ref.LongRef longRef = new Ref.LongRef();
            longRef.element = System.currentTimeMillis();

            Log.d("x1y2z", "startDataUpdating " + mUpdateGameDataSize + " " + mUpdateGameDataSizeUpdated);

            mDownloadingStatus = true;
            PRDownloader.download(downloadUrl, parentFile.getAbsolutePath(), partFile.getName()).build().setOnStartOrResumeListener(null).setOnPauseListener(null).setOnCancelListener(null).setOnProgressListener(new OnProgressListener() {
                @Override
                public void onProgress(Progress progress) {
                    mDownloadingStatus = true;
                    if(System.currentTimeMillis() - longRef.element > 100) {
                        longRef.element = System.currentTimeMillis();
                        Message obtain = Message.obtain(mInHandler, 4);
                        obtain.getData().putString("status", UpdateActivity.UpdateStatus.DownloadGameData.name());
                        obtain.getData().putBoolean("withProgress", true);
                        obtain.getData().putLong("current", longRef1.element+progress.currentBytes);
                        obtain.getData().putLong("total", mUpdateGameDataSize);
                        obtain.getData().putString("filename", arrayList1.get(fileIndex));
                        obtain.getData().putLong("totalfiles", arrayList.size());
                        obtain.getData().putLong("currentfile", fileIndex);
                        if (mActivityMessenger != null) {
                            try {
                                mActivityMessenger.send(obtain);
                            } catch (RemoteException e) {
                                e.printStackTrace();
                            }
                        }
                    }

                }
            }).start(new OnDownloadListener() {
                @Override
                public void onDownloadComplete() {
                    boolean validPart = isCompleteFile(
                            partFile,
                            expectedSize,
                            expectedHash,
                            expectedHash.length() == 64 ? "sha256" : ""
                    );
                    boolean moved = false;
                    if (validPart) {
                        // Both files are in the same directory. Replace only after
                        // validation so a failed download never destroys the old file.
                        try {
                            Files.move(
                                    partFile.toPath(),
                                    file.toPath(),
                                    StandardCopyOption.REPLACE_EXISTING,
                                    StandardCopyOption.ATOMIC_MOVE
                            );
                            moved = true;
                        } catch (AtomicMoveNotSupportedException ignored) {
                            try {
                                Files.move(
                                        partFile.toPath(),
                                        file.toPath(),
                                        StandardCopyOption.REPLACE_EXISTING
                                );
                                moved = true;
                            } catch (IOException replaceError) {
                                Log.w("UpdateService", "Non-atomic data replacement failed: " + file, replaceError);
                                moved = partFile.renameTo(file);
                            }
                        } catch (IOException moveError) {
                            Log.e("UpdateService", "Unable to install verified data file: " + file, moveError);
                        }
                    }
                    if (!moved && partFile.exists()) {
                        partFile.delete();
                    }
                    mDownloadingStatus = false;
                    if (moved) {
                        longRef1.element += expectedSize;
                        Log.d("x1y2z", "completed");
                    } else {
                        mUpdateFiles.add(queuedPath);
                        mUpdateFilesName.add(arrayList1.get(fileIndex));
                        mUpdateFilesSize.add(expectedSize);
                        mUpdateFilesUrl.add(downloadUrl);
                        mUpdateFilesHash.add(expectedHash);
                        mLastDataDownloadFailure = "Arquivo baixado invalido: " + queuedPath;
                    }
                }

                @Override
                public void onError(Error error) {
                    mDownloadingStatus = false;
                    if (partFile.exists()) {
                        partFile.delete();
                    }
                    mUpdateFiles.add(queuedPath);
                    mUpdateFilesName.add(arrayList1.get(fileIndex));
                    mUpdateFilesSize.add(expectedSize);
                    mUpdateFilesUrl.add(arrayList3.get(fileIndex));
                    mUpdateFilesHash.add(expectedHash);
                    mLastDataDownloadFailure = "Falha ao baixar " + queuedPath + ": "
                            + (error == null ? "erro desconhecido" : error.toString());
                    Log.e("UpdateService", mLastDataDownloadFailure);
                }
            });

            do {
                try {
                    Thread.sleep(30);
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }
            } while (mDownloadingStatus);


            intRef.element++;

        }

        mDownloadingStatus = false;
        if (!mUpdateFiles.isEmpty()) {
            DataVariantPreferences.rollbackInstall(this);
            String reason = mLastDataDownloadFailure;
            if (reason.isEmpty()) {
                reason = "Faltam " + mUpdateFiles.size() + " arquivo(s) na instalacao da data.";
            }
            Log.e("UpdateService", "Data download incomplete: " + reason);
            setDataDownloadFailed(reason);
            return;
        }
        completeGameDataInstall();
    }

    public void downloadGame()
    {
        Log.d("UpdateService", "downloadGame");
        if (mUpdateGameURL == null || mUpdateGameURL.trim().isEmpty()) {
            mDownloadingStatus = false;
            setUpdateStatus(UpdateActivity.UpdateStatus.SourceUnavailable);
            return;
        }
        mDownloadingStatus = true;

        File file = new File(getExternalFilesDir(null) + "/download/update.apk");
        file.getParentFile().mkdirs();
        if (file.exists()) {
            file.delete();
        }

        Ref.LongRef longRef = new Ref.LongRef();
        longRef.element = System.currentTimeMillis();

        mDownloadingStatus = true;
        PRDownloader.download(mUpdateGameURL, getExternalFilesDir(null) + "/download", "update.apk").build().setOnStartOrResumeListener(null).setOnPauseListener(null).setOnCancelListener(null).setOnProgressListener(new OnProgressListener() {
            @Override
            public void onProgress(Progress progress) {
                mDownloadingStatus = true;
                if(System.currentTimeMillis() - longRef.element > 100) {
                    longRef.element = System.currentTimeMillis();
                    Message obtain = Message.obtain(mInHandler, 4);
                    obtain.getData().putString("status", UpdateActivity.UpdateStatus.DownloadGame.name());
                    obtain.getData().putBoolean("withProgress", true);
                    obtain.getData().putLong("current", progress.currentBytes);
                    obtain.getData().putLong("total", progress.totalBytes);
                    obtain.getData().putString("filename", "update.apk");
                    obtain.getData().putLong("totalfiles", 1);
                    obtain.getData().putLong("currentfile", 1);
                    if (mActivityMessenger != null) {
                        try {
                            mActivityMessenger.send(obtain);
                        } catch (RemoteException e) {
                            e.printStackTrace();
                        }
                    }
                }

            }
        }).start(new OnDownloadListener() {
            @Override
            public void onDownloadComplete() {
                if (!file.exists() || !file.isFile() || file.length() <= 0L) {
                    mDownloadingStatus = false;
                    setUpdateStatus(UpdateActivity.UpdateStatus.SourceUnavailable);
                    return;
                }
                Message obtain = Message.obtain(UpdateService.this.mInHandler, 2);
                obtain.getData().putBoolean("status", true);
                obtain.getData().putString("apkPath", file.getAbsolutePath());
                obtain.replyTo = mMessenger;
                if (mActivityMessenger != null) {
                    try {
                        mActivityMessenger.send(obtain);
                    } catch (RemoteException e5) {
                        e5.printStackTrace();
                    }
                }
                setUpdateStatus(UpdateActivity.UpdateStatus.Undefined);
                mDownloadingStatus = false;
                Log.d("x1y2z", "completed");
            }

            @Override
            public void onError(Error error) {
                mDownloadingStatus = false;
                setUpdateStatus(UpdateActivity.UpdateStatus.SourceUnavailable);
                Log.d("x1y2z", "error downloadgame");
            }
        });

        do {
            try {
                Thread.sleep(30);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        } while (mDownloadingStatus);

        mDownloadingStatus = false;
    }

    public boolean isGameUpdateExists() {
        PackageInfo packageInfo = null;
        try {
            packageInfo = getPackageManager().getPackageInfo("com.supremo.launcher", PackageManager.GET_ACTIVITIES);
        } catch (PackageManager.NameNotFoundException e) {
            e.printStackTrace();
        }
        if (packageInfo == null) {
            return true;
        }
        Log.d("x1y2z", "isGameUpdateExists -> currentVersion " + packageInfo.versionCode + " | mUpdateVersion " + this.mUpdateVersion);
        return packageInfo.versionCode == this.mUpdateVersion ? false:true;
    }

    private void sendLoadingScreen(boolean unpacking, String filename, long current, long total) {
        new Thread(new Runnable() {
            public void run() {
                Message obtain = Message.obtain(UpdateService.this.mInHandler, 4);
                obtain.getData().putString("status", UpdateActivity.UpdateStatus.CheckUpdate.name());
                obtain.getData().putBoolean("withProgress", true);
                obtain.getData().putString("filename", filename);
                obtain.getData().putBoolean("unpacking", unpacking);
                obtain.getData().putLong("current", current);
                obtain.getData().putLong("total", total);
                obtain.replyTo = mMessenger;
                if (mActivityMessenger != null) {
                    try {
                        mActivityMessenger.send(obtain);
                    } catch (RemoteException e5) {
                        e5.printStackTrace();
                    }
                }
            }
        }).start();
    }

    private void resetUpdateState() {
        mDownloadingStatus = false;
        mUpdateGameDataSize = 0;
        mUpdateGameDataSizeUpdated = 0;
        mUpdateGameURL = "";
        mUpdateVersion = 0;
        mLastDataDownloadFailure = "";
        DataVariantPreferences.rollbackInstall(this);
        mDataTerminalOutcome = false;
        mQueuedDataPaths.clear();
        mUpdateFiles = new ArrayList<>();
        mUpdateFilesName = new ArrayList<>();
        mUpdateFilesSize = new ArrayList<>();
        mUpdateFilesUrl = new ArrayList<>();
        mUpdateFilesHash = new ArrayList<>();
        Util.responseFiles = "";
        Util.responseFilesInt = 0;
    }


}
