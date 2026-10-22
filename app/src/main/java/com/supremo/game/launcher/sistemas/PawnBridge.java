package com.supremo.launcher.launcher.sistemas;

import java.net.HttpURLConnection;
import java.net.URL;

public class PawnBridge {

    private static final String SERVER =
            "https://sistemas-production-aeb8.up.railway.app/send";

    public static void send(String action, String data) {

        new Thread(() -> {
            HttpURLConnection conn = null;

            try {
                URL url = new URL(SERVER);
                conn = (HttpURLConnection) url.openConnection();

                conn.setRequestMethod("POST");
                conn.setRequestProperty("Content-Type", "application/json");
                conn.setDoOutput(true);

                String json = "{"
                        + "\"action\":\"" + action + "\","
                        + "\"data\":\"" + data + "\""
                        + "}";

                conn.getOutputStream().write(json.getBytes());
                conn.getResponseCode();

            } catch (Exception e) {
                e.printStackTrace();
            } finally {
                if (conn != null) conn.disconnect();
            }
        }).start();
    }
}