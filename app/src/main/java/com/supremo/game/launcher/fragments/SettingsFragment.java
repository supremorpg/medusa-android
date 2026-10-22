package com.supremo.launcher.launcher.fragments;

import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.os.Bundle;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.CompoundButton;
import android.widget.EditText;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.widget.SwitchCompat;
import androidx.fragment.app.Fragment;

import com.supremo.launcher.BuildConfig;
import com.supremo.launcher.R;
import com.supremo.launcher.launcher.EntryActivity;
import com.supremo.launcher.launcher.UpdateActivity;
import com.supremo.launcher.launcher.util.ButtonAnimator;
import com.supremo.launcher.launcher.util.ServerConfigManager;
import com.supremo.launcher.launcher.util.SharedPreferenceCore;

import org.ini4j.Wini;

import java.io.File;
import java.io.IOException;

public class SettingsFragment extends Fragment {

    private Wini mWini;
    private SwitchCompat mKeyboardSwitch;
    private SwitchCompat mVoipSwitch;
    private TextView nicknameValue;

    @Override
    public View onCreateView(LayoutInflater layoutInflater, ViewGroup viewGroup, Bundle bundle) {
        View view = layoutInflater.inflate(R.layout.fragment_settings, viewGroup, false);

        mKeyboardSwitch = view.findViewById(R.id.keyboard_switch);
        mVoipSwitch = view.findViewById(R.id.voip_switch);
        nicknameValue = view.findViewById(R.id.player_name_value);
        TextView editProfileButton = view.findViewById(R.id.button_edit_profile);
        TextView addServerButton = view.findViewById(R.id.button_add_server_shortcut);
        TextView reinstallButton = view.findViewById(R.id.button_reinstall);

        openSettingsFile();
        configureChatSpinner(view);
        loadNickname();
        configureLauncherRenderControls(view);

        if (editProfileButton != null) {
            editProfileButton.setOnTouchListener(new ButtonAnimator(getContext(), editProfileButton));
            editProfileButton.setOnClickListener(v -> showNicknameDialog());
        }
        if (nicknameValue != null) {
            nicknameValue.setOnTouchListener(new ButtonAnimator(getContext(), nicknameValue));
            nicknameValue.setOnClickListener(v -> showNicknameDialog());
        }

        if (addServerButton != null) {
            addServerButton.setOnTouchListener(new ButtonAnimator(getContext(), addServerButton));
            addServerButton.setOnClickListener(v -> showAddServerDialog());
        }

        if (reinstallButton != null) {
            if (isLauncherRole()) {
                reinstallButton.setText("Abrir download do Game");
            } else if (isToolsRole()) {
                reinstallButton.setText("Download fica no Game");
            }
            reinstallButton.setOnTouchListener(new ButtonAnimator(getContext(), reinstallButton));
            reinstallButton.setOnClickListener(v -> handleDataAction());
        }

        if (mKeyboardSwitch != null) {
            mKeyboardSwitch.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
                @Override
                public void onCheckedChanged(CompoundButton compoundButton, boolean checked) {
                    new SharedPreferenceCore().setBoolean(
                            requireContext().getApplicationContext(),
                            "ANDROID_KEYBOARD",
                            checked
                    );
                    saveIniValue("gui", "androidkeyboard23123", checked);
                }
            });
        }

        if (mVoipSwitch != null) {
            mVoipSwitch.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
                @Override
                public void onCheckedChanged(CompoundButton compoundButton, boolean checked) {
                    new SharedPreferenceCore().setBoolean(
                            requireContext().getApplicationContext(),
                            "VOICE_CHAT_ENABLE",
                            checked
                    );
                    saveIniValue("gui", "VoiceChatEnable", checked);
                }
            });
        }

        return view;
    }

    private void configureLauncherRenderControls(View view) {
        TextView fps30 = view.findViewById(R.id.launcher_fps_30);
        TextView fps60 = view.findViewById(R.id.launcher_fps_60);
        TextView fps90 = view.findViewById(R.id.launcher_fps_90);
        TextView fps120 = view.findViewById(R.id.launcher_fps_120);
        TextView[] buttons = new TextView[]{fps30, fps60, fps90, fps120};
        int[] values = new int[]{30, 60, 90, 120};

        int selectedFps = readIntSetting("gui", "FPSLimit", 120);
        boolean migratedHighRefresh = parseBooleanSetting(
                readIniValue("gui", "HighRefreshDefaultMigrated", "0"),
                false
        );
        if (!migratedHighRefresh && selectedFps <= 60) {
            selectedFps = 120;
            saveIniValue("gui", "FPSLimit", selectedFps);
            saveIniValue("gui", "HighRefreshDefaultMigrated", 1);
        }
        updateLauncherFpsButtons(buttons, values, selectedFps);
        for (int index = 0; index < buttons.length; index++) {
            TextView button = buttons[index];
            final int fps = values[index];
            if (button == null) {
                continue;
            }
            button.setOnTouchListener(new ButtonAnimator(getContext(), button));
            button.setOnClickListener(v -> {
                saveIniValue("gui", "FPSLimit", fps);
                saveIniValue("gui", "fps", 0);
                updateLauncherFpsButtons(buttons, values, fps);
            });
        }

        TextView qualityValue = view.findViewById(R.id.launcher_quality_value);
        if (qualityValue != null) {
            updateLauncherQualityText(qualityValue);
            qualityValue.setOnTouchListener(new ButtonAnimator(getContext(), qualityValue));
            qualityValue.setOnClickListener(v -> cycleLauncherQuality(qualityValue));
        }
    }

    private void updateLauncherFpsButtons(TextView[] buttons, int[] values, int selectedFps) {
        for (int index = 0; index < buttons.length; index++) {
            TextView button = buttons[index];
            if (button == null) {
                continue;
            }
            boolean selected = values[index] == selectedFps;
            button.setBackgroundResource(selected ? R.drawable.vice_button_active : R.drawable.vice_button_outline);
            button.setTextColor(Color.parseColor(selected ? "#111111" : "#FFFFFFFF"));
        }
    }

    private void updateLauncherQualityText(TextView qualityValue) {
        int renderDistance = readIntSetting("gui", "RenderDistance", 45);
        boolean lowRam = parseBooleanSetting(readIniValue("gui", "LowRamMode", "1"), true);
        boolean effects = parseBooleanSetting(readIniValue("gui", "Effects", "0"), false);
        boolean shadows = parseBooleanSetting(readIniValue("gui", "Shadows", "0"), false);

        if (lowRam || renderDistance <= 55 || !effects) {
            qualityValue.setText("Baixo (Performance)");
            qualityValue.setTag("low");
        } else if (renderDistance >= 120 && shadows) {
            qualityValue.setText("Alto (Visual)");
            qualityValue.setTag("high");
        } else {
            qualityValue.setText("Medio (Equilibrado)");
            qualityValue.setTag("medium");
        }
    }

    private void cycleLauncherQuality(TextView qualityValue) {
        Object tag = qualityValue.getTag();
        String current = tag == null ? "low" : tag.toString();
        if ("low".equals(current)) {
            saveIniValue("gui", "RenderDistance", 80);
            saveIniValue("gui", "LowRamMode", 0);
            saveIniValue("gui", "Effects", 1);
            saveIniValue("gui", "Shadows", 0);
            qualityValue.setText("Medio (Equilibrado)");
            qualityValue.setTag("medium");
            return;
        }

        if ("medium".equals(current)) {
            saveIniValue("gui", "RenderDistance", 140);
            saveIniValue("gui", "LowRamMode", 0);
            saveIniValue("gui", "Effects", 1);
            saveIniValue("gui", "Shadows", 1);
            qualityValue.setText("Alto (Visual)");
            qualityValue.setTag("high");
            return;
        }

        saveIniValue("gui", "RenderDistance", 45);
        saveIniValue("gui", "LowRamMode", 1);
        saveIniValue("gui", "Effects", 0);
        saveIniValue("gui", "Shadows", 0);
        qualityValue.setText("Baixo (Performance)");
        qualityValue.setTag("low");
    }

    @Override
    public void onResume() {
        super.onResume();
        if (mKeyboardSwitch != null) {
            mKeyboardSwitch.setChecked(new SharedPreferenceCore().getBoolean(
                    requireContext().getApplicationContext(),
                    "ANDROID_KEYBOARD"
            ));
        }
        if (mVoipSwitch != null) {
            mVoipSwitch.setChecked(parseBooleanSetting(
                    readIniValue("gui", "VoiceChatEnable", "true"),
                    true
            ));
        }
        loadNickname();
    }

    private void openSettingsFile() {
        if (getActivity() == null || getActivity().getExternalFilesDir(null) == null) {
            return;
        }

        File file = new File(getActivity().getExternalFilesDir(null), "SAMP/settings.ini");
        try {
            if (!file.exists()) {
                File parent = file.getParentFile();
                if (parent != null && !parent.exists()) {
                    parent.mkdirs();
                }
                file.createNewFile();
            }
            mWini = new Wini(file);
        } catch (IOException e) {
            mWini = null;
            e.printStackTrace();
        }
    }

    private void configureChatSpinner(View view) {
        Spinner spinner = view.findViewById(R.id.chat_count_select);
        if (spinner == null) {
            return;
        }

        String[] sort = {"5", "10", "15", "20"};
        ArrayAdapter<String> adapter = new ArrayAdapter<>(requireActivity(), R.layout.spinner_item, sort);
        adapter.setDropDownViewResource(R.layout.spinner_dropdown_item);
        spinner.setAdapter(adapter);

        spinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> adapterView, View selectedView, int index, long id) {
                int chatCount = 5;
                switch (index) {
                    case 1:
                        chatCount = 10;
                        break;
                    case 2:
                        chatCount = 15;
                        break;
                    case 3:
                        chatCount = 20;
                        break;
                    default:
                        chatCount = 5;
                        break;
                }

                Log.d("x1y2z", "chatCount " + chatCount);
                saveIniValue("gui", "ChatMaxMessages", chatCount);
            }

            @Override
            public void onNothingSelected(AdapterView<?> adapterView) {
            }
        });

        String message = readIniValue("gui", "ChatMaxMessages", "5");
        switch (message) {
            case "10":
                spinner.setSelection(1);
                break;
            case "15":
                spinner.setSelection(2);
                break;
            case "20":
                spinner.setSelection(3);
                break;
            default:
                spinner.setSelection(0);
                break;
        }
    }

    private void loadNickname() {
        if (nicknameValue == null) {
            return;
        }

        String nickname = readIniValue("client", "name", "");
        nicknameValue.setText(nickname.isEmpty() ? "Nenhum nome definido" : nickname);
    }

    private void saveNickname(String nickname) {
        String safeNickname = sanitizeNickname(nickname);
        if (safeNickname.isEmpty()) {
            Toast.makeText(getContext(), "Digite um nome para salvar no jogo.", Toast.LENGTH_SHORT).show();
            return;
        }

        if (!saveIniValue("client", "name", safeNickname)) {
            Toast.makeText(getContext(), "Nao foi possivel salvar o nome.", Toast.LENGTH_SHORT).show();
            return;
        }

        String message = safeNickname.equals(nickname)
                ? "Nome salvo no launcher e no arquivo do jogo."
                : "Nome ajustado para " + safeNickname + " e salvo no jogo.";
        Toast.makeText(getContext(), message, Toast.LENGTH_SHORT).show();
        loadNickname();
    }

    private String sanitizeNickname(String value) {
        if (value == null) {
            return "";
        }

        String trimmed = value.trim();
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

    private void handleDataAction() {
        if (getActivity() == null) {
            return;
        }

        if (isLauncherRole()) {
            launchGameDataUpdate();
            return;
        }

        if (isToolsRole()) {
            Toast.makeText(getContext(), "Baixe ou reinstale a data pelo APK Voltara RP.", Toast.LENGTH_LONG).show();
            return;
        }

        Intent intent = new Intent(getActivity(), UpdateActivity.class);
        intent.putExtra("mode", UpdateActivity.UpdateMode.GameDataUpdate.name());
        intent.putExtra(UpdateActivity.EXTRA_FORCE_REINSTALL_DATA, true);
        startActivity(intent);
        getActivity().finish();
    }

    private void launchGameDataUpdate() {
        Intent intent = new Intent();
        intent.setClassName(BuildConfig.SUPREMO_GAME_PACKAGE, "com.supremo.launcher.launcher.EntryActivity");
        intent.putExtra(EntryActivity.EXTRA_FORCE_UPDATE_DATA, true);
        intent.putExtra(UpdateActivity.EXTRA_FORCE_REINSTALL_DATA, true);
        intent.addFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
        try {
            startActivity(intent);
        } catch (ActivityNotFoundException | SecurityException e) {
            Toast.makeText(getContext(), "Instale o APK Voltara RP para baixar a data.", Toast.LENGTH_LONG).show();
        }
    }

    private String readIniValue(String section, String key, String fallback) {
        if (mWini == null) {
            return fallback;
        }

        String value = mWini.get(section, key);
        return value == null ? fallback : value.trim();
    }

    private boolean saveIniValue(String section, String key, Object value) {
        if (mWini == null) {
            return false;
        }

        try {
            mWini.put(section, key, value);
            mWini.store();
            return true;
        } catch (IOException e) {
            e.printStackTrace();
            return false;
        }
    }

    private int readIntSetting(String section, String key, int fallback) {
        try {
            return Integer.parseInt(readIniValue(section, key, String.valueOf(fallback)).trim());
        } catch (NumberFormatException error) {
            return fallback;
        }
    }

    private boolean parseBooleanSetting(String value, boolean fallback) {
        if (value == null || value.trim().isEmpty()) {
            return fallback;
        }

        String normalized = value.trim().toLowerCase();
        if ("1".equals(normalized) || "true".equals(normalized) || "yes".equals(normalized)) {
            return true;
        }
        if ("0".equals(normalized) || "false".equals(normalized) || "no".equals(normalized)) {
            return false;
        }
        return fallback;
    }

    private boolean isLauncherRole() {
        return "launcher".equals(BuildConfig.SUPREMO_APK_ROLE);
    }

    private boolean isToolsRole() {
        return "tools".equals(BuildConfig.SUPREMO_APK_ROLE);
    }

    private void showAddServerDialog() {
        if (getContext() == null) {
            return;
        }

        View dialogView = LayoutInflater.from(requireContext()).inflate(
                R.layout.dialog_add_server,
                null,
                false
        );
        EditText input = dialogView.findViewById(R.id.dialog_server_input);
        TextView cancelButton = dialogView.findViewById(R.id.dialog_server_cancel);
        TextView saveButton = dialogView.findViewById(R.id.dialog_server_save);
        TextView limitText = dialogView.findViewById(R.id.dialog_server_limit);

        if (limitText != null) {
            limitText.setText("Voce pode salvar ate "
                    + ServerConfigManager.getMaxSavedServers()
                    + " servidores.");
        }

        AlertDialog dialog = new AlertDialog.Builder(requireContext())
                .setView(dialogView)
                .create();
        dialog.setCancelable(true);
        if (dialog.getWindow() != null) {
            dialog.getWindow().setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        }

        if (cancelButton != null) {
            cancelButton.setOnTouchListener(new ButtonAnimator(getContext(), cancelButton));
            cancelButton.setOnClickListener(v -> dialog.dismiss());
        }

        if (saveButton != null) {
            saveButton.setOnTouchListener(new ButtonAnimator(getContext(), saveButton));
            saveButton.setOnClickListener(v -> {
                String rawAddress = input != null && input.getText() != null
                        ? input.getText().toString().trim()
                        : "";

                if (!ServerConfigManager.isValidRawAddress(rawAddress)) {
                    Toast.makeText(
                            getContext(),
                            "Use o formato IP:PORTA, por exemplo 127.0.0.1:7777.",
                            Toast.LENGTH_LONG
                    ).show();
                    return;
                }

                ServerConfigManager.ServerOption option =
                        ServerConfigManager.addOrUpdateServer(requireContext(), rawAddress);
                if (!option.isValid()) {
                    Toast.makeText(
                            getContext(),
                            "Voce pode salvar no maximo "
                                    + ServerConfigManager.getMaxSavedServers()
                                    + " servidores.",
                            Toast.LENGTH_SHORT
                    ).show();
                    return;
                }

                Toast.makeText(
                        getContext(),
                        "Servidor adicionado. Ele ja aparece na aba Servidores.",
                        Toast.LENGTH_SHORT
                ).show();
                dialog.dismiss();
            });
        }

        dialog.show();
        if (input != null) {
            input.requestFocus();
        }
    }

    private void showNicknameDialog() {
        if (getContext() == null) {
            return;
        }

        View dialogView = LayoutInflater.from(requireContext()).inflate(
                R.layout.dialog_edit_name,
                null,
                false
        );
        EditText input = dialogView.findViewById(R.id.dialog_name_input);
        TextView cancelButton = dialogView.findViewById(R.id.dialog_name_cancel);
        TextView saveButton = dialogView.findViewById(R.id.dialog_name_save);

        if (input != null) {
            String currentNickname = readIniValue("client", "name", "");
            input.setText(currentNickname);
            input.setSelection(input.getText() != null ? input.getText().length() : 0);
        }

        AlertDialog dialog = new AlertDialog.Builder(requireContext())
                .setView(dialogView)
                .create();
        dialog.setCancelable(true);
        if (dialog.getWindow() != null) {
            dialog.getWindow().setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        }

        if (cancelButton != null) {
            cancelButton.setOnTouchListener(new ButtonAnimator(getContext(), cancelButton));
            cancelButton.setOnClickListener(v -> dialog.dismiss());
        }

        if (saveButton != null) {
            saveButton.setOnTouchListener(new ButtonAnimator(getContext(), saveButton));
            saveButton.setOnClickListener(v -> {
                String nickname = input != null && input.getText() != null
                        ? input.getText().toString().trim()
                        : "";
                if (nickname.isEmpty()) {
                    Toast.makeText(getContext(), "Digite um nome para salvar no jogo.", Toast.LENGTH_SHORT).show();
                    return;
                }
                saveNickname(nickname);
                dialog.dismiss();
            });
        }

        dialog.show();
        if (input != null) {
            input.requestFocus();
        }
    }
}
