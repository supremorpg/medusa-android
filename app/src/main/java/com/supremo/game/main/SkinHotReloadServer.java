package com.supremo.launcher.main;

import android.content.Context;
import android.util.Log;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.Closeable;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.zip.GZIPInputStream;

public final class SkinHotReloadServer {
    public static final int PORT = 12345;

    private static final String TAG = "SkinHotReload";
    private static final String PROTOCOL = "supremo.skin.hot-reload";
    private static final String COMMAND_REPLACE = "skin.replace";
    private static final String COMMAND_RESTORE = "skin.restore";
    private static final String COMMAND_WEAPON_AUDIO_REPLACE = "weapon.audio.replace";
    private static final String COMMAND_WEAPON_AUDIO_RESTORE = "weapon.audio.restore";
    private static final String MODE_PERMANENT = "permanent";
    private static final String MODE_RESTORE = "restore";
    private static final int MAX_HEADER_BYTES = 64 * 1024;
    private static final int MAX_PAYLOAD_BYTES = 32 * 1024 * 1024;

    private final Object lifecycleLock = new Object();
    private volatile boolean running;
    private volatile Context appContext;
    private volatile Listener listener;
    private ServerSocket serverSocket;
    private Thread serverThread;

    public interface Listener {
        Result onSkinHotReload(Request request);
    }

    public void start(Context context, Listener listener) {
        if (context == null || listener == null) {
            return;
        }

        synchronized (lifecycleLock) {
            if (running) {
                return;
            }
            this.appContext = context.getApplicationContext();
            this.listener = listener;
            running = true;
            serverThread = new Thread(this::listenLoop, "supremo-skin-hot-reload");
            serverThread.start();
        }
    }

    public void stop() {
        synchronized (lifecycleLock) {
            running = false;
            closeQuietly(serverSocket);
            serverSocket = null;
            if (serverThread != null) {
                serverThread.interrupt();
                serverThread = null;
            }
        }
    }

    private void listenLoop() {
        ServerSocket socket = null;
        try {
            socket = new ServerSocket();
            socket.setReuseAddress(true);
            socket.bind(new InetSocketAddress(InetAddress.getByName("127.0.0.1"), PORT));
            serverSocket = socket;
            Log.i(TAG, "Skin hot reload listener on 127.0.0.1:" + PORT);

            while (running) {
                Socket client = socket.accept();
                handleClient(client);
            }
        } catch (IOException error) {
            if (running) {
                Log.e(TAG, "Skin hot reload listener stopped.", error);
            }
        } finally {
            closeQuietly(socket);
            synchronized (lifecycleLock) {
                if (serverSocket == socket) {
                    serverSocket = null;
                }
                running = false;
            }
        }
    }

    private void handleClient(Socket client) {
        try (Socket socket = client) {
            socket.setSoTimeout(15000);
            Request request = readRequest(socket);
            Listener activeListener = listener;
            if (activeListener == null) {
                writeResponse(socket, Result.error("Listener do runtime nao esta ativo."), request);
                return;
            }

            Result result = activeListener.onSkinHotReload(request);
            if (result == null) {
                result = Result.error("Runtime nao retornou resultado.");
            }
            writeResponse(socket, result, request);
        } catch (Exception error) {
            Log.e(TAG, "Falha ao processar hot reload.", error);
            try {
                writeResponse(client, Result.error(error.getMessage()), null);
            } catch (IOException ignored) {
            } finally {
                closeQuietly(client);
            }
        }
    }

