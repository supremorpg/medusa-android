package com.xyron.game.main;

import android.util.Log;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

final class XyronUiBatchDispatcher {
    private static final String TAG = "XyronUiBatch";

    private XyronUiBatchDispatcher() {
    }

    static void apply(SAMP samp, String payload) {
        if (samp == null || payload == null) {
            return;
        }

        String trimmed = payload.trim();
        if (trimmed.length() == 0) {
            return;
        }

        if (trimmed.startsWith("{")) {
            applyJson(samp, trimmed);
            return;
        }

        String[] lines = trimmed.split("[\\n;]");
        for (String line : lines) {
            applyLine(samp, line.trim());
        }
    }

    private static void applyJson(SAMP samp, String payload) {
        try {
            JSONObject root = new JSONObject(payload);
            JSONObject hud = root.optJSONObject("hud");
            if (hud != null) {
                samp.UpdateHud(
                        hud.optInt("hp", 100),
                        hud.optInt("armour", 0),
                        hud.optInt("eat", 100),
                        hud.optInt("money", 0),
                        hud.optInt("gunId", 0),
                        hud.optInt("ammo", 0)
                );
            }

            if (root.has("weaponWheel")) {
                Object weaponWheel = root.opt("weaponWheel");
                if (weaponWheel instanceof JSONArray) {
                    samp.UpdateWeaponWheel(((JSONArray) weaponWheel).toString());
                } else if (weaponWheel instanceof String) {
                    samp.UpdateWeaponWheel((String) weaponWheel);
                }
            }

            JSONArray commands = root.optJSONArray("commands");
            if (commands != null) {
                for (int i = 0; i < commands.length(); i++) {
                    applyLine(samp, commands.optString(i, ""));
                }
            }
        } catch (JSONException error) {
            Log.w(TAG, "Invalid UI batch JSON.", error);
        }
    }

    private static void applyLine(SAMP samp, String line) {
        if (line.length() == 0) {
            return;
        }

        String[] parts = line.split("\\|", 8);
        String command = parts[0].trim();
        try {
            if ("showHud".equals(command)) {
                samp.showhud();
            } else if ("hideHud".equals(command)) {
                samp.hidehud();
            } else if ("showLoading".equals(command)) {
                samp.showLoadingScreen();
            } else if ("hideLoading".equals(command)) {
                samp.hideLoadingScreen();
            } else if ("hud".equals(command) && parts.length >= 7) {
                samp.UpdateHud(
                        parseInt(parts[1], 100),
                        parseInt(parts[2], 0),
                        parseInt(parts[3], 100),
                        parseInt(parts[4], 0),
                        parseInt(parts[5], 0),
                        parseInt(parts[6], 0)
                );
            } else if ("weaponWheel".equals(command) && parts.length >= 2) {
                samp.UpdateWeaponWheel(parts[1]);
            } else if ("passenger".equals(command) && parts.length >= 2) {
                samp.togglePassengerButton(parseBoolean(parts[1]));
            } else if ("lock".equals(command) && parts.length >= 2) {
                samp.toggleLockButton(parseBoolean(parts[1]));
            }
        } catch (RuntimeException error) {
            Log.w(TAG, "Failed to apply UI batch line: " + line, error);
        }
    }

    private static boolean parseBoolean(String value) {
        return "1".equals(value) || "true".equalsIgnoreCase(value) || "yes".equalsIgnoreCase(value);
    }

    private static int parseInt(String value, int fallback) {
        try {
            return Integer.parseInt(value.trim());
        } catch (NumberFormatException ignored) {
            return fallback;
        }
    }
}
