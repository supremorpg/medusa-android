package com.xyron.game.launcher;

import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.text.TextUtils;
import android.util.Log;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import com.xyron.game.BuildConfig;
import com.xyron.game.main.SAMP;
import com.xyron.game.launcher.util.ConfigValidator;
import com.xyron.game.launcher.util.GameDataVerifier;
import com.xyron.game.launcher.util.ServerConfigManager;
import com.xyron.game.launcher.util.SharedPreferenceCore;

import org.ini4j.Wini;

import java.io.File;
import java.io.IOException;

public class EntryActivity extends SampActivity {
    public static final String EXTRA_INITIAL_TAB = "initial_tab";
    public static final String EXTRA_FORCE_UPDATE_DATA = "force_update_data";
    public static final String EXTRA_AUTO_PLAY = "auto_play";

    private static final String EXTRA_SERVER_IP = "server_ip";
    private static final String EXTRA_SERVER_PORT = "server_port";
    private static final String EXTRA_NICKNAME = "nickname";
    private static final String TAG = "EntryActivity";
    private static final int REQUEST_GAME_PERMISSIONS = 2301;
    private static final String[] GAME_PERMISSIONS = new String[] {
            "android.permission.READ_EXTERNAL_STORAGE",
            "android.permission.WRITE_EXTERNAL_STORAGE",
            "android.permission.RECORD_AUDIO"
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        if (isGameRole() && !hasGamePermissions()) {
            ActivityCompat.requestPermissions(this, GAME_PERMISSIONS, REQUEST_GAME_PERMISSIONS);
            return;
        }

        continueEntry();
    }

    private void continueEntry() {
        ConfigValidator.validateConfigFiles(this);
        applyIncomingConnection(getIntent());
        applyIncomingGameSettings(getIntent());

        boolean forceUpdate = getIntent() != null
                && getIntent().getBooleanExtra(EXTRA_FORCE_UPDATE_DATA, false);
        if (forceUpdate) {
            Intent updateIntent = new Intent(this, UpdateActivity.class);
            if (getIntent() != null && getIntent().getExtras() != null) {
                updateIntent.putExtras(getIntent().getExtras());
            }
            updateIntent.putExtra("mode", UpdateActivity.UpdateMode.GameDataUpdate.name());
            startActivity(updateIntent);
            finish();
            return;
        }

        if (isGameRole() && shouldAutoPlayGame()) {
            launchGameEntry();
            return;
        }

        Intent launcherIntent = new Intent(this, MainActivity.class);
        if (getIntent() != null && getIntent().getExtras() != null) {
            launcherIntent.putExtras(getIntent().getExtras());
        }
        if (isToolsRole()
                && (getIntent() == null || !getIntent().hasExtra(EXTRA_INITIAL_TAB))) {
            launcherIntent.putExtra(EXTRA_INITIAL_TAB, MainActivity.TAB_HOST);
        }
        launcherIntent.addFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
        startActivity(launcherIntent);
        overridePendingTransition(0, 0);
        finish();
    }

    private boolean shouldAutoPlayGame() {
        return getIntent() != null && getIntent().getBooleanExtra(EXTRA_AUTO_PLAY, false);
    }

    @Override
    public void onRequestPermissionsResult(
            int requestCode,
            @NonNull String[] permissions,
            @NonNull int[] grantResults
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode != REQUEST_GAME_PERMISSIONS) {
            return;
        }

        boolean granted = grantResults.length == GAME_PERMISSIONS.length;
        for (int grantResult : grantResults) {
            granted &= grantResult == PackageManager.PERMISSION_GRANTED;
        }

        if (!granted) {
            Toast.makeText(this, "Permissoes do jogo nao concedidas.", Toast.LENGTH_LONG).show();
            finish();
            return;
        }