    private Request readRequest(Socket socket) throws IOException, JSONException {
        DataInputStream input = new DataInputStream(new BufferedInputStream(socket.getInputStream()));
        int headerLength = input.readInt();
        if (headerLength <= 0 || headerLength > MAX_HEADER_BYTES) {
            throw new IOException("Cabecalho de hot reload invalido.");
        }

        byte[] headerBytes = new byte[headerLength];
        input.readFully(headerBytes);
        JSONObject header = new JSONObject(new String(headerBytes, StandardCharsets.UTF_8));
        if (!PROTOCOL.equals(header.optString("protocol"))) {
            throw new IOException("Protocolo de hot reload desconhecido.");
        }
        String command = header.optString("command");
        if (!COMMAND_REPLACE.equals(command)
                && !COMMAND_RESTORE.equals(command)
                && !COMMAND_WEAPON_AUDIO_REPLACE.equals(command)
                && !COMMAND_WEAPON_AUDIO_RESTORE.equals(command)) {
            throw new IOException("Comando de hot reload nao suportado.");
        }

        if (COMMAND_RESTORE.equals(command) || COMMAND_WEAPON_AUDIO_RESTORE.equals(command)) {
            return Request.from(header, new byte[0], "");
        }

        int payloadLength = header.optInt("payloadBytes", -1);
        long originalLength = header.optLong("originalBytes", -1L);
        if (payloadLength <= 0 || payloadLength > MAX_PAYLOAD_BYTES || originalLength <= 0 || originalLength > MAX_PAYLOAD_BYTES) {
            throw new IOException("Payload de hot reload fora do limite seguro.");
        }

        byte[] payload = new byte[payloadLength];
        input.readFully(payload);
        byte[] decodedBytes = decodePayload(payload, header.optString("encoding"), (int) originalLength);
        String expectedSha1 = header.optString("sha1", "");
        String actualSha1 = sha1(decodedBytes);
        if (!expectedSha1.isEmpty() && !expectedSha1.equalsIgnoreCase(actualSha1)) {
            throw new IOException("SHA1 do payload nao confere.");
        }

        Request request = Request.from(header, decodedBytes, actualSha1);
        stageRequest(request, header);
        return request;
    }

    private byte[] decodePayload(byte[] payload, String encoding, int expectedLength) throws IOException {
        if ("gzip".equalsIgnoreCase(encoding)) {
            ByteArrayOutputStream output = new ByteArrayOutputStream(Math.max(1024, expectedLength));
            try (GZIPInputStream gzip = new GZIPInputStream(new ByteArrayInputStream(payload))) {
                byte[] buffer = new byte[8192];
                int read;
                while ((read = gzip.read(buffer)) != -1) {
                    output.write(buffer, 0, read);
                    if (output.size() > MAX_PAYLOAD_BYTES) {
                        throw new IOException("Payload descompactado excedeu o limite seguro.");
                    }
                }
            }
            return output.toByteArray();
        }

        if ("identity".equalsIgnoreCase(encoding) || encoding == null || encoding.isEmpty()) {
            return payload;
        }

        throw new IOException("Codificacao de payload nao suportada: " + encoding);
    }

    private static File getHotReloadRoot(Context context) throws IOException {
        if (context == null) {
            throw new IOException("Contexto do app indisponivel.");
        }

        File externalRoot = context.getExternalFilesDir(null);
        return new File(externalRoot != null ? externalRoot : context.getFilesDir(), "hot-reload");
    }

    private static File ensureDirectory(File directory) throws IOException {
        if (directory != null && ((directory.exists() && directory.isDirectory()) || directory.mkdirs())) {
            return directory;
        }
        throw new IOException("Nao foi possivel criar pasta de hot reload.");
    }

    private static File getSkinStageRoot(Context context) throws IOException {
        return ensureDirectory(new File(getHotReloadRoot(context), "skins"));
    }

    private static File getWeaponAudioStageRoot(Context context) throws IOException {
        return ensureDirectory(new File(getHotReloadRoot(context), "weapon-audio"));
    }

    private static File getActiveRoot(Context context) throws IOException {
        return ensureDirectory(new File(getHotReloadRoot(context), "active"));
    }

    private static File getActiveManifestFile(Context context, String targetGroup, String textureName) throws IOException {
        String safeGroup = sanitizeName(targetGroup, "player");
        String safeTexture = sanitizeName(textureName, "texture");
        return new File(getActiveRoot(context), safeGroup + "-" + safeTexture + ".json");
    }

