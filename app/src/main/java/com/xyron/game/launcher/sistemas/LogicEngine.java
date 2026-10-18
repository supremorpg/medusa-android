package com.xyron.game.launcher.sistemas;

import org.json.JSONObject;

public class LogicEngine {

    public static void executeCommand(String rawJson) {

        try {
            JSONObject obj = new JSONObject(rawJson);

            String action = obj.optString("action");
            String data = obj.optString("data");
            int playerid = obj.optInt("playerid", -1);

            if (action == null) return;

            switch (action.toLowerCase()) {

                case "open_ui":
                    UIManager.openOverlay(data);
                    break;

                case "givemoney":
                    PawnBridge.send("GIVE_MONEY", playerid + ":" + data);
                    break;

                case "sethealth":
                    PawnBridge.send("SET_HEALTH", playerid + ":" + data);
                    break;

                case "teleport":
                    System.out.println("TELEPORT action ignored in diagnostics build");
                    break;

                default:
                    System.out.println("Unknown action: " + action);
            }

        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}