        continueEntry();
    }

    private void launchGameEntry() {
        if (!GameDataVerifier.hasRequiredGameData(this)) {
            Intent updateIntent = new Intent(this, UpdateActivity.class);
            updateIntent.putExtra("mode", UpdateActivity.UpdateMode.GameDataUpdate.name());
            startActivity(updateIntent);
            finish();
            return;
        }

        ServerConfigManager.ensureSelectedServer(this);
        ServerConfigManager.ServerOption selectedServer =
                ServerConfigManager.getSelectedServer(this);

        if (selectedServer != null && selectedServer.isValid()) {
            Intent gameIntent = new Intent(this, SAMP.class);
            if (getIntent() != null && getIntent().getExtras() != null) {
                gameIntent.putExtras(getIntent().getExtras());
            }
            gameIntent.addFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
            startActivity(gameIntent);
            overridePendingTransition(0, 0);
            finish();
            return;
        }

        Intent setupIntent = new Intent(this, MainActivity.class);
        setupIntent.putExtra(EXTRA_INITIAL_TAB, MainActivity.TAB_SETTINGS);
        setupIntent.addFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
        startActivity(setupIntent);
        overridePendingTransition(0, 0);
        finish();
    }

    private void applyIncomingConnection(Intent intent) {
        if (intent == null) {
            return;
        }

        String host = sanitize(intent.getStringExtra(EXTRA_SERVER_IP));
        int port = intent.getIntExtra(EXTRA_SERVER_PORT, 0);
        if (!TextUtils.isEmpty(host) && port > 0 && port <= 65535) {
            ServerConfigManager.ServerOption option = ServerConfigManager.addOrUpdateServer(
                    this,
                    "Launcher",
                    host,
                    port,
                    true
            );

            if (option == null || !option.isValid()) {
                java.util.List<ServerConfigManager.ServerOption> servers =
                        ServerConfigManager.getAvailableServers(this);
                if (!servers.isEmpty()) {
                    ServerConfigManager.removeServer(this, servers.get(servers.size() - 1));
                    option = ServerConfigManager.addOrUpdateServer(
                            this,
                            "Launcher",
                            host,
                            port,
                            true
                    );
                }
            }

            if (option == null || !option.isValid()) {
                option = new ServerConfigManager.ServerOption("Launcher", host, port, true);
            }

            if (option != null && option.isValid()) {
                ServerConfigManager.saveSelectedServer(this, option);
            }
        }

        String nickname = sanitizeNickname(intent.getStringExtra(EXTRA_NICKNAME));
        if (!TextUtils.isEmpty(nickname)) {
            saveNickname(nickname);
        }
    }

    private void saveNickname(String nickname) {
        File settingsFile = new File(getExternalFilesDir(null), "SAMP/settings.ini");
        File parent = settingsFile.getParentFile();
        if (parent != null && !parent.exists() && !parent.mkdirs()) {
            Log.w(TAG, "Could not create settings directory.");
            return;
        }

        try {
            if (!settingsFile.exists() && !settingsFile.createNewFile()) {
                Log.w(TAG, "Could not create settings.ini.");
                return;
            }

            Wini wini = new Wini(settingsFile);
            wini.put("client", "name", nickname);
            wini.store();
        } catch (IOException e) {
            Log.e(TAG, "Could not save incoming nickname.", e);
        }
    }

    private void applyIncomingGameSettings(Intent intent) {
        if (intent == null) {
            return;
        }

        boolean hasChat = intent.hasExtra(SAMP.EXTRA_CHAT_MAX_MESSAGES);
        boolean hasKeyboard = intent.hasExtra(SAMP.EXTRA_ANDROID_KEYBOARD);
        boolean hasVoice = intent.hasExtra(SAMP.EXTRA_VOICE_CHAT_ENABLE);
        if (!hasChat && !hasKeyboard && !hasVoice) {
            return;
        }

        File settingsFile = new File(getExternalFilesDir(null), "SAMP/settings.ini");
        File parent = settingsFile.getParentFile();
        if (parent != null && !parent.exists() && !parent.mkdirs()) {
            Log.w(TAG, "Could not create settings directory for incoming game settings.");
            return;
        }

        try {
            if (!settingsFile.exists() && !settingsFile.createNewFile()) {
                Log.w(TAG, "Could not create settings.ini for incoming game settings.");
                return;
            }

            Wini wini = new Wini(settingsFile);
            if (hasChat) {
                int chatMaxMessages = intent.getIntExtra(SAMP.EXTRA_CHAT_MAX_MESSAGES, 5);
                wini.put("gui", "ChatMaxMessages", Math.max(chatMaxMessages, 5));
            }
            if (hasKeyboard) {
                boolean keyboardEnabled = intent.getBooleanExtra(SAMP.EXTRA_ANDROID_KEYBOARD, false);
                wini.put("gui", "androidkeyboard23123", keyboardEnabled);
                wini.put("gui", "androidkeyboard", keyboardEnabled ? 1 : 0);
                new SharedPreferenceCore().setBoolean(
                        getApplicationContext(),
                        "ANDROID_KEYBOARD",
                        keyboardEnabled
                );
            }
            if (hasVoice) {
                boolean voiceEnabled = intent.getBooleanExtra(SAMP.EXTRA_VOICE_CHAT_ENABLE, true);
                wini.put("gui", "VoiceChatEnable", voiceEnabled);
                new SharedPreferenceCore().setBoolean(
                        getApplicationContext(),
                        "VOICE_CHAT_ENABLE",
                        voiceEnabled
                );
            }
            wini.store();
        } catch (IOException e) {
            Log.e(TAG, "Could not save incoming game settings.", e);
        }
    }

    private String sanitize(String value) {
        return value == null ? "" : value.trim();
    }

    private String sanitizeNickname(String value) {
        String trimmed = sanitize(value);
        if (trimmed.isEmpty()) {
            return "";
        }

        StringBuilder builder = new StringBuilder();
        for (int i = 0; i < trimmed.length() && builder.length() < 24; i++) {
            char current = trimmed.charAt(i);
            boolean accepted = (current >= 'A' && current <= 'Z')
                    || (current >= 'a' && current <= 'z')
                    || (current >= '0' && current <= '9')
                    || current == '_';
            if (accepted) {
                builder.append(current);
            }
        }

        return builder.length() >= 3 ? builder.toString() : "Player123";
    }

    private boolean isGameRole() {
        return "game".equals(BuildConfig.XYRON_APK_ROLE);
    }

    private boolean isToolsRole() {
        return "tools".equals(BuildConfig.XYRON_APK_ROLE);
    }

    private boolean hasGamePermissions() {
        for (String permission : GAME_PERMISSIONS) {
            if (ContextCompat.checkSelfPermission(this, permission)
                    != PackageManager.PERMISSION_GRANTED) {
                return false;
            }
        }
        return true;
    }
}