    private void stageRequest(Request request, JSONObject header) throws IOException, JSONException {
        Context context = appContext;
        if (context == null) {
            throw new IOException("Contexto do app indisponivel.");
        }

        File root = request.isWeaponAudio() ? getWeaponAudioStageRoot(context) : getSkinStageRoot(context);

        String safeTexture = sanitizeName(request.textureName, request.isWeaponAudio() ? "weapon_audio" : "texture");
        String extension = sanitizeExtension(request.extension, request.fileName);
        String stamp = String.valueOf(System.currentTimeMillis());
        File stagedFile = new File(root, safeTexture + "-" + stamp + extension);
        try (FileOutputStream output = new FileOutputStream(stagedFile)) {
            output.write(request.textureBytes);
        }

        File manifestFile = new File(root, safeTexture + "-" + stamp + ".json");
        JSONObject manifest = new JSONObject(header.toString());
        manifest.put("receivedAt", System.currentTimeMillis());
        manifest.put("stagedFile", stagedFile.getAbsolutePath());
        manifest.put("manifestFile", manifestFile.getAbsolutePath());
        try (FileOutputStream output = new FileOutputStream(manifestFile)) {
            output.write(manifest.toString(2).getBytes(StandardCharsets.UTF_8));
        }

        request.stagedFile = stagedFile;
        request.manifestFile = manifestFile;
    }

    public static void persistActiveMod(Context context, Request request) throws IOException, JSONException {
        if (request == null || request.stagedFile == null || !request.stagedFile.isFile()) {
            throw new IOException("Arquivo staged indisponivel para salvar permanente.");
        }

        File activeManifest = getActiveManifestFile(context, request.targetGroup, request.textureName);
        JSONObject manifest = request.toManifestJson();
        manifest.put("installMode", MODE_PERMANENT);
        manifest.put("persistedAt", System.currentTimeMillis());
        manifest.put("stagedFile", request.stagedFile.getAbsolutePath());
        manifest.put("activeManifestFile", activeManifest.getAbsolutePath());
        try (FileOutputStream output = new FileOutputStream(activeManifest)) {
            output.write(manifest.toString(2).getBytes(StandardCharsets.UTF_8));
        }
        request.activeManifestFile = activeManifest;
    }

    public static int removeActiveMod(Context context, String targetGroup, String textureName) throws IOException {
        File activeManifest = getActiveManifestFile(context, targetGroup, textureName);
        int removed = 0;
        if (activeManifest.isFile()) {
            deleteStagedFileReferencedBy(activeManifest);
            if (activeManifest.delete()) {
                removed++;
            }
        }
        return removed;
    }

    public static List<Request> loadActiveRequests(Context context) {
        List<Request> requests = new ArrayList<>();
        File root;
        try {
            root = getActiveRoot(context);
        } catch (IOException error) {
            Log.w(TAG, "Nao foi possivel abrir mods permanentes.", error);
            return requests;
        }

        File[] files = root.listFiles((directory, name) -> name != null && name.toLowerCase(Locale.US).endsWith(".json"));
        if (files == null) {
            return requests;
        }

        for (File file : files) {
            try {
                JSONObject manifest = new JSONObject(readFileUtf8(file));
                Request request = Request.fromManifest(manifest, file);
                if (request.stagedFile != null && request.stagedFile.isFile()) {
                    requests.add(request);
                }
            } catch (Exception error) {
                Log.w(TAG, "Ignorando mod permanente invalido: " + file.getAbsolutePath(), error);
            }
        }
        return requests;
    }

