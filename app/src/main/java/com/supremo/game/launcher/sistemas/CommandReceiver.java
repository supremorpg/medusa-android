package com.supremo.launcher.launcher.sistemas;

import android.util.Log;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.net.HttpURLConnection;
import java.net.URL;

public class CommandReceiver {
    private static final String TAG = "CommandReceiver";

    private static final String SERVER =
            "https://sistemas-production-aeb8.up.railway.app/get";
    private static final long POLL_INTERVAL_MS = 5000L;
    private static volatile boolean started;

    public static void startListening() {
        if (started) {
            return;
        }
        started = true;

        new Thread(() -> {

            while (true) {
                HttpURLConnection conn = null;
                try {
                    URL url = new URL(SERVER);
                    conn = (HttpURLConnection) url.openConnection();

                    conn.setRequestMethod("GET");
                    conn.setConnectTimeout(5000);
                    conn.setReadTimeout(5000);

                    String response;
                    try (BufferedReader br = new BufferedReader(
                            new InputStreamReader(conn.getInputStream()))) {
                        StringBuilder sb = new StringBuilder();
                        String line;
                        while ((line = br.readLine()) != null) {
                            sb.append(line);
                        }
                        response = sb.toString();
                    }

                    Log.d(TAG, "Receiver response: " + response);

                    if (response.contains("\"open_ui\"") ||
                            response.contains("\"givemoney\"") ||
                            response.contains("\"sethealth\"") ||
                            response.contains("\"teleport\"")) {

                        LogicEngine.executeCommand(response);
                    }

                } catch (Exception e) {
                    Log.w(TAG, "Command polling failed: " + e.getMessage());
                } finally {
                    if (conn != null) {
                        conn.disconnect();
                    }
                    sleepQuietly();
                }
            }

        }, "supremo-command-receiver").start();
    }

    private static void sleepQuietly() {
        try {
            Thread.sleep(POLL_INTERVAL_MS);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
    }
}