    private void writeResponse(Socket socket, Result result, Request request) throws IOException {
        JSONObject response = new JSONObject();
        try {
            response.put("protocol", PROTOCOL);
            response.put("version", 1);
            response.put("ok", result.ok);
            response.put("nativeTextureApplied", result.nativeTextureApplied);
            response.put("message", result.message == null ? "" : result.message);
            if (request != null) {
                response.put("requestId", request.requestId);
                response.put("command", request.command);
                response.put("targetGroup", request.targetGroup);
                response.put("textureName", request.textureName);
                response.put("skinId", request.skinId);
                response.put("weaponId", request.weaponId);
                response.put("weaponName", request.weaponName);
                if (request.stagedFile != null) {
                    response.put("stagedFile", request.stagedFile.getAbsolutePath());
                }
                if (request.manifestFile != null) {
                    response.put("manifestFile", request.manifestFile.getAbsolutePath());
                }
                if (request.activeManifestFile != null) {
                    response.put("activeManifestFile", request.activeManifestFile.getAbsolutePath());
                }
            }
        } catch (JSONException error) {
            throw new IOException(error);
        }

        byte[] responseBytes = response.toString().getBytes(StandardCharsets.UTF_8);
        DataOutputStream output = new DataOutputStream(new BufferedOutputStream(socket.getOutputStream()));
        output.writeInt(responseBytes.length);
        output.write(responseBytes);
        output.flush();
    }

    private static String sha1(byte[] bytes) throws IOException {
        try {
            MessageDigest digest = MessageDigest.getInstance("SHA-1");
            byte[] hash = digest.digest(bytes);
            StringBuilder builder = new StringBuilder(hash.length * 2);
            for (byte item : hash) {
                builder.append(String.format(Locale.US, "%02x", item & 0xff));
            }
            return builder.toString();
        } catch (NoSuchAlgorithmException error) {
            throw new IOException(error);
        }
    }

    private static String sanitizeName(String value, String fallback) {
        String cleaned = String.valueOf(value == null ? "" : value)
                .toLowerCase(Locale.US)
                .replaceAll("[^a-z0-9_]+", "_")
                .replaceAll("^_+|_+$", "");
        if (cleaned.isEmpty()) {
            cleaned = fallback;
        }
        return cleaned.length() > 32 ? cleaned.substring(0, 32) : cleaned;
    }

    private static String sanitizeExtension(String extension, String fileName) {
        String ext = extension == null ? "" : extension.trim().toLowerCase(Locale.US);
        if (ext.isEmpty() && fileName != null) {
            int index = fileName.lastIndexOf('.');
            if (index >= 0) {
                ext = fileName.substring(index).toLowerCase(Locale.US);
            }
        }
        if (!ext.startsWith(".")) {
            ext = "." + ext;
        }
        if (!ext.matches("\\.[a-z0-9]{1,8}")) {
            return ".bin";
        }
        return ext;
    }

    private static String readFileUtf8(File file) throws IOException {
        try (FileInputStream input = new FileInputStream(file);
             ByteArrayOutputStream output = new ByteArrayOutputStream()) {
            byte[] buffer = new byte[4096];
            int read;
            while ((read = input.read(buffer)) != -1) {
                output.write(buffer, 0, read);
            }
            return output.toString(StandardCharsets.UTF_8.name());
        }
    }

    private static void deleteStagedFileReferencedBy(File manifestFile) {
        try {
            JSONObject manifest = new JSONObject(readFileUtf8(manifestFile));
            String path = manifest.optString("stagedFile", "");
            if (!path.isEmpty()) {
                File stagedFile = new File(path);
                if (stagedFile.isFile()) {
                    stagedFile.delete();
                }
            }
        } catch (Exception ignored) {
        }
    }

    private static void closeQuietly(Closeable closeable) {
        if (closeable == null) {
            return;
        }
        try {
            closeable.close();
        } catch (IOException ignored) {
        }
    }

    public static final class Request {
        public final String requestId;
        public final String command;
        public final String installMode;
        public final String textureName;
        public final String targetGroup;
        public final String modelName;
        public final String skinName;
        public final String action;
        public final String fileName;
        public final String extension;
        public final String format;
        public final String sha1;
        public final int skinId;
        public final int weaponId;
        public final String weaponName;
        public final String audioName;
        public final int width;
        public final int height;
        public final byte[] textureBytes;
        public File stagedFile;
        public File manifestFile;
        public File activeManifestFile;

        private Request(JSONObject header, byte[] textureBytes, String sha1) {
            this.requestId = header.optString("requestId", "");
            this.command = header.optString("command", COMMAND_REPLACE);
            boolean restoring = COMMAND_RESTORE.equals(this.command) || COMMAND_WEAPON_AUDIO_RESTORE.equals(this.command);
            this.installMode = header.optString("installMode", restoring ? MODE_RESTORE : "temporary");
            this.textureName = header.optString("textureName", "skin");
            this.targetGroup = header.optString("targetGroup", "player");
            this.modelName = header.optString("modelName", "");
            this.skinName = header.optString("skinName", "");
            this.action = header.optString("action", "replace");
            this.fileName = header.optString("fileName", "texture.bin");
            this.extension = header.optString("extension", ".bin");
            this.format = header.optString("format", "unknown");
            this.skinId = header.optInt("skinId", 0);
            this.weaponId = header.optInt("weaponId", -1);
            this.weaponName = header.optString("weaponName", "");
            this.audioName = header.optString("audioName", "");
            this.width = header.optInt("width", 0);
            this.height = header.optInt("height", 0);
            this.textureBytes = textureBytes;
            this.sha1 = sha1;
        }

        static Request from(JSONObject header, byte[] textureBytes, String sha1) {
            return new Request(header, textureBytes, sha1);
        }

        static Request fromManifest(JSONObject manifest, File activeManifestFile) {
            Request request = new Request(manifest, new byte[0], manifest.optString("sha1", ""));
            String stagedPath = manifest.optString("stagedFile", "");
            if (!stagedPath.isEmpty()) {
                request.stagedFile = new File(stagedPath);
            }
            request.activeManifestFile = activeManifestFile;
            return request;
        }

        boolean isPermanent() {
            return MODE_PERMANENT.equalsIgnoreCase(installMode);
        }

        boolean isWeaponAudio() {
            return COMMAND_WEAPON_AUDIO_REPLACE.equals(command)
                    || COMMAND_WEAPON_AUDIO_RESTORE.equals(command)
                    || "weapon_audio".equalsIgnoreCase(targetGroup);
        }

        boolean isRestore() {
            return COMMAND_RESTORE.equals(command)
                    || COMMAND_WEAPON_AUDIO_RESTORE.equals(command)
                    || MODE_RESTORE.equalsIgnoreCase(installMode);
        }

        JSONObject toManifestJson() throws JSONException {
            JSONObject manifest = new JSONObject();
            manifest.put("protocol", PROTOCOL);
            manifest.put("version", 1);
            manifest.put("command", command);
            manifest.put("installMode", installMode);
            manifest.put("requestId", requestId);
            manifest.put("textureName", textureName);
            manifest.put("targetGroup", targetGroup);
            manifest.put("modelName", modelName);
            manifest.put("skinName", skinName);
            manifest.put("action", action);
            manifest.put("fileName", fileName);
            manifest.put("extension", extension);
            manifest.put("format", format);
            manifest.put("sha1", sha1);
            manifest.put("skinId", skinId);
            manifest.put("weaponId", weaponId);
            manifest.put("weaponName", weaponName);
            manifest.put("audioName", audioName);
            manifest.put("width", width);
            manifest.put("height", height);
            if (stagedFile != null) {
                manifest.put("stagedFile", stagedFile.getAbsolutePath());
            }
            if (manifestFile != null) {
                manifest.put("manifestFile", manifestFile.getAbsolutePath());
            }
            return manifest;
        }
    }

    public static final class Result {
        public final boolean ok;
        public final boolean nativeTextureApplied;
        public final String message;

        private Result(boolean ok, boolean nativeTextureApplied, String message) {
            this.ok = ok;
            this.nativeTextureApplied = nativeTextureApplied;
            this.message = message;
        }

        public static Result success(boolean nativeTextureApplied, String message) {
            return new Result(true, nativeTextureApplied, message);
        }

        public static Result error(String message) {
            return new Result(false, false, message == null ? "Falha no hot reload." : message);
        }
    }
}
