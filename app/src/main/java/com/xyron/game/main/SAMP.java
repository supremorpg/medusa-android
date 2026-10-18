package com.xyron.game.main;

import android.app.ActivityManager;
import android.app.Instrumentation;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.drawable.GradientDrawable;
import android.hardware.input.InputManager;
import android.media.AudioAttributes;
import android.media.AudioManager;
import android.media.SoundPool;
import android.os.Bundle;
import android.os.Build;
import android.os.Looper;
import android.os.SystemClock;
import android.util.Log;
import android.util.DisplayMetrics;
import android.util.SparseIntArray;
import android.util.TypedValue;
import android.view.InputDevice;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.TextView;

import androidx.constraintlayout.widget.ConstraintLayout;

import com.google.firebase.crashlytics.FirebaseCrashlytics;
import com.xyron.game.BuildConfig;
import com.xyron.game.R;
import com.xyron.game.launcher.MainActivity;
import com.xyron.game.main.ui.AttachEdit;
import com.xyron.game.main.ui.CustomKeyboard;
import com.xyron.game.main.ui.HdMapView;
import com.xyron.game.main.ui.PickupCreatorOverlay;
import com.xyron.game.main.ui.dialog.DialogManager;
import com.xyron.game.main.ui.RadialMenu;
import com.xyron.game.main.ui.RadialVehicles;
import com.xyron.game.main.ui.Radinho;
import com.xyron.game.main.perf.XyronPerformanceMonitor;
import com.xyron.game.main.perf.XyronRuntimeMonitor;

import java.io.UnsupportedEncodingException;
import java.nio.charset.StandardCharsets;
import java.text.Normalizer;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;
import java.util.regex.Pattern;

//API
import android.os.Handler;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import com.android.volley.Request;
import com.android.volley.RequestQueue;
import com.android.volley.toolbox.JsonObjectRequest;
import com.android.volley.toolbox.Volley;
import org.json.JSONException;
import org.json.JSONObject;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import android.widget.EditText;
import android.widget.Button;
import android.widget.Toast;
import android.os.AsyncTask;
import java.net.URL;
import java.net.HttpURLConnection;
import java.io.OutputStream;
import java.util.Scanner;
import java.lang.reflect.Method;

import com.xyron.game.launcher.util.ConfigValidator;
import com.xyron.game.launcher.util.HostShellEngine;
import com.xyron.game.launcher.util.PickupStudioManager;
import com.xyron.game.launcher.util.ServerConfigManager;
import com.xyron.game.launcher.util.SharedPreferenceCore;
import org.json.JSONArray;
import org.ini4j.Wini;

public class SAMP extends com.raiferoleplay.game.game.SAMP implements CustomKeyboard.InputListener, HeightProvider.HeightListener {
    private static final String TAG = "SAMP";
    private static final int SOURCE_ACTION_VEHICLE = 1;
    private static final int SOURCE_ACTION_ATTACK = 2;
    private static final int SOURCE_ACTION_ACCELERATE = 4;
    private static final int SOURCE_ACTION_BRAKE = 5;
    private static final int SOURCE_ACTION_HANDBRAKE = 6;
    private static final int SOURCE_ACTION_SPRINT = 7;
    private static final int SOURCE_ACTION_JUMP = 8;
    private static final int SOURCE_ACTION_HORN = 9;
    private static final int SOURCE_ACTION_CAMERA = 10;
    public static final String EXTRA_SERVER_IP = "server_ip";
    public static final String EXTRA_SERVER_PORT = "server_port";
    public static final String EXTRA_NICKNAME = "nickname";
    public static final String EXTRA_CHAT_MAX_MESSAGES = "chat_max_messages";
    public static final String EXTRA_ANDROID_KEYBOARD = "android_keyboard";
    public static final String EXTRA_VOICE_CHAT_ENABLE = "voice_chat_enable";
    public static final String EXTRA_RUNTIME_COMMAND = "runtime_command";
    private static final int SYSTEM_UI_IMMERSIVE_FLAGS =
            View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                    | View.SYSTEM_UI_FLAG_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                    | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                    | View.SYSTEM_UI_FLAG_LAYOUT_STABLE;
    private static final long RUNTIME_COMMAND_FILE_POLL_MS = 250L;
    private static final long AUTOMATIC_PAUSE_RESUME_SUPPRESSION_RELEASE_DELAY_MS = 1600L;
    private static final long[] HOT_RELOAD_REPLAY_DELAYS_MS = new long[]{1500L, 4000L, 8000L, 15000L, 30000L};
    private static final int RUNTIME_COMMAND_READY_RETRY_MAX = 24;
    private static final long RUNTIME_COMMAND_READY_RETRY_DELAY_MS = 1000L;
    private static final int[] VEHICLE_SELF_TEST_DEFAULT_MODELS = {560, 522, 487, 432};
    private static final long VEHICLE_SELF_TEST_MODEL_SPACING_MS = 23000L;
    private static final long[] NATIVE_MENU_RESUME_AFTER_BACKGROUND_DELAYS_MS = new long[]{220L, 760L, 1420L, 2200L};
    private static final long NATIVE_MENU_RESUME_AFTER_SYSTEM_UI_DELAY_MS = 450L;
    private static final long NATIVE_SETTINGS_FIRST_TAP_DELAY_MS = 380L;
    private static final long NATIVE_SETTINGS_SECOND_TAP_DELAY_MS = 720L;
    private static final float NATIVE_MENU_RESUME_X_RATIO = 0.50f;
    private static final float NATIVE_MENU_RESUME_Y_RATIO = 0.865f;
    private static final float NATIVE_MENU_SETTINGS_X_RATIO = 0.631f;
    private static final float NATIVE_MENU_SETTINGS_Y_RATIO = 0.762f;
    private static final boolean USE_NATIVE_IMGUI_OVERLAYS = true;
    private static final int NATIVE_OVERLAY_NONE = 0;
    private static final int NATIVE_OVERLAY_PHONE = 1;
    private static final int NATIVE_OVERLAY_INVENTORY = 2;
    private static final int NATIVE_OVERLAY_WEAPON_WHEEL = 3;
    private static final String HUD_RENDER_PREFS = "xyron_hud_render_settings";
    private static final String HUD_LAYOUT_PREFS = "xyron_hud_layout_settings";
    private static final String HUD_QUICK_PREFS = "xyron_hud_quick_settings";
    private static final String SMART_OPTIMIZER_PREFS = "xyron_smart_optimizer";
    private static final int HUD_QUALITY_FPS = 0;
    private static final int HUD_QUALITY_BALANCED = 1;
    private static final int HUD_QUALITY_HIGH = 2;
    private static final int HUD_RENDER_DISTANCE_MIN = 30;
    private static final int HUD_RENDER_DISTANCE_MAX = 160;
    private static final int HUD_FPS_LIMIT_MIN = 30;
    private static final int HUD_FPS_LIMIT_MAX = 120;
    private static final int SMART_OPTIMIZER_NORMAL = 0;
    private static final int SMART_OPTIMIZER_WARN = 1;
    private static final int SMART_OPTIMIZER_CRITICAL = 2;
    private static final long SMART_OPTIMIZER_INTERVAL_MS = 2000L;
    private static final long SMART_OPTIMIZER_LOG_INTERVAL_MS = 12000L;
    private static final long SMART_OPTIMIZER_QUARANTINE_INTERVAL_MS = 45000L;
    private static final int SMART_OPTIMIZER_WARN_FREE_RAM_MB = 240;
    private static final int SMART_OPTIMIZER_CRITICAL_FREE_RAM_MB = 150;
    private static final int SMART_OPTIMIZER_WARN_FPS = 24;
    private static final int SMART_OPTIMIZER_CRITICAL_FPS = 18;
    private static final String HUD_HIGH_REFRESH_MIGRATION_KEY = "high_refresh_default_120";
    private static final int HUD_CHAT_MAX_LINES = 60;
    private static final int ROLEPLAY_CHAT_MAX_CHARS = 144;
    private static final int ROLEPLAY_CHAT_MAX_WORDS = 28;
    private static final int HUD_CHAT_AREA_DEFAULT_LEFT_DP = 46;
    private static final int HUD_CHAT_AREA_DEFAULT_TOP_DP = 4;
    private static final int HUD_CHAT_AREA_DEFAULT_WIDTH_DP = 132;
    private static final int HUD_CHAT_AREA_DEFAULT_HEIGHT_DP = 24;
    private static final int HUD_CHAT_AREA_MIN_WIDTH_DP = 84;
    private static final int HUD_CHAT_AREA_MIN_HEIGHT_DP = 24;
    private static final int HUD_CHAT_MAP_ACTION_NONE = 0;
    private static final int HUD_CHAT_MAP_ACTION_NEW = 1;
    private static final int HUD_CHAT_MAP_ACTION_MOVE = 2;
    private static final int HUD_CHAT_MAP_ACTION_RESIZE = 3;
    private static final long HUD_CONTROL_FADE_MS = 160L;
    private static final long HUD_TOP_INFO_INTERVAL_MS = 15000L;
    private static final float HUD_NATIVE_VOICE_CHAT_DEFAULT_SIZE = 30.0f;
    private static final float HUD_NATIVE_VOICE_CHAT_DEFAULT_POS_X = 1520.0f;
    private static final float HUD_NATIVE_VOICE_CHAT_DEFAULT_POS_Y = 480.0f;
    private static final float HUD_NATIVE_VOICE_CHAT_DESIGN_WIDTH = 1920.0f;
    private static final float HUD_NATIVE_VOICE_CHAT_DESIGN_HEIGHT = 1080.0f;
    private static final float HUD_NATIVE_VOICE_CHAT_IMGUI_WIDTH = 103.0f;
    private static final float HUD_NATIVE_VOICE_CHAT_IMGUI_HEIGHT = 100.0f;
    private static final String HUD_EDIT_KEY_IMGUI_VOIP = "imgui_voip";
    private static final float HUD_CONTROL_ALPHA = 0.58f;
    private static final int[] HUD_CONTROL_ALPHA_OPTIONS = {60, 72, 85, 100};
    private static final int HUD_NEED_FOOD_COLOR = Color.rgb(245, 158, 11);
    private static final int HUD_NEED_THIRST_COLOR = Color.rgb(14, 165, 233);
    private static final int HUD_NEED_SLEEP_COLOR = Color.rgb(168, 85, 247);
    private static final Pattern CHAT_COLOR_PATTERN = Pattern.compile("\\{[0-9A-Fa-f]{6}\\}");
    private static final String DEFAULT_WEAPON_WHEEL_JSON = "[{\"id\":0,\"ammo\":0,\"current\":true}]";
    private static final int WEAPON_WHEEL_VISIBLE_SLOT_COUNT = 5;
    private static final int WEAPON_WHEEL_EMPTY_SLOT_BASE_ID = -1000;
    private static final int WEAPON_WHEEL_MAX_CARRIED_WEAPONS = WEAPON_WHEEL_VISIBLE_SLOT_COUNT - 1;
    private static final int[] CJ_OUTFIT_SKINS = {0, 101, 102, 103, 104, 105, 106, 107};
    private static final String[] CJ_OUTFIT_LABELS = {
            "CJ padrao",
            "CJ casual",
            "CJ rua 1",
            "CJ rua 2",
            "CJ esportivo",
            "CJ jaqueta",
            "CJ gang 1",
            "CJ gang 2"
    };
    private static SAMP instance;
    private static boolean sampNativeLibraryLoadAttempted;
    private static boolean sampNativeLibraryAvailable;
    private XyronPerformanceMonitor performanceMonitor;
    private XyronRuntimeMonitor runtimeMonitor;

    private static String normalizeWeaponAlias(String value) {
        if (value == null) {
            return "";
        }
        String normalized = Normalizer.normalize(value, Normalizer.Form.NFD)
                .replaceAll("\\p{M}", "")
                .toLowerCase(Locale.US);
        return normalized.replaceAll("[^a-z0-9]", "");
    }

    private static synchronized boolean ensureSampNativeLibraryLoaded() {
        if (sampNativeLibraryLoadAttempted) {
            return sampNativeLibraryAvailable;
        }
        sampNativeLibraryLoadAttempted = true;
        String libraryName = GTASA.getSampLibraryName();
        try {
            System.loadLibrary(libraryName);
            sampNativeLibraryAvailable = true;
        } catch (UnsatisfiedLinkError error) {
            Log.e(TAG, "Could not load lib" + libraryName + ".so.", error);
            sampNativeLibraryAvailable = false;
        }
        return sampNativeLibraryAvailable;
    }

    native void sendCommandV(byte[] str);
    native void requestAppInventory();
    native void requestReconnect();
    native boolean isNativeGameConnected();
    native boolean isNativePlayerSpawnReady();
    native boolean isNativeLocalPlayerInVehicle();
    native void startVehicleSelfTest(int durationMs, int steering, boolean throttle, boolean brake);
    native void exitVehicleVisualSelfTest();
    native void runWorldCollisionSelfTest();
    native boolean applyNativeMapScanTeleport(float x, float y, float z, int area);
    native String runNativeMapScanProbe(float x, float y, float z, int area);
    native boolean queueNativeMapScanProbe(String zoneId, float x, float y, float z, int area);
    native void setNativeOverlayState(int overlayType);
    native int getNativeOverlayState();
    native boolean isNativeUserPauseActiveNative();
    native int getNativeHudFps();
    native String getNativeRuntimeMonitorSnapshotJson();
    native void setNativeVoipEnabled(boolean enabled);
    native void applyNativeVoiceChatLayout(float posX, float posY, float sizeScale);
    native void resetNativeVoiceChatLayout();
    native void setNativeRadarEnabled(boolean enabled);
    native void setAllowNextNativePauseMenu(boolean allow);
    native void forceEndNativeUserPause();
    native void setNativeActionButtonState(int action, boolean pressed);
    native void setNativeAnalogState(int leftRight, int upDown);
    native void addNativeCameraLookDelta(float deltaX, float deltaY, int screenWidth, int screenHeight);
    native void resetNativeSourceControlState();
    native void setHotReloadWeaponAudioActive(int weaponId, boolean active);
    native void sendSyntheticNativeTouch(int x, int y);
    native void applyNativeMenuSettings(boolean androidKeyboardEnabled, int chatMaxMessages, boolean voiceChatEnabled);
    native void applyNativeChatArea(int left, int top, int width, int height, int screenWidth, int screenHeight);
    native float[] getPlayerPlacementSnapshot();
    native boolean showLocalPickupPreview(int modelId, int pickupType, float x, float y, float z);
    native void selectWeapon(int weaponId);
    native void giveWeaponToLocalPlayer(int weaponId, int ammo);
    native void setLocalPlayerSkin(int skinId);
    native int applyHotReloadTexture(String textureName, String targetGroup, int skinId, String stagedFilePath, String format, int width, int height);
    native int applyHotReloadBitmap(String textureName, String targetGroup, int skinId, byte[] rgbaPixels, int width, int height);
    native int restoreHotReloadTexture(String textureName, String targetGroup, int skinId);
    native void applyNativeRenderSettings(int fpsLimit, int renderDistance, boolean shadowsEnabled, boolean effectsEnabled);

    private CustomKeyboard mKeyboard;
    private DialogManager mDialog;
    private HeightProvider mHeightProvider;
    private PickupCreatorOverlay mPickupCreatorOverlay;

    private View overlayBlurScrim;
    private boolean runtimeLowRamMode;
    private boolean overlayBlurActive;
    private boolean receivedNativeWeaponWheelSnapshot;
    private String lastWeaponWheelJson = DEFAULT_WEAPON_WHEEL_JSON;
    private final LinkedHashMap<Integer, Integer> localWeaponWheelInventory = new LinkedHashMap<>();
    private View hudSettingsPanel;
    private TextView hudSettingsSummary;
    private TextView hudRenderDistanceValue;
    private TextView hudFpsLimitValue;
    private TextView hudQualityLowButton;
    private TextView hudQualityBalancedButton;
    private TextView hudQualityHighButton;
    private TextView hudShadowsToggle;
    private TextView hudEffectsToggle;
    private TextView hudSmartOptimizerToggle;
    private TextView hudSmartOptimizerStatus;
    private TextView hudAndroidKeyboardToggle;
    private TextView hudVoiceChatToggle;
    private TextView hudChatLinesCycle;
    private TextView hudControlAlphaCycle;
    private TextView hudMapToggle;
    private TextView hudHdMapOpenButton;
    private TextView hudChatAreaMapButton;
    private TextView hudChatAreaResetButton;
    private TextView hudCjOutfitCycleButton;
    private TextView hudCjOutfitResetButton;
    private SeekBar hudRenderDistanceSeek;
    private SeekBar hudFpsLimitSeek;
    private View hudStatusPanel;
    private ProgressBar hudStatusHealthBar;
    private ProgressBar hudStatusArmourBar;
    private ProgressBar hudStatusFoodBar;
    private ProgressBar hudStatusThirstBar;
    private ProgressBar hudStatusSleepBar;
    private TextView hudStatusHealthText;
    private TextView hudStatusArmourText;
    private TextView hudStatusFoodText;
    private TextView hudStatusThirstText;
    private TextView hudStatusSleepText;
    private View hudChatPanel;
    private LinearLayout hudChatMessages;
    private ScrollView hudChatScroll;
    private EditText hudChatInput;
    private TextView hudChatSendButton;
    private TextView hudChatCloseButton;
    private View hudMenuButton;
    private View hudInteractionButton;
    private View hudChatButton;
    private View hudChatClickArea;
    private View hudChatAreaHint;
    private View hudSettingsGearButton;
    private View hudChatMapLayer;
    private View hudHdMapLayer;
    private View hudHdMapHotspot;
    private HdMapView hudHdMapView;
    private View hudChatAreaPreview;
    private View hudChatAreaSaveButton;
    private View hudChatAreaCancelButton;
    private View hudSourceAttackButton;
    private View hudSourceAnalog;
    private View hudSourceAnalogKnob;
    private View hudSourceAccelerateButton;
    private View hudSourceBrakeButton;
    private View hudSourceHandbrakeButton;
    private View hudSourceHornButton;
    private View hudSourceSprintButton;
    private View hudSourceJumpButton;
    private View hudSourceVehicleButton;
    private View hudSourceLockButton;
    private View hudSourceCameraButton;
    private View hudWeaponButton;
    private ImageView hudWeaponButtonIcon;
    private TextView hudWeaponButtonAmmo;
    private View hudFpsCounter;
    private TextView hudFpsCounterValue;
    private View hudTopRightPanel;
    private TextView hudTopMoneyValue;
    private TextView hudTopCityName;
    private TextView hudTopClockValue;
    private TextView hudTopCoinValue;

    private static final class HdMapPoi {
        final int viewId;
        final String name;
        final float worldX;
        final float worldY;

        HdMapPoi(int viewId, String name, float worldX, float worldY) {
            this.viewId = viewId;
            this.name = name;
            this.worldX = worldX;
            this.worldY = worldY;
        }
    }

    private static final HdMapPoi[] HUD_HD_MAP_POIS = {
            new HdMapPoi(R.id.hud_hd_map_poi_prefeitura, "Prefeitura", 1481.0f, -1749.0f),
            new HdMapPoi(R.id.hud_hd_map_poi_departamento, "Departamento", 1554.0f, -1675.0f),
            new HdMapPoi(R.id.hud_hd_map_poi_aeroporto, "Aeroporto", 1682.0f, -2333.0f),
            new HdMapPoi(R.id.hud_hd_map_poi_policia, "Policia", 2290.0f, 2424.0f),
            new HdMapPoi(R.id.hud_hd_map_poi_hospital, "Hospital", 1177.0f, -1323.0f),
            new HdMapPoi(R.id.hud_hd_map_poi_mecanica, "Mecanica", 1024.0f, -1025.0f),
            new HdMapPoi(R.id.hud_hd_map_poi_armas, "Loja de armas", 1366.0f, -1279.0f),
            new HdMapPoi(R.id.hud_hd_map_poi_hotel, "Hotel", 2221.0f, -1159.0f)
    };

    private View hudOptionsButton;
    private View hudOptionsPanel;
    private View hudEditLayer;
    private View hudGameplayTouchPassthrough;
    private boolean gameplayLookGestureActive;
    private int gameplayLookPointerId = -1;
    private float gameplayLookLastX;
    private float gameplayLookLastY;
    private final int[] gameplayTouchLocation = new int[2];
    private View hudEditDoneButton;
    private View hudOptionVoip;
    private View hudOptionChat;
    private View hudOptionWeapons;
    private View hudOptionSettings;
    private View hudOptionFps;
    private View hudOptionClose;
    private boolean hudWeaponButtonVisible = true;
    private boolean hudSourceVehicleVisible;
    private boolean hudSourceLockVisible;
    private boolean hudRuntimeShowRequested;
    private long lastSourceHudVehicleRefreshMs;
    private boolean hudSettingsGearButtonVisible;
    private boolean hudFpsCounterVisible;
    private boolean nativeHudFpsUnavailable;
    private boolean nativeChatShowUnavailable;
    private boolean nativeChatHideUnavailable;
    private boolean hudMoneyVisible = true;
    private boolean hudVitalsVisible = true;
    private boolean hudNeedsVisible = true;
    private boolean hudOptionsPanelVisible;
    private boolean hudNativeChatVisible;
    private boolean hudControlsDocked;
    private boolean hudEditMode;
    private boolean hudTopInfoTickerRunning;
    private View hudEditDragTarget;
    private String hudEditDragKey;
    private float hudEditDownRawX;
    private float hudEditDownRawY;
    private float hudEditStartTranslationX;
    private float hudEditStartTranslationY;
    private boolean hudNativeVoipLayoutLoaded;
    private int hudNativeVoipLayoutParentWidth;
    private int hudNativeVoipLayoutParentHeight;
    private float hudNativeVoipRatioX;
    private float hudNativeVoipRatioY;
    private float hudNativeVoipSizeScale = 1.0f;
    private float hudNativeVoipDragStartRatioX;
    private float hudNativeVoipDragStartRatioY;
    private boolean hudMainListenersBound;
    private volatile boolean hudChatPanelVisible;
    private boolean hudChatAreaMappingMode;
    private int hudChatMapAction = HUD_CHAT_MAP_ACTION_NONE;
    private float hudChatMapStartX;
    private float hudChatMapStartY;
    private int hudChatMapStartLeft;
    private int hudChatMapStartTop;
    private int hudChatMapStartWidth;
    private int hudChatMapStartHeight;
    private boolean legacyHudChromeHidden;
    private boolean hudShortcutButtonsVisible;
    private int lastHudHp = Integer.MIN_VALUE;
    private int lastHudArmour = Integer.MIN_VALUE;
    private int lastHudEat = Integer.MIN_VALUE;
    private int lastHudMoney = Integer.MIN_VALUE;
    private int lastHudGunId = Integer.MIN_VALUE;
    private int lastHudAmmo = Integer.MIN_VALUE;
    private int renderedHudGunId = Integer.MIN_VALUE;
    private int renderedHudAmmo = Integer.MIN_VALUE;
    private boolean renderedHudAmmoVisible;
    private final Object hudUpdateLock = new Object();
    private boolean hudUpdateQueued;
    private int pendingHudHp;
    private int pendingHudArmour;
    private int pendingHudEat;
    private int pendingHudMoney;
    private int pendingHudGunId;
    private int pendingHudAmmo;
    private final Object weaponWheelUpdateLock = new Object();
    private boolean weaponWheelUpdateQueued;
    private String pendingWeaponWheelJson = DEFAULT_WEAPON_WHEEL_JSON;
    private FrameLayout weaponWheelOverlay;
    private final LinkedHashMap<Integer, View> weaponWheelNodeViews = new LinkedHashMap<>();
    private final ArrayList<Integer> weaponWheelDisplayOrder = new ArrayList<>();
    private boolean weaponWheelOverlayVisible;
    private int weaponWheelDragWeaponId = -1;
    private float weaponWheelDragStartRawX;
    private float weaponWheelDragStartRawY;
    private boolean weaponWheelDragMoved;
    private int hudRenderDistance = 70;
    private int hudFpsLimit = 120;
    private int hudQuality = HUD_QUALITY_BALANCED;
    private boolean hudShadowsEnabled = false;
    private boolean hudEffectsEnabled = true;
    private boolean smartOptimizerEnabled = false;
    private int smartOptimizerLevel = SMART_OPTIMIZER_NORMAL;
    private int smartOptimizerNormalTicks;
    private long smartOptimizerLastLogMs;
    private long smartOptimizerLastQuarantineMs;
    private boolean smartOptimizerHasRuntimeOverride;
    private int smartOptimizerRestoreRenderDistance;
    private int smartOptimizerRestoreFpsLimit;
    private boolean smartOptimizerRestoreShadowsEnabled;
    private boolean smartOptimizerRestoreEffectsEnabled;
    private SmartOptimizerSnapshot smartOptimizerLastSnapshot;
    private String smartOptimizerLastAction = "Aguardando telemetria.";
    private boolean hudAndroidKeyboardEnabled = true;
    private boolean hudVoiceChatEnabled = true;
    private boolean hudMapEnabled = true;
    private int hudControlAlphaPercent = 72;
    private int hudChatMaxMessages = 5;
    private int cjOutfitIndex = 0;
    private final Handler uiHandler = new Handler(Looper.getMainLooper());
    private final Runnable hudFpsPollRunnable = new Runnable() {
        @Override
        public void run() {
            if (!hudFpsCounterVisible) {
                return;
            }
            int fps = readHudRuntimeFps();
            if (hudFpsCounterValue != null) {
                hudFpsCounterValue.setText(fps > 0 ? String.valueOf(fps) : "--");
            }
            uiHandler.postDelayed(this, 500L);
        }
    };
    private final Runnable hudTopInfoTickerRunnable = new Runnable() {
        @Override
        public void run() {
            if (!hudTopInfoTickerRunning) {
                return;
            }
            updateHudTopInfo();
            uiHandler.postDelayed(this, HUD_TOP_INFO_INTERVAL_MS);
        }
    };
    private final Runnable smartOptimizerRunnable = new Runnable() {
        @Override
        public void run() {
            try {
                runSmartOptimizerTick();
            } finally {
                uiHandler.postDelayed(this, SMART_OPTIMIZER_INTERVAL_MS);
            }
        }
    };
    private int readHudRuntimeFps() {
        int fps = 0;
        if (!nativeHudFpsUnavailable) {
            try {
                fps = getNativeHudFps();
            } catch (UnsatisfiedLinkError error) {
                nativeHudFpsUnavailable = true;
                Log.w(TAG, "Native FPS unavailable; using measured UI FPS.");
            }
        }
        if (fps <= 0) {
            fps = getMeasuredFrameFps();
        }
        int clampedFps = clampInt(fps, 0, 240);
        if (performanceMonitor != null) {
            performanceMonitor.setNativeFps(clampedFps);
        }
        return clampedFps;
    }
    private final SparseIntArray weaponDrawableCache = new SparseIntArray();
    private boolean pendingSystemUiMenuDismiss;
    private boolean pendingBackgroundMenuDismiss;
    private int backgroundMenuDismissStep;
    private boolean backgroundReturnDismissArmed;
    private boolean suppressAutomaticNativePauseResume;
    private boolean skipNextAutomaticLifecycleResumeEvent;
    private boolean nativeUserPauseActive;
    private int nativeMenuStateRecheckGeneration;
    private boolean nativeMenuHudSnapshotActive;
    private final SparseIntArray nativeMenuHudVisibilitySnapshot = new SparseIntArray();
    private long buttonLockCD;
    private int lastSystemUiVisibility = -1;
    private volatile boolean localHostBootstrapRequested;
    private final Runnable clearAutomaticPauseResumeSuppressionRunnable = new Runnable() {
        @Override
        public void run() {
            suppressAutomaticNativePauseResume = false;
        }
    };
    private final Runnable dismissMenuAfterSystemUiRunnable = new Runnable() {
        @Override
        public void run() {
            pendingSystemUiMenuDismiss = false;
            if (!shouldHandleSystemUiMapMenu()) {
                return;
            }
            performNativeMenuTap(NATIVE_MENU_RESUME_X_RATIO, NATIVE_MENU_RESUME_Y_RATIO);
        }
    };
    private final Runnable dismissMenuAfterBackgroundRunnable = new Runnable() {
        @Override
        public void run() {
            Log.i(TAG, "backgroundMenuDismiss step=" + backgroundMenuDismissStep + " pending=" + pendingBackgroundMenuDismiss);
            if (!shouldAutoDismissNativeMenu()) {
                pendingBackgroundMenuDismiss = false;
                backgroundMenuDismissStep = 0;
                return;
            }
            requestNativeGameplayResume();
            performNativeMenuTap(NATIVE_MENU_RESUME_X_RATIO, NATIVE_MENU_RESUME_Y_RATIO);
            backgroundMenuDismissStep += 1;
            if (!pendingBackgroundMenuDismiss
                    || backgroundMenuDismissStep >= NATIVE_MENU_RESUME_AFTER_BACKGROUND_DELAYS_MS.length) {
                pendingBackgroundMenuDismiss = false;
                backgroundMenuDismissStep = 0;
                return;
            }

            long previousDelay = NATIVE_MENU_RESUME_AFTER_BACKGROUND_DELAYS_MS[backgroundMenuDismissStep - 1];
            long nextDelay = NATIVE_MENU_RESUME_AFTER_BACKGROUND_DELAYS_MS[backgroundMenuDismissStep];
            uiHandler.postDelayed(
                    dismissMenuAfterBackgroundRunnable,
                    nextDelay - previousDelay
            );
        }
    };

    //public static SoundPool soundPool = null;
    private AttachEdit mAttachEdit;
    private RadialMenu mRadialMenu;
    private RadialVehicles mRadialVehicles;
    private Radinho mRadinho;

    ConstraintLayout hud_main;
    ConstraintLayout loadingscreen;
    private TextView loadingStatusText;
    private int loadingStatusDotCount;
    private final Runnable loadingStatusTicker = new Runnable() {
        @Override
        public void run() {
            if (loadingscreen == null || loadingStatusText == null || loadingscreen.getVisibility() != View.VISIBLE) {
                return;
            }

            loadingStatusDotCount = (loadingStatusDotCount + 1) % 4;
            StringBuilder status = new StringBuilder("Sincronizando");
            for (int i = 0; i < loadingStatusDotCount; i++) {
                status.append('.');
            }
            loadingStatusText.setText(status.toString());

            if (handler != null) {
                handler.postDelayed(this, 420);
            }
        }
    };

    private int iShowHud;
    private boolean iShowLogo;
    private boolean TeclasAbertas;
    //API
    private View connectScreenView;
    private View LoginScreenView;
    private View RegisterScreenView;
    private Handler handler;
    private Runnable apiCheckerRunnable;
    private SkinHotReloadServer skinHotReloadServer;
    private final Object hotReloadApplyLock = new Object();
    private final Object weaponAudioLock = new Object();
    private SoundPool weaponAudioSoundPool;
    private final SparseIntArray weaponAudioSoundIds = new SparseIntArray();
    private final SparseIntArray weaponAudioLoaded = new SparseIntArray();
    private final SparseIntArray weaponAudioSampleToWeapon = new SparseIntArray();
    private final long[] weaponAudioLastPlayMs = new long[47];
    private long runtimeCommandFileLastModifiedMs;
    private String runtimeCommandFileLastPayload = "";
    private final Map<String, Integer> mapScanTeleportRetryCounts = new LinkedHashMap<>();
    private final Runnable persistentHotReloadReplayRunnable = new Runnable() {
        @Override
        public void run() {
            applyPersistedHotReloadMods();
        }
    };
    private final Runnable runtimeCommandFileWatcherRunnable = new Runnable() {
        @Override
        public void run() {
            pollRuntimeCommandFile();
            if (handler != null) {
                handler.postDelayed(this, RUNTIME_COMMAND_FILE_POLL_MS);
            }
        }
    };

    @Override
    public void onCreate(Bundle savedInstanceState) {
        Log.i(TAG, "**** onCreate");

        ConfigValidator.validateConfigFiles(this);
        ensureSampNativeLibraryLoaded();

        super.onCreate(savedInstanceState);

        handler = new Handler(Looper.getMainLooper());
        runtimeLowRamMode = detectLowRamMode();
        loadSmartOptimizerSettings();
        performanceMonitor = new XyronPerformanceMonitor(this, this::readHudRuntimeFps);
        performanceMonitor.start();
        runtimeMonitor = new XyronRuntimeMonitor(this, this::getNativeRuntimeMonitorSnapshotJson, () -> performanceMonitor != null
                ? performanceMonitor.snapshot()
                : XyronPerformanceMonitor.Snapshot.empty());
        runtimeMonitor.start();
        startRuntimeCommandFileWatcher();
        mHeightProvider = new HeightProvider(this);

        mDialog = new DialogManager(this);
        mAttachEdit = new AttachEdit(this);
        mRadialMenu = new RadialMenu(this);
        mRadialVehicles = new RadialVehicles();
        mRadinho = new Radinho(this);

        hud_main = (ConstraintLayout) getLayoutInflater().inflate(R.layout.hud, null);
        addContentView(hud_main, new ConstraintLayout.LayoutParams(-1, -1));
        hud_main.setVisibility(View.GONE);
        initializeRuntimeOverlays();

        loadingscreen = (ConstraintLayout) getLayoutInflater().inflate(R.layout.loading_screen, null);
        loadingStatusText = loadingscreen.findViewById(R.id.loading_status_text);
        addContentView(loadingscreen, new ConstraintLayout.LayoutParams(-1, -1));
        showInitialLoadingScreen();

        mKeyboard = new CustomKeyboard(this);
        mPickupCreatorOverlay = new PickupCreatorOverlay(this, new PickupCreatorOverlay.Listener() {
            @Override
            public float[] captureCurrentPlacement() {
                return getPlayerPlacementSnapshot();
            }

            @Override
            public boolean previewPickup(PickupStudioManager.PickupDefinition pickup) {
                return pickup != null && showLocalPickupPreview(
                        pickup.modelId,
                        pickup.pickupType,
                        pickup.x,
                        pickup.y,
                        pickup.z
                );
            }
        });

        instance = this;

        applyDirectConnectExtras(getIntent());
        ensureLocalHostRuntimeBeforeNativeConnect();

        try {
            initializeSAMP();
            setAllowNextNativePauseMenu(false);
        } catch (UnsatisfiedLinkError e5) {
            Log.e(TAG, e5.getMessage());
        }
        startSkinHotReloadServer();
        schedulePersistedHotReloadReplay();
        startSmartOptimizer();
        scheduleRuntimeCommandExtra(getIntent(), 3500L);
        hideSystemUI();
        installSystemUiWatcher();

    }

    private void applyDirectConnectExtras(Intent intent) {
        if (intent == null) {
            return;
        }

        String receivedHost = sanitizeDirectConnectHost(intent.getStringExtra(EXTRA_SERVER_IP));
        int receivedPort = intent.getIntExtra(EXTRA_SERVER_PORT, 0);
        String receivedNickname = sanitizeDirectConnectNickname(intent.getStringExtra(EXTRA_NICKNAME));

        if (receivedHost.length() > 0 && receivedPort > 0 && receivedPort <= 65535) {
            ServerConfigManager.ServerOption option = ServerConfigManager.addOrUpdateServer(
                    this,
                    "Direct Connect",
                    receivedHost,
                    receivedPort,
                    true
            );

            if (option == null || !option.isValid()) {
                java.util.List<ServerConfigManager.ServerOption> servers = ServerConfigManager.getAvailableServers(this);
                if (!servers.isEmpty()) {
                    ServerConfigManager.removeServer(this, servers.get(servers.size() - 1));
                    option = ServerConfigManager.addOrUpdateServer(
                            this,
                            "Direct Connect",
                            receivedHost,
                            receivedPort,
                            true
                    );
                }
            }

            if (option == null || !option.isValid()) {
                option = new ServerConfigManager.ServerOption("Direct Connect", receivedHost, receivedPort, true);
            }

            boolean saved = ServerConfigManager.saveSelectedServer(this, option);
            Log.i(TAG, "Direct connect target received: " + receivedHost + ":" + receivedPort + " saved=" + saved);
        }

        if (receivedNickname.length() > 0) {
            saveDirectConnectNickname(receivedNickname);
        }

        applyDirectConnectGameSettings(intent);
    }

    private String sanitizeDirectConnectHost(String host) {
        return host == null ? "" : host.trim();
    }

    private String sanitizeDirectConnectNickname(String nickname) {
        if (nickname == null) {
            return "";
        }

        StringBuilder builder = new StringBuilder();
        String trimmed = nickname.trim();
        for (int i = 0; i < trimmed.length() && builder.length() < 20; i++) {
            char value = trimmed.charAt(i);
            boolean accepted = (value >= 'A' && value <= 'Z')
                    || (value >= 'a' && value <= 'z')
                    || (value >= '0' && value <= '9')
                    || value == '_';
            if (accepted) {
                builder.append(value);
            }
        }
        return builder.length() >= 3 ? builder.toString() : "Player123";
    }

    private void saveDirectConnectNickname(String nickname) {
        File settingsFile = new File(getExternalFilesDir(null), "SAMP/settings.ini");
        File parent = settingsFile.getParentFile();
        if (parent != null && !parent.exists() && !parent.mkdirs()) {
            Log.w(TAG, "Could not create SAMP settings directory for direct nickname.");
            return;
        }

        try {
            if (!settingsFile.exists() && !settingsFile.createNewFile()) {
                Log.w(TAG, "Could not create settings.ini for direct nickname.");
                return;
            }

            Wini wini = new Wini(settingsFile);
            wini.put("client", "name", nickname);
            wini.store();
            Log.i(TAG, "Direct connect nickname applied: " + nickname);
        } catch (IOException e) {
            Log.e(TAG, "Failed to persist direct connect nickname.", e);
        }
    }

    private void applyDirectConnectGameSettings(Intent intent) {
        if (intent == null) {
            return;
        }

        boolean hasChat = intent.hasExtra(EXTRA_CHAT_MAX_MESSAGES);
        boolean hasKeyboard = intent.hasExtra(EXTRA_ANDROID_KEYBOARD);
        boolean hasVoice = intent.hasExtra(EXTRA_VOICE_CHAT_ENABLE);
        if (!hasChat && !hasKeyboard && !hasVoice) {
            return;
        }

        File settingsFile = new File(getExternalFilesDir(null), "SAMP/settings.ini");
        File parent = settingsFile.getParentFile();
        if (parent != null && !parent.exists() && !parent.mkdirs()) {
            Log.w(TAG, "Could not create SAMP settings directory for launcher settings.");
            return;
        }

        try {
            if (!settingsFile.exists() && !settingsFile.createNewFile()) {
                Log.w(TAG, "Could not create settings.ini for launcher settings.");
                return;
            }

            Wini wini = new Wini(settingsFile);
            if (hasChat) {
                int chatMaxMessages = intent.getIntExtra(EXTRA_CHAT_MAX_MESSAGES, 5);
                if (chatMaxMessages < 5) {
                    chatMaxMessages = 5;
                }
                wini.put("gui", "ChatMaxMessages", chatMaxMessages);
            }

            if (hasKeyboard) {
                boolean keyboardEnabled = intent.getBooleanExtra(EXTRA_ANDROID_KEYBOARD, false);
                wini.put("gui", "androidkeyboard23123", keyboardEnabled);
                wini.put("gui", "androidkeyboard", keyboardEnabled ? 1 : 0);
                new SharedPreferenceCore().setBoolean(
                        getApplicationContext(),
                        "ANDROID_KEYBOARD",
                        keyboardEnabled
                );
            }

            if (hasVoice) {
                boolean voiceEnabled = intent.getBooleanExtra(EXTRA_VOICE_CHAT_ENABLE, true);
                wini.put("gui", "VoiceChatEnable", voiceEnabled);
                new SharedPreferenceCore().setBoolean(
                        getApplicationContext(),
                        "VOICE_CHAT_ENABLE",
                        voiceEnabled
                );
            }

            wini.store();
            Log.i(TAG, "Launcher game settings applied.");
        } catch (IOException e) {
            Log.e(TAG, "Failed to persist launcher game settings.", e);
        }
    }

    private void ensureLocalHostRuntimeBeforeNativeConnect() {
        if ("game".equals(BuildConfig.XYRON_APK_ROLE)) {
            return;
        }

        ServerConfigManager.ServerOption option = ServerConfigManager.getSelectedServer(this);
        if (option == null || !option.isValid()) {
            return;
        }
        boolean localLoopback = "127.0.0.1".equals(option.host) || "localhost".equalsIgnoreCase(option.host);
        if (!localLoopback || option.port != 7777) {
            return;
        }
        if (HostShellEngine.isHostReady(this)
                || HostShellEngine.isHostRunning(this)
                || HostShellEngine.isHostStarting(this)) {
            return;
        }
        if (localHostBootstrapRequested) {
            return;
        }
        localHostBootstrapRequested = true;

        Context appContext = getApplicationContext();
        new Thread(() -> {
            HostShellEngine.CommandResult result = HostShellEngine.bootHost(appContext);
            if (result == null || !result.success) {
                runOnUiThread(() -> Toast.makeText(
                        SAMP.this,
                        extractLocalHostRuntimeMessage(result),
                        Toast.LENGTH_LONG
                ).show());
            }
        }, "xyron-local-host-bootstrap").start();
    }

    private String extractLocalHostRuntimeMessage(HostShellEngine.CommandResult result) {
        if (result == null || result.output == null || result.output.trim().isEmpty()) {
            return "Nao foi possivel ligar o host local antes do jogo.";
        }
        String[] lines = result.output.trim().split("\\r?\\n");
        for (String rawLine : lines) {
            String line = rawLine == null ? "" : rawLine.trim();
            if (line.isEmpty() || "Fluxo rapido do host".equalsIgnoreCase(line)) {
                continue;
            }
            String normalized = line.toLowerCase(Locale.US);
            if (normalized.contains("falha")
                    || normalized.contains("erro")
                    || normalized.contains("processo saiu")
                    || normalized.contains("nao foi possivel")) {
                return line;
            }
        }
        return "Host local preparado para o jogo.";
    }
     public void hideSystemUI() {
        View decorView = getWindow() != null ? getWindow().getDecorView() : null;
        if (decorView == null) {
            return;
        }
        int currentVisibility = decorView.getSystemUiVisibility();
        if (lastSystemUiVisibility == SYSTEM_UI_IMMERSIVE_FLAGS
                && currentVisibility == SYSTEM_UI_IMMERSIVE_FLAGS) {
            return;
        }
        decorView.setSystemUiVisibility(SYSTEM_UI_IMMERSIVE_FLAGS);
        lastSystemUiVisibility = SYSTEM_UI_IMMERSIVE_FLAGS;
    }

    public void showSystemUI() {
        View decorView = getWindow() != null ? getWindow().getDecorView() : null;
        if (decorView == null) {
            return;
        }
        int visibleFlags = View.SYSTEM_UI_FLAG_LAYOUT_STABLE;
        if (lastSystemUiVisibility == visibleFlags && decorView.getSystemUiVisibility() == visibleFlags) {
            return;
        }
        decorView.setSystemUiVisibility(visibleFlags);
        lastSystemUiVisibility = visibleFlags;
    }

    private void installSystemUiWatcher() {
        View decorView = getWindow() != null ? getWindow().getDecorView() : null;
        if (decorView == null) {
            return;
        }

        decorView.setOnSystemUiVisibilityChangeListener(visibility -> {
            boolean fullscreenVisible = (visibility & View.SYSTEM_UI_FLAG_FULLSCREEN) != 0;
            if (!fullscreenVisible) {
                pendingSystemUiMenuDismiss = shouldHandleSystemUiMapMenu();
                return;
            }
            hideSystemUI();
            if (!pendingSystemUiMenuDismiss) {
                return;
            }
            uiHandler.removeCallbacks(dismissMenuAfterSystemUiRunnable);
            uiHandler.postDelayed(
                    dismissMenuAfterSystemUiRunnable,
                    NATIVE_MENU_RESUME_AFTER_SYSTEM_UI_DELAY_MS
            );
        });
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        boolean ignoredTransientLoss = !hasFocus && shouldIgnoreTransientFocusLoss();
        if (ignoredTransientLoss) {
            hideSystemUI();
            return;
        }
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            hideSystemUI();
            if (suppressAutomaticNativePauseResume) {
                scheduleAutomaticPauseResumeSuppressionRelease();
            }
            if (pendingBackgroundMenuDismiss) {
                scheduleBackgroundMenuDismissSequence();
            }
        }
    }
    public native void togglePlayer(int toggle);

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);
        applyDirectConnectExtras(intent);
        scheduleRuntimeCommandExtra(intent, 350L);
    }


    @Override
    public void onStart() {
        Log.i(TAG, "**** onStart");
        super.onStart();
    }

    @Override
    public void onRestart() {
        Log.i(TAG, "**** onRestart");
        backgroundReturnDismissArmed = true;
        pendingBackgroundMenuDismiss = true;
        super.onRestart();
    }

    @Override
    public void onResume() {
        Log.i(TAG, "**** onResume");
        boolean suppressAutomaticResume = skipNextAutomaticLifecycleResumeEvent;
        boolean originalResumeEventDone = ResumeEventDone;
        if (suppressAutomaticResume) {
            ResumeEventDone = false;
        }
        try {
            super.onResume();
        } finally {
            ResumeEventDone = originalResumeEventDone;
            skipNextAutomaticLifecycleResumeEvent = false;
        }
        mHeightProvider.init(view);
        if (suppressAutomaticNativePauseResume) {
            scheduleAutomaticPauseResumeSuppressionRelease();
        }
        if (backgroundReturnDismissArmed) {
            backgroundReturnDismissArmed = false;
            pendingBackgroundMenuDismiss = true;
            requestNativeGameplayResume();
            scheduleBackgroundMenuDismissSequence();
        } else if (pendingBackgroundMenuDismiss) {
            scheduleBackgroundMenuDismissSequence();
        }
        if (performanceMonitor != null) {
            performanceMonitor.start();
        }
        if (runtimeMonitor != null) {
            runtimeMonitor.start();
        }
        if (iShowHud == 1 && hud_main != null) {
            applyClassicHudVisibilityFromQuickSettings();
        }
    }

    native void onClickButton(int action);
    //onClickButton(2);

    @Override
    public boolean dispatchTouchEvent(MotionEvent event) {
        handleGameplayCameraDispatchTouch(event);
        return super.dispatchTouchEvent(event);
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (event != null) {
            int keyCode = event.getKeyCode();
            if (keyCode == KeyEvent.KEYCODE_MENU) {
                hideSystemUI();
                return true;
            }
            if (keyCode == KeyEvent.KEYCODE_BACK || keyCode == KeyEvent.KEYCODE_ESCAPE) {
                if (event.getAction() == KeyEvent.ACTION_DOWN) {
                    if (!handleRuntimeBack()) {
                        onEventBackPressed();
                        hideSystemUI();
                    }
                } else if (event.getAction() == KeyEvent.ACTION_UP) {
                    hideSystemUI();
                }
                return true;
            }
        }
        return super.dispatchKeyEvent(event);
    }

    @Override
    public void onBackPressed() {
        if (handleRuntimeBack()) {
            return;
        }
        onEventBackPressed();
        hideSystemUI();
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_MENU) {
            hideSystemUI();
            return true;
        }
        if(keyCode == KeyEvent.KEYCODE_BACK)
        {
            if (handleRuntimeBack()) {
                return true;
            }
            onEventBackPressed();
            hideSystemUI();
            return true;
        }
        return super.onKeyDown(keyCode, event);
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_BACK) {
            hideSystemUI();
            return true;
        }
        return super.onKeyUp(keyCode, event);
    }

    @Override
    public void onPause() {
        Log.i(TAG, "**** onPause");
        resetNativeSourceControls();
        stopHudTopInfoTicker();
        hideHudHdMapOverlay();
        boolean suppressAutomaticPause = shouldSuppressAutomaticPauseMenu();
        suppressAutomaticNativePauseResume = suppressAutomaticPause;
        skipNextAutomaticLifecycleResumeEvent = suppressAutomaticPause;
        pendingBackgroundMenuDismiss = shouldAutoDismissNativeMenu();
        backgroundMenuDismissStep = 0;
        uiHandler.removeCallbacks(clearAutomaticPauseResumeSuppressionRunnable);
        uiHandler.removeCallbacks(dismissMenuAfterBackgroundRunnable);
        if (performanceMonitor != null) {
            performanceMonitor.stop();
            performanceMonitor.shutdown();
        }
        if (runtimeMonitor != null) {
            runtimeMonitor.stop();
        }

        boolean originalResumeEventDone = ResumeEventDone;
        if (suppressAutomaticPause) {
            ResumeEventDone = false;
        }
        try {
            super.onPause();
        } finally {
            ResumeEventDone = originalResumeEventDone;
        }
    }

    @Override
    public void onStop() {
        Log.i(TAG, "**** onStop");
        super.onStop();
    }

    @Override
    public void onDestroy() {
        Log.i(TAG, "**** onDestroy");
        resetNativeSourceControls();
        uiHandler.removeCallbacks(clearAutomaticPauseResumeSuppressionRunnable);
        uiHandler.removeCallbacks(dismissMenuAfterBackgroundRunnable);
        uiHandler.removeCallbacks(dismissMenuAfterSystemUiRunnable);
        uiHandler.removeCallbacks(persistentHotReloadReplayRunnable);
        uiHandler.removeCallbacks(smartOptimizerRunnable);
        stopHudTopInfoTicker();
        if (handler != null) {
            handler.removeCallbacks(runtimeCommandFileWatcherRunnable);
        }
        if (performanceMonitor != null) {
            performanceMonitor.stop();
        }
        if (runtimeMonitor != null) {
            runtimeMonitor.stop();
            runtimeMonitor.shutdown();
        }
        stopSkinHotReloadServer();
        releaseWeaponAudioSoundPool();
        destroyRuntimeOverlays();
        super.onDestroy();
    }

    private void startRuntimeCommandFileWatcher() {
        if (handler == null) {
            return;
        }
        handler.removeCallbacks(runtimeCommandFileWatcherRunnable);
        handler.postDelayed(runtimeCommandFileWatcherRunnable, RUNTIME_COMMAND_FILE_POLL_MS);
    }

    private void pollRuntimeCommandFile() {
        File root = getExternalFilesDir(null);
        if (root == null) {
            return;
        }
        File commandFile = new File(root, "SAMP/xyron_monitor/runtime_command.txt");
        if (!commandFile.isFile() || !commandFile.canRead()) {
            return;
        }
        long modified = commandFile.lastModified();
        if (modified <= 0L) {
            return;
        }
        String payload = readRuntimeCommandFile(commandFile);
        if (modified == runtimeCommandFileLastModifiedMs && payload.equals(runtimeCommandFileLastPayload)) {
            return;
        }
        runtimeCommandFileLastModifiedMs = modified;
        if (!commandFile.delete()) {
            Log.d(TAG, "Runtime command file consumed but not deleted: " + commandFile.getAbsolutePath());
        }
        if (payload.isEmpty()) {
            return;
        }
        runtimeCommandFileLastPayload = payload;
        Log.i(TAG, "Runtime command from file: " + payload);
        scheduleRuntimeCommandWhenReady(payload, 0L, 0);
    }

    private String readRuntimeCommandFile(File commandFile) {
        try (BufferedReader reader = new BufferedReader(new FileReader(commandFile))) {
            String line = reader.readLine();
            return line == null ? "" : line.trim();
        } catch (IOException error) {
            Log.w(TAG, "Could not read runtime command file.", error);
            return "";
        }
    }

    private void startSkinHotReloadServer() {
        if (skinHotReloadServer == null) {
            skinHotReloadServer = new SkinHotReloadServer();
        }
        skinHotReloadServer.start(this, this::handleSkinHotReloadRequest);
    }

    private void stopSkinHotReloadServer() {
        if (skinHotReloadServer != null) {
            skinHotReloadServer.stop();
        }
    }

    private void schedulePersistedHotReloadReplay() {
        uiHandler.removeCallbacks(persistentHotReloadReplayRunnable);
        for (long delay : HOT_RELOAD_REPLAY_DELAYS_MS) {
            uiHandler.postDelayed(persistentHotReloadReplayRunnable, delay);
        }
    }

    private void applyPersistedHotReloadMods() {
        List<SkinHotReloadServer.Request> requests = SkinHotReloadServer.loadActiveRequests(this);
        if (requests.isEmpty()) {
            return;
        }

        for (SkinHotReloadServer.Request request : requests) {
            SkinHotReloadServer.Result result = applySkinHotReloadOnUiThread(request, false, false);
            Log.i(TAG, "Persistent hot reload replay command=" + request.command + " target=" + request.textureName + " ok=" + result.ok + " native=" + result.nativeTextureApplied);
        }
    }

    private void scheduleHotReloadRuntimeRestart() {
        uiHandler.postDelayed(() -> {
            try {
                recreate();
            } catch (RuntimeException error) {
                Log.e(TAG, "Falha reiniciando runtime apos restore.", error);
            }
        }, 900L);
    }

    private static final class SmartOptimizerSnapshot {
        final long availRamMb;
        final long totalRamMb;
        final long thresholdRamMb;
        final boolean systemLowMemory;
        final long javaUsedMb;
        final long javaMaxMb;
        final float javaPressure;
        final int fps;
        final float uiSlowFramePercent;
        final int uiSlowFrameCount;
        final int uiFrozenFrameCount;
        final long uiAvgFrameMs;
        final long uiMaxFrameMs;

        SmartOptimizerSnapshot(
                long availRamMb,
                long totalRamMb,
                long thresholdRamMb,
                boolean systemLowMemory,
                long javaUsedMb,
                long javaMaxMb,
                float javaPressure,
                int fps,
                float uiSlowFramePercent,
                int uiSlowFrameCount,
                int uiFrozenFrameCount,
                long uiAvgFrameMs,
                long uiMaxFrameMs
        ) {
            this.availRamMb = availRamMb;
            this.totalRamMb = totalRamMb;
            this.thresholdRamMb = thresholdRamMb;
            this.systemLowMemory = systemLowMemory;
            this.javaUsedMb = javaUsedMb;
            this.javaMaxMb = javaMaxMb;
            this.javaPressure = javaPressure;
            this.fps = fps;
            this.uiSlowFramePercent = uiSlowFramePercent;
            this.uiSlowFrameCount = uiSlowFrameCount;
            this.uiFrozenFrameCount = uiFrozenFrameCount;
            this.uiAvgFrameMs = uiAvgFrameMs;
            this.uiMaxFrameMs = uiMaxFrameMs;
        }
    }

    private void loadSmartOptimizerSettings() {
        smartOptimizerEnabled = false;
        smartOptimizerLevel = SMART_OPTIMIZER_NORMAL;
        smartOptimizerNormalTicks = 0;
        smartOptimizerHasRuntimeOverride = false;
        smartOptimizerLastAction = "Controle automatico removido.";
        saveSmartOptimizerSettings();
    }

    private void saveSmartOptimizerSettings() {
        getSharedPreferences(SMART_OPTIMIZER_PREFS, MODE_PRIVATE)
                .edit()
                .putBoolean("enabled", smartOptimizerEnabled)
                .apply();
    }

    private void startSmartOptimizer() {
        uiHandler.removeCallbacks(smartOptimizerRunnable);
    }

    private void setSmartOptimizerEnabled(boolean enabled) {
        smartOptimizerEnabled = false;
        smartOptimizerLevel = SMART_OPTIMIZER_NORMAL;
        smartOptimizerNormalTicks = 0;
        smartOptimizerHasRuntimeOverride = false;
        smartOptimizerLastAction = "Controle automatico removido.";
        saveSmartOptimizerSettings();
        syncSmartOptimizerUi();
    }

    private SmartOptimizerSnapshot collectSmartOptimizerSnapshot() {
        ActivityManager activityManager = (ActivityManager) getSystemService(ACTIVITY_SERVICE);
        long availMb = -1L;
        long totalMb = -1L;
        long thresholdMb = -1L;
        boolean lowMemory = false;
        if (activityManager != null) {
            ActivityManager.MemoryInfo memoryInfo = new ActivityManager.MemoryInfo();
            activityManager.getMemoryInfo(memoryInfo);
            availMb = memoryInfo.availMem / (1024L * 1024L);
            totalMb = memoryInfo.totalMem / (1024L * 1024L);
            thresholdMb = memoryInfo.threshold / (1024L * 1024L);
            lowMemory = memoryInfo.lowMemory;
        }

        Runtime runtime = Runtime.getRuntime();
        long javaMaxMb = runtime.maxMemory() / (1024L * 1024L);
        long javaUsedMb = (runtime.totalMemory() - runtime.freeMemory()) / (1024L * 1024L);
        float javaPressure = javaMaxMb > 0L ? (float) javaUsedMb / (float) javaMaxMb : 0.0f;
        int fps = readHudRuntimeFps();
        XyronPerformanceMonitor.Snapshot perfSnapshot = performanceMonitor != null
                ? performanceMonitor.snapshot()
                : XyronPerformanceMonitor.Snapshot.empty();

        return new SmartOptimizerSnapshot(
                availMb,
                totalMb,
                thresholdMb,
                lowMemory,
                javaUsedMb,
                javaMaxMb,
                javaPressure,
                fps,
                perfSnapshot.slowFramePercent(),
                perfSnapshot.slowFrameCount,
                perfSnapshot.frozenFrameCount,
                perfSnapshot.avgFrameMs,
                perfSnapshot.maxFrameMs
        );
    }

    private int computeSmartOptimizerLevel(SmartOptimizerSnapshot snapshot) {
        if (snapshot == null) {
            return SMART_OPTIMIZER_NORMAL;
        }

        boolean criticalRam = snapshot.systemLowMemory
                || (snapshot.availRamMb >= 0L && snapshot.availRamMb <= SMART_OPTIMIZER_CRITICAL_FREE_RAM_MB)
                || snapshot.javaPressure >= 0.92f;
        boolean criticalFps = snapshot.fps > 0 && snapshot.fps <= SMART_OPTIMIZER_CRITICAL_FPS;
        boolean criticalJank = snapshot.uiFrozenFrameCount > 0
                || snapshot.uiMaxFrameMs >= 700L
                || snapshot.uiSlowFramePercent >= 45.0f;
        if (criticalRam || criticalFps || criticalJank) {
            return SMART_OPTIMIZER_CRITICAL;
        }

        boolean warnRam = (snapshot.availRamMb >= 0L && snapshot.availRamMb <= SMART_OPTIMIZER_WARN_FREE_RAM_MB)
                || snapshot.javaPressure >= 0.84f;
        boolean warnFps = snapshot.fps > 0 && snapshot.fps <= SMART_OPTIMIZER_WARN_FPS;
        boolean warnJank = snapshot.uiMaxFrameMs >= 120L || snapshot.uiSlowFramePercent >= 25.0f;
        if (warnRam || warnFps || warnJank) {
            return SMART_OPTIMIZER_WARN;
        }
        return SMART_OPTIMIZER_NORMAL;
    }

    private void runSmartOptimizerTick() {
        smartOptimizerEnabled = false;
        smartOptimizerLevel = SMART_OPTIMIZER_NORMAL;
        smartOptimizerNormalTicks = 0;
        smartOptimizerHasRuntimeOverride = false;
        smartOptimizerLastAction = "Controle automatico removido.";
        syncSmartOptimizerUi();
    }

    private void rememberSmartOptimizerBaseline() {
        if (smartOptimizerHasRuntimeOverride) {
            return;
        }
        smartOptimizerRestoreRenderDistance = hudRenderDistance;
        smartOptimizerRestoreFpsLimit = hudFpsLimit;
        smartOptimizerRestoreShadowsEnabled = hudShadowsEnabled;
        smartOptimizerRestoreEffectsEnabled = hudEffectsEnabled;
        smartOptimizerHasRuntimeOverride = true;
    }

    private String applySmartOptimizerProfile(int level) {
        rememberSmartOptimizerBaseline();
        int targetDistance = level >= SMART_OPTIMIZER_CRITICAL ? 35 : 50;
        int targetFps = HUD_FPS_LIMIT_MAX;
        boolean changed = false;

        if (hudRenderDistance > targetDistance) {
            hudRenderDistance = targetDistance;
            changed = true;
        }
        if (hudFpsLimit > targetFps) {
            hudFpsLimit = targetFps;
            changed = true;
        }
        if (hudShadowsEnabled) {
            hudShadowsEnabled = false;
            changed = true;
        }
        if (hudEffectsEnabled) {
            hudEffectsEnabled = false;
            changed = true;
        }

        if (hudQuality != HUD_QUALITY_FPS) {
            hudQuality = HUD_QUALITY_FPS;
            changed = true;
        }
        if (changed) {
            syncHudRenderSettingsUi();
            commitHudRenderSettingsLive();
            return level >= SMART_OPTIMIZER_CRITICAL
                    ? "Perfil critico aplicado: renderizacao, sombras e efeitos reduzidos."
                    : "Perfil preventivo aplicado: renderizacao e efeitos reduzidos.";
        }
        return level >= SMART_OPTIMIZER_CRITICAL
                ? "Perfil critico ja estava ativo."
                : "Perfil preventivo ja estava ativo.";
    }

    private String restoreSmartOptimizerBaseline(String reason) {
        if (!smartOptimizerHasRuntimeOverride) {
            return reason;
        }

        hudRenderDistance = clampInt(
                smartOptimizerRestoreRenderDistance,
                HUD_RENDER_DISTANCE_MIN,
                HUD_RENDER_DISTANCE_MAX
        );
        hudFpsLimit = clampInt(
                smartOptimizerRestoreFpsLimit,
                HUD_FPS_LIMIT_MIN,
                HUD_FPS_LIMIT_MAX
        );
        hudShadowsEnabled = smartOptimizerRestoreShadowsEnabled;
        hudEffectsEnabled = smartOptimizerRestoreEffectsEnabled;
        smartOptimizerHasRuntimeOverride = false;
        syncHudRenderSettingsUi();
        commitHudRenderSettingsLive();
        return reason + " Perfil anterior restaurado.";
    }

    private int quarantineRiskyHotReloadTexture(SmartOptimizerSnapshot snapshot) {
        long now = SystemClock.uptimeMillis();
        if (now - smartOptimizerLastQuarantineMs < SMART_OPTIMIZER_QUARANTINE_INTERVAL_MS) {
            return 0;
        }

        List<SkinHotReloadServer.Request> requests = SkinHotReloadServer.loadActiveRequests(this);
        SkinHotReloadServer.Request selected = null;
        long selectedBytes = -1L;
        for (SkinHotReloadServer.Request request : requests) {
            if (request == null || request.isWeaponAudio() || request.stagedFile == null || !request.stagedFile.isFile()) {
                continue;
            }
            long length = request.stagedFile.length();
            if (length > selectedBytes) {
                selected = request;
                selectedBytes = length;
            }
        }
        if (selected == null) {
            return 0;
        }

        int nativeResult = 0;
        try {
            nativeResult = restoreHotReloadTexture(selected.textureName, selected.targetGroup, selected.skinId);
        } catch (UnsatisfiedLinkError error) {
            Log.w(TAG, "Native restore indisponivel para quarentena smart.", error);
        } catch (RuntimeException error) {
            Log.w(TAG, "Falha restaurando textura pela quarentena smart.", error);
        }

        int removed = 0;
        try {
            removed = SkinHotReloadServer.removeActiveMod(this, selected.targetGroup, selected.textureName);
        } catch (IOException error) {
            Log.w(TAG, "Falha removendo manifest de textura pela quarentena smart.", error);
        }

        if (removed > 0 || nativeResult >= 2) {
            smartOptimizerLastQuarantineMs = now;
            String action = "Quarentena: " + selected.textureName + " (" + selectedBytes + " bytes).";
            recordSmartOptimizerEvent("texture_quarantine", snapshot, action);
            Toast.makeText(this, "Uma textura pesada foi restaurada.", Toast.LENGTH_SHORT).show();
            return 1;
        }
        return 0;
    }

    private JSONObject buildSmartOptimizerPayload(String event, SmartOptimizerSnapshot snapshot, String action) {
        JSONObject payload = new JSONObject();
        try {
            payload.put("event", event);
            payload.put("enabled", smartOptimizerEnabled);
            payload.put("level", smartOptimizerLevel);
            payload.put("levelLabel", smartOptimizerLevelLabel(smartOptimizerLevel));
            payload.put("action", action == null ? "" : action);
            payload.put("device", Build.MANUFACTURER + " " + Build.MODEL);
            payload.put("packageName", BuildConfig.APPLICATION_ID);
            payload.put("renderDistance", hudRenderDistance);
            payload.put("fpsLimit", hudFpsLimit);
            payload.put("shadows", hudShadowsEnabled);
            payload.put("effects", hudEffectsEnabled);
            if (snapshot != null) {
                payload.put("availRamMb", snapshot.availRamMb);
                payload.put("totalRamMb", snapshot.totalRamMb);
                payload.put("thresholdRamMb", snapshot.thresholdRamMb);
                payload.put("systemLowMemory", snapshot.systemLowMemory);
                payload.put("javaUsedMb", snapshot.javaUsedMb);
                payload.put("javaMaxMb", snapshot.javaMaxMb);
                payload.put("javaPressure", snapshot.javaPressure);
                payload.put("fps", snapshot.fps);
                payload.put("uiSlowFramePercent", snapshot.uiSlowFramePercent);
                payload.put("uiSlowFrameCount", snapshot.uiSlowFrameCount);
                payload.put("uiFrozenFrameCount", snapshot.uiFrozenFrameCount);
                payload.put("uiAvgFrameMs", snapshot.uiAvgFrameMs);
                payload.put("uiMaxFrameMs", snapshot.uiMaxFrameMs);
            }
        } catch (JSONException error) {
            Log.w(TAG, "Falha montando log do Smart Optimizer.", error);
        }
        return payload;
    }

    private void recordSmartOptimizerEvent(String event, SmartOptimizerSnapshot snapshot, String action) {
        JSONObject payload = buildSmartOptimizerPayload(event, snapshot, action);
        Log.i(TAG, "SmartOptimizer " + payload.toString());
        writeSmartOptimizerLocalLog(payload);
        postSmartOptimizerLog(payload);
    }

    private void writeSmartOptimizerLocalLog(JSONObject payload) {
        File root = getExternalFilesDir(null);
        if (root == null) {
            return;
        }
        File logFile = new File(root, "SAMP/smart_optimizer.jsonl");
        File parent = logFile.getParentFile();
        if (parent != null && !parent.exists() && !parent.mkdirs()) {
            return;
        }
        try (FileOutputStream output = new FileOutputStream(logFile, true)) {
            output.write((payload.toString() + "\n").getBytes(StandardCharsets.UTF_8));
        } catch (IOException error) {
            Log.w(TAG, "Falha gravando log local do Smart Optimizer.", error);
        }
    }

    private void postSmartOptimizerLog(JSONObject payload) {
        final String body = payload.toString();
        new Thread(() -> {
            HttpURLConnection connection = null;
            try {
                URL url = new URL("http://127.0.0.1:5177/api/device-log");
                connection = (HttpURLConnection) url.openConnection();
                connection.setConnectTimeout(900);
                connection.setReadTimeout(900);
                connection.setRequestMethod("POST");
                connection.setDoOutput(true);
                connection.setRequestProperty("content-type", "application/json; charset=utf-8");
                try (OutputStream output = connection.getOutputStream()) {
                    output.write(body.getBytes(StandardCharsets.UTF_8));
                }
                connection.getResponseCode();
            } catch (IOException error) {
                Log.d(TAG, "Servidor de logs da IDE indisponivel para Smart Optimizer.");
            } finally {
                if (connection != null) {
                    connection.disconnect();
                }
            }
        }, "SmartOptimizerLog").start();
    }

    private String smartOptimizerLevelLabel(int level) {
        if (level >= SMART_OPTIMIZER_CRITICAL) {
            return "Critico";
        }
        if (level >= SMART_OPTIMIZER_WARN) {
            return "Atencao";
        }
        return "Estavel";
    }

    private void syncSmartOptimizerUi() {
        smartOptimizerEnabled = false;
        smartOptimizerLevel = SMART_OPTIMIZER_NORMAL;
        if (hudSmartOptimizerToggle != null) {
            hudSmartOptimizerToggle.setVisibility(View.GONE);
            Object parent = hudSmartOptimizerToggle.getParent();
            if (parent instanceof View) {
                ((View) parent).setVisibility(View.GONE);
            }
        }
        if (hudSmartOptimizerStatus != null) {
            hudSmartOptimizerStatus.setVisibility(View.GONE);
        }
    }

    private static final class HotReloadBitmap {
        final byte[] rgbaPixels;
        final int width;
        final int height;

        HotReloadBitmap(byte[] rgbaPixels, int width, int height) {
            this.rgbaPixels = rgbaPixels;
            this.width = width;
            this.height = height;
        }
    }

    private HotReloadBitmap decodeHotReloadBitmap(File file) {
        if (file == null || !file.isFile()) {
            return null;
        }

        try {
            BitmapFactory.Options options = new BitmapFactory.Options();
            options.inPreferredConfig = Bitmap.Config.ARGB_8888;
            Bitmap decoded = BitmapFactory.decodeFile(file.getAbsolutePath(), options);
            if (decoded == null) {
                Log.w(TAG, "BitmapFactory nao decodificou hot reload: " + file.getAbsolutePath());
                return null;
            }

            Bitmap bitmap = decoded.getConfig() == Bitmap.Config.ARGB_8888
                    ? decoded
                    : decoded.copy(Bitmap.Config.ARGB_8888, false);
            if (bitmap == null) {
                decoded.recycle();
                return null;
            }

            int width = bitmap.getWidth();
            int height = bitmap.getHeight();
            int[] argbPixels = new int[width * height];
            byte[] rgbaPixels = new byte[width * height * 4];
            bitmap.getPixels(argbPixels, 0, width, 0, 0, width, height);

            for (int index = 0; index < argbPixels.length; index++) {
                int color = argbPixels[index];
                int offset = index * 4;
                rgbaPixels[offset] = (byte) ((color >> 16) & 0xFF);
                rgbaPixels[offset + 1] = (byte) ((color >> 8) & 0xFF);
                rgbaPixels[offset + 2] = (byte) (color & 0xFF);
                rgbaPixels[offset + 3] = (byte) ((color >> 24) & 0xFF);
            }

            bitmap.recycle();
            if (decoded != bitmap) {
                decoded.recycle();
            }
            return new HotReloadBitmap(rgbaPixels, width, height);
        } catch (OutOfMemoryError error) {
            Log.e(TAG, "Sem memoria para decodificar hot reload.", error);
            return null;
        } catch (RuntimeException error) {
            Log.e(TAG, "Falha decodificando bitmap de hot reload.", error);
            return null;
        }
    }

    private SkinHotReloadServer.Result handleSkinHotReloadRequest(SkinHotReloadServer.Request request) {
        if (request == null) {
            return SkinHotReloadServer.Result.error("Requisicao de hot reload vazia.");
        }

        AtomicReference<SkinHotReloadServer.Result> resultRef = new AtomicReference<>();
        CountDownLatch latch = new CountDownLatch(1);
        uiHandler.post(() -> {
            try {
                resultRef.set(applySkinHotReloadOnUiThread(request, true, true));
            } finally {
                latch.countDown();
            }
        });

        try {
            if (!latch.await(3500L, TimeUnit.MILLISECONDS)) {
                return SkinHotReloadServer.Result.error("Runtime demorou para aplicar a skin.");
            }
        } catch (InterruptedException error) {
            Thread.currentThread().interrupt();
            return SkinHotReloadServer.Result.error("Hot reload interrompido.");
        }

        SkinHotReloadServer.Result result = resultRef.get();
        return result != null ? result : SkinHotReloadServer.Result.error("Runtime nao retornou resultado.");
    }

    private int normalizeWeaponAudioId(int weaponId) {
        return weaponId >= 0 && weaponId < weaponAudioLastPlayMs.length ? weaponId : -1;
    }

    private String getWeaponAudioLabel(SkinHotReloadServer.Request request) {
        if (request != null && request.weaponName != null && !request.weaponName.trim().isEmpty()) {
            return request.weaponName.trim();
        }
        return request != null && request.weaponId >= 0 ? "arma " + request.weaponId : "arma";
    }

    private long getWeaponAudioCooldownMs(int weaponId) {
        switch (weaponId) {
            case 25:
            case 26:
            case 27:
                return 90L;
            case 33:
            case 34:
                return 120L;
            case 35:
            case 36:
                return 180L;
            case 37:
                return 15L;
            case 38:
                return 15L;
            case 28:
            case 29:
            case 30:
            case 31:
            case 32:
                return 20L;
            default:
                return 45L;
        }
    }

    private void setNativeHotReloadWeaponAudioActive(int weaponId, boolean active) {
        try {
            setHotReloadWeaponAudioActive(weaponId, active);
        } catch (UnsatisfiedLinkError error) {
            Log.w(TAG, "Native weapon audio active bridge indisponivel.", error);
        } catch (RuntimeException error) {
            Log.w(TAG, "Falha atualizando estado nativo do audio de arma.", error);
        }
    }

    private void ensureWeaponAudioSoundPoolLocked() {
        if (weaponAudioSoundPool != null) {
            return;
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            AudioAttributes attributes = new AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_GAME)
                    .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
                    .setFlags(AudioAttributes.FLAG_LOW_LATENCY)
                    .build();
            weaponAudioSoundPool = new SoundPool.Builder()
                    .setMaxStreams(8)
                    .setAudioAttributes(attributes)
                    .build();
        } else {
            weaponAudioSoundPool = new SoundPool(8, AudioManager.STREAM_MUSIC, 0);
        }

        weaponAudioSoundPool.setOnLoadCompleteListener((soundPool, sampleId, status) -> {
            synchronized (weaponAudioLock) {
                int weaponId = weaponAudioSampleToWeapon.get(sampleId, -1);
                if (weaponId < 0) {
                    return;
                }
                if (status == 0) {
                    weaponAudioLoaded.put(weaponId, 1);
                    setNativeHotReloadWeaponAudioActive(weaponId, true);
                    Log.i(TAG, "Weapon audio sample loaded weapon=" + weaponId + " sample=" + sampleId);
                    return;
                }

                if (weaponAudioSoundIds.get(weaponId, 0) == sampleId) {
                    weaponAudioSoundIds.delete(weaponId);
                    weaponAudioLoaded.delete(weaponId);
                }
                weaponAudioSampleToWeapon.delete(sampleId);
                setNativeHotReloadWeaponAudioActive(weaponId, false);
                Log.e(TAG, "Falha carregando audio de arma weapon=" + weaponId + " sample=" + sampleId + " status=" + status);
            }
        });
    }

    private int loadHotReloadWeaponSample(int weaponId, File file) {
        int normalizedWeaponId = normalizeWeaponAudioId(weaponId);
        if (normalizedWeaponId < 0 || file == null || !file.isFile() || !file.canRead()) {
            return 0;
        }

        synchronized (weaponAudioLock) {
            ensureWeaponAudioSoundPoolLocked();
            int oldSampleId = weaponAudioSoundIds.get(normalizedWeaponId, 0);
            if (oldSampleId > 0) {
                weaponAudioSoundPool.unload(oldSampleId);
                weaponAudioSampleToWeapon.delete(oldSampleId);
            }

            int sampleId = weaponAudioSoundPool.load(file.getAbsolutePath(), 1);
            if (sampleId <= 0) {
                weaponAudioSoundIds.delete(normalizedWeaponId);
                weaponAudioLoaded.delete(normalizedWeaponId);
                return 0;
            }

            weaponAudioSoundIds.put(normalizedWeaponId, sampleId);
            weaponAudioLoaded.put(normalizedWeaponId, 0);
            weaponAudioSampleToWeapon.put(sampleId, normalizedWeaponId);
            weaponAudioLastPlayMs[normalizedWeaponId] = 0L;
            return sampleId;
        }
    }

    private int unloadHotReloadWeaponSample(int weaponId) {
        int normalizedWeaponId = normalizeWeaponAudioId(weaponId);
        if (normalizedWeaponId < 0) {
            return 0;
        }

        synchronized (weaponAudioLock) {
            int sampleId = weaponAudioSoundIds.get(normalizedWeaponId, 0);
            if (sampleId > 0 && weaponAudioSoundPool != null) {
                weaponAudioSoundPool.unload(sampleId);
            }
            weaponAudioSoundIds.delete(normalizedWeaponId);
            weaponAudioLoaded.delete(normalizedWeaponId);
            weaponAudioSampleToWeapon.delete(sampleId);
            weaponAudioLastPlayMs[normalizedWeaponId] = 0L;
            return sampleId > 0 ? 1 : 0;
        }
    }

    private void releaseWeaponAudioSoundPool() {
        synchronized (weaponAudioLock) {
            if (weaponAudioSoundPool != null) {
                try {
                    weaponAudioSoundPool.release();
                } catch (RuntimeException error) {
                    Log.w(TAG, "Falha liberando SoundPool de hot reload.", error);
                }
            }
            weaponAudioSoundPool = null;
            weaponAudioSoundIds.clear();
            weaponAudioLoaded.clear();
            weaponAudioSampleToWeapon.clear();
            for (int index = 0; index < weaponAudioLastPlayMs.length; index++) {
                weaponAudioLastPlayMs[index] = 0L;
                setNativeHotReloadWeaponAudioActive(index, false);
            }
        }
    }

    public void playHotReloadWeaponAudio(int weaponId, float volume, float pan) {
        int normalizedWeaponId = normalizeWeaponAudioId(weaponId);
        if (normalizedWeaponId < 0) {
            return;
        }

        float safeVolume = Math.max(0.0f, Math.min(1.0f, volume));
        if (safeVolume <= 0.01f) {
            return;
        }
        float safePan = Math.max(-1.0f, Math.min(1.0f, pan));
        float leftVolume = safeVolume;
        float rightVolume = safeVolume;
        if (safePan > 0.0f) {
            leftVolume *= 1.0f - (safePan * 0.45f);
        } else if (safePan < 0.0f) {
            rightVolume *= 1.0f + (safePan * 0.45f);
        }

        SoundPool soundPool;
        int sampleId;
        synchronized (weaponAudioLock) {
            soundPool = weaponAudioSoundPool;
            sampleId = weaponAudioSoundIds.get(normalizedWeaponId, 0);
            if (soundPool == null || sampleId <= 0 || weaponAudioLoaded.get(normalizedWeaponId, 0) != 1) {
                return;
            }

            long now = SystemClock.uptimeMillis();
            if (now - weaponAudioLastPlayMs[normalizedWeaponId] < getWeaponAudioCooldownMs(normalizedWeaponId)) {
                return;
            }
            weaponAudioLastPlayMs[normalizedWeaponId] = now;
        }

        try {
            soundPool.play(sampleId, leftVolume, rightVolume, 10, 0, 1.0f);
        } catch (RuntimeException error) {
            Log.w(TAG, "Falha tocando audio customizado de arma.", error);
        }
    }

    private SkinHotReloadServer.Result applyWeaponAudioHotReloadOnUiThread(SkinHotReloadServer.Request request, boolean showToast, boolean allowPersistenceChange) {
        int weaponId = normalizeWeaponAudioId(request.weaponId);
        if (weaponId < 0) {
            return SkinHotReloadServer.Result.error("Arma invalida para hot reload de audio.");
        }

        String weaponLabel = getWeaponAudioLabel(request);
        if (request.isRestore()) {
            int removed = 0;
            if (allowPersistenceChange) {
                try {
                    removed = SkinHotReloadServer.removeActiveMod(this, request.targetGroup, request.textureName);
                } catch (IOException error) {
                    Log.e(TAG, "Falha removendo audio permanente.", error);
                    return SkinHotReloadServer.Result.error("Nao foi possivel remover o audio permanente.");
                }
            }

            int unloaded = unloadHotReloadWeaponSample(weaponId);
            setNativeHotReloadWeaponAudioActive(weaponId, false);
            String message;
            if (removed > 0 && unloaded > 0) {
                message = "Audio original restaurado e permanente removido.";
            } else if (removed > 0) {
                message = "Permanente removido; o audio customizado nao estava carregado.";
            } else if (unloaded > 0) {
                message = "Audio original restaurado para " + weaponLabel + ".";
            } else {
                message = "Nenhum audio customizado desta arma estava ativo.";
            }

            if (showToast) {
                Toast.makeText(this, message, Toast.LENGTH_SHORT).show();
            }
            Log.i(TAG, "Weapon audio restore request=" + request.requestId + " weapon=" + weaponId + " removed=" + removed + " unloaded=" + unloaded);
            return SkinHotReloadServer.Result.success(unloaded > 0, message);
        }

        if (request.stagedFile == null || !request.stagedFile.isFile()) {
            return SkinHotReloadServer.Result.error("Arquivo de audio nao chegou ao staging do APK.");
        }

        setNativeHotReloadWeaponAudioActive(weaponId, false);
        int sampleId = loadHotReloadWeaponSample(weaponId, request.stagedFile);
        if (sampleId <= 0) {
            setNativeHotReloadWeaponAudioActive(weaponId, false);
            return SkinHotReloadServer.Result.error("Android nao conseguiu carregar esse audio.");
        }

        String message = "Audio da " + weaponLabel + " recebido; o Android esta carregando o sample.";
        if (request.isPermanent() && allowPersistenceChange) {
            try {
                SkinHotReloadServer.persistActiveMod(this, request);
                message = "Audio permanente salvo para " + weaponLabel + ".";
            } catch (IOException | JSONException error) {
                Log.e(TAG, "Falha salvando audio permanente.", error);
                message = "Audio aplicado agora, mas nao salvou permanente.";
            }
        }

        if (showToast) {
            Toast.makeText(this, message, Toast.LENGTH_SHORT).show();
        }
        Log.i(TAG, "Weapon audio hot reload request=" + request.requestId + " weapon=" + weaponId + " sample=" + sampleId + " permanent=" + request.isPermanent());
        return SkinHotReloadServer.Result.success(true, message);
    }

    private SkinHotReloadServer.Result applySkinHotReloadOnUiThread(SkinHotReloadServer.Request request, boolean showToast, boolean allowPersistenceChange) {
        synchronized (hotReloadApplyLock) {
            if (request.isWeaponAudio()) {
                return applyWeaponAudioHotReloadOnUiThread(request, showToast, allowPersistenceChange);
            }

            if (request.isRestore()) {
                int removed = 0;
                if (allowPersistenceChange) {
                    try {
                        removed = SkinHotReloadServer.removeActiveMod(this, request.targetGroup, request.textureName);
                    } catch (IOException error) {
                        Log.e(TAG, "Falha removendo mod permanente.", error);
                        return SkinHotReloadServer.Result.error("Nao foi possivel remover o mod permanente.");
                    }
                }

                int nativeResult = 0;
                try {
                    nativeResult = restoreHotReloadTexture(
                            request.textureName,
                            request.targetGroup,
                            request.skinId
                    );
                } catch (UnsatisfiedLinkError error) {
                    Log.e(TAG, "Native restoreHotReloadTexture indisponivel.", error);
                } catch (RuntimeException error) {
                    Log.e(TAG, "Falha restaurando textura hot reload.", error);
                }

                boolean textureRestored = nativeResult >= 2;
                String message;
                if (removed > 0 && textureRestored) {
                    message = "Original restaurado e permanente removido.";
                } else if (removed > 0) {
                    message = "Permanente removido; reinicie o jogo se a textura ativa continuar.";
                } else if (textureRestored) {
                    message = "Original restaurado sem reiniciar o jogo.";
                } else {
                    message = "Nenhuma textura ativa desta sessao para restaurar.";
                }
                if (showToast) {
                    Toast.makeText(this, message, Toast.LENGTH_SHORT).show();
                }
                Log.i(TAG, "Skin hot reload restore request=" + request.requestId + " texture=" + request.textureName + " removed=" + removed + " result=" + nativeResult);
                return SkinHotReloadServer.Result.success(textureRestored, message);
            }

            int nativeResult = 0;
            String stagedPath = request.stagedFile != null ? request.stagedFile.getAbsolutePath() : "";
            try {
                nativeResult = applyHotReloadTexture(
                        request.textureName,
                        request.targetGroup,
                        request.skinId,
                        stagedPath,
                        request.format,
                        request.width,
                        request.height
                );
            } catch (UnsatisfiedLinkError error) {
                Log.w(TAG, "Native hot reload bridge indisponivel; usando fallback de skin.", error);
            } catch (RuntimeException error) {
                Log.e(TAG, "Falha no hot reload nativo.", error);
            }

            if (nativeResult < 2) {
                HotReloadBitmap bitmap = decodeHotReloadBitmap(request.stagedFile);
                if (bitmap != null) {
                    try {
                        int bitmapResult = applyHotReloadBitmap(
                                request.textureName,
                                request.targetGroup,
                                request.skinId,
                                bitmap.rgbaPixels,
                                bitmap.width,
                                bitmap.height
                        );
                        nativeResult = Math.max(nativeResult, bitmapResult);
                    } catch (UnsatisfiedLinkError error) {
                        Log.w(TAG, "Native hot reload bitmap indisponivel.", error);
                    } catch (RuntimeException error) {
                        Log.e(TAG, "Falha no hot reload por bitmap.", error);
                    }
                }
            }

            if (nativeResult == 0 && request.skinId >= 0 && request.skinId <= 311) {
                try {
                    setLocalPlayerSkin(request.skinId);
                    nativeResult = 1;
                } catch (UnsatisfiedLinkError error) {
                    Log.e(TAG, "Native setLocalPlayerSkin indisponivel no hot reload.", error);
                }
            }

            boolean textureApplied = nativeResult >= 2;
            boolean runtimeTouched = nativeResult > 0;
            String message = textureApplied
                    ? "Hot reload aplicado: textura e skin atualizadas."
                    : runtimeTouched
                    ? "Skin atualizada; textura recebida no staging do APK."
                    : "Textura recebida no staging, mas o runtime nativo nao aplicou.";

            if (request.isPermanent() && allowPersistenceChange) {
                if (textureApplied) {
                    try {
                        SkinHotReloadServer.persistActiveMod(this, request);
                        message = "Permanente salvo e aplicado no APK.";
                    } catch (IOException | JSONException error) {
                        Log.e(TAG, "Falha salvando mod permanente.", error);
                        message = "Aplicou agora, mas nao salvou permanente.";
                    }
                } else {
                    message = "Nao salvei permanente porque a textura nao aplicou no material ativo.";
                }
            }

            if (showToast) {
                Toast.makeText(this, message, Toast.LENGTH_SHORT).show();
            }
            Log.i(TAG, "Skin hot reload request=" + request.requestId + " texture=" + request.textureName + " result=" + nativeResult);
            return SkinHotReloadServer.Result.success(textureApplied, message);
        }
    }

    @Override
    public void onHeightChanged(int orientation, int height) {
        mKeyboard.onHeightChanged(height);
        mDialog.onHeightChanged(height);
    }

    @Override
    public void OnInputEnd(String str) {
        String text = limitRoleplayChatText(str);
        if (text.isEmpty()) {
            hideKeyboard();
            return;
        }
        if (text.startsWith("/")) {
            dispatchRuntimeCommand(text);
            hideKeyboard();
            hideSystemUI();
            return;
        }
        try {
            onInputEnd(text.getBytes(StandardCharsets.UTF_8));
        } catch (UnsatisfiedLinkError error) {
            Log.e(TAG, "Native chat input bridge unavailable.", error);
        }
        hideKeyboard();
        hideSystemUI();
    }

    public void hideKeyboard() {
        if (mKeyboard != null) {
            mKeyboard.HideInputLayout();
        }
        hideHudChatKeyboard();
        hideSystemUI();
    }

    public void showKeyboard() {
        runOnUiThread(() -> {
            if (mKeyboard != null) {
                mKeyboard.ShowInputLayout();
            }
            hideSystemUI();
        });
    }

    public void showTab() {
    }

    public void hideTab() {
    }

    public void clearTab() {
    }

    public void setTab(int id, String name, int score, int ping) {
    }

    public void showLoadingScreen() {
        runOnUiThread(() -> {
            if (loadingscreen != null) {
                loadingStatusDotCount = 0;
                if (loadingStatusText != null) {
                    loadingStatusText.setText("Sincronizando");
                }
                if (handler != null) {
                    handler.removeCallbacks(loadingStatusTicker);
                }
                loadingscreen.setVisibility(View.VISIBLE);
                loadingscreen.bringToFront();
                if (handler != null) {
                    handler.postDelayed(loadingStatusTicker, 420);
                }
            }
        });
    }

    private void showInitialLoadingScreen() {
        if (loadingscreen == null) {
            return;
        }

        loadingStatusDotCount = 0;
        if (loadingStatusText != null) {
            loadingStatusText.setText("Sincronizando");
        }
        loadingscreen.setVisibility(View.VISIBLE);
        loadingscreen.bringToFront();
        if (handler != null) {
            handler.removeCallbacks(loadingStatusTicker);
            handler.postDelayed(loadingStatusTicker, 420);
        }
    }

    public void hideLoadingScreen() {
        runOnUiThread(() -> {
            if (handler != null) {
                handler.removeCallbacks(loadingStatusTicker);
            }
            if (loadingscreen != null) {
                loadingscreen.setVisibility(View.GONE);
            }
            hideSystemUI();
        });
    }

    public void setPauseState(boolean paused) {
        runOnUiThread(() -> {
            if (paused) {
                nativeMenuStateRecheckGeneration++;
                nativeUserPauseActive = true;
                hideHudSettingsPanel();
                hideHudHdMapOverlay();
                setHudOptionsPanelVisible(false);
                hideHudStatusPanel();
                hideHudChatPanel();
                hideWeaponWheelOverlayInternal();
                if (RadialMenu.menuVisible && mRadialMenu != null) {
                    mRadialMenu.hide();
                }
                if (Radinho.radinhoVisible && mRadinho != null) {
                    mRadinho.hide();
                }
            } else {
                nativeUserPauseActive = readNativeUserPauseActiveRaw();
                if (nativeUserPauseActive) {
                    scheduleNativeMenuStateRecheck();
                } else {
                    nativeMenuStateRecheckGeneration++;
                }
                hideHudSettingsPanel();
                hideHudHdMapOverlay();
                setHudOptionsPanelVisible(false);
            }
            refreshRuntimeChrome();
            hideSystemUI();
        });
    }

    private void closeNativeMapOrPause() {
        try {
            setAllowNextNativePauseMenu(false);
        } catch (UnsatisfiedLinkError error) {
            Log.e(TAG, "Nao foi possivel bloquear menu nativo automatico.", error);
        }
        requestNativeGameplayResume();
        performNativeMenuTap(NATIVE_MENU_RESUME_X_RATIO, NATIVE_MENU_RESUME_Y_RATIO);
        uiHandler.postDelayed(
                () -> {
                    requestNativeGameplayResume();
                    performNativeMenuTap(NATIVE_MENU_RESUME_X_RATIO, NATIVE_MENU_RESUME_Y_RATIO);
                },
                180L
        );
        uiHandler.postDelayed(() -> {
            nativeUserPauseActive = false;
            requestNativeGameplayResume();
            refreshRuntimeChrome();
        }, 420L);
    }

    public void showDialog(int dialogId, int dialogStyle, byte[] caption, byte[] text, byte[] button1, byte[] button2) {
        runOnUiThread(() -> {
            if (mDialog != null) {
                mDialog.show(
                        dialogId,
                        dialogStyle,
                        decodeNativeText(caption),
                        decodeNativeText(text),
                        decodeNativeText(button1),
                        decodeNativeText(button2)
                );
            }
        });
    }

    public void showWithoutReset() {
        runOnUiThread(() -> {
            if (isHudSettingsVisible()) {
                hideAttachEditorOverlay();
                return;
            }
            if (mAttachEdit != null) {
                mAttachEdit.showWithoutReset();
            }
        });
    }

    public void hideWithoutReset() {
        runOnUiThread(() -> {
            if (mAttachEdit != null) {
                mAttachEdit.hideWithoutReset();
            }
        });
    }

    public void showEditObject() {
        runOnUiThread(() -> {
            if (isHudSettingsVisible()) {
                hideAttachEditorOverlay();
                return;
            }
            if (mAttachEdit != null) {
                mAttachEdit.show();
            }
        });
    }

    public void hideEditObject() {
        runOnUiThread(() -> {
            if (mAttachEdit != null) {
                mAttachEdit.hide();
            }
        });
    }

    private String decodeNativeText(byte[] value) {
        return value == null ? "" : new String(value, StandardCharsets.UTF_8);
    }

    public ConstraintLayout logo;

    public ConstraintLayout hud1;

    public ConstraintLayout hud_y_v;
    public ConstraintLayout hud_f_v;

    public void ShowLogo(boolean show)
    {
        iShowLogo = show;
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
            }
        });
    }

    public void togglePassengerButton(boolean toggle)
    {
        runOnUiThread(() -> {
            hudSourceVehicleVisible = toggle;
            if(hud_main == null) return;
            applySourceVehicleButtonsVisibility(toggle, false);
        });
    }

    void toggleLockButton(boolean toggle)
    {
        runOnUiThread(() -> {
            hudSourceLockVisible = toggle;
            if(hud_main == null) return;
            applySourceVehicleButtonsVisibility(false, toggle);
        });
    }

    void MostrarTeclas() {
        setActionKeysVisible(false);
    }

    void EsconderTeclas() {
        setActionKeysVisible(false);
    }

    private void toggleActionKeys() {
        runOnUiThread(() -> {
            if (hud_main == null) return;
            hud_y_v = hud_main.findViewById(R.id.hud_y);
            hud_f_v = hud_main.findViewById(R.id.hud_f);
            boolean visible = (hud_y_v != null && hud_y_v.getVisibility() == View.VISIBLE)
                    || (hud_f_v != null && hud_f_v.getVisibility() == View.VISIBLE);
            setActionKeysVisibleInternal(!visible);
        });
    }

    private void setActionKeysVisible(boolean visible) {
        runOnUiThread(() -> setActionKeysVisibleInternal(visible));
    }

    private void setActionKeysVisibleInternal(boolean visible) {
        if (hud_main == null) return;
        hud_y_v = hud_main.findViewById(R.id.hud_y);
        hud_f_v = hud_main.findViewById(R.id.hud_f);
        if (hud_y_v == null || hud_f_v == null) return;

        int visibility = View.GONE;
        hud_y_v.setVisibility(visibility);
        hud_f_v.setVisibility(visibility);
        TeclasAbertas = false;
        hideSystemUI();
    }

    private void hideLegacyVehicleButtonViews() {
        if (hud_main == null) {
            return;
        }
        View oldEnter = hud_main.findViewById(R.id.enter_passenger);
        View oldLock = hud_main.findViewById(R.id.vehicle_lock_butt);
        disableLegacyHudButton(oldEnter);
        disableLegacyHudButton(oldLock);
    }

    private void disableLegacyHudButton(View view) {
        if (view == null) {
            return;
        }
        view.setOnClickListener(null);
        view.setOnTouchListener(null);
        view.setClickable(false);
        view.setFocusable(false);
        view.setEnabled(false);
        view.setPressed(false);
        view.setAlpha(0.0f);
        setVisibilityIfChanged(view, View.GONE);
    }

    private void toggleHudShortcutButtons() {
        runOnUiThread(() -> {
            noteHudControlInteractionInternal();
            setHudShortcutButtonsVisibleInternal(!hudShortcutButtonsVisible);
        });
    }

    private void setHudShortcutButtonsVisibleInternal(boolean visible) {
        if (hud_main == null) return;
        cacheHudControlViews();

        int visibility = visible ? View.VISIBLE : View.INVISIBLE;

        if (hudInteractionButton != null) {
            setVisibilityIfChanged(hudInteractionButton, visibility);
            if (visible) {
                hudInteractionButton.bringToFront();
            }
        }
        if (hudOptionsButton != null) {
            setVisibilityIfChanged(hudOptionsButton, View.GONE);
        }
        if (hudMenuButton != null) {
            setVisibilityIfChanged(hudMenuButton, View.VISIBLE);
            hudMenuButton.bringToFront();
        }

        hideHudButtonsMovedToRadial();
        if (!visible) {
            setHudOptionsPanelVisible(false);
        }
        hudShortcutButtonsVisible = visible;
        TeclasAbertas = visible;
        hideSystemUI();
    }
    
    native void ClickLockVehicleButton();
    public native void ClickEnterPassengerButton();
    public native void changeGun();
    native void MostrarChat();
    native void AbrirChatBotao();
    native void OcultarChatBotao();

    private int dpToPx(float dp) {
        return Math.round(TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, dp, getResources().getDisplayMetrics()));
    }

    private boolean isInsideCircularHudHotspot(View view, MotionEvent event) {
        if (view == null || event == null || view.getWidth() <= 0 || view.getHeight() <= 0) {
            return false;
        }
        float radius = Math.min(view.getWidth(), view.getHeight()) * 0.43f;
        float centerX = view.getWidth() * 0.5f;
        float centerY = view.getHeight() * 0.5f;
        float dx = event.getX() - centerX;
        float dy = event.getY() - centerY;
        return (dx * dx + dy * dy) <= (radius * radius);
    }

    private void installHudHdMapHotspotOpenHandler() {
        if (hudHdMapHotspot == null) {
            return;
        }
        hudHdMapHotspot.setOnClickListener(null);
        hudHdMapHotspot.setOnTouchListener((view, event) -> {
            if (hudEditMode || !shouldShowHudHdMapHotspot()) {
                return false;
            }
            switch (event.getActionMasked()) {
                case MotionEvent.ACTION_DOWN:
                    return isInsideCircularHudHotspot(view, event);
                case MotionEvent.ACTION_UP:
                    if (isInsideCircularHudHotspot(view, event)) {
                        noteHudControlInteraction();
                        showHudHdMapOverlay();
                    }
                    return true;
                case MotionEvent.ACTION_CANCEL:
                    return true;
                default:
                    return true;
            }
        });
    }

    private void installSourceAttackTouchHandler() {
        installSourceHoldButton(hudSourceAttackButton, SOURCE_ACTION_ATTACK);
    }

    private void installGameplayTouchPassthroughHandler() {
        if (hudGameplayTouchPassthrough == null) {
            return;
        }
        hudGameplayTouchPassthrough.setOnTouchListener(null);
        hudGameplayTouchPassthrough.setClickable(false);
        hudGameplayTouchPassthrough.setFocusable(false);
        hudGameplayTouchPassthrough.setEnabled(false);
    }

    private boolean forwardGameplayTouchToNative(MotionEvent event) {
        handleGameplayCameraDispatchTouch(event);
        return false;
    }

    private void handleGameplayCameraDispatchTouch(MotionEvent event) {
        if (event == null) {
            resetGameplayLookGesture();
            return;
        }
        if (!canCaptureGameplayCameraLook()) {
            resetGameplayLookGesture();
            return;
        }

        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                if (isTouchOnGameplayHudControl(event.getRawX(), event.getRawY())) {
                    resetGameplayLookGesture();
                    return;
                }
                handleSourceCameraLookTouch(event);
                break;
            case MotionEvent.ACTION_MOVE:
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
            case MotionEvent.ACTION_POINTER_UP:
                if (gameplayLookGestureActive) {
                    handleSourceCameraLookTouch(event);
                } else if (event.getActionMasked() == MotionEvent.ACTION_UP
                        || event.getActionMasked() == MotionEvent.ACTION_CANCEL) {
                    resetGameplayLookGesture();
                }
                break;
            case MotionEvent.ACTION_POINTER_DOWN:
                if (gameplayLookGestureActive) {
                    handleSourceCameraLookTouch(event);
                }
                break;
            default:
                break;
        }
    }

    private boolean canCaptureGameplayCameraLook() {
        return !hudEditMode
                && !hudChatAreaMappingMode
                && !hudOptionsPanelVisible
                && !shouldHideGameplayHudForNativeMenu()
                && !isHudSettingsVisible()
                && !isHudHdMapVisible()
                && !isHudChatPanelVisible()
                && !isPhoneOverlayVisible()
                && !isInventoryOverlayVisible()
                && !isWeaponWheelOverlayVisible()
                && !isPickupCreatorVisible()
                && !RadialMenu.menuVisible
                && !Radinho.radinhoVisible;
    }

    private boolean isTouchOnGameplayHudControl(float rawX, float rawY) {
        return isRawTouchInsideView(hudSourceAnalog, rawX, rawY)
                || isRawTouchInsideView(hudSourceAttackButton, rawX, rawY)
                || isRawTouchInsideView(hudSourceAccelerateButton, rawX, rawY)
                || isRawTouchInsideView(hudSourceBrakeButton, rawX, rawY)
                || isRawTouchInsideView(hudSourceHandbrakeButton, rawX, rawY)
                || isRawTouchInsideView(hudSourceHornButton, rawX, rawY)
                || isRawTouchInsideView(hudSourceSprintButton, rawX, rawY)
                || isRawTouchInsideView(hudSourceJumpButton, rawX, rawY)
                || isRawTouchInsideView(hudSourceVehicleButton, rawX, rawY)
                || isRawTouchInsideView(hudSourceLockButton, rawX, rawY)
                || isRawTouchInsideView(hudSourceCameraButton, rawX, rawY)
                || isRawTouchInsideView(hudWeaponButton, rawX, rawY)
                || isRawTouchInsideView(hudMenuButton, rawX, rawY)
                || isRawTouchInsideView(hudInteractionButton, rawX, rawY)
                || isRawTouchInsideView(hudChatButton, rawX, rawY)
                || isRawTouchInsideView(hudChatClickArea, rawX, rawY)
                || isRawTouchInsideView(hudHdMapHotspot, rawX, rawY)
                || isRawTouchInsideView(hudFpsCounter, rawX, rawY)
                || isRawTouchInsideView(hudTopRightPanel, rawX, rawY)
                || isRawTouchInsideView(hud_main != null ? hud_main.findViewById(R.id.hud_left_panel) : null, rawX, rawY)
                || isRawTouchInsideView(hud_main != null ? hud_main.findViewById(R.id.hud_needs_panel) : null, rawX, rawY);
    }

    private boolean isRawTouchInsideView(View view, float rawX, float rawY) {
        if (view == null
                || view.getVisibility() != View.VISIBLE
                || view.getAlpha() <= 0.01f
                || view.getWidth() <= 0
                || view.getHeight() <= 0) {
            return false;
        }

        view.getLocationOnScreen(gameplayTouchLocation);
        return rawX >= gameplayTouchLocation[0]
                && rawX <= gameplayTouchLocation[0] + view.getWidth()
                && rawY >= gameplayTouchLocation[1]
                && rawY <= gameplayTouchLocation[1] + view.getHeight();
    }

    private void handleSourceCameraLookTouch(MotionEvent event) {
        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN: {
                int pointerIndex = event.getActionIndex();
                gameplayLookGestureActive = true;
                gameplayLookPointerId = event.getPointerId(pointerIndex);
                gameplayLookLastX = event.getX(pointerIndex);
                gameplayLookLastY = event.getY(pointerIndex);
                break;
            }
            case MotionEvent.ACTION_POINTER_DOWN:
                if (!gameplayLookGestureActive) {
                    int pointerIndex = event.getActionIndex();
                    gameplayLookGestureActive = true;
                    gameplayLookPointerId = event.getPointerId(pointerIndex);
                    gameplayLookLastX = event.getX(pointerIndex);
                    gameplayLookLastY = event.getY(pointerIndex);
                }
                break;
            case MotionEvent.ACTION_MOVE:
                if (!gameplayLookGestureActive) {
                    return;
                }
                int pointerIndex = event.findPointerIndex(gameplayLookPointerId);
                if (pointerIndex < 0) {
                    pointerIndex = 0;
                    gameplayLookPointerId = event.getPointerId(pointerIndex);
                }
                float x = event.getX(pointerIndex);
                float y = event.getY(pointerIndex);
                float deltaX = x - gameplayLookLastX;
                float deltaY = y - gameplayLookLastY;
                gameplayLookLastX = x;
                gameplayLookLastY = y;
                if (Math.abs(deltaX) >= 0.5f || Math.abs(deltaY) >= 0.5f) {
                    sendSourceCameraLookDelta(deltaX, deltaY);
                }
                break;
            case MotionEvent.ACTION_POINTER_UP:
                if (gameplayLookGestureActive && event.getPointerId(event.getActionIndex()) == gameplayLookPointerId) {
                    resetGameplayLookGesture();
                }
                break;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                resetGameplayLookGesture();
                break;
            default:
                break;
        }
    }

    private void sendSourceCameraLookDelta(float deltaX, float deltaY) {
        int screenWidth = hud_main != null ? hud_main.getWidth() : 0;
        int screenHeight = hud_main != null ? hud_main.getHeight() : 0;
        try {
            addNativeCameraLookDelta(deltaX, deltaY, screenWidth, screenHeight);
        } catch (UnsatisfiedLinkError ignored) {
        }
    }

    private void resetGameplayLookGesture() {
        gameplayLookGestureActive = false;
        gameplayLookPointerId = -1;
        gameplayLookLastX = 0.0f;
        gameplayLookLastY = 0.0f;
    }

    private void installSourceControlTouchHandlers() {
        installSourceAttackTouchHandler();
        installSourceHoldButton(hudSourceAccelerateButton, SOURCE_ACTION_ACCELERATE);
        installSourceHoldButton(hudSourceBrakeButton, SOURCE_ACTION_BRAKE);
        installSourceHoldButton(hudSourceHandbrakeButton, SOURCE_ACTION_HANDBRAKE);
        installSourceHoldButton(hudSourceHornButton, SOURCE_ACTION_HORN);
        installSourceHoldButton(hudSourceSprintButton, SOURCE_ACTION_SPRINT);
        installSourceHoldButton(hudSourceJumpButton, SOURCE_ACTION_JUMP);
        installSourceTapRunnableButton(hudSourceVehicleButton, this::pressSourceVehicleAction);
        installSourceTapButton(hudSourceCameraButton, SOURCE_ACTION_CAMERA);
        installSourceTapRunnableButton(hudSourceLockButton, this::pressSourceLockAction);
        installSourceAnalogTouchHandler();
    }

    private void installSourceHoldButton(View button, int action) {
        if (button == null) {
            return;
        }
        button.setOnTouchListener((view, event) -> {
            if (hudEditMode || shouldHideGameplayHudForNativeMenu()) {
                setNativeActionButtonState(action, false);
                return false;
            }
            switch (event.getActionMasked()) {
                case MotionEvent.ACTION_DOWN:
                    noteHudControlInteraction();
                    view.setPressed(true);
                    setNativeActionButtonState(action, true);
                    return true;
                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_CANCEL:
                    view.setPressed(false);
                    setNativeActionButtonState(action, false);
                    return true;
                default:
                    return true;
            }
        });
    }

    private void installSourceTapButton(View button, int action) {
        installSourceTapRunnableButton(button, () -> setNativeActionButtonState(action, true));
    }

    private void installSourceTapRunnableButton(View button, Runnable action) {
        if (button == null) {
            return;
        }
        button.setOnTouchListener((view, event) -> {
            if (hudEditMode || shouldHideGameplayHudForNativeMenu()) {
                view.setPressed(false);
                return false;
            }
            switch (event.getActionMasked()) {
                case MotionEvent.ACTION_DOWN:
                    noteHudControlInteraction();
                    view.setPressed(true);
                    return true;
                case MotionEvent.ACTION_UP:
                    boolean activate = view.isPressed() && isInsideCircularHudHotspot(view, event);
                    view.setPressed(false);
                    if (activate && action != null) {
                        action.run();
                    }
                    return true;
                case MotionEvent.ACTION_CANCEL:
                    view.setPressed(false);
                    return true;
                default:
                    return true;
            }
        });
    }

    private void installSourceAnalogTouchHandler() {
        if (hudSourceAnalog == null || hudSourceAnalogKnob == null) {
            return;
        }
        hudSourceAnalog.setOnTouchListener((view, event) -> {
            if (hudEditMode || shouldHideGameplayHudForNativeMenu()) {
                resetSourceAnalogVisual();
                setNativeAnalogState(0, 0);
                return false;
            }
            switch (event.getActionMasked()) {
                case MotionEvent.ACTION_DOWN:
                    noteHudControlInteraction();
                    view.setPressed(true);
                    updateSourceAnalogFromTouch(view, event);
                    return true;
                case MotionEvent.ACTION_MOVE:
                    updateSourceAnalogFromTouch(view, event);
                    return true;
                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_CANCEL:
                    view.setPressed(false);
                    resetSourceAnalogVisual();
                    setNativeAnalogState(0, 0);
                    return true;
                default:
                    return true;
            }
        });
    }

    private void updateSourceAnalogFromTouch(View view, MotionEvent event) {
        float centerX = view.getWidth() * 0.5f;
        float centerY = view.getHeight() * 0.5f;
        float dx = event.getX() - centerX;
        float dy = event.getY() - centerY;
        float radius = Math.max(1.0f, Math.min(view.getWidth(), view.getHeight()) * 0.36f);
        float distance = (float) Math.sqrt(dx * dx + dy * dy);
        if (distance > radius) {
            float scale = radius / distance;
            dx *= scale;
            dy *= scale;
        }
        if (hudSourceAnalogKnob != null) {
            hudSourceAnalogKnob.setTranslationX(dx);
            hudSourceAnalogKnob.setTranslationY(dy);
        }
        int leftRight = clampInt(Math.round((dx / radius) * 127.0f), -128, 127);
        int upDown = clampInt(Math.round((dy / radius) * 127.0f), -128, 127);
        setNativeAnalogState(leftRight, upDown);
    }

    private void resetSourceAnalogVisual() {
        if (hudSourceAnalogKnob != null) {
            hudSourceAnalogKnob.animate().cancel();
            hudSourceAnalogKnob.setTranslationX(0.0f);
            hudSourceAnalogKnob.setTranslationY(0.0f);
        }
    }

    private void resetNativeSourceControls() {
        try {
            resetNativeSourceControlState();
        } catch (UnsatisfiedLinkError error) {
            setNativeActionButtonState(SOURCE_ACTION_ATTACK, false);
            setNativeActionButtonState(SOURCE_ACTION_ACCELERATE, false);
            setNativeActionButtonState(SOURCE_ACTION_BRAKE, false);
            setNativeActionButtonState(SOURCE_ACTION_HANDBRAKE, false);
            setNativeActionButtonState(SOURCE_ACTION_HORN, false);
            setNativeActionButtonState(SOURCE_ACTION_SPRINT, false);
            setNativeActionButtonState(SOURCE_ACTION_JUMP, false);
            setNativeAnalogState(0, 0);
        }
        resetSourceAnalogVisual();
        setPressedIfPresent(hudSourceAttackButton, false);
        setPressedIfPresent(hudSourceAccelerateButton, false);
        setPressedIfPresent(hudSourceBrakeButton, false);
        setPressedIfPresent(hudSourceHandbrakeButton, false);
        setPressedIfPresent(hudSourceHornButton, false);
        setPressedIfPresent(hudSourceSprintButton, false);
        setPressedIfPresent(hudSourceJumpButton, false);
        setPressedIfPresent(hudSourceVehicleButton, false);
        setPressedIfPresent(hudSourceLockButton, false);
        setPressedIfPresent(hudSourceCameraButton, false);
    }

    private void setPressedIfPresent(View view, boolean pressed) {
        if (view != null) {
            view.setPressed(pressed);
        }
    }

    private void pressSourceVehicleAction() {
        if (hudEditMode || shouldHideGameplayHudForNativeMenu()) {
            return;
        }
        setNativeActionButtonState(SOURCE_ACTION_VEHICLE, true);
    }

    private void pressSourceLockAction() {
        if (hudEditMode || shouldHideGameplayHudForNativeMenu()) {
            return;
        }
        long currTime = System.currentTimeMillis() / 1000;
        if (buttonLockCD > currTime) {
            return;
        }
        buttonLockCD = currTime + 2;
        ClickLockVehicleButton();
    }

    private boolean setVisibilityIfChanged(View view, int visibility) {
        if (view == null || view.getVisibility() == visibility) {
            return false;
        }
        view.setVisibility(visibility);
        return true;
    }

    private void cacheHudControlViews() {
        if (hud_main == null) {
            return;
        }
        if (hudMenuButton == null) {
            hudMenuButton = hud_main.findViewById(R.id.btn_0);
        }
        if (hudInteractionButton == null) {
            hudInteractionButton = hud_main.findViewById(R.id.btn_1);
        }
        if (hudOptionsButton == null) {
            hudOptionsButton = hud_main.findViewById(R.id.btn_hud_options);
        }
        if (hudGameplayTouchPassthrough == null) {
            hudGameplayTouchPassthrough = hud_main.findViewById(R.id.hud_game_touch_passthrough);
        }
        if (hudChatButton == null) {
            hudChatButton = hud_main.findViewById(R.id.btn_chat_toggle);
        }
        ensureHudChatClickArea();
        if (hudChatAreaHint == null) {
            hudChatAreaHint = hud_main.findViewById(R.id.hud_chat_area_hint);
        }
        if (hudSettingsGearButton == null) {
            hudSettingsGearButton = hud_main.findViewById(R.id.btn_hud_settings_gear);
        }
        if (hudChatMapLayer == null) {
            hudChatMapLayer = hud_main.findViewById(R.id.hud_chat_map_layer);
        }
        if (hudHdMapLayer == null) {
            hudHdMapLayer = hud_main.findViewById(R.id.hud_hd_map_layer);
        }
        if (hudHdMapHotspot == null) {
            hudHdMapHotspot = hud_main.findViewById(R.id.hud_hd_map_hotspot);
            if (hudHdMapHotspot != null) {
                installHudHdMapHotspotOpenHandler();
            }
        }
        if (hudHdMapView == null) {
            hudHdMapView = hud_main.findViewById(R.id.hud_hd_map_view);
            if (hudHdMapView != null) {
                hudHdMapView.setTileRoot("gtag-satellite");
            }
        }
        if (hudChatAreaPreview == null) {
            hudChatAreaPreview = hud_main.findViewById(R.id.hud_chat_area_preview);
        }
        if (hudChatAreaSaveButton == null) {
            hudChatAreaSaveButton = hud_main.findViewById(R.id.hud_chat_area_save);
        }
        if (hudChatAreaCancelButton == null) {
            hudChatAreaCancelButton = hud_main.findViewById(R.id.hud_chat_area_cancel);
        }
        if (hudSourceAttackButton == null) {
            hudSourceAttackButton = hud_main.findViewById(R.id.hud_source_attack_button);
        }
        if (hudSourceAnalog == null) {
            hudSourceAnalog = hud_main.findViewById(R.id.hud_source_analog);
        }
        if (hudSourceAnalogKnob == null) {
            hudSourceAnalogKnob = hud_main.findViewById(R.id.hud_source_analog_knob);
        }
        if (hudSourceAccelerateButton == null) {
            hudSourceAccelerateButton = hud_main.findViewById(R.id.hud_source_accelerate_button);
        }
        if (hudSourceBrakeButton == null) {
            hudSourceBrakeButton = hud_main.findViewById(R.id.hud_source_brake_button);
        }
        if (hudSourceHandbrakeButton == null) {
            hudSourceHandbrakeButton = hud_main.findViewById(R.id.hud_source_handbrake_button);
        }
        if (hudSourceHornButton == null) {
            hudSourceHornButton = hud_main.findViewById(R.id.hud_source_horn_button);
        }
        if (hudSourceSprintButton == null) {
            hudSourceSprintButton = hud_main.findViewById(R.id.hud_source_sprint_button);
        }
        if (hudSourceJumpButton == null) {
            hudSourceJumpButton = hud_main.findViewById(R.id.hud_source_jump_button);
        }
        if (hudSourceVehicleButton == null) {
            hudSourceVehicleButton = hud_main.findViewById(R.id.hud_source_vehicle_button);
        }
        if (hudSourceLockButton == null) {
            hudSourceLockButton = hud_main.findViewById(R.id.hud_source_lock_button);
        }
        if (hudSourceCameraButton == null) {
            hudSourceCameraButton = hud_main.findViewById(R.id.hud_source_camera_button);
        }
        hideLegacyVehicleButtonViews();
        installGameplayTouchPassthroughHandler();
        installSourceControlTouchHandlers();
        if (hudWeaponButton == null) {
            hudWeaponButton = hud_main.findViewById(R.id.btn_weapon_wheel);
        }
        if (hudFpsCounter == null) {
            hudFpsCounter = hud_main.findViewById(R.id.hud_fps_counter);
        }
        if (hudFpsCounterValue == null) {
            hudFpsCounterValue = hud_main.findViewById(R.id.hud_fps_counter_value);
        }
        if (hudTopRightPanel == null) {
            hudTopRightPanel = hud_main.findViewById(R.id.hud_top_right_panel);
        }
        if (hudTopMoneyValue == null) {
            hudTopMoneyValue = hud_main.findViewById(R.id.hud_top_money_value);
        }
        if (hudTopCityName == null) {
            hudTopCityName = hud_main.findViewById(R.id.hud_city_name);
        }
        if (hudTopClockValue == null) {
            hudTopClockValue = hud_main.findViewById(R.id.hud_clock_value);
        }
        if (hudTopCoinValue == null) {
            hudTopCoinValue = hud_main.findViewById(R.id.hud_coin_value);
        }
        if (hudEditLayer == null) {
            hudEditLayer = hud_main.findViewById(R.id.hud_edit_layer);
        }
        if (hudEditDoneButton == null) {
            hudEditDoneButton = hud_main.findViewById(R.id.hud_edit_done);
        }
    }

    private void initializeHudSettingsGearButton() {
        if (hud_main == null) {
            return;
        }
        if (hudSettingsGearButton == null) {
            hudSettingsGearButton = hud_main.findViewById(R.id.btn_hud_settings_gear);
        }
        if (hudSettingsGearButton != null) {
            hudSettingsGearButton.setOnClickListener(null);
            hudSettingsGearButtonVisible = false;
            setVisibilityIfChanged(hudSettingsGearButton, View.GONE);
        }
    }

    private void initializeHudChatAreaMapping() {
        if (hud_main == null) {
            return;
        }
        cacheHudControlViews();
        if (hudChatAreaSaveButton != null) {
            hudChatAreaSaveButton.setOnClickListener(v -> {
                saveHudChatClickAreaFromPreview();
                finishHudChatAreaMapping(true);
            });
        }
        if (hudChatAreaCancelButton != null) {
            hudChatAreaCancelButton.setOnClickListener(v -> finishHudChatAreaMapping(false));
        }
        if (hudChatMapLayer != null) {
            hudChatMapLayer.setOnTouchListener((view, event) -> {
                if (!hudChatAreaMappingMode) {
                    return false;
                }
                int[] layerLocation = new int[2];
                hudChatMapLayer.getLocationOnScreen(layerLocation);
                float x = event.getRawX() - layerLocation[0];
                float y = event.getRawY() - layerLocation[1];
                switch (event.getActionMasked()) {
                    case MotionEvent.ACTION_DOWN:
                        beginHudChatMapGesture(x, y);
                        return true;
                    case MotionEvent.ACTION_MOVE:
                        updateHudChatMapGesture(x, y);
                        return true;
                    case MotionEvent.ACTION_UP:
                    case MotionEvent.ACTION_CANCEL:
                        updateHudChatMapGesture(x, y);
                        saveHudChatClickAreaFromPreview();
                        hudChatMapAction = HUD_CHAT_MAP_ACTION_NONE;
                        return true;
                    default:
                        return true;
                }
            });
        }
    }

    private void beginHudChatMapGesture(float x, float y) {
        hudChatMapStartX = x;
        hudChatMapStartY = y;
        hudChatMapStartLeft = getHudChatAreaPreviewLeft();
        hudChatMapStartTop = getHudChatAreaPreviewTop();
        hudChatMapStartWidth = getHudChatAreaPreviewWidth();
        hudChatMapStartHeight = getHudChatAreaPreviewHeight();

        if (isInsideHudChatAreaResizeHandle(x, y)) {
            hudChatMapAction = HUD_CHAT_MAP_ACTION_RESIZE;
            return;
        }
        if (isInsideHudChatAreaPreview(x, y)) {
            hudChatMapAction = HUD_CHAT_MAP_ACTION_MOVE;
            return;
        }

        hudChatMapAction = HUD_CHAT_MAP_ACTION_NEW;
        updateHudChatAreaPreview(hudChatMapStartX, hudChatMapStartY, x, y);
    }

    private void updateHudChatMapGesture(float x, float y) {
        if (hudChatMapAction == HUD_CHAT_MAP_ACTION_MOVE) {
            int left = hudChatMapStartLeft + Math.round(x - hudChatMapStartX);
            int top = hudChatMapStartTop + Math.round(y - hudChatMapStartY);
            setHudChatAreaPreviewBounds(left, top, hudChatMapStartWidth, hudChatMapStartHeight);
            return;
        }
        if (hudChatMapAction == HUD_CHAT_MAP_ACTION_RESIZE) {
            int width = hudChatMapStartWidth + Math.round(x - hudChatMapStartX);
            int height = hudChatMapStartHeight + Math.round(y - hudChatMapStartY);
            setHudChatAreaPreviewBounds(hudChatMapStartLeft, hudChatMapStartTop, width, height);
            return;
        }
        if (hudChatMapAction == HUD_CHAT_MAP_ACTION_NEW) {
            updateHudChatAreaPreview(hudChatMapStartX, hudChatMapStartY, x, y);
        }
    }

    private boolean isInsideHudChatAreaPreview(float x, float y) {
        int left = getHudChatAreaPreviewLeft();
        int top = getHudChatAreaPreviewTop();
        return x >= left
                && x <= left + getHudChatAreaPreviewWidth()
                && y >= top
                && y <= top + getHudChatAreaPreviewHeight();
    }

    private boolean isInsideHudChatAreaResizeHandle(float x, float y) {
        int left = getHudChatAreaPreviewLeft();
        int top = getHudChatAreaPreviewTop();
        int width = getHudChatAreaPreviewWidth();
        int height = getHudChatAreaPreviewHeight();
        int handle = dpToPx(34.0f);
        return x >= left + width - handle
                && x <= left + width + handle
                && y >= top + height - handle
                && y <= top + height + handle;
    }

    private void beginHudChatAreaMapping() {
        if (hud_main == null) {
            return;
        }
        initializeHudChatAreaMapping();
        hideHudSettingsPanel();
        setHudOptionsPanelVisible(false);
        hideHudStatusPanel();
        hideHudChatPanel();
        hudChatAreaMappingMode = true;
        if (hudChatMapLayer != null) {
            setVisibilityIfChanged(hudChatMapLayer, View.VISIBLE);
            hudChatMapLayer.bringToFront();
        }
        showCurrentHudChatAreaPreview();
        updateHudChatClickAreaVisibility();
        Toast.makeText(this, "Arraste na tela para marcar a area do chat.", Toast.LENGTH_LONG).show();
        hideSystemUI();
    }

    private void finishHudChatAreaMapping(boolean saved) {
        hudChatAreaMappingMode = false;
        hudChatMapAction = HUD_CHAT_MAP_ACTION_NONE;
        if (hudChatMapLayer != null) {
            setVisibilityIfChanged(hudChatMapLayer, View.GONE);
        }
        if (saved) {
            Toast.makeText(this, "Area e posicao do chat salvas.", Toast.LENGTH_SHORT).show();
        } else {
            applySavedHudChatArea();
        }
        updateHudChatClickAreaVisibility();
        hideSystemUI();
    }

    private void showCurrentHudChatAreaPreview() {
        cacheHudControlViews();
        if (hudChatAreaPreview == null || hudChatMapLayer == null) {
            return;
        }
        if (hudChatMapLayer.getWidth() <= 0 || hudChatMapLayer.getHeight() <= 0) {
            hudChatMapLayer.post(this::showCurrentHudChatAreaPreview);
            return;
        }
        SharedPreferences preferences = getSharedPreferences(HUD_LAYOUT_PREFS, MODE_PRIVATE);
        int left = preferences.getInt("chat_area_left", dpToPx(HUD_CHAT_AREA_DEFAULT_LEFT_DP));
        int top = preferences.getInt("chat_area_top", dpToPx(HUD_CHAT_AREA_DEFAULT_TOP_DP));
        int width = preferences.getInt("chat_area_width", dpToPx(HUD_CHAT_AREA_DEFAULT_WIDTH_DP));
        int height = preferences.getInt("chat_area_height", dpToPx(HUD_CHAT_AREA_DEFAULT_HEIGHT_DP));
        setHudChatAreaPreviewBounds(left, top, width, height);
        setVisibilityIfChanged(hudChatAreaPreview, View.VISIBLE);
    }

    private void updateHudChatAreaPreview(float startX, float startY, float endX, float endY) {
        int minWidth = dpToPx(HUD_CHAT_AREA_MIN_WIDTH_DP);
        int minHeight = dpToPx(HUD_CHAT_AREA_MIN_HEIGHT_DP);
        int left = Math.round(Math.min(startX, endX));
        int top = Math.round(Math.min(startY, endY));
        int width = Math.max(Math.round(Math.abs(endX - startX)), minWidth);
        int height = Math.max(Math.round(Math.abs(endY - startY)), minHeight);
        setHudChatAreaPreviewBounds(left, top, width, height);
    }

    private void setHudChatAreaPreviewBounds(int left, int top, int width, int height) {
        if (hudChatAreaPreview == null) {
            return;
        }
        int safeWidth = Math.max(width, dpToPx(HUD_CHAT_AREA_MIN_WIDTH_DP));
        int safeHeight = Math.max(height, dpToPx(HUD_CHAT_AREA_MIN_HEIGHT_DP));
        if (hudChatMapLayer != null && hudChatMapLayer.getWidth() > 0 && hudChatMapLayer.getHeight() > 0) {
            safeWidth = Math.min(safeWidth, hudChatMapLayer.getWidth());
            safeHeight = Math.min(safeHeight, hudChatMapLayer.getHeight());
            left = clampInt(left, 0, Math.max(0, hudChatMapLayer.getWidth() - safeWidth));
            top = clampInt(top, 0, Math.max(0, hudChatMapLayer.getHeight() - safeHeight));
        }
        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                safeWidth,
                safeHeight
        );
        params.leftMargin = Math.max(0, left);
        params.topMargin = Math.max(0, top);
        hudChatAreaPreview.setLayoutParams(params);
        if (hudChatAreaMappingMode) {
            applyNativeChatAreaFromHudArea(params.leftMargin, params.topMargin, safeWidth, safeHeight);
        }
    }

    private int getHudChatAreaPreviewLeft() {
        ViewGroup.LayoutParams rawParams = hudChatAreaPreview != null ? hudChatAreaPreview.getLayoutParams() : null;
        return rawParams instanceof FrameLayout.LayoutParams
                ? ((FrameLayout.LayoutParams) rawParams).leftMargin
                : 0;
    }

    private int getHudChatAreaPreviewTop() {
        ViewGroup.LayoutParams rawParams = hudChatAreaPreview != null ? hudChatAreaPreview.getLayoutParams() : null;
        return rawParams instanceof FrameLayout.LayoutParams
                ? ((FrameLayout.LayoutParams) rawParams).topMargin
                : 0;
    }

    private int getHudChatAreaPreviewWidth() {
        if (hudChatAreaPreview == null) {
            return dpToPx(HUD_CHAT_AREA_DEFAULT_WIDTH_DP);
        }
        int width = hudChatAreaPreview.getWidth();
        if (width <= 0) {
            width = hudChatAreaPreview.getLayoutParams() != null
                    ? hudChatAreaPreview.getLayoutParams().width
                    : dpToPx(HUD_CHAT_AREA_DEFAULT_WIDTH_DP);
        }
        return Math.max(width, dpToPx(HUD_CHAT_AREA_MIN_WIDTH_DP));
    }

    private int getHudChatAreaPreviewHeight() {
        if (hudChatAreaPreview == null) {
            return dpToPx(HUD_CHAT_AREA_DEFAULT_HEIGHT_DP);
        }
        int height = hudChatAreaPreview.getHeight();
        if (height <= 0) {
            height = hudChatAreaPreview.getLayoutParams() != null
                    ? hudChatAreaPreview.getLayoutParams().height
                    : dpToPx(HUD_CHAT_AREA_DEFAULT_HEIGHT_DP);
        }
        return Math.max(height, dpToPx(HUD_CHAT_AREA_MIN_HEIGHT_DP));
    }

    private void saveHudChatClickAreaFromPreview() {
        if (hudChatAreaPreview == null) {
            return;
        }
        saveHudChatClickArea(
                getHudChatAreaPreviewLeft(),
                getHudChatAreaPreviewTop(),
                getHudChatAreaPreviewWidth(),
                getHudChatAreaPreviewHeight()
        );
    }

    private void saveHudChatClickAreaFromDrag(float startX, float startY, float endX, float endY) {
        int minWidth = dpToPx(HUD_CHAT_AREA_MIN_WIDTH_DP);
        int minHeight = dpToPx(HUD_CHAT_AREA_MIN_HEIGHT_DP);
        int left = Math.round(Math.min(startX, endX));
        int top = Math.round(Math.min(startY, endY));
        int width = Math.max(Math.round(Math.abs(endX - startX)), minWidth);
        int height = Math.max(Math.round(Math.abs(endY - startY)), minHeight);
        saveHudChatClickArea(left, top, width, height);
    }

    private void saveHudChatClickArea(int left, int top, int width, int height) {
        getSharedPreferences(HUD_LAYOUT_PREFS, MODE_PRIVATE)
                .edit()
                .putBoolean("chat_area_custom", true)
                .putInt("chat_area_left", Math.max(0, left))
                .putInt("chat_area_top", Math.max(0, top))
                .putInt("chat_area_width", Math.max(width, dpToPx(HUD_CHAT_AREA_MIN_WIDTH_DP)))
                .putInt("chat_area_height", Math.max(height, dpToPx(HUD_CHAT_AREA_MIN_HEIGHT_DP)))
                .putFloat("chat_x", 0.0f)
                .putFloat("chat_y", 0.0f)
                .apply();
        if (!hudNativeChatVisible) {
            hudNativeChatVisible = true;
            saveHudQuickSettings();
            showNativeSampChatOnly();
        }
        if (hudChatButton != null) {
            hudChatButton.setTranslationX(0.0f);
            hudChatButton.setTranslationY(0.0f);
        }
        applySavedHudChatArea();
    }

    private void resetHudChatClickArea() {
        getSharedPreferences(HUD_LAYOUT_PREFS, MODE_PRIVATE)
                .edit()
                .remove("chat_area_custom")
                .remove("chat_area_left")
                .remove("chat_area_top")
                .remove("chat_area_width")
                .remove("chat_area_height")
                .putFloat("chat_x", 0.0f)
                .putFloat("chat_y", 0.0f)
                .apply();
        if (hudChatButton != null) {
            hudChatButton.setTranslationX(0.0f);
            hudChatButton.setTranslationY(0.0f);
        }
        applySavedHudChatArea();
        Toast.makeText(this, "Area do chat voltou ao padrao.", Toast.LENGTH_SHORT).show();
    }

    private void applySavedHudChatArea() {
        cacheHudControlViews();
        if (hud_main == null || hudChatButton == null) {
            return;
        }
        ensureHudChatClickArea();
        SharedPreferences preferences = getSharedPreferences(HUD_LAYOUT_PREFS, MODE_PRIVATE);
        boolean customArea = preferences.getBoolean("chat_area_custom", false);
        int left = preferences.getInt("chat_area_left", dpToPx(HUD_CHAT_AREA_DEFAULT_LEFT_DP));
        int top = preferences.getInt("chat_area_top", dpToPx(HUD_CHAT_AREA_DEFAULT_TOP_DP));
        int width = preferences.getInt("chat_area_width", dpToPx(HUD_CHAT_AREA_DEFAULT_WIDTH_DP));
        int height = preferences.getInt("chat_area_height", dpToPx(HUD_CHAT_AREA_DEFAULT_HEIGHT_DP));

        View parent = hud_main;
        if (parent != null && parent.getWidth() > 0 && parent.getHeight() > 0) {
            width = Math.min(width, parent.getWidth());
            height = Math.min(height, parent.getHeight());
            left = clampInt(left, 0, Math.max(0, parent.getWidth() - width));
            top = clampInt(top, 0, Math.max(0, parent.getHeight() - height));
        }
        int safeWidth = Math.max(width, dpToPx(HUD_CHAT_AREA_MIN_WIDTH_DP));
        int safeHeight = Math.max(height, dpToPx(HUD_CHAT_AREA_MIN_HEIGHT_DP));

        applyHudChatClickAreaBounds(left, top, safeWidth, safeHeight);
        applyHudChatButtonDefaultBounds();

        hudChatButton.setTranslationX(0.0f);
        hudChatButton.setTranslationY(0.0f);
        applyHudChatPanelArea(left, top, safeWidth, safeHeight);
        if (customArea) {
            applyNativeChatAreaFromHudArea(left, top, safeWidth, safeHeight);
        } else {
            clearNativeChatAreaOverride();
        }
        updateHudChatClickAreaVisibility();
    }

    private void ensureHudChatClickArea() {
        if (hudChatClickArea != null || hud_main == null) {
            return;
        }
        hudChatClickArea = new View(this);
        hudChatClickArea.setBackgroundColor(Color.TRANSPARENT);
        hudChatClickArea.setClickable(false);
        hudChatClickArea.setFocusable(false);
        hudChatClickArea.setContentDescription("Abrir chat");
        hudChatClickArea.setOnClickListener(null);
        ConstraintLayout.LayoutParams params = new ConstraintLayout.LayoutParams(
                dpToPx(HUD_CHAT_AREA_DEFAULT_WIDTH_DP),
                dpToPx(HUD_CHAT_AREA_DEFAULT_HEIGHT_DP)
        );
        params.startToStart = ConstraintLayout.LayoutParams.PARENT_ID;
        params.topToTop = ConstraintLayout.LayoutParams.PARENT_ID;
        params.leftMargin = dpToPx(HUD_CHAT_AREA_DEFAULT_LEFT_DP);
        params.topMargin = dpToPx(HUD_CHAT_AREA_DEFAULT_TOP_DP);
        hud_main.addView(hudChatClickArea, params);
        setVisibilityIfChanged(hudChatClickArea, View.GONE);
    }

    private void applyHudChatClickAreaBounds(int left, int top, int width, int height) {
        ensureHudChatClickArea();
        if (hudChatClickArea == null) {
            return;
        }
        ViewGroup.LayoutParams rawParams = hudChatClickArea.getLayoutParams();
        if (rawParams instanceof ConstraintLayout.LayoutParams) {
            ConstraintLayout.LayoutParams params = (ConstraintLayout.LayoutParams) rawParams;
            params.width = Math.max(width, dpToPx(HUD_CHAT_AREA_MIN_WIDTH_DP));
            params.height = Math.max(height, dpToPx(HUD_CHAT_AREA_MIN_HEIGHT_DP));
            params.leftMargin = Math.max(0, left);
            params.topMargin = Math.max(0, top);
            params.startToStart = ConstraintLayout.LayoutParams.PARENT_ID;
            params.topToTop = ConstraintLayout.LayoutParams.PARENT_ID;
            hudChatClickArea.setLayoutParams(params);
        }
        hudChatClickArea.setTranslationX(0.0f);
        hudChatClickArea.setTranslationY(0.0f);
    }

    private void applyHudChatButtonDefaultBounds() {
        if (hudChatButton == null) {
            return;
        }
        ViewGroup.LayoutParams rawParams = hudChatButton.getLayoutParams();
        if (rawParams instanceof ConstraintLayout.LayoutParams) {
            ConstraintLayout.LayoutParams params = (ConstraintLayout.LayoutParams) rawParams;
            params.width = dpToPx(HUD_CHAT_AREA_DEFAULT_WIDTH_DP);
            params.height = dpToPx(HUD_CHAT_AREA_DEFAULT_HEIGHT_DP);
            params.leftMargin = dpToPx(HUD_CHAT_AREA_DEFAULT_LEFT_DP);
            params.topMargin = dpToPx(HUD_CHAT_AREA_DEFAULT_TOP_DP);
            params.startToStart = ConstraintLayout.LayoutParams.PARENT_ID;
            params.topToTop = ConstraintLayout.LayoutParams.PARENT_ID;
            hudChatButton.setLayoutParams(params);
        }
    }

    private void updateHudChatClickAreaVisibility() {
        if (hudChatClickArea == null) {
            return;
        }
        setVisibilityIfChanged(hudChatClickArea, View.GONE);
        if (hudChatMapLayer != null && hudChatAreaMappingMode) {
            hudChatMapLayer.bringToFront();
        }
    }

    private void applyHudChatPanelArea(int left, int top, int width, int height) {
        if (hudChatPanel == null) {
            initializeHudChatPanel();
        }
        if (hudChatPanel == null) {
            return;
        }
        View parent = (View) hudChatPanel.getParent();
        int safeWidth = Math.max(width, dpToPx(HUD_CHAT_AREA_MIN_WIDTH_DP));
        int safeHeight = Math.max(height, dpToPx(HUD_CHAT_AREA_MIN_HEIGHT_DP));
        if (parent != null && parent.getWidth() > 0 && parent.getHeight() > 0) {
            left = clampInt(left, 0, Math.max(0, parent.getWidth() - dpToPx(HUD_CHAT_AREA_MIN_WIDTH_DP)));
            top = clampInt(top, 0, Math.max(0, parent.getHeight() - dpToPx(HUD_CHAT_AREA_MIN_HEIGHT_DP)));
            safeWidth = Math.min(safeWidth, Math.max(dpToPx(HUD_CHAT_AREA_MIN_WIDTH_DP), parent.getWidth() - left));
            safeHeight = Math.min(safeHeight, Math.max(dpToPx(HUD_CHAT_AREA_MIN_HEIGHT_DP), parent.getHeight() - top));
        }

        ViewGroup.LayoutParams rawParams = hudChatPanel.getLayoutParams();
        if (rawParams instanceof ConstraintLayout.LayoutParams) {
            ConstraintLayout.LayoutParams params = (ConstraintLayout.LayoutParams) rawParams;
            params.width = safeWidth;
            params.height = safeHeight;
            params.leftMargin = Math.max(0, left);
            params.topMargin = Math.max(0, top);
            params.startToStart = ConstraintLayout.LayoutParams.PARENT_ID;
            params.topToTop = ConstraintLayout.LayoutParams.PARENT_ID;
            hudChatPanel.setLayoutParams(params);
        }
    }

    private void applyNativeChatAreaFromHudArea(int left, int top, int width, int height) {
        View parent = hud_main;
        if (parent == null || parent.getWidth() <= 0 || parent.getHeight() <= 0) {
            return;
        }
        try {
            applyNativeChatArea(
                    Math.max(0, left),
                    Math.max(0, top),
                    Math.max(width, dpToPx(HUD_CHAT_AREA_MIN_WIDTH_DP)),
                    Math.max(height, dpToPx(HUD_CHAT_AREA_MIN_HEIGHT_DP)),
                    parent.getWidth(),
                    parent.getHeight()
            );
            applyNativeMenuSettings(
                    hudAndroidKeyboardEnabled,
                    getHudChatMaxMessagesForArea(height),
                    hudVoiceChatEnabled
            );
        } catch (UnsatisfiedLinkError error) {
            Log.w(TAG, "Native chat area runtime settings unavailable.", error);
        }
    }

    private void clearNativeChatAreaOverride() {
        View parent = hud_main;
        if (parent == null || parent.getWidth() <= 0 || parent.getHeight() <= 0) {
            return;
        }
        try {
            applyNativeChatArea(0, 0, 0, 0, parent.getWidth(), parent.getHeight());
            applyNativeMenuSettings(hudAndroidKeyboardEnabled, hudChatMaxMessages, hudVoiceChatEnabled);
        } catch (UnsatisfiedLinkError error) {
            Log.w(TAG, "Native chat area runtime settings unavailable.", error);
        }
    }

    private int getHudChatMaxMessagesForArea(int height) {
        int lineHeight = Math.max(dpToPx(13.0f), 1);
        int areaLines = Math.max(1, Math.round((float) height / (float) lineHeight));
        return clampInt(areaLines, 1, 30);
    }

    private int getRoleplayChatMaxCharsForCurrentArea() {
        int width = dpToPx(HUD_CHAT_AREA_DEFAULT_WIDTH_DP);
        if (hudChatClickArea != null && hudChatClickArea.getWidth() > 0) {
            width = hudChatClickArea.getWidth();
        } else {
            SharedPreferences preferences = getSharedPreferences(HUD_LAYOUT_PREFS, MODE_PRIVATE);
            width = preferences.getInt("chat_area_width", width);
        }
        int averageCharWidth = Math.max(dpToPx(4.9f), 1);
        int dynamicLimit = Math.max(24, width / averageCharWidth);
        return clampInt(dynamicLimit, 24, ROLEPLAY_CHAT_MAX_CHARS);
    }

    private void noteHudControlInteraction() {
        if (Looper.myLooper() == Looper.getMainLooper()) {
            noteHudControlInteractionInternal();
        } else {
            uiHandler.post(this::noteHudControlInteractionInternal);
        }
    }

    private void noteHudControlInteractionInternal() {
        applyOfficialHudControlStyle();
    }

    private void scheduleHudControlsDocking() {
        applyOfficialHudControlStyle();
    }

    private void cancelHudControlsDocking() {
        applyOfficialHudControlStyle();
    }

    private void setHudControlsDocked(boolean docked) {
        applyOfficialHudControlStyle();
    }

    private void applyOfficialHudControlStyle() {
        if (hud_main == null) {
            hudControlsDocked = false;
            return;
        }
        cacheHudControlViews();
        hudControlsDocked = false;

        float alpha = getHudControlsAlpha();
        if (hudEditMode) {
            setHudControlAlpha(hudMenuButton, 0.86f);
            setHudControlAlpha(hudInteractionButton, 0.86f);
            setHudViewVisibility(R.id.btn_2, false);
            setVisibilityIfChanged(hudOptionsButton, View.GONE);
            setHudControlAlpha(hudChatButton, 1.0f);
            setHudControlAlpha(hudHdMapHotspot, 0.86f);
            setSourceHudControlsAlpha(0.86f);
            setVisibilityIfChanged(hudWeaponButton, View.GONE);
            setHudControlAlpha(hudFpsCounter, 0.86f);
            setHudControlAlpha(hudTopRightPanel, 0.92f);
            resetFixedHudControl(hud_main.findViewById(R.id.WeaponShowLayout), 0.92f);
            setHudControlAlpha(hud_main.findViewById(R.id.hud_left_panel), 0.92f);
            setHudControlAlpha(hud_main.findViewById(R.id.hud_needs_panel), 0.92f);
            return;
        }

        animateHudControl(hudMenuButton, getSavedHudTranslation("menu", true), getSavedHudTranslation("menu", false), alpha);
        animateHudControl(hudInteractionButton, getSavedHudTranslation("radial", true), getSavedHudTranslation("radial", false), alpha);
        setHudViewVisibility(R.id.btn_2, false);
        setVisibilityIfChanged(hudOptionsButton, View.GONE);
        animateHudControl(hudChatButton, getSavedHudTranslation("chat", true), getSavedHudTranslation("chat", false), 1.0f);
        clearSavedHudControlTranslation("minimap_hotspot");
        resetFixedHudControl(hudHdMapHotspot, alpha);
        animateSourceHudControls(alpha);
        setVisibilityIfChanged(hudWeaponButton, View.GONE);
        animateHudControl(hudFpsCounter, getSavedHudTranslation("fps", true), getSavedHudTranslation("fps", false), alpha);
        resetFixedHudControl(hudTopRightPanel, 1.0f);
        resetFixedHudControl(hud_main.findViewById(R.id.WeaponShowLayout), 1.0f);
        animateHudControl(hud_main.findViewById(R.id.hud_left_panel), getSavedHudTranslation("classic_status", true), getSavedHudTranslation("classic_status", false), 1.0f);
        animateHudControl(hud_main.findViewById(R.id.hud_needs_panel), getSavedHudTranslation("needs", true), getSavedHudTranslation("needs", false), 1.0f);
    }

    private void resetFixedHudControl(View view, float alpha) {
        if (view != null) {
            view.animate().cancel();
            view.setTranslationX(0.0f);
            view.setTranslationY(0.0f);
            view.setAlpha(alpha);
        }
    }

    private void setHudControlAlpha(View view, float alpha) {
        if (view != null) {
            view.animate().cancel();
            view.setAlpha(alpha);
        }
    }

    private float getHudControlsAlpha() {
        return clampInt(hudControlAlphaPercent, 45, 100) / 100.0f;
    }

    private boolean isNativeLocalPlayerInVehicleSafe() {
        try {
            if (!isNativeGameConnected()) {
                return false;
            }
            return isNativeLocalPlayerInVehicle();
        } catch (UnsatisfiedLinkError error) {
            return false;
        }
    }

    private int getSourceVehicleButtonVisibility(boolean requestedVisible, boolean inVehicle) {
        return (hudEditMode || inVehicle || requestedVisible) ? View.VISIBLE : View.INVISIBLE;
    }

    private void applySourceVehicleButtonsVisibility(boolean bringVehicleToFront, boolean bringLockToFront) {
        if (hud_main == null) {
            return;
        }
        cacheHudControlViews();
        hideLegacyVehicleButtonViews();
        boolean inVehicle = isNativeLocalPlayerInVehicleSafe();
        setVisibilityIfChanged(hudSourceVehicleButton, getSourceVehicleButtonVisibility(hudSourceVehicleVisible, inVehicle));
        setVisibilityIfChanged(hudSourceLockButton, getSourceVehicleButtonVisibility(hudSourceLockVisible, inVehicle));
        if (bringVehicleToFront && hudSourceVehicleButton != null && hudSourceVehicleButton.getVisibility() == View.VISIBLE) {
            hudSourceVehicleButton.bringToFront();
        }
        if (bringLockToFront && hudSourceLockButton != null && hudSourceLockButton.getVisibility() == View.VISIBLE) {
            hudSourceLockButton.bringToFront();
        }
    }

    private float getSavedHudTranslation(String key, boolean xAxis) {
        return getSharedPreferences(HUD_LAYOUT_PREFS, MODE_PRIVATE)
                .getFloat(key + (xAxis ? "_x" : "_y"), 0.0f);
    }

    private void setSourceHudControlsAlpha(float alpha) {
        setHudControlAlpha(hudSourceAnalog, alpha);
        setHudControlAlpha(hudSourceAttackButton, alpha);
        setHudControlAlpha(hudSourceAccelerateButton, alpha);
        setHudControlAlpha(hudSourceBrakeButton, alpha);
        setHudControlAlpha(hudSourceHandbrakeButton, alpha);
        setHudControlAlpha(hudSourceHornButton, alpha);
        setHudControlAlpha(hudSourceSprintButton, alpha);
        setHudControlAlpha(hudSourceJumpButton, alpha);
        setHudControlAlpha(hudSourceVehicleButton, alpha);
        setHudControlAlpha(hudSourceLockButton, alpha);
        setHudControlAlpha(hudSourceCameraButton, alpha);
    }

    private void setSourceHudControlsVisibility(int visibility) {
        if (visibility == View.GONE) {
            setVisibilityIfChanged(hudSourceAnalog, View.GONE);
            setVisibilityIfChanged(hudSourceAttackButton, View.GONE);
            setVisibilityIfChanged(hudSourceAccelerateButton, View.GONE);
            setVisibilityIfChanged(hudSourceBrakeButton, View.GONE);
            setVisibilityIfChanged(hudSourceHandbrakeButton, View.GONE);
            setVisibilityIfChanged(hudSourceHornButton, View.GONE);
            setVisibilityIfChanged(hudSourceSprintButton, View.GONE);
            setVisibilityIfChanged(hudSourceJumpButton, View.GONE);
            setVisibilityIfChanged(hudSourceCameraButton, View.GONE);
            setVisibilityIfChanged(hudSourceVehicleButton, View.GONE);
            setVisibilityIfChanged(hudSourceLockButton, View.GONE);
            return;
        }

        if (hudEditMode) {
            setVisibilityIfChanged(hudSourceAnalog, View.VISIBLE);
            setVisibilityIfChanged(hudSourceAttackButton, View.VISIBLE);
            setVisibilityIfChanged(hudSourceAccelerateButton, View.VISIBLE);
            setVisibilityIfChanged(hudSourceBrakeButton, View.VISIBLE);
            setVisibilityIfChanged(hudSourceHandbrakeButton, View.VISIBLE);
            setVisibilityIfChanged(hudSourceHornButton, View.VISIBLE);
            setVisibilityIfChanged(hudSourceSprintButton, View.VISIBLE);
            setVisibilityIfChanged(hudSourceJumpButton, View.VISIBLE);
            setVisibilityIfChanged(hudSourceCameraButton, View.VISIBLE);
            setVisibilityIfChanged(hudSourceVehicleButton, View.VISIBLE);
            setVisibilityIfChanged(hudSourceLockButton, View.VISIBLE);
            return;
        }

        boolean inVehicle = isNativeLocalPlayerInVehicleSafe();
        int onFootVisibility = inVehicle ? View.INVISIBLE : View.VISIBLE;
        int vehicleVisibility = inVehicle ? View.VISIBLE : View.INVISIBLE;

        setVisibilityIfChanged(hudSourceAnalog, View.VISIBLE);
        setVisibilityIfChanged(hudSourceAttackButton, View.VISIBLE);
        setVisibilityIfChanged(hudSourceAccelerateButton, vehicleVisibility);
        setVisibilityIfChanged(hudSourceBrakeButton, vehicleVisibility);
        setVisibilityIfChanged(hudSourceHandbrakeButton, vehicleVisibility);
        setVisibilityIfChanged(hudSourceHornButton, vehicleVisibility);
        setVisibilityIfChanged(hudSourceSprintButton, onFootVisibility);
        setVisibilityIfChanged(hudSourceJumpButton, onFootVisibility);
        setVisibilityIfChanged(hudSourceCameraButton, View.VISIBLE);
        setVisibilityIfChanged(hudSourceVehicleButton, getSourceVehicleButtonVisibility(hudSourceVehicleVisible, inVehicle));
        setVisibilityIfChanged(hudSourceLockButton, getSourceVehicleButtonVisibility(hudSourceLockVisible, inVehicle));
    }

    private void refreshSourceHudControlsRuntimeVisibility() {
        if (hud_main == null || hudEditMode || iShowHud != 1 || shouldHideGameplayHudForNativeMenu()) {
            return;
        }
        long now = SystemClock.uptimeMillis();
        if (now - lastSourceHudVehicleRefreshMs < 160) {
            return;
        }
        lastSourceHudVehicleRefreshMs = now;
        setSourceHudControlsVisibility(View.VISIBLE);
    }

    private void animateSourceHudControls(float alpha) {
        animateHudControl(hudSourceAnalog, getSavedHudTranslation("source_analog", true), getSavedHudTranslation("source_analog", false), alpha);
        animateHudControl(hudSourceAttackButton, getSavedHudTranslation("source_attack", true), getSavedHudTranslation("source_attack", false), alpha);
        animateHudControl(hudSourceAccelerateButton, getSavedHudTranslation("source_accelerate", true), getSavedHudTranslation("source_accelerate", false), alpha);
        animateHudControl(hudSourceBrakeButton, getSavedHudTranslation("source_brake", true), getSavedHudTranslation("source_brake", false), alpha);
        animateHudControl(hudSourceHandbrakeButton, getSavedHudTranslation("source_handbrake", true), getSavedHudTranslation("source_handbrake", false), alpha);
        animateHudControl(hudSourceHornButton, getSavedHudTranslation("source_horn", true), getSavedHudTranslation("source_horn", false), alpha);
        animateHudControl(hudSourceSprintButton, getSavedHudTranslation("source_sprint", true), getSavedHudTranslation("source_sprint", false), alpha);
        animateHudControl(hudSourceJumpButton, getSavedHudTranslation("source_jump", true), getSavedHudTranslation("source_jump", false), alpha);
        animateHudControl(hudSourceVehicleButton, getSavedHudTranslation("source_vehicle", true), getSavedHudTranslation("source_vehicle", false), alpha);
        animateHudControl(hudSourceLockButton, getSavedHudTranslation("source_lock", true), getSavedHudTranslation("source_lock", false), alpha);
        animateHudControl(hudSourceCameraButton, getSavedHudTranslation("source_camera", true), getSavedHudTranslation("source_camera", false), alpha);
    }

    private void applySavedSourceHudControlLayout() {
        applySavedHudControlTranslation(hudSourceAnalog, "source_analog");
        applySavedHudControlTranslation(hudSourceAttackButton, "source_attack");
        applySavedHudControlTranslation(hudSourceAccelerateButton, "source_accelerate");
        applySavedHudControlTranslation(hudSourceBrakeButton, "source_brake");
        applySavedHudControlTranslation(hudSourceHandbrakeButton, "source_handbrake");
        applySavedHudControlTranslation(hudSourceHornButton, "source_horn");
        applySavedHudControlTranslation(hudSourceSprintButton, "source_sprint");
        applySavedHudControlTranslation(hudSourceJumpButton, "source_jump");
        applySavedHudControlTranslation(hudSourceVehicleButton, "source_vehicle");
        applySavedHudControlTranslation(hudSourceLockButton, "source_lock");
        applySavedHudControlTranslation(hudSourceCameraButton, "source_camera");
    }

    private void saveSourceHudControlLayout() {
        saveHudControlTranslation("source_analog", hudSourceAnalog);
        saveHudControlTranslation("source_attack", hudSourceAttackButton);
        saveHudControlTranslation("source_accelerate", hudSourceAccelerateButton);
        saveHudControlTranslation("source_brake", hudSourceBrakeButton);
        saveHudControlTranslation("source_handbrake", hudSourceHandbrakeButton);
        saveHudControlTranslation("source_horn", hudSourceHornButton);
        saveHudControlTranslation("source_sprint", hudSourceSprintButton);
        saveHudControlTranslation("source_jump", hudSourceJumpButton);
        saveHudControlTranslation("source_vehicle", hudSourceVehicleButton);
        saveHudControlTranslation("source_lock", hudSourceLockButton);
        saveHudControlTranslation("source_camera", hudSourceCameraButton);
    }

    private void resetSourceHudControlLayout() {
        resetHudControlTranslation(hudSourceAnalog);
        resetHudControlTranslation(hudSourceAttackButton);
        resetHudControlTranslation(hudSourceAccelerateButton);
        resetHudControlTranslation(hudSourceBrakeButton);
        resetHudControlTranslation(hudSourceHandbrakeButton);
        resetHudControlTranslation(hudSourceHornButton);
        resetHudControlTranslation(hudSourceSprintButton);
        resetHudControlTranslation(hudSourceJumpButton);
        resetHudControlTranslation(hudSourceVehicleButton);
        resetHudControlTranslation(hudSourceLockButton);
        resetHudControlTranslation(hudSourceCameraButton);
        resetSourceAnalogVisual();
    }

    private void installHudEditTouchHandlers() {
        cacheHudControlViews();
        installHudEditLayerTouchHandler();
        installHudEditableControl(hudMenuButton, "menu");
        installHudEditableControl(hudInteractionButton, "radial");
        installHudEditableControl(hudOptionsButton, "quick");
        installHudEditableControl(hudChatButton, "chat");
        installHudEditableControl(hudFpsCounter, "fps");
        installHudEditableControl(hud_main.findViewById(R.id.hud_left_panel), "classic_status");
        installHudEditableControl(hud_main.findViewById(R.id.hud_needs_panel), "needs");
        installHudEditableControl(hudSourceAnalog, "source_analog");
        installHudEditableControl(hudSourceAttackButton, "source_attack");
        installHudEditableControl(hudSourceAccelerateButton, "source_accelerate");
        installHudEditableControl(hudSourceBrakeButton, "source_brake");
        installHudEditableControl(hudSourceHandbrakeButton, "source_handbrake");
        installHudEditableControl(hudSourceHornButton, "source_horn");
        installHudEditableControl(hudSourceSprintButton, "source_sprint");
        installHudEditableControl(hudSourceJumpButton, "source_jump");
        installHudEditableControl(hudSourceVehicleButton, "source_vehicle");
        installHudEditableControl(hudSourceLockButton, "source_lock");
        installHudEditableControl(hudSourceCameraButton, "source_camera");
    }

    private void installHudEditLayerTouchHandler() {
        if (hudEditLayer == null) {
            return;
        }
        hudEditLayer.setOnTouchListener((view, event) -> {
            if (!hudEditMode) {
                return false;
            }
            switch (event.getActionMasked()) {
                case MotionEvent.ACTION_DOWN:
                    selectHudEditDragTarget(event.getRawX(), event.getRawY());
                    return true;
                case MotionEvent.ACTION_MOVE:
                    if (HUD_EDIT_KEY_IMGUI_VOIP.equals(hudEditDragKey)) {
                        updateNativeVoiceChatLayoutFromDrag(event.getRawX(), event.getRawY(), false);
                    } else if (hudEditDragTarget != null) {
                        hudEditDragTarget.setTranslationX(
                                hudEditStartTranslationX + event.getRawX() - hudEditDownRawX
                        );
                        hudEditDragTarget.setTranslationY(
                                hudEditStartTranslationY + event.getRawY() - hudEditDownRawY
                        );
                    }
                    return true;
                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_CANCEL:
                    if (HUD_EDIT_KEY_IMGUI_VOIP.equals(hudEditDragKey)) {
                        updateNativeVoiceChatLayoutFromDrag(event.getRawX(), event.getRawY(), true);
                    } else if (hudEditDragTarget != null && hudEditDragKey != null) {
                        saveHudControlTranslation(hudEditDragKey, hudEditDragTarget);
                    }
                    hudEditDragTarget = null;
                    hudEditDragKey = null;
                    return true;
                default:
                    return true;
            }
        });
    }

    private void selectHudEditDragTarget(float rawX, float rawY) {
        hudEditDragTarget = null;
        hudEditDragKey = null;
        boolean selectedView = trySelectHudEditDragTarget(hud_main.findViewById(R.id.hud_left_panel), "classic_status", rawX, rawY)
                || trySelectHudEditDragTarget(hud_main.findViewById(R.id.hud_needs_panel), "needs", rawX, rawY)
                || trySelectHudEditDragTarget(hudFpsCounter, "fps", rawX, rawY)
                || trySelectHudEditDragTarget(hudSourceAnalog, "source_analog", rawX, rawY)
                || trySelectHudEditDragTarget(hudSourceAttackButton, "source_attack", rawX, rawY)
                || trySelectHudEditDragTarget(hudSourceAccelerateButton, "source_accelerate", rawX, rawY)
                || trySelectHudEditDragTarget(hudSourceBrakeButton, "source_brake", rawX, rawY)
                || trySelectHudEditDragTarget(hudSourceHandbrakeButton, "source_handbrake", rawX, rawY)
                || trySelectHudEditDragTarget(hudSourceHornButton, "source_horn", rawX, rawY)
                || trySelectHudEditDragTarget(hudSourceSprintButton, "source_sprint", rawX, rawY)
                || trySelectHudEditDragTarget(hudSourceJumpButton, "source_jump", rawX, rawY)
                || trySelectHudEditDragTarget(hudSourceVehicleButton, "source_vehicle", rawX, rawY)
                || trySelectHudEditDragTarget(hudSourceLockButton, "source_lock", rawX, rawY)
                || trySelectHudEditDragTarget(hudSourceCameraButton, "source_camera", rawX, rawY)
                || trySelectHudEditDragTarget(hudChatButton, "chat", rawX, rawY)
                || trySelectHudEditDragTarget(hudOptionsButton, "quick", rawX, rawY)
                || trySelectHudEditDragTarget(hudInteractionButton, "radial", rawX, rawY)
                || trySelectHudEditDragTarget(hudMenuButton, "menu", rawX, rawY);
        if (selectedView) {
            hudEditDownRawX = rawX;
            hudEditDownRawY = rawY;
            hudEditStartTranslationX = hudEditDragTarget.getTranslationX();
            hudEditStartTranslationY = hudEditDragTarget.getTranslationY();
            return;
        }
        if (trySelectNativeVoiceChatEditTarget(rawX, rawY)) {
            hudEditDownRawX = rawX;
            hudEditDownRawY = rawY;
            hudNativeVoipDragStartRatioX = hudNativeVoipRatioX;
            hudNativeVoipDragStartRatioY = hudNativeVoipRatioY;
        }
    }

    private boolean trySelectHudEditDragTarget(View view, String key, float rawX, float rawY) {
        if (view == null || view.getVisibility() != View.VISIBLE) {
            return false;
        }
        int[] location = new int[2];
        view.getLocationOnScreen(location);
        int padding = getHudEditSelectionPadding(key, view);
        boolean inside = rawX >= location[0] - padding
                && rawX <= location[0] + view.getWidth() + padding
                && rawY >= location[1] - padding
                && rawY <= location[1] + view.getHeight() + padding;
        if (!inside) {
            return false;
        }
        hudEditDragTarget = view;
        hudEditDragKey = key;
        view.animate().cancel();
        view.bringToFront();
        return true;
    }

    private int getHudEditSelectionPadding(String key, View view) {
        if (key != null && key.startsWith("source_")) {
            return dpToPx(4.0f);
        }
        if ("chat".equals(key) || "quick".equals(key) || "radial".equals(key) || "menu".equals(key)) {
            return dpToPx(6.0f);
        }
        int shortestSide = Math.min(view.getWidth(), view.getHeight());
        if (shortestSide <= dpToPx(64.0f)) {
            return dpToPx(5.0f);
        }
        return dpToPx(10.0f);
    }

    private boolean trySelectNativeVoiceChatEditTarget(float rawX, float rawY) {
        if (hud_main == null || !hudVoiceChatEnabled) {
            return false;
        }
        loadNativeVoiceChatLayoutState();
        int[] parentLocation = new int[2];
        hud_main.getLocationOnScreen(parentLocation);
        float buttonWidth = getNativeVoiceButtonWidth();
        float buttonHeight = getNativeVoiceButtonHeight();
        float maxX = Math.max(1.0f, hud_main.getWidth() - buttonWidth);
        float maxY = Math.max(1.0f, hud_main.getHeight() - buttonHeight);
        float left = parentLocation[0] + hudNativeVoipRatioX * maxX;
        float top = parentLocation[1] + hudNativeVoipRatioY * maxY;
        float padding = dpToPx(18.0f);
        boolean inside = rawX >= left - padding
                && rawX <= left + buttonWidth + padding
                && rawY >= top - padding
                && rawY <= top + buttonHeight + padding;
        if (!inside) {
            return false;
        }
        hudEditDragKey = HUD_EDIT_KEY_IMGUI_VOIP;
        return true;
    }

    private void loadNativeVoiceChatLayoutState() {
        int parentWidth = hud_main != null && hud_main.getWidth() > 0 ? hud_main.getWidth() : getResources().getDisplayMetrics().widthPixels;
        int parentHeight = hud_main != null && hud_main.getHeight() > 0 ? hud_main.getHeight() : getResources().getDisplayMetrics().heightPixels;
        if (hudNativeVoipLayoutLoaded
                && hudNativeVoipLayoutParentWidth == parentWidth
                && hudNativeVoipLayoutParentHeight == parentHeight) {
            return;
        }
        float settingX = HUD_NATIVE_VOICE_CHAT_DEFAULT_POS_X;
        float settingY = HUD_NATIVE_VOICE_CHAT_DEFAULT_POS_Y;
        float settingSize = HUD_NATIVE_VOICE_CHAT_DEFAULT_SIZE;
        File settingsFile = new File(getExternalFilesDir(null), "SAMP/settings.ini");
        if (settingsFile.exists()) {
            try {
                Wini wini = new Wini(settingsFile);
                settingX = parseHudFloat(wini.get("gui", "VoiceChatPosX"), settingX);
                settingY = parseHudFloat(wini.get("gui", "VoiceChatPosY"), settingY);
                settingSize = parseHudFloat(wini.get("gui", "VoiceChatSize"), settingSize);
            } catch (IOException error) {
                Log.e(TAG, "Could not read native VOIP layout.", error);
            }
        }
        hudNativeVoipSizeScale = clampFloat(settingSize / HUD_NATIVE_VOICE_CHAT_DEFAULT_SIZE, 0.65f, 1.85f);
        hudNativeVoipLayoutParentWidth = parentWidth;
        hudNativeVoipLayoutParentHeight = parentHeight;
        hudNativeVoipRatioX = resolveNativeVoicePositionRatio(
                settingX,
                Math.max(1.0f, parentWidth - getNativeVoiceButtonWidth()),
                Math.max(1.0f, parentWidth),
                HUD_NATIVE_VOICE_CHAT_DESIGN_WIDTH
        );
        hudNativeVoipRatioY = resolveNativeVoicePositionRatio(
                settingY,
                Math.max(1.0f, parentHeight - getNativeVoiceButtonHeight()),
                Math.max(1.0f, parentHeight),
                HUD_NATIVE_VOICE_CHAT_DESIGN_HEIGHT
        );
        hudNativeVoipLayoutLoaded = true;
        applyNativeVoiceChatLayoutFromState(false);
    }

    private float parseHudFloat(String value, float fallback) {
        if (value == null) {
            return fallback;
        }
        try {
            return Float.parseFloat(value.trim());
        } catch (NumberFormatException error) {
            return fallback;
        }
    }

    private float resolveNativeVoicePositionRatio(float setting, float maxPosition, float displaySize, float designSize) {
        if (setting >= 0.0f && setting <= 1.0f) {
            return clampFloat(setting, 0.0f, 1.0f);
        }
        float pixelPosition = setting * (displaySize / Math.max(1.0f, designSize));
        return clampFloat(pixelPosition / Math.max(1.0f, maxPosition), 0.0f, 1.0f);
    }

    private float getNativeVoiceButtonWidth() {
        return HUD_NATIVE_VOICE_CHAT_IMGUI_WIDTH * hudNativeVoipSizeScale;
    }

    private float getNativeVoiceButtonHeight() {
        return HUD_NATIVE_VOICE_CHAT_IMGUI_HEIGHT * hudNativeVoipSizeScale;
    }

    private void updateNativeVoiceChatLayoutFromDrag(float rawX, float rawY, boolean persist) {
        if (hud_main == null) {
            return;
        }
        loadNativeVoiceChatLayoutState();
        float maxX = Math.max(1.0f, hud_main.getWidth() - getNativeVoiceButtonWidth());
        float maxY = Math.max(1.0f, hud_main.getHeight() - getNativeVoiceButtonHeight());
        hudNativeVoipRatioX = clampFloat(hudNativeVoipDragStartRatioX + (rawX - hudEditDownRawX) / maxX, 0.0f, 1.0f);
        hudNativeVoipRatioY = clampFloat(hudNativeVoipDragStartRatioY + (rawY - hudEditDownRawY) / maxY, 0.0f, 1.0f);
        applyNativeVoiceChatLayoutFromState(persist);
    }

    private void applyNativeVoiceChatLayoutFromState(boolean persist) {
        try {
            applyNativeVoiceChatLayout(hudNativeVoipRatioX, hudNativeVoipRatioY, hudNativeVoipSizeScale);
        } catch (UnsatisfiedLinkError error) {
            Log.w(TAG, "Native VOIP layout update unavailable.", error);
        }
        if (persist) {
            saveNativeVoiceChatLayoutSettings(hudNativeVoipRatioX, hudNativeVoipRatioY, hudNativeVoipSizeScale);
        }
    }

    private void installHudEditableControl(View view, String key) {
        if (view == null) {
            return;
        }
        view.setOnTouchListener(new View.OnTouchListener() {
            private float downRawX;
            private float downRawY;
            private float startTranslationX;
            private float startTranslationY;

            @Override
            public boolean onTouch(View target, MotionEvent event) {
                if (!hudEditMode) {
                    return false;
                }
                switch (event.getActionMasked()) {
                    case MotionEvent.ACTION_DOWN:
                        target.animate().cancel();
                        target.bringToFront();
                        downRawX = event.getRawX();
                        downRawY = event.getRawY();
                        startTranslationX = target.getTranslationX();
                        startTranslationY = target.getTranslationY();
                        return true;
                    case MotionEvent.ACTION_MOVE:
                        target.setTranslationX(startTranslationX + event.getRawX() - downRawX);
                        target.setTranslationY(startTranslationY + event.getRawY() - downRawY);
                        return true;
                    case MotionEvent.ACTION_UP:
                    case MotionEvent.ACTION_CANCEL:
                        saveHudControlTranslation(key, target);
                        return true;
                    default:
                        return true;
                }
            }
        });
    }

    private void saveHudControlTranslation(String key, View view) {
        if (view == null) {
            return;
        }
        getSharedPreferences(HUD_LAYOUT_PREFS, MODE_PRIVATE)
                .edit()
                .putFloat(key + "_x", view.getTranslationX())
                .putFloat(key + "_y", view.getTranslationY())
                .apply();
        Log.i(TAG, "HUD position saved: " + key
                + " x=" + view.getTranslationX()
                + " y=" + view.getTranslationY());
    }

    private void applySavedHudLayout() {
        cacheHudControlViews();
        applySavedHudChatArea();
        applySavedHudControlTranslation(hudMenuButton, "menu");
        applySavedHudControlTranslation(hudInteractionButton, "radial");
        applySavedHudControlTranslation(hudOptionsButton, "quick");
        hudNativeVoipLayoutLoaded = false;
        loadNativeVoiceChatLayoutState();
        applySavedHudControlTranslation(hudChatButton, "chat");
        clearSavedHudControlTranslation("minimap_hotspot");
        resetFixedHudControl(hudHdMapHotspot, 1.0f);
        applySavedHudControlTranslation(hudFpsCounter, "fps");
        applySavedSourceHudControlLayout();
        resetFixedHudControl(hudTopRightPanel, 1.0f);
        clearSavedHudControlTranslation("enter_passenger");
        clearSavedHudControlTranslation("vehicle_lock");
        hideLegacyVehicleButtonViews();
        clearSavedHudControlTranslation("action_2");
        clearSavedHudControlTranslation("hud_y");
        clearSavedHudControlTranslation("hud_f");
        clearSavedHudControlTranslation("classic_weapon");
        resetFixedHudControl(hud_main.findViewById(R.id.WeaponShowLayout), 1.0f);
        applySavedHudControlTranslation(hud_main.findViewById(R.id.hud_left_panel), "classic_status");
        applySavedHudControlTranslation(hud_main.findViewById(R.id.hud_needs_panel), "needs");
    }

    private void clearSavedHudControlTranslation(String key) {
        getSharedPreferences(HUD_LAYOUT_PREFS, MODE_PRIVATE)
                .edit()
                .remove(key + "_x")
                .remove(key + "_y")
                .apply();
    }

    private void applySavedHudControlTranslation(View view, String key) {
        if (view == null) {
            return;
        }
        view.setTranslationX(getSavedHudTranslation(key, true));
        view.setTranslationY(getSavedHudTranslation(key, false));
    }

    private void saveNativeVoiceChatLayoutSettings(float posX, float posY, float sizeScale) {
        File settingsFile = new File(getExternalFilesDir(null), "SAMP/settings.ini");
        File parent = settingsFile.getParentFile();
        if (parent != null && !parent.exists() && !parent.mkdirs()) {
            Log.w(TAG, "Could not create VOIP layout settings directory.");
            return;
        }
        try {
            if (!settingsFile.exists() && !settingsFile.createNewFile()) {
                Log.w(TAG, "Could not create settings.ini for VOIP layout.");
                return;
            }
            Wini wini = new Wini(settingsFile);
            wini.put("gui", "VoiceChatPosX", clampFloat(posX, 0.0f, 1.0f));
            wini.put("gui", "VoiceChatPosY", clampFloat(posY, 0.0f, 1.0f));
            wini.put("gui", "VoiceChatSize", HUD_NATIVE_VOICE_CHAT_DEFAULT_SIZE * clampFloat(sizeScale, 0.65f, 1.85f));
            wini.store();
        } catch (IOException error) {
            Log.e(TAG, "Could not store VOIP layout.", error);
        }
    }

    private void resetNativeVoiceChatLayoutToDefault() {
        try {
            resetNativeVoiceChatLayout();
        } catch (UnsatisfiedLinkError error) {
            Log.w(TAG, "Native VOIP layout reset unavailable.", error);
        }

        File settingsFile = new File(getExternalFilesDir(null), "SAMP/settings.ini");
        File parent = settingsFile.getParentFile();
        if (parent != null && !parent.exists() && !parent.mkdirs()) {
            Log.w(TAG, "Could not create VOIP reset settings directory.");
            return;
        }
        try {
            if (!settingsFile.exists() && !settingsFile.createNewFile()) {
                Log.w(TAG, "Could not create settings.ini for VOIP reset.");
                return;
            }
            Wini wini = new Wini(settingsFile);
            wini.put("gui", "VoiceChatPosX", HUD_NATIVE_VOICE_CHAT_DEFAULT_POS_X);
            wini.put("gui", "VoiceChatPosY", HUD_NATIVE_VOICE_CHAT_DEFAULT_POS_Y);
            wini.put("gui", "VoiceChatSize", HUD_NATIVE_VOICE_CHAT_DEFAULT_SIZE);
            wini.store();
            hudNativeVoipLayoutLoaded = false;
        } catch (IOException error) {
            Log.e(TAG, "Could not reset VOIP layout.", error);
        }
    }

    private void setHudEditMode(boolean enabled) {
        hudEditMode = enabled;
        cacheHudControlViews();
        if (enabled) {
            hideHudSettingsPanel();
            setHudOptionsPanelVisible(false);
            setHudShortcutButtonsVisibleInternal(true);
            setHudEditControlVisibility(true);
            if (hudChatAreaHint != null) {
                setVisibilityIfChanged(hudChatAreaHint, View.VISIBLE);
            }
            if (hudEditLayer != null) {
                setVisibilityIfChanged(hudEditLayer, View.VISIBLE);
                hudEditLayer.bringToFront();
            }
            if (hudEditDoneButton != null) {
                setVisibilityIfChanged(hudEditDoneButton, View.VISIBLE);
                hudEditDoneButton.bringToFront();
            }
            applySavedHudLayout();
            applyOfficialHudControlStyle();
            installHudEditTouchHandlers();
            Toast.makeText(this, "Arraste botoes e HUDs. Toque em Salvar HUD ao terminar.", Toast.LENGTH_LONG).show();
            return;
        }

        saveHudControlTranslation("menu", hudMenuButton);
        saveHudControlTranslation("radial", hudInteractionButton);
        saveHudControlTranslation("quick", hudOptionsButton);
        if (hudNativeVoipLayoutLoaded) {
            saveNativeVoiceChatLayoutSettings(hudNativeVoipRatioX, hudNativeVoipRatioY, hudNativeVoipSizeScale);
        }
        saveHudControlTranslation("chat", hudChatButton);
        clearSavedHudControlTranslation("minimap_hotspot");
        resetFixedHudControl(hudHdMapHotspot, 1.0f);
        saveHudControlTranslation("fps", hudFpsCounter);
        saveSourceHudControlLayout();
        clearSavedHudControlTranslation("enter_passenger");
        clearSavedHudControlTranslation("vehicle_lock");
        hideLegacyVehicleButtonViews();
        clearSavedHudControlTranslation("action_2");
        clearSavedHudControlTranslation("hud_y");
        clearSavedHudControlTranslation("hud_f");
        clearSavedHudControlTranslation("classic_weapon");
        resetFixedHudControl(hud_main.findViewById(R.id.WeaponShowLayout), 1.0f);
        saveHudControlTranslation("classic_status", hud_main.findViewById(R.id.hud_left_panel));
        saveHudControlTranslation("needs", hud_main.findViewById(R.id.hud_needs_panel));
        if (hudEditLayer != null) {
            setVisibilityIfChanged(hudEditLayer, View.GONE);
        }
        if (hudEditDoneButton != null) {
            setVisibilityIfChanged(hudEditDoneButton, View.GONE);
        }
        if (hudChatAreaHint != null) {
            setVisibilityIfChanged(hudChatAreaHint, View.GONE);
        }
        restoreHudControlVisibilityAfterEdit();
        applyOfficialHudControlStyle();
        Toast.makeText(this, "HUD salvo.", Toast.LENGTH_SHORT).show();
    }

    private void setHudEditControlVisibility(boolean visible) {
        int visibility = visible ? View.VISIBLE : View.GONE;
        setVisibilityIfChanged(hudMenuButton, View.VISIBLE);
        setVisibilityIfChanged(hudInteractionButton, visible ? View.VISIBLE : View.INVISIBLE);
        setHudViewVisibility(R.id.btn_2, false);
        setVisibilityIfChanged(hudOptionsButton, View.GONE);
        setVisibilityIfChanged(hudChatButton, View.VISIBLE);
        setVisibilityIfChanged(hudHdMapHotspot, View.GONE);
        setSourceHudControlsVisibility(visibility);
        setVisibilityIfChanged(hudWeaponButton, View.GONE);
        setVisibilityIfChanged(hudFpsCounter, visibility);
        setVisibilityIfChanged(hudTopRightPanel, visibility);
        setHudViewVisibility(R.id.hud_y, false);
        setHudViewVisibility(R.id.hud_f, false);
        setHudViewVisibility(R.id.enter_passenger, false);
        setHudViewVisibility(R.id.vehicle_lock_butt, false);
        setHudViewVisibility(R.id.WeaponShowLayout, true);
        setHudViewVisibility(R.id.hud_left_panel, true);
        setHudViewVisibility(R.id.hud_status_box_health, true);
        setHudViewVisibility(R.id.hud_status_box_armour, true);
        setHudViewVisibility(R.id.hud_money_inline_row, false);
        setHudViewVisibility(R.id.hud_needs_panel, true);
        setVisibilityIfChanged(hudSettingsGearButton, View.GONE);
    }

    private void restoreHudControlVisibilityAfterEdit() {
        setVisibilityIfChanged(hudMenuButton, View.VISIBLE);
        int shortcutVisibility = hudShortcutButtonsVisible ? View.VISIBLE : View.INVISIBLE;
        setVisibilityIfChanged(hudInteractionButton, shortcutVisibility);
        setVisibilityIfChanged(hudOptionsButton, View.GONE);
        setVisibilityIfChanged(hudChatButton, hudNativeChatVisible ? View.VISIBLE : View.GONE);
        setSourceHudControlsVisibility(View.VISIBLE);
        installSourceControlTouchHandlers();
        setVisibilityIfChanged(hudWeaponButton, View.GONE);
        setVisibilityIfChanged(hudFpsCounter, hudFpsCounterVisible ? View.VISIBLE : View.GONE);
        setVisibilityIfChanged(hudTopRightPanel, hudMoneyVisible ? View.VISIBLE : View.GONE);
        setVisibilityIfChanged(hudSettingsGearButton, View.GONE);
        installHudHdMapHotspotOpenHandler();
        applyClassicHudVisibilityFromQuickSettings();
    }

    private void resetHudLayout() {
        getSharedPreferences(HUD_LAYOUT_PREFS, MODE_PRIVATE).edit().clear().apply();
        cacheHudControlViews();
        resetHudControlTranslation(hudMenuButton);
        resetHudControlTranslation(hudInteractionButton);
        resetHudControlTranslation(hudOptionsButton);
        clearSavedHudControlTranslation("voip");
        resetNativeVoiceChatLayoutToDefault();
        hudNativeVoipLayoutLoaded = false;
        resetHudControlTranslation(hudChatButton);
        resetHudControlTranslation(hudHdMapHotspot);
        clearSavedHudControlTranslation("minimap_hotspot");
        resetHudControlTranslation(hudFpsCounter);
        resetSourceHudControlLayout();
        resetHudControlTranslation(hudTopRightPanel);
        clearSavedHudControlTranslation("enter_passenger");
        clearSavedHudControlTranslation("vehicle_lock");
        hideLegacyVehicleButtonViews();
        resetHudControlTranslation(hud_main.findViewById(R.id.WeaponShowLayout));
        resetHudControlTranslation(hud_main.findViewById(R.id.hud_left_panel));
        resetHudControlTranslation(hud_main.findViewById(R.id.hud_needs_panel));
        applyOfficialHudControlStyle();
        Toast.makeText(this, "HUD voltou para o padrao.", Toast.LENGTH_SHORT).show();
    }

    private void resetHudControlTranslation(View view) {
        if (view == null) {
            return;
        }
        view.animate().cancel();
        view.setTranslationX(0.0f);
        view.setTranslationY(0.0f);
    }

    private void hideHudEditModeVisuals() {
        hudEditMode = false;
        cacheHudControlViews();
        if (hudEditLayer != null) {
            setVisibilityIfChanged(hudEditLayer, View.GONE);
        }
        if (hudEditDoneButton != null) {
            setVisibilityIfChanged(hudEditDoneButton, View.GONE);
        }
        if (hudChatAreaHint != null) {
            setVisibilityIfChanged(hudChatAreaHint, View.GONE);
        }
        restoreHudControlVisibilityAfterEdit();
    }

    private void hideAttachEditorOverlay() {
        if (mAttachEdit != null) {
            mAttachEdit.hideWithoutReset();
        }
        View attachEditor = findViewById(R.id.attach_main_layot);
        if (attachEditor != null) {
            setVisibilityIfChanged(attachEditor, View.GONE);
        }
    }

    private void animateHudControl(View view, float translationX, float translationY, float alpha) {
        if (view == null) {
            return;
        }
        if (Math.abs(view.getTranslationX() - translationX) < 0.5f
                && Math.abs(view.getTranslationY() - translationY) < 0.5f
                && Math.abs(view.getAlpha() - alpha) < 0.01f) {
            return;
        }
        view.animate()
                .translationX(translationX)
                .translationY(translationY)
                .alpha(alpha)
                .setDuration(HUD_CONTROL_FADE_MS)
                .start();
    }

    private void installHudIdleTouchWakeups() {
        applyOfficialHudControlStyle();
    }

    private void applyHudLeftPanelLayout() {
        if (hud_main == null) return;
        View panel = hud_main.findViewById(R.id.hud_left_panel);
        if (panel == null) return;
        ConstraintLayout.LayoutParams lp = (ConstraintLayout.LayoutParams) panel.getLayoutParams();
        lp.width = ConstraintLayout.LayoutParams.WRAP_CONTENT;
        lp.height = dpToPx(24.0f);
        lp.leftToLeft = ConstraintLayout.LayoutParams.UNSET;
        lp.leftToRight = ConstraintLayout.LayoutParams.UNSET;
        lp.startToStart = ConstraintLayout.LayoutParams.UNSET;
        lp.startToEnd = ConstraintLayout.LayoutParams.UNSET;
        lp.rightToRight = ConstraintLayout.LayoutParams.PARENT_ID;
        lp.endToEnd = ConstraintLayout.LayoutParams.PARENT_ID;
        lp.topToTop = ConstraintLayout.LayoutParams.PARENT_ID;
        lp.bottomToBottom = ConstraintLayout.LayoutParams.PARENT_ID;
        lp.verticalBias = 1.0f;
        lp.leftMargin = 0;
        lp.topMargin = 0;
        lp.rightMargin = dpToPx(92.0f);
        lp.bottomMargin = dpToPx(12.0f);
        panel.setLayoutParams(lp);
    }

    private int clampInt(int value, int min, int max) {
        return Math.max(min, Math.min(max, value));
    }

    private float clampFloat(float value, float min, float max) {
        return Math.max(min, Math.min(max, value));
    }

    private void ensureHudVoiceChatMasterEnabled() {
        if (hudVoiceChatEnabled) {
            return;
        }
        hudVoiceChatEnabled = true;
        saveHudNativeMenuSettings();
        try {
            applyNativeMenuSettings(hudAndroidKeyboardEnabled, hudChatMaxMessages, true);
        } catch (UnsatisfiedLinkError error) {
            Log.w(TAG, "Native voice master setting unavailable.", error);
        }
        syncHudNativeMenuSettingsUi();
    }

    private void setHudVoiceChatEnabled(boolean enabled) {
        if (hudVoiceChatEnabled == enabled) {
            syncHudOptionsPanel();
            syncHudNativeMenuSettingsUi();
            hideSystemUI();
            return;
        }
        hudVoiceChatEnabled = enabled;
        if (!enabled) {
            try {
                setNativeVoipEnabled(false);
            } catch (UnsatisfiedLinkError error) {
                Log.w(TAG, "Native VOIP stop unavailable.", error);
            }
        }
        commitHudNativeMenuSettingsLive();
        syncHudOptionsPanel();
        hideSystemUI();
    }

    private void initializeHudOptionsPanel() {
        if (hud_main == null) {
            return;
        }
        if (hudOptionsButton != null
                && hudOptionsPanel != null
                && hudOptionVoip != null
                && hudOptionChat != null
                && hudOptionWeapons != null
                && hudOptionSettings != null
                && hudOptionFps != null
                && hudOptionClose != null) {
            syncHudOptionsPanel();
            return;
        }
        hudOptionsButton = hud_main.findViewById(R.id.btn_hud_options);
        hudOptionsPanel = hud_main.findViewById(R.id.hud_options_panel);
        hudOptionVoip = hud_main.findViewById(R.id.hud_option_voip);
        hudOptionChat = hud_main.findViewById(R.id.hud_option_chat);
        hudOptionWeapons = hud_main.findViewById(R.id.hud_option_weapons);
        hudOptionSettings = hud_main.findViewById(R.id.hud_option_settings);
        hudOptionFps = hud_main.findViewById(R.id.hud_option_fps);
        hudOptionClose = hud_main.findViewById(R.id.hud_option_close);

        if (hudOptionsButton != null) {
            hudOptionsButton.setOnClickListener(v -> toggleHudOptionsPanel());
        }
        if (hudOptionVoip != null) {
            hudOptionVoip.setOnClickListener(v -> {
                noteHudControlInteraction();
                setHudVoiceChatEnabled(!hudVoiceChatEnabled);
            });
        }
        if (hudOptionChat != null) {
            hudOptionChat.setOnClickListener(v -> {
                noteHudControlInteraction();
                setHudChatButtonVisible(!hudNativeChatVisible);
                saveHudQuickSettings();
            });
        }
        if (hudOptionWeapons != null) {
            hudOptionWeapons.setOnClickListener(v -> {
                noteHudControlInteraction();
                setHudWeaponButtonVisible(!hudWeaponButtonVisible);
                saveHudQuickSettings();
            });
        }
        if (hudOptionSettings != null) {
            hudOptionSettings.setOnClickListener(v -> {
                noteHudControlInteraction();
                setHudSettingsGearButtonVisible(!hudSettingsGearButtonVisible);
            });
        }
        if (hudOptionFps != null) {
            hudOptionFps.setOnClickListener(v -> {
                noteHudControlInteraction();
                setHudFpsCounterVisible(!hudFpsCounterVisible);
                saveHudQuickSettings();
            });
        }
        if (hudOptionClose != null) {
            hudOptionClose.setOnClickListener(v -> {
                noteHudControlInteraction();
                setHudOptionsPanelVisible(false);
            });
        }

        syncHudOptionsPanel();
    }

    private void toggleHudOptionsPanel() {
        noteHudControlInteraction();
        if (!hudOptionsPanelVisible && isAnyNativeOverlayVisible()) {
            showExclusiveOverlayBlockedMessage();
            return;
        }
        setHudOptionsPanelVisible(!hudOptionsPanelVisible);
    }

    private void setHudOptionsPanelVisible(boolean visible) {
        if (hudOptionsPanel == null && hud_main != null) {
            hudOptionsPanel = hud_main.findViewById(R.id.hud_options_panel);
        }
        if (visible && isAnyNativeOverlayVisible()) {
            showExclusiveOverlayBlockedMessage();
            return;
        }
        hudOptionsPanelVisible = visible;
        if (hudOptionsPanel != null) {
            setVisibilityIfChanged(hudOptionsPanel, visible ? View.VISIBLE : View.GONE);
            if (visible) {
                hideHudChatPanel();
                hideHudSettingsPanel();
                hideHudStatusPanel();
                if (RadialMenu.menuVisible && mRadialMenu != null) {
                    mRadialMenu.hide();
                }
                hudOptionsPanel.bringToFront();
            }
        }
        hideSystemUI();
    }

    private void initializeHudFpsCounter() {
        if (hud_main == null) {
            return;
        }
        cacheHudControlViews();
        if (hudFpsCounter != null) {
            setVisibilityIfChanged(hudFpsCounter, hudFpsCounterVisible ? View.VISIBLE : View.GONE);
            if (hudFpsCounterVisible) {
                hudFpsCounter.bringToFront();
                startHudFpsCounter();
            }
        }
    }

    private void setHudFpsCounterVisible(boolean visible) {
        if (hudFpsCounterVisible == visible) {
            syncHudOptionsPanel();
            hideSystemUI();
            return;
        }
        hudFpsCounterVisible = visible;
        if (hud_main != null) {
            cacheHudControlViews();
            if (hudFpsCounter != null) {
                setVisibilityIfChanged(hudFpsCounter, visible ? View.VISIBLE : View.GONE);
                if (visible) {
                    hudFpsCounter.bringToFront();
                    startHudFpsCounter();
                } else {
                    stopHudFpsCounter();
                }
            }
        }
        syncHudOptionsPanel();
        hideSystemUI();
    }

    private void startHudFpsCounter() {
        if (hudFpsCounterValue != null) {
            hudFpsCounterValue.setText("--");
        }
        uiHandler.removeCallbacks(hudFpsPollRunnable);
        uiHandler.post(hudFpsPollRunnable);
    }

    private void stopHudFpsCounter() {
        uiHandler.removeCallbacks(hudFpsPollRunnable);
    }

    private void setHudChatButtonVisible(boolean visible) {
        if (hudNativeChatVisible == visible) {
            syncHudChatActivationVisuals(visible);
            if (visible) {
                showNativeSampChatOnly();
            }
            syncHudOptionsPanel();
            hideSystemUI();
            return;
        }
        hudNativeChatVisible = visible;
        if (hudChatButton == null && hud_main != null) {
            hudChatButton = hud_main.findViewById(R.id.btn_chat_toggle);
        }
        ensureHudChatClickArea();
        if (hudChatButton != null) {
            setVisibilityIfChanged(hudChatButton, visible ? View.VISIBLE : View.GONE);
            if (visible) {
                hudChatButton.bringToFront();
            } else {
                hideHudChatPanel();
            }
        }
        updateHudChatClickAreaVisibility();
        syncHudChatActivationVisuals(visible);
        if (visible) {
            showNativeSampChatOnly();
        } else {
            hideNativeSampChatOnly();
        }
        syncHudOptionsPanel();
        hideSystemUI();
    }

    private void syncHudChatActivationVisuals(boolean visible) {
        if (hudChatAreaHint == null && hud_main != null) {
            hudChatAreaHint = hud_main.findViewById(R.id.hud_chat_area_hint);
        }
        if (hudChatAreaHint != null) {
            boolean showAreaHint = visible && (hudEditMode || hudChatAreaMappingMode);
            setVisibilityIfChanged(hudChatAreaHint, showAreaHint ? View.VISIBLE : View.GONE);
        }
        updateHudChatClickAreaVisibility();
        if (visible && hudChatButton != null) {
            hudChatButton.bringToFront();
        }
    }

    private void showNativeSampChatOnly() {
        if (nativeChatShowUnavailable) {
            return;
        }
        try {
            MostrarChat();
        } catch (UnsatisfiedLinkError error) {
            nativeChatShowUnavailable = true;
            Log.w(TAG, "Native chat show unavailable; using Java HUD state only.");
        }
    }

    private void hideNativeSampChatOnly() {
        if (nativeChatHideUnavailable) {
            return;
        }
        try {
            OcultarChatBotao();
        } catch (UnsatisfiedLinkError error) {
            nativeChatHideUnavailable = true;
            Log.w(TAG, "Native chat hide unavailable; using Java HUD state only.");
        }
    }

    private void setHudWeaponButtonVisible(boolean visible) {
        if (hudWeaponButtonVisible == visible) {
            applyClassicHudVisibilityFromQuickSettings();
            syncHudOptionsPanel();
            hideSystemUI();
            return;
        }
        hudWeaponButtonVisible = visible;
        if (hud_main != null) {
            cacheHudControlViews();
            if (hudWeaponButton != null) {
                setVisibilityIfChanged(hudWeaponButton, View.GONE);
            }
            applyClassicHudVisibilityFromQuickSettings();
            updateClassicHudWeapon(
                    lastHudGunId == Integer.MIN_VALUE ? 0 : lastHudGunId,
                    lastHudAmmo == Integer.MIN_VALUE ? 0 : lastHudAmmo
            );
        }
        syncHudOptionsPanel();
        hideSystemUI();
    }

    private void setHudMoneyVisible(boolean visible) {
        hudMoneyVisible = visible;
        applyClassicHudVisibilityFromQuickSettings();
        syncHudOptionsPanel();
        hideSystemUI();
    }

    private void setHudVitalsVisible(boolean visible) {
        hudVitalsVisible = visible;
        applyClassicHudVisibilityFromQuickSettings();
        syncHudOptionsPanel();
        hideSystemUI();
    }

    private void setHudNeedsVisible(boolean visible) {
        hudNeedsVisible = visible;
        applyClassicHudVisibilityFromQuickSettings();
        syncHudOptionsPanel();
        hideSystemUI();
    }

    private void setHudSettingsGearButtonVisible(boolean visible) {
        hudSettingsGearButtonVisible = false;
        if (hudSettingsGearButton == null && hud_main != null) {
            hudSettingsGearButton = hud_main.findViewById(R.id.btn_hud_settings_gear);
        }
        if (hudSettingsGearButton != null) {
            setVisibilityIfChanged(hudSettingsGearButton, View.GONE);
        }
        syncHudOptionsPanel();
        hideSystemUI();
    }

    private void loadHudQuickSettings() {
        SharedPreferences preferences = getSharedPreferences(HUD_QUICK_PREFS, MODE_PRIVATE);
        hudNativeChatVisible = preferences.getBoolean("chat_visible", hudNativeChatVisible);
        hudWeaponButtonVisible = preferences.getBoolean("weapon_visible", hudWeaponButtonVisible);
        hudFpsCounterVisible = preferences.getBoolean("fps_visible", hudFpsCounterVisible);
        hudMoneyVisible = preferences.getBoolean("money_visible", hudMoneyVisible);
        hudVitalsVisible = preferences.getBoolean("vitals_visible", hudVitalsVisible);
        hudNeedsVisible = preferences.getBoolean("needs_visible", hudNeedsVisible);
        hudSettingsGearButtonVisible = false;
    }

    private void saveHudQuickSettings() {
        getSharedPreferences(HUD_QUICK_PREFS, MODE_PRIVATE)
                .edit()
                .putBoolean("chat_visible", hudNativeChatVisible)
                .putBoolean("weapon_visible", hudWeaponButtonVisible)
                .putBoolean("fps_visible", hudFpsCounterVisible)
                .putBoolean("money_visible", hudMoneyVisible)
                .putBoolean("vitals_visible", hudVitalsVisible)
                .putBoolean("needs_visible", hudNeedsVisible)
                .apply();
    }

    private void applyHudQuickSettingsVisibility() {
        if (hud_main == null) {
            return;
        }
        cacheHudControlViews();
        if (shouldHideGameplayHudForNativeMenu()) {
            applyHudButtonVisibilityForRuntimeState();
            syncHudOptionsPanel();
            return;
        }
        ensureHudChatClickArea();
        if (hudChatButton != null) {
            setVisibilityIfChanged(hudChatButton, hudNativeChatVisible ? View.VISIBLE : View.GONE);
        }
        updateHudChatClickAreaVisibility();
        if (hudWeaponButton != null) {
            setVisibilityIfChanged(hudWeaponButton, View.GONE);
        }
        applyClassicHudVisibilityFromQuickSettings();
        if (hudFpsCounter != null) {
            setVisibilityIfChanged(hudFpsCounter, hudFpsCounterVisible ? View.VISIBLE : View.GONE);
            if (hudFpsCounterVisible) {
                startHudFpsCounter();
            } else {
                stopHudFpsCounter();
            }
        }
        if (hudNativeChatVisible) {
            showNativeSampChatOnly();
        } else {
            hideNativeSampChatOnly();
        }
        syncHudChatActivationVisuals(hudNativeChatVisible);
        syncHudOptionsPanel();
        applyHudButtonVisibilityForRuntimeState();
    }

    private void applyClassicHudVisibilityFromQuickSettings() {
        if (hud_main == null) {
            return;
        }
        cacheHudControlViews();
        boolean hideForPhone = isPhoneOverlayVisible();
        boolean hideForNativeMenu = shouldHideGameplayHudForNativeMenu();
        boolean hideGameplayHud = hideForPhone || hideForNativeMenu;
        boolean showTopRightHud = !hideGameplayHud && hudMoneyVisible;
        setHudViewVisibility(R.id.hud_money_row, false);
        setHudViewVisibility(R.id.hud_left_panel, !hideGameplayHud && hudVitalsVisible);
        setHudViewVisibility(R.id.hud_status_box_health, !hideGameplayHud && hudVitalsVisible);
        setHudViewVisibility(R.id.hud_status_box_armour, !hideGameplayHud && hudVitalsVisible);
        setHudViewVisibility(R.id.hud_money_inline_row, false);
        setVisibilityIfChanged(hudTopRightPanel, showTopRightHud ? View.VISIBLE : View.GONE);
        if (showTopRightHud) {
            startHudTopInfoTicker();
        } else {
            stopHudTopInfoTicker();
        }
        setHudViewVisibility(R.id.hud_needs_panel, !hideGameplayHud && hudNeedsVisible);
        setHudViewVisibility(R.id.WeaponShowLayout, !hideGameplayHud && hudWeaponButtonVisible);
        setSourceHudControlsVisibility(hideGameplayHud ? View.GONE : View.VISIBLE);
        if (hideGameplayHud) {
            resetNativeSourceControls();
        }
        setHudViewVisibility(R.id.btn_weapon_wheel, false);
    }

    private void setHudViewVisibility(int viewId, boolean visible) {
        if (hud_main == null) {
            return;
        }
        View view = hud_main.findViewById(viewId);
        if (view != null) {
            setVisibilityIfChanged(view, visible ? View.VISIBLE : View.GONE);
        }
    }

    private void syncHudOptionsPanel() {
        if (hudOptionVoip != null) {
            setVisibilityIfChanged(hudOptionVoip, View.VISIBLE);
        }
        setHudOptionState(hudOptionVoip, "VOIP", hudVoiceChatEnabled);
        setHudOptionState(hudOptionChat, "CHAT", hudNativeChatVisible);
        setHudOptionState(hudOptionWeapons, "ARMAS", hudWeaponButtonVisible);
        setHudOptionState(hudOptionSettings, "CONFIG", hudSettingsGearButtonVisible);
        setHudOptionState(hudOptionFps, "FPS", hudFpsCounterVisible);
        applyClassicHudVisibilityFromQuickSettings();
        syncHudConfigQuickSettingsUi();
    }

    private void syncHudConfigQuickSettingsUi() {
        if (hud_main == null) {
            return;
        }
        View voipQuickRow = hud_main.findViewById(R.id.hud_config_quick_voip);
        if (voipQuickRow != null) {
            setVisibilityIfChanged(voipQuickRow, View.VISIBLE);
        }
        setHudConfigQuickState(voipQuickRow, "VOIP", hudVoiceChatEnabled);
        setHudConfigQuickState(hud_main.findViewById(R.id.hud_config_quick_chat), "Chat", hudNativeChatVisible);
        setHudConfigQuickState(hud_main.findViewById(R.id.hud_config_quick_weapons), "Armas", hudWeaponButtonVisible);
        setHudConfigQuickState(hud_main.findViewById(R.id.hud_config_quick_money), "Dinheiro", hudMoneyVisible);
        setHudConfigQuickState(hud_main.findViewById(R.id.hud_config_quick_vitals), "Vida e colete", hudVitalsVisible);
        setHudConfigQuickState(hud_main.findViewById(R.id.hud_config_quick_needs), "Fome, sede e sono", hudNeedsVisible);
        setHudConfigQuickState(hud_main.findViewById(R.id.hud_config_quick_fps), "Contador FPS", hudFpsCounterVisible);
    }

    private void setHudConfigQuickState(View row, String label, boolean enabled) {
        if (row == null) {
            return;
        }
        View labelView = row.findViewWithTag("hud_config_quick_label");
        if (labelView instanceof TextView) {
            TextView text = (TextView) labelView;
            text.setText(label);
            text.setTextColor(Color.parseColor(enabled ? "#FFFFFFFF" : "#D4DEE4"));
        }
        View switchTrack = row.findViewWithTag("hud_config_switch");
        setHudConfigSwitchVisual(switchTrack, enabled);
    }

    private void setHudConfigSwitchVisual(View switchTrack, boolean enabled) {
        if (switchTrack == null) {
            return;
        }
        switchTrack.setBackgroundResource(enabled
                ? R.drawable.hud_config_switch_track_on
                : R.drawable.hud_config_switch_track_off);
        View switchThumb = switchTrack.findViewWithTag("hud_config_switch_thumb");
        if (switchThumb != null) {
            ViewGroup.LayoutParams params = switchThumb.getLayoutParams();
            if (params instanceof FrameLayout.LayoutParams) {
                FrameLayout.LayoutParams frameParams = (FrameLayout.LayoutParams) params;
                frameParams.gravity = Gravity.CENTER_VERTICAL | (enabled ? Gravity.END : Gravity.START);
                int margin = dpToPx(2.0f);
                frameParams.setMargins(margin, 0, margin, 0);
                switchThumb.setLayoutParams(frameParams);
            }
        }
    }

    private void setHudOptionState(View option, String label, boolean active) {
        if (option == null) {
            return;
        }

        option.setAlpha(1.0f);
        option.setSelected(active);
        option.setContentDescription(label + (active ? " ligado" : " desligado"));

        View labelView = option.findViewWithTag("hud_option_label");
        if (labelView instanceof TextView) {
            TextView optionLabel = (TextView) labelView;
            optionLabel.setText(label);
            optionLabel.setTextColor(active ? Color.parseColor("#FFFFFFFF") : Color.parseColor("#D4DEE4"));
        } else if (option instanceof TextView) {
            TextView optionText = (TextView) option;
            optionText.setText(label);
            optionText.setTextColor(active ? Color.parseColor("#FFFFFFFF") : Color.parseColor("#D4DEE4"));
        }

        View switchTrack = option.findViewWithTag("hud_option_switch");
        if (switchTrack != null) {
            switchTrack.setBackgroundResource(active ? R.drawable.hud_switch_track_on : R.drawable.hud_switch_track_off);
        }

        View switchThumb = option.findViewWithTag("hud_option_switch_thumb");
        if (switchThumb != null) {
            ViewGroup.LayoutParams params = switchThumb.getLayoutParams();
            if (params instanceof FrameLayout.LayoutParams) {
                FrameLayout.LayoutParams frameParams = (FrameLayout.LayoutParams) params;
                frameParams.gravity = Gravity.CENTER_VERTICAL | (active ? Gravity.END : Gravity.START);
                int margin = dpToPx(2.0f);
                frameParams.setMargins(margin, 0, margin, 0);
                switchThumb.setLayoutParams(frameParams);
            }
        }
    }

    private void initializeHudChatPanel() {
        if (hud_main == null) {
            return;
        }
        if (hudChatPanel != null) {
            return;
        }
        hudChatPanel = hud_main.findViewById(R.id.hud_chat_panel);
        if (hudChatPanel != null) {
            setVisibilityIfChanged(hudChatPanel, View.GONE);
        }
    }

    private boolean isHudChatPanelVisible() {
        return hudChatPanel != null && hudChatPanel.getVisibility() == View.VISIBLE;
    }

    private void openChatFromButton() {
        noteHudControlInteraction();
        setHudOptionsPanelVisible(false);
        openNativeSampChatInput();
    }

    private void showHudChatPanel() {
        openNativeSampChatInput();
    }

    private void openNativeSampChatInput() {
        hideHudSettingsPanel();
        hideHudStatusPanel();
        hidePhoneOverlayInternal();
        hideInventoryOverlayInternal();
        hideWeaponWheelOverlayInternal();
        if (RadialMenu.menuVisible && mRadialMenu != null) {
            mRadialMenu.hide();
        }
        if (Radinho.radinhoVisible && mRadinho != null) {
            mRadinho.hide();
        }

        if (hudChatPanel != null) {
            setVisibilityIfChanged(hudChatPanel, View.GONE);
        }
        hudChatPanelVisible = false;
        hudNativeChatVisible = true;
        saveHudQuickSettings();
        syncHudChatActivationVisuals(true);
        try {
            AbrirChatBotao();
        } catch (UnsatisfiedLinkError error) {
            Log.e(TAG, "Native chat input unavailable.", error);
            showKeyboard();
        }
        syncHudOptionsPanel();
        refreshRuntimeChrome();
        hideSystemUI();
    }

    private void hideHudChatPanel() {
        if (hudChatPanel != null) {
            setVisibilityIfChanged(hudChatPanel, View.GONE);
        }
        hudChatPanelVisible = false;
        hideHudChatKeyboard();
        syncHudOptionsPanel();
        refreshRuntimeChrome();
        hideSystemUI();
    }

    private void focusHudChatInput() {
        if (hudChatInput == null) {
            return;
        }
        hudChatInput.requestFocus();
        hudChatInput.postDelayed(() -> {
            InputMethodManager manager = (InputMethodManager) getSystemService(INPUT_METHOD_SERVICE);
            if (manager != null) {
                manager.showSoftInput(hudChatInput, InputMethodManager.SHOW_IMPLICIT);
            }
        }, 80L);
    }

    private void hideHudChatKeyboard() {
        if (hudChatInput == null) {
            return;
        }
        InputMethodManager manager = (InputMethodManager) getSystemService(INPUT_METHOD_SERVICE);
        if (manager != null) {
            manager.hideSoftInputFromWindow(hudChatInput.getWindowToken(), 0);
        }
        hudChatInput.clearFocus();
    }

    private void sendHudChatInput() {
        if (hudChatInput == null) {
            return;
        }
        String text = limitRoleplayChatText(
                hudChatInput.getText() == null ? "" : hudChatInput.getText().toString()
        );
        if (text.isEmpty()) {
            focusHudChatInput();
            return;
        }
        hudChatInput.setText("");
        if (text.startsWith("/")) {
            appendHudChatLine("me", "Comando", text);
            dispatchRuntimeCommand(text);
        } else {
            appendHudChatLine("me", "Eu", text);
            OnInputEnd(text);
        }
        focusHudChatInput();
    }

    public void pushNativeChatMessage(String type, String sender, String message) {
        // Chat rendering is handled by the native SA-MP chat. Keep this bridge idle
        // so incoming messages do not rebuild a second Java chat view.
    }

    private void appendHudChatLine(String type, String sender, String message) {
        initializeHudChatPanel();
        if (hudChatMessages == null) {
            return;
        }

        String safeMessage = sanitizeChatText(message);
        if (safeMessage.isEmpty()) {
            return;
        }
        String safeSender = sanitizeChatText(sender);
        String safeType = type == null ? "info" : type;
        String prefix = safeSender.isEmpty() ? "" : safeSender + ": ";

        TextView line = new TextView(this);
        line.setLayoutParams(new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
        ));
        line.setIncludeFontPadding(false);
        line.setMaxLines(4);
        line.setSingleLine(false);
        line.setText(prefix + safeMessage);
        line.setTextColor(chatLineColor(safeType));
        line.setTextSize(10.5f);
        line.setPadding(dpToPx(2), dpToPx(3), dpToPx(2), dpToPx(3));

        hudChatMessages.addView(line);
        while (hudChatMessages.getChildCount() > HUD_CHAT_MAX_LINES) {
            hudChatMessages.removeViewAt(0);
        }
        if (hudChatScroll != null) {
            hudChatScroll.post(() -> hudChatScroll.fullScroll(View.FOCUS_DOWN));
        }
    }

    private String sanitizeChatText(String value) {
        if (value == null) {
            return "";
        }
        return CHAT_COLOR_PATTERN.matcher(value)
                .replaceAll("")
                .replace('\r', ' ')
                .replace('\n', ' ')
                .trim();
    }

    private String limitRoleplayChatText(String value) {
        String sanitized = sanitizeChatText(value).replaceAll("\\s+", " ");
        if (sanitized.isEmpty()) {
            return "";
        }

        String[] words = sanitized.split(" ");
        StringBuilder limited = new StringBuilder();
        int maxWords = Math.min(words.length, ROLEPLAY_CHAT_MAX_WORDS);
        int maxChars = getRoleplayChatMaxCharsForCurrentArea();
        for (int i = 0; i < maxWords; i++) {
            String word = words[i];
            if (word.isEmpty()) {
                continue;
            }
            int extra = limited.length() == 0 ? word.length() : word.length() + 1;
            if (limited.length() + extra > maxChars) {
                int available = maxChars - limited.length();
                if (available > 1) {
                    if (limited.length() > 0) {
                        limited.append(' ');
                        available -= 1;
                    }
                    limited.append(word, 0, Math.min(word.length(), available));
                }
                break;
            }
            if (limited.length() > 0) {
                limited.append(' ');
            }
            limited.append(word);
        }
        return limited.toString().trim();
    }

    private int chatLineColor(String type) {
        if ("me".equals(type)) {
            return Color.parseColor("#FBBF24");
        }
        if ("player".equals(type)) {
            return Color.parseColor("#FFFFFFFF");
        }
        if ("debug".equals(type)) {
            return Color.parseColor("#C8D3DC");
        }
        if ("client".equals(type)) {
            return Color.parseColor("#E8F4FF");
        }
        return Color.parseColor("#67E8F9");
    }

    private void initializeHudSettingsPanel() {
        if (hud_main == null) {
            return;
        }

        hudSettingsPanel = hud_main.findViewById(R.id.hud_settings_panel);
        if (hudSettingsPanel == null) {
            return;
        }
        hudEditLayer = hud_main.findViewById(R.id.hud_edit_layer);
        hudEditDoneButton = hud_main.findViewById(R.id.hud_edit_done);

        hudSettingsSummary = hud_main.findViewById(R.id.hud_settings_summary);
        hudRenderDistanceValue = hud_main.findViewById(R.id.hud_render_distance_value);
        hudFpsLimitValue = hud_main.findViewById(R.id.hud_fps_limit_value);
        hudQualityLowButton = hud_main.findViewById(R.id.hud_quality_low);
        hudQualityBalancedButton = hud_main.findViewById(R.id.hud_quality_balanced);
        hudQualityHighButton = hud_main.findViewById(R.id.hud_quality_high);
        hudShadowsToggle = hud_main.findViewById(R.id.hud_shadows_toggle);
        hudEffectsToggle = hud_main.findViewById(R.id.hud_effects_toggle);
        hudSmartOptimizerToggle = hud_main.findViewById(R.id.hud_smart_optimizer_toggle);
        hudSmartOptimizerStatus = hud_main.findViewById(R.id.hud_smart_optimizer_status);
        hudAndroidKeyboardToggle = hud_main.findViewById(R.id.hud_android_keyboard_toggle);
        hudVoiceChatToggle = hud_main.findViewById(R.id.hud_voice_chat_toggle);
        hudChatLinesCycle = hud_main.findViewById(R.id.hud_chat_lines_cycle);
        hudControlAlphaCycle = hud_main.findViewById(R.id.hud_control_alpha_cycle);
        hudMapToggle = hud_main.findViewById(R.id.hud_map_toggle);
        hudHdMapOpenButton = hud_main.findViewById(R.id.hud_hd_map_open);
        hudChatAreaMapButton = hud_main.findViewById(R.id.hud_chat_area_map);
        hudChatAreaResetButton = hud_main.findViewById(R.id.hud_chat_area_reset);
        hudCjOutfitCycleButton = hud_main.findViewById(R.id.hud_cj_outfit_cycle);
        hudCjOutfitResetButton = hud_main.findViewById(R.id.hud_cj_outfit_reset);
        hudRenderDistanceSeek = hud_main.findViewById(R.id.hud_render_distance_seek);
        hudFpsLimitSeek = hud_main.findViewById(R.id.hud_fps_limit_seek);

        loadHudRenderSettings();
        loadHudNativeMenuSettings();
        initializeHudConfigNavigation();
        initializeHudConfigQuickSettings();

        if (hudQualityLowButton != null) {
            hudQualityLowButton.setOnClickListener(v -> {
                selectHudQuality(HUD_QUALITY_FPS);
            });
        }
        if (hudQualityBalancedButton != null) {
            hudQualityBalancedButton.setOnClickListener(v -> {
                selectHudQuality(HUD_QUALITY_BALANCED);
            });
        }
        if (hudQualityHighButton != null) {
            hudQualityHighButton.setOnClickListener(v -> {
                selectHudQuality(HUD_QUALITY_HIGH);
            });
        }
        if (hudShadowsToggle != null) {
            setHudConfigRowClickListener(hudShadowsToggle, v -> {
                hudShadowsEnabled = !hudShadowsEnabled;
                syncHudRenderSettingsUi();
                commitHudRenderSettingsLive();
            });
        }
        if (hudEffectsToggle != null) {
            setHudConfigRowClickListener(hudEffectsToggle, v -> {
                hudEffectsEnabled = !hudEffectsEnabled;
                syncHudRenderSettingsUi();
                commitHudRenderSettingsLive();
            });
        }
        syncSmartOptimizerUi();
        hideHudConfigRow(hudCjOutfitCycleButton);
        hideHudConfigRow(hudCjOutfitResetButton);
        View nativeResumeButton = hud_main.findViewById(R.id.hud_native_resume);
        if (nativeResumeButton != null) {
            nativeResumeButton.setOnClickListener(v -> hideHudSettingsPanel());
        }
        View editHudButton = hud_main.findViewById(R.id.hud_edit_mode);
        if (editHudButton != null) {
            setHudConfigRowClickListener(editHudButton, v -> setHudEditMode(true));
        }
        View controlsEditHudButton = hud_main.findViewById(R.id.hud_controls_edit_hud);
        if (controlsEditHudButton != null) {
            setHudConfigRowClickListener(controlsEditHudButton, v -> setHudEditMode(true));
        }
        View resetHudButton = hud_main.findViewById(R.id.hud_reset_hud);
        if (resetHudButton != null) {
            setHudConfigRowClickListener(resetHudButton, v -> resetHudLayout());
        }
        View controlsResetHudButton = hud_main.findViewById(R.id.hud_controls_reset_hud);
        if (controlsResetHudButton != null) {
            setHudConfigRowClickListener(controlsResetHudButton, v -> resetHudLayout());
        }
        View nativeExitButton = hud_main.findViewById(R.id.hud_native_exit);
        if (nativeExitButton != null) {
            nativeExitButton.setOnClickListener(v -> exitGameFromHud());
        }
        if (hudEditDoneButton != null) {
            hudEditDoneButton.setOnClickListener(v -> setHudEditMode(false));
        }
        if (hudAndroidKeyboardToggle != null) {
            hudAndroidKeyboardToggle.setOnClickListener(v -> {
                hudAndroidKeyboardEnabled = !hudAndroidKeyboardEnabled;
                commitHudNativeMenuSettingsLive();
            });
        }
        if (hudVoiceChatToggle != null) {
            hudVoiceChatToggle.setOnClickListener(v -> {
                setHudVoiceChatEnabled(!hudVoiceChatEnabled);
            });
        }
        if (hudChatLinesCycle != null) {
            hudChatLinesCycle.setOnClickListener(v -> {
                hudChatMaxMessages = nextHudChatMaxMessages(hudChatMaxMessages);
                commitHudNativeMenuSettingsLive();
            });
        }
        if (hudControlAlphaCycle != null) {
            setHudConfigRowClickListener(hudControlAlphaCycle, v -> {
                cycleHudControlAlpha();
                saveHudRenderSettings();
                syncHudRenderSettingsUi();
                applyOfficialHudControlStyle();
            });
        }
        if (hudMapToggle != null) {
            setHudConfigRowClickListener(hudMapToggle, v -> {
                hudMapEnabled = !hudMapEnabled;
                syncHudRenderSettingsUi();
                commitHudRenderSettingsLive();
            });
        }
        hideHudConfigRow(hudHdMapOpenButton);
        if (hudChatAreaMapButton != null) {
            setHudConfigRowClickListener(hudChatAreaMapButton, v -> beginHudChatAreaMapping());
        }
        View controlsChatAreaMapButton = hud_main.findViewById(R.id.hud_controls_map_chat);
        if (controlsChatAreaMapButton != null) {
            setHudConfigRowClickListener(controlsChatAreaMapButton, v -> beginHudChatAreaMapping());
        }
        if (hudChatAreaResetButton != null) {
            setHudConfigRowClickListener(hudChatAreaResetButton, v -> resetHudChatClickArea());
        }
        View controlsChatAreaResetButton = hud_main.findViewById(R.id.hud_controls_reset_chat);
        if (controlsChatAreaResetButton != null) {
            setHudConfigRowClickListener(controlsChatAreaResetButton, v -> resetHudChatClickArea());
        }
        if (hudRenderDistanceSeek != null) {
            hudRenderDistanceSeek.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
                @Override
                public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                    hudRenderDistance = clampInt(
                            HUD_RENDER_DISTANCE_MIN + progress,
                            HUD_RENDER_DISTANCE_MIN,
                            HUD_RENDER_DISTANCE_MAX
                    );
                    syncHudRenderSettingsText();
                    if (fromUser) {
                        commitHudRenderSettingsLive();
                    }
                }

                @Override
                public void onStartTrackingTouch(SeekBar seekBar) {
                }

                @Override
                public void onStopTrackingTouch(SeekBar seekBar) {
                    commitHudRenderSettingsLive();
                }
            });
        }
        if (hudFpsLimitSeek != null) {
            hudFpsLimitSeek.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
                @Override
                public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                    hudFpsLimit = clampInt(
                            HUD_FPS_LIMIT_MIN + progress,
                            HUD_FPS_LIMIT_MIN,
                            HUD_FPS_LIMIT_MAX
                    );
                    syncHudRenderSettingsText();
                    if (fromUser) {
                        commitHudRenderSettingsLive();
                    }
                }

                @Override
                public void onStartTrackingTouch(SeekBar seekBar) {
                }

                @Override
                public void onStopTrackingTouch(SeekBar seekBar) {
                    commitHudRenderSettingsLive();
                }
            });
        }
        if (hudFpsLimitValue != null) {
            hudFpsLimitValue.setOnClickListener(v -> {
                hudFpsLimit = nextHudFpsLimit(hudFpsLimit);
                syncHudRenderSettingsText();
                commitHudRenderSettingsLive();
            });
        }

        View closeButton = hud_main.findViewById(R.id.hud_settings_close);
        if (closeButton != null) {
            closeButton.setOnClickListener(v -> hideHudSettingsPanel());
        }
        View applyButton = hud_main.findViewById(R.id.hud_settings_apply);
        if (applyButton != null) {
            applyButton.setOnClickListener(v -> {
                commitHudRenderSettingsLive();
                commitHudNativeMenuSettingsLive();
                hideHudSettingsPanel();
                Toast.makeText(this, "Configuracoes aplicadas no jogo.", Toast.LENGTH_SHORT).show();
            });
        }

        syncHudRenderSettingsUi();
        syncHudNativeMenuSettingsUi();
        applyHudNativeMenuSettingsRuntime();
        applyHudRenderSettingsRuntime();
        installHudEditTouchHandlers();
        initializeHudChatAreaMapping();
        applySavedHudLayout();
    }

    private void initializeHudConfigNavigation() {
        View gameplayTab = hud_main.findViewById(R.id.hud_config_tab_gameplay);
        View graphicsTab = hud_main.findViewById(R.id.hud_config_tab_graphics);
        View controlsTab = hud_main.findViewById(R.id.hud_config_tab_controls);
        View communicationTab = hud_main.findViewById(R.id.hud_config_tab_communication);
        if (gameplayTab != null) {
            gameplayTab.setOnClickListener(v -> showHudConfigSection(true));
        }
        if (graphicsTab != null) {
            graphicsTab.setOnClickListener(v -> showHudConfigSection(false));
        }
        if (controlsTab != null) {
            controlsTab.setOnClickListener(v -> showHudConfigControlsSection());
        }
        if (communicationTab != null) {
            communicationTab.setOnClickListener(null);
            setVisibilityIfChanged(communicationTab, View.GONE);
        }
        showHudConfigSection(false);
    }

    private void initializeHudConfigQuickSettings() {
        setupHudConfigQuickRow(R.id.hud_config_quick_voip, () -> setHudVoiceChatEnabled(!hudVoiceChatEnabled));
        setupHudConfigQuickRow(R.id.hud_config_quick_chat, () -> setHudChatButtonVisible(!hudNativeChatVisible));
        setupHudConfigQuickRow(R.id.hud_config_quick_weapons, () -> setHudWeaponButtonVisible(!hudWeaponButtonVisible));
        setupHudConfigQuickRow(R.id.hud_config_quick_money, () -> setHudMoneyVisible(!hudMoneyVisible));
        setupHudConfigQuickRow(R.id.hud_config_quick_vitals, () -> setHudVitalsVisible(!hudVitalsVisible));
        setupHudConfigQuickRow(R.id.hud_config_quick_needs, () -> setHudNeedsVisible(!hudNeedsVisible));
        setupHudConfigQuickRow(R.id.hud_config_quick_fps, () -> setHudFpsCounterVisible(!hudFpsCounterVisible));
        syncHudConfigQuickSettingsUi();
    }

    private void setHudConfigRowClickListener(View view, View.OnClickListener listener) {
        if (view == null) {
            return;
        }
        view.setOnClickListener(listener);
        Object parent = view.getParent();
        if (parent instanceof View) {
            View parentView = (View) parent;
            parentView.setClickable(true);
            parentView.setFocusable(true);
            parentView.setOnClickListener(listener);
        }
    }

    private void hideHudConfigRow(View view) {
        if (view == null) {
            return;
        }
        view.setOnClickListener(null);
        view.setVisibility(View.GONE);
        Object parent = view.getParent();
        if (parent instanceof View) {
            View parentView = (View) parent;
            parentView.setOnClickListener(null);
            parentView.setClickable(false);
            parentView.setFocusable(false);
            parentView.setVisibility(View.GONE);
        }
    }

    private void setupHudConfigQuickRow(int rowId, Runnable action) {
        View row = hud_main.findViewById(rowId);
        if (row != null) {
            row.setOnClickListener(v -> {
                noteHudControlInteraction();
                action.run();
                saveHudQuickSettings();
            });
        }
    }

    private void showHudConfigSection(boolean gameplay) {
        View gameplayContent = hud_main.findViewById(R.id.hud_config_gameplay_content);
        View graphicsContent = hud_main.findViewById(R.id.hud_config_graphics_content);
        View controlsContent = hud_main.findViewById(R.id.hud_config_controls_content);
        setVisibilityIfChanged(gameplayContent, gameplay ? View.VISIBLE : View.GONE);
        setVisibilityIfChanged(graphicsContent, gameplay ? View.GONE : View.VISIBLE);
        setVisibilityIfChanged(controlsContent, View.GONE);
        setHudConfigTabSelected(hud_main.findViewById(R.id.hud_config_tab_gameplay), gameplay);
        setHudConfigTabSelected(hud_main.findViewById(R.id.hud_config_tab_graphics), !gameplay);
        setHudConfigTabSelected(hud_main.findViewById(R.id.hud_config_tab_controls), false);
        setHudConfigTabSelected(hud_main.findViewById(R.id.hud_config_tab_communication), false);
        syncHudConfigQuickSettingsUi();
        hideSystemUI();
    }

    private void showHudConfigControlsSection() {
        View gameplayContent = hud_main.findViewById(R.id.hud_config_gameplay_content);
        View graphicsContent = hud_main.findViewById(R.id.hud_config_graphics_content);
        View controlsContent = hud_main.findViewById(R.id.hud_config_controls_content);
        setVisibilityIfChanged(gameplayContent, View.GONE);
        setVisibilityIfChanged(graphicsContent, View.GONE);
        setVisibilityIfChanged(controlsContent, View.VISIBLE);
        setHudConfigTabSelected(hud_main.findViewById(R.id.hud_config_tab_gameplay), false);
        setHudConfigTabSelected(hud_main.findViewById(R.id.hud_config_tab_graphics), false);
        setHudConfigTabSelected(hud_main.findViewById(R.id.hud_config_tab_controls), true);
        setHudConfigTabSelected(hud_main.findViewById(R.id.hud_config_tab_communication), false);
        hideSystemUI();
    }

    private void setHudConfigTabSelected(View tab, boolean selected) {
        if (!(tab instanceof TextView)) {
            return;
        }
        TextView text = (TextView) tab;
        if (selected) {
            text.setBackgroundResource(R.drawable.hud_config_sidebar_selected);
        } else {
            text.setBackgroundColor(Color.parseColor("#AA050708"));
        }
        text.setTextColor(Color.parseColor(selected ? "#111418" : "#D4D8DD"));
    }

    private void loadHudRenderSettings() {
        SharedPreferences preferences = getSharedPreferences(HUD_RENDER_PREFS, MODE_PRIVATE);
        int storedFpsLimit = preferences.getInt("fps_limit", hudFpsLimit);
        boolean migratedHighRefresh = preferences.getBoolean(HUD_HIGH_REFRESH_MIGRATION_KEY, false);
        if (!migratedHighRefresh && storedFpsLimit <= 60) {
            storedFpsLimit = HUD_FPS_LIMIT_MAX;
            preferences.edit()
                    .putInt("fps_limit", storedFpsLimit)
                    .putBoolean(HUD_HIGH_REFRESH_MIGRATION_KEY, true)
                    .apply();
        }
        hudRenderDistance = clampInt(
                preferences.getInt("render_distance", hudRenderDistance),
                HUD_RENDER_DISTANCE_MIN,
                HUD_RENDER_DISTANCE_MAX
        );
        hudFpsLimit = clampInt(
                storedFpsLimit,
                HUD_FPS_LIMIT_MIN,
                HUD_FPS_LIMIT_MAX
        );
        hudQuality = clampInt(
                preferences.getInt("quality", hudQuality),
                HUD_QUALITY_FPS,
                HUD_QUALITY_HIGH
        );
        hudShadowsEnabled = preferences.getBoolean("shadows", hudShadowsEnabled);
        hudEffectsEnabled = preferences.getBoolean("effects", hudEffectsEnabled);
        hudMapEnabled = preferences.getBoolean("map_enabled", true);
        hudControlAlphaPercent = clampInt(
                preferences.getInt("control_alpha", hudControlAlphaPercent),
                45,
                100
        );
    }

    private void loadHudNativeMenuSettings() {
        hudAndroidKeyboardEnabled = true;
        hudVoiceChatEnabled = true;
        hudChatMaxMessages = 5;
        SharedPreferences preferences = getSharedPreferences(SharedPreferenceCore.APP_PREFERENCES, MODE_PRIVATE);
        if (preferences.contains("ANDROID_KEYBOARD")) {
            hudAndroidKeyboardEnabled = preferences.getBoolean("ANDROID_KEYBOARD", hudAndroidKeyboardEnabled);
        }
        if (preferences.contains("VOICE_CHAT_ENABLE")) {
            hudVoiceChatEnabled = preferences.getBoolean("VOICE_CHAT_ENABLE", hudVoiceChatEnabled);
        }

        File settingsFile = new File(getExternalFilesDir(null), "SAMP/settings.ini");
        if (!settingsFile.exists()) {
            return;
        }

        try {
            Wini wini = new Wini(settingsFile);
            String keyboardValue = wini.get("gui", "androidkeyboard23123");
            if (keyboardValue != null) {
                hudAndroidKeyboardEnabled = parseHudBoolean(keyboardValue, hudAndroidKeyboardEnabled);
            }
            String voiceValue = wini.get("gui", "VoiceChatEnable");
            if (voiceValue != null) {
                hudVoiceChatEnabled = parseHudBoolean(voiceValue, hudVoiceChatEnabled);
            }
            String chatValue = wini.get("gui", "ChatMaxMessages");
            if (chatValue != null) {
                hudChatMaxMessages = normalizeHudChatMaxMessages(parseHudInt(chatValue, hudChatMaxMessages));
            }
        } catch (IOException error) {
            Log.e(TAG, "Could not read native menu settings.", error);
        }
    }

    private void saveHudNativeMenuSettings() {
        SharedPreferenceCore preferences = new SharedPreferenceCore();
        preferences.setBoolean(getApplicationContext(), "ANDROID_KEYBOARD", hudAndroidKeyboardEnabled);
        preferences.setBoolean(getApplicationContext(), "VOICE_CHAT_ENABLE", hudVoiceChatEnabled);

        File settingsFile = new File(getExternalFilesDir(null), "SAMP/settings.ini");
        File parent = settingsFile.getParentFile();
        if (parent != null && !parent.exists() && !parent.mkdirs()) {
            Log.w(TAG, "Could not create native menu settings directory.");
            return;
        }

        try {
            if (!settingsFile.exists() && !settingsFile.createNewFile()) {
                Log.w(TAG, "Could not create settings.ini for native menu settings.");
                return;
            }
            Wini wini = new Wini(settingsFile);
            wini.put("gui", "androidkeyboard23123", hudAndroidKeyboardEnabled);
            wini.put("gui", "androidkeyboard", hudAndroidKeyboardEnabled ? 1 : 0);
            wini.put("gui", "VoiceChatEnable", hudVoiceChatEnabled);
            wini.put("gui", "ChatMaxMessages", hudChatMaxMessages);
            wini.store();
        } catch (IOException error) {
            Log.e(TAG, "Could not store native menu settings.", error);
        }
    }

    private void commitHudNativeMenuSettingsLive() {
        saveHudNativeMenuSettings();
        applyHudNativeMenuSettingsRuntime();
        syncHudNativeMenuSettingsUi();
    }

    private void applyHudNativeMenuSettingsRuntime() {
        try {
            applyNativeMenuSettings(hudAndroidKeyboardEnabled, hudChatMaxMessages, hudVoiceChatEnabled);
            if (!hudVoiceChatEnabled) {
                setNativeVoipEnabled(false);
            }
        } catch (UnsatisfiedLinkError error) {
            Log.w(TAG, "Native menu runtime settings unavailable.", error);
        }
        hideSystemUI();
    }

    private void syncHudNativeMenuSettingsUi() {
        setHudToggleState(hudAndroidKeyboardToggle, "Teclado", hudAndroidKeyboardEnabled);
        setHudToggleState(hudVoiceChatToggle, "Voice", hudVoiceChatEnabled);
        if (hudChatLinesCycle != null) {
            hudChatLinesCycle.setText("Chat: " + hudChatMaxMessages + " linhas");
            hudChatLinesCycle.setBackgroundResource(R.drawable.launcher_field_surface_dark);
            hudChatLinesCycle.setTextColor(Color.parseColor("#D4DEE4"));
        }
    }

    private boolean parseHudBoolean(String value, boolean fallback) {
        if (value == null) {
            return fallback;
        }
        String normalized = value.trim().toLowerCase(Locale.US);
        if ("1".equals(normalized) || "true".equals(normalized) || "yes".equals(normalized)) {
            return true;
        }
        if ("0".equals(normalized) || "false".equals(normalized) || "no".equals(normalized)) {
            return false;
        }
        return fallback;
    }

    private int parseHudInt(String value, int fallback) {
        try {
            return Integer.parseInt(value.trim());
        } catch (NumberFormatException error) {
            return fallback;
        }
    }

    private int normalizeHudChatMaxMessages(int value) {
        if (value <= 5) {
            return 5;
        }
        if (value <= 10) {
            return 10;
        }
        if (value <= 15) {
            return 15;
        }
        return 20;
    }

    private int nextHudChatMaxMessages(int value) {
        int normalized = normalizeHudChatMaxMessages(value);
        if (normalized == 5) {
            return 10;
        }
        if (normalized == 10) {
            return 15;
        }
        if (normalized == 15) {
            return 20;
        }
        return 5;
    }

    private int nextHudFpsLimit(int value) {
        int safeValue = clampInt(value, HUD_FPS_LIMIT_MIN, HUD_FPS_LIMIT_MAX);
        if (safeValue < 60) {
            return 60;
        }
        if (safeValue < 90) {
            return 90;
        }
        if (safeValue < 120) {
            return 120;
        }
        return 30;
    }

    private void cycleHudControlAlpha() {
        int current = clampInt(hudControlAlphaPercent, 45, 100);
        for (int option : HUD_CONTROL_ALPHA_OPTIONS) {
            if (option > current) {
                hudControlAlphaPercent = option;
                return;
            }
        }
        hudControlAlphaPercent = HUD_CONTROL_ALPHA_OPTIONS[0];
    }

    private void selectHudQuality(int quality) {
        hudQuality = clampInt(quality, HUD_QUALITY_FPS, HUD_QUALITY_HIGH);
        if (hudQuality == HUD_QUALITY_FPS) {
            hudRenderDistance = 45;
            hudFpsLimit = HUD_FPS_LIMIT_MAX;
            hudShadowsEnabled = false;
            hudEffectsEnabled = false;
        } else if (hudQuality == HUD_QUALITY_HIGH) {
            hudRenderDistance = 140;
            hudFpsLimit = HUD_FPS_LIMIT_MAX;
            hudShadowsEnabled = true;
            hudEffectsEnabled = true;
        } else {
            hudRenderDistance = 80;
            hudFpsLimit = HUD_FPS_LIMIT_MAX;
            hudShadowsEnabled = false;
            hudEffectsEnabled = true;
        }
        syncHudRenderSettingsUi();
        commitHudRenderSettingsLive();
    }

    private void commitHudRenderSettingsLive() {
        saveHudRenderSettings();
        applyHudRenderSettingsRuntime();
    }

    private void saveHudRenderSettings() {
        runtimeLowRamMode = detectLowRamMode()
                || hudQuality == HUD_QUALITY_FPS
                || !hudEffectsEnabled;
        getSharedPreferences(HUD_RENDER_PREFS, MODE_PRIVATE)
                .edit()
                .putInt("render_distance", hudRenderDistance)
                .putInt("fps_limit", hudFpsLimit)
                .putInt("quality", hudQuality)
                .putBoolean("shadows", hudShadowsEnabled)
                .putBoolean("effects", hudEffectsEnabled)
                .putBoolean("map_enabled", hudMapEnabled)
                .putInt("control_alpha", hudControlAlphaPercent)
                .apply();
        writeHudRenderSettingsFile();
        writeNativeGameRenderSettingsFile();
    }

    private void writeHudRenderSettingsFile() {
        File root = getExternalFilesDir(null);
        if (root == null) {
            return;
        }

        File settingsFile = new File(root, "SAMP/render_settings.ini");
        File parent = settingsFile.getParentFile();
        if (parent != null && !parent.exists() && !parent.mkdirs()) {
            Log.w(TAG, "Could not create render settings directory.");
            return;
        }

        try {
            if (!settingsFile.exists() && !settingsFile.createNewFile()) {
                Log.w(TAG, "Could not create render_settings.ini.");
                return;
            }
            Wini wini = new Wini(settingsFile);
            wini.put("render", "profile", hudQualityLabel());
            wini.put("render", "distance", hudRenderDistance);
            wini.put("render", "fps_limit", hudFpsLimit);
            wini.put("render", "shadows", hudShadowsEnabled ? 1 : 0);
            wini.put("render", "effects", hudEffectsEnabled ? 1 : 0);
            wini.put("render", "map_enabled", hudMapEnabled ? 1 : 0);
            wini.put("render", "control_alpha", hudControlAlphaPercent);
            wini.store();
        } catch (IOException error) {
            Log.e(TAG, "Could not store render settings.", error);
        }
    }

    private void writeNativeGameRenderSettingsFile() {
        File root = getExternalFilesDir(null);
        if (root == null) {
            return;
        }

        File settingsFile = new File(root, "SAMP/settings.ini");
        File parent = settingsFile.getParentFile();
        if (parent != null && !parent.exists() && !parent.mkdirs()) {
            Log.w(TAG, "Could not create native settings directory.");
            return;
        }

        try {
            if (!settingsFile.exists() && !settingsFile.createNewFile()) {
                Log.w(TAG, "Could not create settings.ini for render settings.");
                return;
            }
            Wini wini = new Wini(settingsFile);
            wini.put("gui", "FPSLimit", hudFpsLimit);
            wini.put("gui", "fps", 0);
            wini.put("gui", "RenderDistance", hudRenderDistance);
            wini.put("gui", "LowRamMode", runtimeLowRamMode ? 1 : 0);
            wini.put("gui", "Shadows", hudShadowsEnabled ? 1 : 0);
            wini.put("gui", "Effects", hudEffectsEnabled ? 1 : 0);
            wini.put("gui", "HudMap", hudMapEnabled ? 1 : 0);
            wini.put("gui", "HudButtonAlpha", hudControlAlphaPercent);
            wini.store();
        } catch (IOException error) {
            Log.e(TAG, "Could not store native render settings.", error);
        }
    }

    private void applyHudRenderSettingsRuntime() {
        runtimeLowRamMode = detectLowRamMode()
                || hudQuality == HUD_QUALITY_FPS
                || !hudEffectsEnabled;
        setFrameRateLimit(hudFpsLimit);
        requestPreferredFrameRate(hudFpsLimit);
        try {
            applyNativeRenderSettings(hudFpsLimit, hudRenderDistance, hudShadowsEnabled, hudEffectsEnabled);
            setNativeRadarEnabled(hudMapEnabled);
        } catch (UnsatisfiedLinkError error) {
            Log.w(TAG, "Native render runtime settings unavailable.", error);
        }
        refreshRuntimeChrome();
        hideSystemUI();
    }

    private boolean shouldShowHudHdMapHotspot() {
        return hudMapEnabled
                && !shouldHideGameplayHudForNativeMenu()
                && !isHudSettingsVisible()
                && !isHudHdMapVisible()
                && !isHudChatPanelVisible()
                && !isPhoneOverlayVisible()
                && !isInventoryOverlayVisible()
                && !isWeaponWheelOverlayVisible()
                && !isPickupCreatorVisible()
                && !hudEditMode
                && !hudChatAreaMappingMode
                && !RadialMenu.menuVisible
                && !Radinho.radinhoVisible;
    }

    private void syncHudHdMapHotspot() {
        if (hud_main == null) {
            return;
        }
        if (hudHdMapHotspot == null) {
            hudHdMapHotspot = hud_main.findViewById(R.id.hud_hd_map_hotspot);
            if (hudHdMapHotspot != null) {
                installHudHdMapHotspotOpenHandler();
            }
        }
        if (hudHdMapHotspot == null) {
            return;
        }
        if (!hudEditMode) {
            installHudHdMapHotspotOpenHandler();
        }
        boolean visible = shouldShowHudHdMapHotspot();
        setVisibilityIfChanged(hudHdMapHotspot, visible ? View.VISIBLE : View.GONE);
        if (visible) {
            hudHdMapHotspot.bringToFront();
        }
    }

    private String hudQualityLabel() {
        if (hudQuality == HUD_QUALITY_FPS) {
            return "fps";
        }
        if (hudQuality == HUD_QUALITY_HIGH) {
            return "high";
        }
        return "balanced";
    }

    private void syncHudRenderSettingsUi() {
        if (hudRenderDistanceSeek != null) {
            hudRenderDistanceSeek.setProgress(hudRenderDistance - HUD_RENDER_DISTANCE_MIN);
        }
        if (hudFpsLimitSeek != null) {
            hudFpsLimitSeek.setProgress(hudFpsLimit - HUD_FPS_LIMIT_MIN);
        }
        syncHudRenderSettingsText();
        setHudQualityButtonState(hudQualityLowButton, hudQuality == HUD_QUALITY_FPS);
        setHudQualityButtonState(hudQualityBalancedButton, hudQuality == HUD_QUALITY_BALANCED);
        setHudQualityButtonState(hudQualityHighButton, hudQuality == HUD_QUALITY_HIGH);
        setHudToggleState(hudShadowsToggle, "Sombras", hudShadowsEnabled);
        setHudToggleState(hudEffectsToggle, "Efeitos", hudEffectsEnabled);
        syncHudControlAlphaButton();
        setHudToggleState(hudMapToggle, "Mapa", hudMapEnabled);
        syncSmartOptimizerUi();
        syncCjOutfitButtons();
    }

    private void syncHudControlAlphaButton() {
        if (hudControlAlphaCycle != null) {
            hudControlAlphaCycle.setText("Botoes: " + clampInt(hudControlAlphaPercent, 45, 100) + "%");
            hudControlAlphaCycle.setTextColor(Color.parseColor("#D4DEE4"));
        }
    }

    private void syncHudRenderSettingsText() {
        if (hudRenderDistanceValue != null) {
            hudRenderDistanceValue.setText(hudRenderDistance + "m");
        }
        if (hudFpsLimitValue != null) {
            hudFpsLimitValue.setText(hudFpsLimit + " FPS");
        }
        if (hudSettingsSummary != null) {
            String profile = hudQuality == HUD_QUALITY_FPS
                    ? "Perfil FPS"
                    : hudQuality == HUD_QUALITY_HIGH ? "Perfil alto" : "Perfil medio";
            hudSettingsSummary.setText(
                    "APLICADO INSTANTANEAMENTE | " + profile + " | " + hudRenderDistance + "m | " + hudFpsLimit + " FPS"
            );
        }
        syncCjOutfitButtons();
    }

    private void syncCjOutfitButtons() {
        if (hudCjOutfitCycleButton != null) {
            hudCjOutfitCycleButton.setText("Trocar roupa");
        }
        if (hudCjOutfitResetButton != null) {
            hudCjOutfitResetButton.setText("Resetar roupa");
        }
    }

    private void setHudQualityButtonState(TextView view, boolean selected) {
        if (view == null) {
            return;
        }
        view.setBackgroundResource(selected
                ? R.drawable.home_server_select_button
                : R.drawable.launcher_field_surface_dark);
        view.setTextColor(Color.parseColor(selected ? "#FFFFFFFF" : "#D4DEE4"));
    }

    private void setHudToggleState(TextView view, String label, boolean enabled) {
        if (view == null) {
            return;
        }
        String displayLabel = resolveHudConfigLabel(view, label);
        View parent = view.getParent() instanceof View ? (View) view.getParent() : null;
        View switchTrack = parent != null ? parent.findViewWithTag("hud_config_switch") : null;
        if (switchTrack != null) {
            view.setText(displayLabel);
            view.setTextColor(Color.parseColor(enabled ? "#FFFFFFFF" : "#D4DEE4"));
            switchTrack.setBackgroundResource(enabled
                    ? R.drawable.hud_config_switch_track_on
                    : R.drawable.hud_config_switch_track_off);
            View switchThumb = switchTrack.findViewWithTag("hud_config_switch_thumb");
            if (switchThumb != null) {
                ViewGroup.LayoutParams params = switchThumb.getLayoutParams();
                if (params instanceof FrameLayout.LayoutParams) {
                    FrameLayout.LayoutParams frameParams = (FrameLayout.LayoutParams) params;
                    frameParams.gravity = Gravity.CENTER_VERTICAL | (enabled ? Gravity.END : Gravity.START);
                    int margin = dpToPx(2.0f);
                    frameParams.setMargins(margin, 0, margin, 0);
                    switchThumb.setLayoutParams(frameParams);
                }
            }
            return;
        }
        view.setText(displayLabel + ": " + (enabled ? "ON" : "OFF"));
        view.setBackgroundResource(enabled
                ? R.drawable.home_server_select_button
                : R.drawable.launcher_field_surface_dark);
        view.setTextColor(Color.parseColor(enabled ? "#FFFFFFFF" : "#D4DEE4"));
    }

    private String resolveHudConfigLabel(TextView view, String fallback) {
        Object tag = view.getTag();
        if (tag instanceof String) {
            String tagText = (String) tag;
            if (tagText.startsWith("label:")) {
                return tagText.substring("label:".length());
            }
        }
        return fallback;
    }

    private boolean isHudSettingsVisible() {
        return hudSettingsPanel != null && hudSettingsPanel.getVisibility() == View.VISIBLE;
    }

    private void showHudSettingsPanel() {
        initializeHudSettingsPanel();
        if (hudSettingsPanel == null) {
            Toast.makeText(this, "Painel de configuracao indisponivel.", Toast.LENGTH_SHORT).show();
            return;
        }
        hideHudEditModeVisuals();
        hideHudHdMapOverlay();
        hideAttachEditorOverlay();
        hidePhoneOverlayInternal();
        hideInventoryOverlayInternal();
        hideWeaponWheelOverlayInternal();
        hideHudStatusPanel();
        hideHudChatPanel();
        if (RadialMenu.menuVisible && mRadialMenu != null) {
            mRadialMenu.hide();
        }
        if (Radinho.radinhoVisible && mRadinho != null) {
            mRadinho.hide();
        }
        hudSettingsPanel.bringToFront();
        setVisibilityIfChanged(hudSettingsPanel, View.VISIBLE);
        uiHandler.postDelayed(this::hideAttachEditorOverlay, 100L);
        refreshRuntimeChrome();
        hideSystemUI();
    }

    private void hideHudSettingsPanel() {
        if (hudSettingsPanel != null) {
            setVisibilityIfChanged(hudSettingsPanel, View.GONE);
        }
        refreshRuntimeChrome();
        hideSystemUI();
    }

    private boolean isHudHdMapVisible() {
        return hudHdMapLayer != null && hudHdMapLayer.getVisibility() == View.VISIBLE;
    }

    private void initializeHudHdMapOverlay() {
        if (hud_main == null) {
            return;
        }
        cacheHudControlViews();
        View closeButton = hud_main.findViewById(R.id.hud_hd_map_close);
        View resetButton = hud_main.findViewById(R.id.hud_hd_map_reset);
        View zoomInButton = hud_main.findViewById(R.id.hud_hd_map_zoom_in);
        View zoomOutButton = hud_main.findViewById(R.id.hud_hd_map_zoom_out);
        View clearRouteButton = hud_main.findViewById(R.id.hud_hd_map_clear_route);
        if (closeButton != null) {
            closeButton.setOnClickListener(v -> hideHudHdMapOverlay());
        }
        if (resetButton != null) {
            resetButton.setOnClickListener(v -> {
                if (hudHdMapView != null) {
                    hudHdMapView.resetView();
                }
            });
        }
        if (zoomInButton != null) {
            zoomInButton.setOnClickListener(v -> {
                if (hudHdMapView != null) {
                    hudHdMapView.zoomIn();
                }
            });
        }
        if (zoomOutButton != null) {
            zoomOutButton.setOnClickListener(v -> {
                if (hudHdMapView != null) {
                    hudHdMapView.zoomOut();
                }
            });
        }
        if (clearRouteButton != null) {
            clearRouteButton.setOnClickListener(v -> {
                noteHudControlInteraction();
                if (hudHdMapView != null) {
                    hudHdMapView.clearRouteDestination();
                    Toast.makeText(this, "Rota limpa.", Toast.LENGTH_SHORT).show();
                }
            });
        }
        for (HdMapPoi poi : HUD_HD_MAP_POIS) {
            View poiButton = hud_main.findViewById(poi.viewId);
            if (poiButton != null) {
                poiButton.setOnClickListener(v -> selectHudHdMapPoi(poi));
            }
        }
    }

    private void selectHudHdMapPoi(HdMapPoi poi) {
        if (poi == null || hudHdMapView == null) {
            return;
        }
        noteHudControlInteraction();
        focusHudHdMapOnRuntimePosition(false);
        hudHdMapView.setRouteDestination(poi.worldX, poi.worldY, true);
        Toast.makeText(this, "Rota marcada: " + poi.name, Toast.LENGTH_SHORT).show();
    }

    private void showHudHdMapOverlay() {
        if (hud_main == null) {
            return;
        }
        initializeHudHdMapOverlay();
        hideHudSettingsPanel();
        setHudOptionsPanelVisible(false);
        hideHudStatusPanel();
        hideHudChatPanel();
        hidePhoneOverlayInternal();
        hideInventoryOverlayInternal();
        hideWeaponWheelOverlayInternal();
        if (RadialMenu.menuVisible && mRadialMenu != null) {
            mRadialMenu.hide();
        }
        if (hudHdMapView != null) {
            hudHdMapView.setTileRoot("gtag-satellite");
            focusHudHdMapOnRuntimePosition(true);
        }
        if (hudHdMapLayer != null) {
            hudHdMapLayer.bringToFront();
            setVisibilityIfChanged(hudHdMapLayer, View.VISIBLE);
        }
        refreshRuntimeChrome();
        hideSystemUI();
    }

    private void hideHudHdMapOverlay() {
        if (hudHdMapLayer != null) {
            setVisibilityIfChanged(hudHdMapLayer, View.GONE);
        }
        if (hudHdMapView != null) {
            hudHdMapView.releaseMemory();
        }
        refreshRuntimeChrome();
        hideSystemUI();
    }

    private void focusHudHdMapOnRuntimePosition(boolean resetZoom) {
        if (hudHdMapView == null) {
            return;
        }
        float[] position = readLatestRuntimeWorldPosition();
        if (position != null) {
            hudHdMapView.setWorldFocus(position[0], position[1], resetZoom);
        } else if (resetZoom) {
            hudHdMapView.resetView();
        }
    }

    private float[] readLatestRuntimeWorldPosition() {
        File root = getExternalFilesDir(null);
        if (root == null) {
            return null;
        }
        File latest = new File(root, "SAMP/xyron_monitor/latest.json");
        if (!latest.isFile() || !latest.canRead()) {
            return null;
        }
        try (BufferedReader reader = new BufferedReader(new FileReader(latest))) {
            String payload = reader.readLine();
            if (payload == null || payload.trim().isEmpty()) {
                return null;
            }
            JSONObject sample = new JSONObject(payload);
            JSONObject nativeSnapshot = sample.optJSONObject("native");
            if (nativeSnapshot == null || !nativeSnapshot.optBoolean("nativeReady", false)) {
                return null;
            }
            double x = nativeSnapshot.optDouble("x", Double.NaN);
            double y = nativeSnapshot.optDouble("y", Double.NaN);
            if (!isFiniteMapCoordinate(x) || !isFiniteMapCoordinate(y)) {
                return null;
            }
            return new float[]{(float) x, (float) y};
        } catch (IOException | JSONException error) {
            Log.w(TAG, "Could not read HD map runtime focus.", error);
            return null;
        }
    }

    private static boolean isFiniteMapCoordinate(double value) {
        return !Double.isNaN(value) && !Double.isInfinite(value) && value >= -3200.0 && value <= 3200.0;
    }

    private void initializeHudStatusPanel() {
        if (hud_main == null) {
            return;
        }
        if (hudStatusPanel != null
                && hudStatusHealthBar != null
                && hudStatusArmourBar != null
                && hudStatusFoodBar != null
                && hudStatusThirstBar != null
                && hudStatusSleepBar != null) {
            return;
        }
        hudStatusPanel = hud_main.findViewById(R.id.hud_status_panel);
        hudStatusHealthBar = hud_main.findViewById(R.id.hud_status_health_bar);
        hudStatusArmourBar = hud_main.findViewById(R.id.hud_status_armour_bar);
        hudStatusFoodBar = hud_main.findViewById(R.id.hud_status_food_bar);
        hudStatusThirstBar = hud_main.findViewById(R.id.hud_status_thirst_bar);
        hudStatusSleepBar = hud_main.findViewById(R.id.hud_status_sleep_bar);
        hudStatusHealthText = hud_main.findViewById(R.id.hud_status_health_text);
        hudStatusArmourText = hud_main.findViewById(R.id.hud_status_armour_text);
        hudStatusFoodText = hud_main.findViewById(R.id.hud_status_food_text);
        hudStatusThirstText = hud_main.findViewById(R.id.hud_status_thirst_text);
        hudStatusSleepText = hud_main.findViewById(R.id.hud_status_sleep_text);
    }

    private void hideHudStatusPanel() {
        if (hudStatusPanel == null) {
            initializeHudStatusPanel();
        }
        if (hudStatusPanel != null) {
            setVisibilityIfChanged(hudStatusPanel, View.GONE);
        }
        hideSystemUI();
    }

    private void toggleHudStatusPanel() {
        initializeHudStatusPanel();
        if (hudStatusPanel == null) {
            return;
        }
        hideHudSettingsPanel();
        hideWeaponWheelOverlayInternal();
        hideHudChatPanel();
        boolean showPanel = hudStatusPanel.getVisibility() != View.VISIBLE;
        setVisibilityIfChanged(hudStatusPanel, showPanel ? View.VISIBLE : View.GONE);
        if (showPanel) {
            updateHudStatusPanelValues(
                    lastHudHp == Integer.MIN_VALUE ? 100 : lastHudHp,
                    lastHudArmour == Integer.MIN_VALUE ? 0 : lastHudArmour,
                    lastHudEat == Integer.MIN_VALUE ? 100 : lastHudEat
            );
        }
        hideSystemUI();
    }

    public void toggleHudStatusFromRadial() {
        hideHudStatusPanel();
    }

    public void toggleHudSettingsFromRadial() {
        openSafeHudSettings();
    }

    private int clampHudStatus(int value) {
        return Math.max(0, Math.min(100, value));
    }

    private void setCircularStatusProgress(ProgressBar progressBar, int value) {
        if (progressBar == null) {
            return;
        }
        int safeValue = clampHudStatus(value);
        progressBar.setMax(100);
        progressBar.setProgress(safeValue);
        if (progressBar.getProgressDrawable() != null) {
            progressBar.getProgressDrawable().setLevel(safeValue * 100);
        }
    }

    private void setLinearStatusProgress(ProgressBar progressBar, TextView valueText, int value) {
        int safeValue = clampHudStatus(value);
        if (progressBar != null) {
            if (progressBar.getMax() != 100) {
                progressBar.setMax(100);
            }
            if (progressBar.getProgress() != safeValue) {
                progressBar.setProgress(safeValue);
            }
        }
        if (valueText != null) {
            String nextText = safeValue + "%";
            if (!nextText.contentEquals(valueText.getText())) {
                valueText.setText(nextText);
            }
        }
    }

    private void updateHudStatusPanelValues(int hp, int armour, int eat) {
        if (hudStatusPanel == null) {
            initializeHudStatusPanel();
        }
        if (hudStatusPanel == null || hudStatusPanel.getVisibility() != View.VISIBLE) {
            return;
        }
        setLinearStatusProgress(hudStatusHealthBar, hudStatusHealthText, hp);
        setLinearStatusProgress(hudStatusArmourBar, hudStatusArmourText, armour);
        setLinearStatusProgress(hudStatusFoodBar, hudStatusFoodText, eat);
        setLinearStatusProgress(hudStatusThirstBar, hudStatusThirstText, eat - 10);
        setLinearStatusProgress(hudStatusSleepBar, hudStatusSleepText, 88);
    }

    private void startHudTopInfoTicker() {
        if (hud_main == null) {
            return;
        }
        cacheHudControlViews();
        updateHudTopInfo();
        if (hudTopInfoTickerRunning) {
            return;
        }
        hudTopInfoTickerRunning = true;
        uiHandler.removeCallbacks(hudTopInfoTickerRunnable);
        uiHandler.postDelayed(hudTopInfoTickerRunnable, HUD_TOP_INFO_INTERVAL_MS);
    }

    private void stopHudTopInfoTicker() {
        hudTopInfoTickerRunning = false;
        uiHandler.removeCallbacks(hudTopInfoTickerRunnable);
    }

    private void updateHudTopInfo() {
        if (hud_main == null) {
            return;
        }
        cacheHudControlViews();
        updateHudTopClockValue();
        updateHudTopCityValue();
        updateHudTopCoinValue(0);
    }

    private void updateHudTopClockValue() {
        if (hudTopClockValue == null) {
            return;
        }
        Calendar now = Calendar.getInstance();
        String nextText = String.format(
                Locale.US,
                "%02d:%02d",
                now.get(Calendar.HOUR_OF_DAY),
                now.get(Calendar.MINUTE)
        );
        if (!nextText.contentEquals(hudTopClockValue.getText())) {
            hudTopClockValue.setText(nextText);
        }
    }

    private void updateHudTopCityValue() {
        if (hudTopCityName == null) {
            return;
        }
        String cityName = "Los Santos";
        float[] position = readLatestRuntimeWorldPosition();
        if (position != null) {
            cityName = resolveHudCityName(position[0], position[1]);
        }
        if (!cityName.contentEquals(hudTopCityName.getText())) {
            hudTopCityName.setText(cityName);
        }
    }

    private String resolveHudCityName(float x, float y) {
        if (x < -1100.0f && y > -1600.0f) {
            return "San Fierro";
        }
        if (x > 700.0f && y > 350.0f) {
            return "Las Venturas";
        }
        if (x > -1100.0f && y < -700.0f) {
            return "Los Santos";
        }
        if (x < -900.0f) {
            return "San Fierro";
        }
        if (y > 450.0f) {
            return "Las Venturas";
        }
        return "San Andreas";
    }

    private void updateHudTopMoneyValue(int moneyValue) {
        if (hudTopMoneyValue == null) {
            cacheHudControlViews();
        }
        if (hudTopMoneyValue == null) {
            return;
        }
        String nextText = "$ " + formatHudMoney(moneyValue);
        if (!nextText.contentEquals(hudTopMoneyValue.getText())) {
            hudTopMoneyValue.setText(nextText);
        }
    }

    private void updateHudTopCoinValue(int coinValue) {
        if (hudTopCoinValue == null) {
            return;
        }
        String nextText = String.valueOf(Math.max(0, coinValue));
        if (!nextText.contentEquals(hudTopCoinValue.getText())) {
            hudTopCoinValue.setText(nextText);
        }
    }

    private void updateClassicHudValues(int hp, int armour, int eat, int moneyValue) {
        if (hud_main == null) {
            return;
        }
        hideLegacyHudChrome();
        setLinearStatusProgress(
                hud_main.findViewById(R.id.progressBarHeart),
                hud_main.findViewById(R.id.hpText),
                hp
        );
        setHudStatusBoxLevel(R.id.hud_status_box_health, hp);
        setLinearStatusProgress(
                hud_main.findViewById(R.id.progressBarArmour),
                hud_main.findViewById(R.id.Armourtext),
                armour
        );
        setHudStatusBoxLevel(R.id.hud_status_box_armour, armour);
        setLinearStatusProgress(
                hud_main.findViewById(R.id.progressBarEat),
                null,
                eat
        );
        setHudStatusBoxLevel(R.id.hud_status_box_food, eat);
        setHudNeedPercent(R.id.hud_food_percent, eat);
        setHudNeedRing(R.id.hud_food_ring, eat, HUD_NEED_FOOD_COLOR);
        setLinearStatusProgress(
                hud_main.findViewById(R.id.progressBarSede),
                null,
                eat - 10
        );
        setHudStatusBoxLevel(R.id.hud_status_box_thirst, eat - 10);
        setHudNeedPercent(R.id.hud_thirst_percent, eat - 10);
        setHudNeedRing(R.id.hud_thirst_ring, eat - 10, HUD_NEED_THIRST_COLOR);
        ProgressBar sleepBar = hud_main.findViewById(R.id.progressBarSono);
        if (sleepBar != null) {
            sleepBar.setVisibility(View.GONE);
        }
        setHudStatusBoxLevel(R.id.hud_status_box_sleep, 88);
        setHudNeedPercent(R.id.hud_sleep_percent, 88);
        setHudNeedRing(R.id.hud_sleep_ring, 88, HUD_NEED_SLEEP_COLOR);
        TextView moneyText = hud_main.findViewById(R.id.money);
        if (moneyText != null) {
            moneyText.setText(formatHudMoney(moneyValue));
        }
        updateHudTopMoneyValue(moneyValue);
    }

    private void setHudNeedPercent(int textId, int value) {
        if (hud_main == null) {
            return;
        }
        TextView valueText = hud_main.findViewById(textId);
        if (valueText == null) {
            return;
        }
        int safeValue = clampInt(value, 0, 100);
        String nextText = safeValue + "%";
        if (!nextText.contentEquals(valueText.getText())) {
            valueText.setText(nextText);
        }
    }

    private void setHudNeedRing(int ringId, int value, int color) {
        if (hud_main == null) {
            return;
        }
        View ring = hud_main.findViewById(ringId);
        if (!(ring instanceof HudCircleProgressView)) {
            return;
        }
        HudCircleProgressView circle = (HudCircleProgressView) ring;
        circle.setProgressColor(color);
        circle.setProgressValue(clampInt(value, 0, 100));
    }

    private void setHudStatusBoxLevel(int viewId, int value) {
        if (hud_main == null) {
            return;
        }
        View box = hud_main.findViewById(viewId);
        if (box == null) {
            return;
        }
        box.setAlpha(1.0f);
    }

    private String formatHudMoney(int moneyValue) {
        int safeMoney = Math.max(0, moneyValue);
        return String.format(Locale.US, "%,d", safeMoney).replace(',', '.');
    }

    private void updateClassicHudWeapon(int gunId, int ammo) {
        if (hud_main == null) {
            return;
        }
        View weaponLayout = hud_main.findViewById(R.id.WeaponShowLayout);
        ImageView gunImage = hud_main.findViewById(R.id.gunImg);
        ImageView fistImage = hud_main.findViewById(R.id.Fist);
        TextView ammoText = hud_main.findViewById(R.id.ammo);
        int safeGunId = isSupportedWeaponId(gunId) ? gunId : 0;
        if (weaponLayout != null) {
            weaponLayout.setVisibility(hudWeaponButtonVisible ? View.VISIBLE : View.GONE);
            weaponLayout.setContentDescription(weaponLabel(safeGunId));
            bindClassicWeaponHudClick(weaponLayout);
        }
        if (gunImage != null) {
            gunImage.setImageResource(resolveWeaponDrawable(safeGunId));
            gunImage.setVisibility(safeGunId == 0 ? View.GONE : View.VISIBLE);
        }
        if (fistImage != null) {
            fistImage.setImageResource(resolveWeaponDrawable(0));
            fistImage.setVisibility(safeGunId == 0 ? View.VISIBLE : View.GONE);
        }
        if (ammoText != null) {
            boolean showAmmo = safeGunId != 0 && ammo > 0;
            ammoText.setVisibility(showAmmo ? View.VISIBLE : View.GONE);
            if (showAmmo) {
                ammoText.setText(String.valueOf(Math.max(0, ammo)));
            }
        }
    }

    private void bindClassicWeaponHudClick(View weaponLayout) {
        if (weaponLayout == null) {
            return;
        }
        weaponLayout.setClickable(true);
        weaponLayout.setFocusable(true);
        weaponLayout.setOnClickListener(v -> {
            if (hudEditMode) {
                return;
            }
            noteHudControlInteraction();
            Log.i("LogCat", "Called classic weapon HUD");
            toggleWeaponWheelOverlay();
        });
    }

    private void initializeHudWeaponButton() {
        if (hud_main == null) {
            return;
        }
        hudWeaponButtonIcon = hud_main.findViewById(R.id.hud_weapon_button_icon);
        hudWeaponButtonAmmo = hud_main.findViewById(R.id.hud_weapon_button_ammo);
        cacheHudControlViews();
        if (hudWeaponButton != null) {
            setVisibilityIfChanged(hudWeaponButton, View.GONE);
        }
        renderedHudGunId = Integer.MIN_VALUE;
        renderedHudAmmo = Integer.MIN_VALUE;
        renderedHudAmmoVisible = false;
        applyClassicHudVisibilityFromQuickSettings();
        updateHudWeaponButton(lastHudGunId == Integer.MIN_VALUE ? 0 : lastHudGunId,
                lastHudAmmo == Integer.MIN_VALUE ? 0 : lastHudAmmo);
    }

    private void updateHudWeaponButton(int gunId, int ammo) {
        if (hud_main == null) {
            return;
        }
        if (!hudWeaponButtonVisible) {
            return;
        }
        if (hudWeaponButtonIcon == null) {
            hudWeaponButtonIcon = hud_main.findViewById(R.id.hud_weapon_button_icon);
        }
        if (hudWeaponButtonAmmo == null) {
            hudWeaponButtonAmmo = hud_main.findViewById(R.id.hud_weapon_button_ammo);
        }

        int safeGunId = isSupportedWeaponId(gunId) ? gunId : 0;
        if (hudWeaponButtonIcon != null && renderedHudGunId != safeGunId) {
            int weaponResId = resolveWeaponDrawable(safeGunId);
            hudWeaponButtonIcon.setImageResource(weaponResId);
            hudWeaponButtonIcon.setContentDescription(weaponLabel(safeGunId));
        }

        if (hudWeaponButtonAmmo != null) {
            boolean ammoVisible = safeGunId != 0 && ammo > 0;
            int safeAmmo = Math.max(0, ammo);
            if (!ammoVisible) {
                if (renderedHudAmmoVisible || hudWeaponButtonAmmo.getVisibility() != View.GONE) {
                    hudWeaponButtonAmmo.setVisibility(View.GONE);
                }
            } else {
                if (!renderedHudAmmoVisible || hudWeaponButtonAmmo.getVisibility() != View.VISIBLE) {
                    hudWeaponButtonAmmo.setVisibility(View.VISIBLE);
                }
                if (renderedHudAmmo != safeAmmo) {
                    hudWeaponButtonAmmo.setText(String.valueOf(safeAmmo));
                }
            }
            renderedHudAmmoVisible = ammoVisible;
            renderedHudAmmo = safeAmmo;
        }
        renderedHudGunId = safeGunId;
    }

    private int resolveWeaponDrawable(int safeGunId) {
        int cached = weaponDrawableCache.get(safeGunId, 0);
        if (cached != 0) {
            return cached;
        }
        int weaponResId = getResources().getIdentifier(
                "weapon_" + safeGunId,
                "drawable",
                getPackageName()
        );
        if (weaponResId == 0) {
            weaponResId = R.drawable.weapon_0;
        }
        weaponDrawableCache.put(safeGunId, weaponResId);
        return weaponResId;
    }

    private void updateHudWeaponButtonFromPayload(String payload) {
        int currentWeaponId = 0;
        int currentAmmo = 0;
        try {
            JSONArray weapons = new JSONArray(sanitizeWeaponWheelJson(payload));
            for (int i = 0; i < weapons.length(); i++) {
                JSONObject weapon = weapons.optJSONObject(i);
                if (weapon == null || !weapon.optBoolean("current", false)) {
                    continue;
                }
                currentWeaponId = weapon.optInt("id", 0);
                currentAmmo = weapon.optInt("ammo", 0);
                break;
            }
        } catch (JSONException ignored) {
            currentWeaponId = 0;
            currentAmmo = 0;
        }
        updateHudWeaponButton(currentWeaponId, currentAmmo);
    }

    private void hideLegacyHudChrome() {
        if (hud_main == null) {
            return;
        }
        if (legacyHudChromeHidden) {
            return;
        }
        applyClassicHudVisibilityFromQuickSettings();
        int[] hiddenViews = {
                R.id.backgroundmainHud,
                R.id.fistButton,
                R.id.btn_weapon_wheel,
                R.id.enter_passenger,
                R.id.vehicle_lock_butt,
                R.id.hud_classic_radar_ring,
                R.id.hide_chat
        };
        for (int viewId : hiddenViews) {
            View view = hud_main.findViewById(viewId);
            if (view != null) {
                view.setVisibility(View.GONE);
            }
        }
        legacyHudChromeHidden = true;
    }

    private void hideHudButtonsMovedToRadial() {
        if (hud_main == null) {
            return;
        }
        int[] hiddenViews = {
                R.id.btn_2,
                R.id.btn_weapon_wheel,
                R.id.btn_status_toggle,
                R.id.btn_game_settings,
                R.id.enter_passenger,
                R.id.vehicle_lock_butt
        };
        for (int viewId : hiddenViews) {
            View view = hud_main.findViewById(viewId);
            if (view != null) {
                view.setVisibility(View.GONE);
            }
        }
    }

    private void requestRuntimeHudShown() {
        if (hud_main == null) {
            return;
        }
        if (iShowHud == 1 && hud_main.getVisibility() == View.VISIBLE) {
            return;
        }
        if (hudRuntimeShowRequested) {
            return;
        }
        hudRuntimeShowRequested = true;
        runOnUiThread(() -> {
            hudRuntimeShowRequested = false;
            if (hud_main != null) {
                showhud();
            }
        });
    }

    public void showhud()
    {
        iShowHud = 1;
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                hudRuntimeShowRequested = false;
                if(hud_main == null) return;
                setVisibilityIfChanged(hud_main, View.VISIBLE);
                legacyHudChromeHidden = false;
                hud1 = hud_main.findViewById(R.id.hud);
                cacheHudControlViews();
                loadHudQuickSettings();

                setVisibilityIfChanged(hud1, View.VISIBLE);
                hud1.setAlpha(1.0f);
                applyHudLeftPanelLayout();
                initializeHudSettingsPanel();
                initializeHudStatusPanel();
                initializeHudChatPanel();
                initializeHudWeaponButton();
                initializeHudFpsCounter();
                initializeHudOptionsPanel();
                initializeHudSettingsGearButton();
                initializeHudChatAreaMapping();
                applyHudQuickSettingsVisibility();
                updateHudTopMoneyValue(lastHudMoney == Integer.MIN_VALUE ? 0 : lastHudMoney);
                updateHudTopInfo();
                applySavedHudChatArea();
                hud_main.post(SAMP.this::applySavedHudChatArea);
                hideLegacyHudChrome();
                hideHudButtonsMovedToRadial();

                // BotÃ£o para sentar como passageiro
                hideLegacyVehicleButtonViews();

                // BotÃ£o para trancar e destrancar
                setSourceHudControlsVisibility(View.VISIBLE);

                hudShortcutButtonsVisible = false;
                setHudShortcutButtonsVisibleInternal(true);
                refreshRuntimeChrome();

                if (!hudMainListenersBound) {
                View classicWeaponView = hud_main.findViewById(R.id.WeaponShowLayout);
                bindClassicWeaponHudClick(classicWeaponView);
                hud_main.findViewById(R.id.btn_weapon_wheel).setOnClickListener(new View.OnClickListener()
                {
                    @Override
                    public void onClick(View view)
                    {
                        Log.i("LogCat","Called weapon wheel button");
                        toggleWeaponWheelOverlay();
                    }
                });
                hud_main.findViewById(R.id.btn_chat_toggle).setOnClickListener(new View.OnClickListener()
                {
                    @Override
                    public void onClick(View view)
                    {
                        Log.i("LogCat","Called chat button");
                        openChatFromButton();
                    }
                });
                hud_main.findViewById(R.id.btn_status_toggle).setOnClickListener(new View.OnClickListener()
                {
                    @Override
                    public void onClick(View view)
                    {
                        Log.i("LogCat","Ignored status panel shortcut");
                        hideHudStatusPanel();
                    }
                });
                hud_main.findViewById(R.id.btn_1).setOnClickListener(new View.OnClickListener()
                {
                    @Override
                    public void onClick(View view)
                    {
                        Log.i("LogCat","Called change RadialMenu");
                        hideHudChatPanel();
                        mRadialMenu.show();
                    }
                });
                hud_main.findViewById(R.id.btn_0).setOnClickListener(new View.OnClickListener()
                {
                    @Override
                    public void onClick(View view)
                    {
                        Log.i("LogCat","Called shortcut buttons");
                        hideHudChatPanel();
                        hideHudSettingsPanel();
                        hideHudStatusPanel();
                        if (RadialMenu.menuVisible && mRadialMenu != null) {
                            mRadialMenu.hide();
                        }
                        toggleHudShortcutButtons();
                    }
                });
                installSourceControlTouchHandlers();
                hideLegacyVehicleButtonViews();
                hud_main.findViewById(R.id.hide_chat).setOnClickListener(new View.OnClickListener()
                {
                    @Override
                    public void onClick(View view)
                    {
                        Log.i("LogCat","Ignored legacy chat toggle");
                    }
                });
                hud_main.findViewById(R.id.btn_game_settings).setOnClickListener(new View.OnClickListener()
                {
                    @Override
                    public void onClick(View view)
                    {
                        Log.i("LogCat","Called safe HUD settings");
                        openSafeHudSettings();
                    }
                });
                installHudIdleTouchWakeups();
                hudMainListenersBound = true;
                }
                noteHudControlInteractionInternal();
            }
        });
    }

    int DialogId = 0;

    public void hidehud()
    {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                hudRuntimeShowRequested = false;
                if(hud_main == null) return;
                iShowHud = 0;
                resetNativeSourceControls();
                cancelHudControlsDocking();
                hidePhoneOverlayInternal();
                hideInventoryOverlayInternal();
                hideWeaponWheelOverlayInternal();
                hideHudStatusPanel();
                hideHudChatPanel();
                setHudFpsCounterVisible(false);
                stopHudFpsCounter();
                hudNativeChatVisible = true;
                showNativeSampChatOnly();
                try {
                    setNativeVoipEnabled(false);
                } catch (UnsatisfiedLinkError error) {
                    Log.w(TAG, "Native VOIP stop unavailable.", error);
                }
                setOverlayBlurEnabledInternal(false);
                hideHudSettingsPanel();
                hideHudHdMapOverlay();
                stopHudTopInfoTicker();
                hudEditMode = false;
                hudChatAreaMappingMode = false;
                if (hudChatMapLayer != null) {
                    setVisibilityIfChanged(hudChatMapLayer, View.GONE);
                }
                if (hudEditLayer != null) {
                    setVisibilityIfChanged(hudEditLayer, View.GONE);
                }
                if (hudEditDoneButton != null) {
                    setVisibilityIfChanged(hudEditDoneButton, View.GONE);
                }
                if (hudChatAreaHint != null) {
                    setVisibilityIfChanged(hudChatAreaHint, View.GONE);
                }
                setVisibilityIfChanged(hud_main, View.GONE);
                hud1 = hud_main.findViewById(R.id.hud);


                setVisibilityIfChanged(hud1, View.GONE);
                hud1.setAlpha(0.0f);

            }
        });
    }

    public void UpdateHud(int hp, int armour, int eat, int money,int gunId,int ammo)
    {
        requestRuntimeHudShown();
        synchronized (hudUpdateLock) {
            pendingHudHp = hp;
            pendingHudArmour = armour;
            pendingHudEat = eat;
            pendingHudMoney = money;
            pendingHudGunId = gunId;
            pendingHudAmmo = ammo;
            if (hudUpdateQueued) {
                return;
            }
            hudUpdateQueued = true;
        }
        uiHandler.post(this::applyPendingHudUpdate);
    }

    public void UpdateWeaponWheel(String weaponsJson)
    {
        synchronized (weaponWheelUpdateLock) {
            pendingWeaponWheelJson = weaponsJson;
            if (weaponWheelUpdateQueued) {
                return;
            }
            weaponWheelUpdateQueued = true;
        }
        uiHandler.post(this::applyPendingWeaponWheelUpdate);
    }

    public void ApplyUiBatch(String payload)
    {
        XyronUiBatchDispatcher.apply(this, payload);
    }

    private void applyPendingHudUpdate() {
        int hp;
        int armour;
        int eat;
        int money;
        int gunId;
        int ammo;
        synchronized (hudUpdateLock) {
            hp = pendingHudHp;
            armour = pendingHudArmour;
            eat = pendingHudEat;
            money = pendingHudMoney;
            gunId = pendingHudGunId;
            ammo = pendingHudAmmo;
            hudUpdateQueued = false;
        }

        if(hud_main == null) return;

        hideLegacyHudChrome();
        refreshSourceHudControlsRuntimeVisibility();

        if (hp != lastHudHp || armour != lastHudArmour || eat != lastHudEat || money != lastHudMoney) {
            lastHudHp = hp;
            lastHudArmour = armour;
            lastHudEat = eat;
            lastHudMoney = money;
            updateHudStatusPanelValues(hp, armour, eat);
            updateClassicHudValues(hp, armour, eat, money);
        }

        if (gunId != lastHudGunId || ammo != lastHudAmmo) {
            lastHudGunId = gunId;
            lastHudAmmo = ammo;
            updateClassicHudWeapon(gunId, ammo);
            updateHudWeaponButton(gunId, ammo);
            if (!receivedNativeWeaponWheelSnapshot && isWeaponWheelOverlayVisible()) {
                updateWeaponWheelFallback(gunId, ammo);
            }
        }
    }

    private void applyPendingWeaponWheelUpdate() {
        String weaponsJson;
        synchronized (weaponWheelUpdateLock) {
            weaponsJson = pendingWeaponWheelJson;
            weaponWheelUpdateQueued = false;
        }
        receivedNativeWeaponWheelSnapshot = true;
        String nextWeaponWheelJson = mergeLocalWeaponWheelInventory(sanitizeWeaponWheelJson(weaponsJson), -1);
        if (nextWeaponWheelJson.equals(lastWeaponWheelJson)) {
            return;
        }
        lastWeaponWheelJson = nextWeaponWheelJson;
        syncWeaponWheelPayload();
    }
    //API
    private String lerNomeDoPlayer() {
        try {
            File file = new File(getExternalFilesDir(null) + "/SAMP/settings.ini");
            if (file.exists()) {
                BufferedReader reader = new BufferedReader(new FileReader(file));
                String line;
                while ((line = reader.readLine()) != null) {
                    if (line.trim().startsWith("name")) {
                        String[] parts = line.split("=");
                        if (parts.length == 2) {
                            return parts[1].trim();
                        }
                    }
                }
                reader.close();
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
        return "Jogador"; // fallback se der erro
    }

    private void initializeRuntimeOverlays() {
        overlayBlurScrim = hud_main.findViewById(R.id.overlay_blur_scrim);
        hidePhoneOverlayInternal();
        hideInventoryOverlayInternal();
        hideWeaponWheelOverlayInternal();
        setOverlayBlurEnabledInternal(false);
        setNativeOverlayState(NATIVE_OVERLAY_NONE);
    }

    private String sanitizeWeaponWheelJson(String weaponsJson) {
        if (weaponsJson == null) {
            return DEFAULT_WEAPON_WHEEL_JSON;
        }
        String trimmed = weaponsJson.trim();
        if (trimmed.startsWith("[") && trimmed.endsWith("]")) {
            return trimmed;
        }
        return DEFAULT_WEAPON_WHEEL_JSON;
    }

    private String mergeLocalWeaponWheelInventory(String baseJson, int currentWeaponId) {
        LinkedHashMap<Integer, WeaponWheelEntry> entriesByCarrySlot = new LinkedHashMap<>();
        int resolvedCurrentWeaponId = currentWeaponId;

        try {
            JSONArray base = new JSONArray(sanitizeWeaponWheelJson(baseJson));
            for (int i = 0; i < base.length(); i++) {
                JSONObject raw = base.optJSONObject(i);
                if (raw == null) {
                    continue;
                }
                int id = raw.optInt("id", -1);
                if (!isSupportedWeaponId(id)) {
                    continue;
                }
                int carrySlot = weaponCarrySlot(id);
                if (carrySlot < 0 || entriesByCarrySlot.containsKey(carrySlot)) {
                    continue;
                }
                int ammo = Math.max(0, raw.optInt("ammo", 0));
                boolean current = raw.optBoolean("current", false);
                if (current && resolvedCurrentWeaponId < 0) {
                    resolvedCurrentWeaponId = id;
                }
                entriesByCarrySlot.put(carrySlot, new WeaponWheelEntry(id, ammo, current));
            }
        } catch (JSONException error) {
            entriesByCarrySlot.clear();
        }

        if (!entriesByCarrySlot.containsKey(0)) {
            LinkedHashMap<Integer, WeaponWheelEntry> reordered = new LinkedHashMap<>();
            reordered.put(0, new WeaponWheelEntry(0, 0, false));
            reordered.putAll(entriesByCarrySlot);
            entriesByCarrySlot = reordered;
        }

        for (Map.Entry<Integer, Integer> localWeapon : localWeaponWheelInventory.entrySet()) {
            int id = localWeapon.getKey();
            if (!isSupportedWeaponId(id) || id == 0) {
                continue;
            }
            int ammo = Math.max(0, localWeapon.getValue());
            int carrySlot = weaponCarrySlot(id);
            if (carrySlot < 0) {
                continue;
            }
            WeaponWheelEntry existing = entriesByCarrySlot.get(carrySlot);
            if (existing == null) {
                entriesByCarrySlot.put(carrySlot, new WeaponWheelEntry(id, ammo, false));
            } else {
                existing.id = id;
                existing.ammo = Math.max(existing.ammo, ammo);
            }
        }

        if (resolvedCurrentWeaponId < 0 || !containsWeaponEntry(entriesByCarrySlot, resolvedCurrentWeaponId)) {
            resolvedCurrentWeaponId = 0;
        }

        JSONArray output = new JSONArray();
        int carriedWeapons = 0;
        for (WeaponWheelEntry entry : entriesByCarrySlot.values()) {
            if (entry.id != 0) {
                if (carriedWeapons >= WEAPON_WHEEL_MAX_CARRIED_WEAPONS) {
                    continue;
                }
                carriedWeapons++;
            }
            JSONObject item = new JSONObject();
            try {
                item.put("id", entry.id);
                item.put("ammo", Math.max(0, entry.ammo));
                item.put("current", entry.id == resolvedCurrentWeaponId);
                output.put(item);
            } catch (JSONException ignored) {
                // JSONObject backed by primitive values should not fail here.
            }
        }
        return output.length() > 0 ? output.toString() : DEFAULT_WEAPON_WHEEL_JSON;
    }

    private static boolean containsWeaponEntry(LinkedHashMap<Integer, WeaponWheelEntry> entriesByCarrySlot, int weaponId) {
        for (WeaponWheelEntry entry : entriesByCarrySlot.values()) {
            if (entry.id == weaponId) {
                return true;
            }
        }
        return false;
    }

    private void updateWeaponWheelFallback(int gunId, int ammo) {
        int safeGunId = gunId >= 0 && gunId <= 46 ? gunId : 0;
        int safeAmmo = Math.max(0, ammo);
        String baseJson;
        if (safeGunId == 0) {
            baseJson = DEFAULT_WEAPON_WHEEL_JSON;
        } else {
            baseJson = "[{\"id\":0,\"ammo\":0,\"current\":false},"
                    + "{\"id\":" + safeGunId
                    + ",\"ammo\":" + safeAmmo
                    + ",\"current\":true}]";
        }
        lastWeaponWheelJson = mergeLocalWeaponWheelInventory(baseJson, safeGunId);
        syncWeaponWheelPayload(false);
    }

    private void refreshFallbackWeaponWheelIfNeeded() {
        if (receivedNativeWeaponWheelSnapshot) {
            return;
        }
        updateWeaponWheelFallback(
                lastHudGunId == Integer.MIN_VALUE ? 0 : lastHudGunId,
                lastHudAmmo == Integer.MIN_VALUE ? 0 : lastHudAmmo
        );
    }

    private void syncWeaponWheelPayload() {
        syncWeaponWheelPayload(true);
    }

    private void syncWeaponWheelPayload(boolean updateHudButton) {
        String payload = sanitizeWeaponWheelJson(lastWeaponWheelJson);
        if (updateHudButton && hudWeaponButtonVisible) {
            updateHudWeaponButtonFromPayload(payload);
        }
    }

    private static boolean isSupportedWeaponId(int weaponId) {
        return weaponId >= 0 && weaponId <= 18 || weaponId >= 22 && weaponId <= 46;
    }

    private static int weaponCarrySlot(int weaponId) {
        if (weaponId == 0) return 0;
        if (weaponId >= 1 && weaponId <= 9) return 1;
        if (weaponId >= 10 && weaponId <= 15) return 10;
        if (weaponId >= 16 && weaponId <= 18) return 8;
        if (weaponId >= 22 && weaponId <= 24) return 2;
        if (weaponId >= 25 && weaponId <= 27) return 3;
        if (weaponId == 28 || weaponId == 29 || weaponId == 32) return 4;
        if (weaponId == 30 || weaponId == 31) return 5;
        if (weaponId == 33 || weaponId == 34) return 6;
        if (weaponId >= 35 && weaponId <= 38) return 7;
        if (weaponId == 39 || weaponId == 40) return 8;
        if (weaponId >= 41 && weaponId <= 43) return 9;
        if (weaponId == 44 || weaponId == 45) return 11;
        if (weaponId == 46) return 12;
        return -1;
    }

    private static int defaultAmmoForWeapon(int weaponId) {
        if (weaponId == 0) {
            return 0;
        }
        if (weaponId >= 1 && weaponId <= 15) {
            return 1;
        }
        switch (weaponId) {
            case 16:
            case 17:
            case 18:
            case 39:
                return 10;
            case 35:
            case 36:
                return 12;
            case 37:
            case 38:
            case 41:
            case 42:
                return 500;
            case 40:
                return 1;
            case 43:
                return 36;
            default:
                return 250;
        }
    }

    private static String weaponLabel(int weaponId) {
        switch (weaponId) {
            case 0: return "Punho";
            case 1: return "Soco ingles";
            case 2: return "Taco de golf";
            case 3: return "Cassetete";
            case 4: return "Faca";
            case 5: return "Taco";
            case 6: return "Pa";
            case 7: return "Taco de sinuca";
            case 8: return "Katana";
            case 9: return "Motosserra";
            case 16: return "Granada";
            case 17: return "Gas lacrimogeneo";
            case 18: return "Molotov";
            case 22: return "Pistola";
            case 23: return "Pistola silenciada";
            case 24: return "Desert Eagle";
            case 25: return "Shotgun";
            case 26: return "Sawnoff";
            case 27: return "Combat Shotgun";
            case 28: return "Uzi";
            case 29: return "MP5";
            case 30: return "AK-47";
            case 31: return "M4";
            case 32: return "Tec-9";
            case 33: return "Rifle";
            case 34: return "Sniper";
            case 35: return "RPG";
            case 36: return "Heat Seeker";
            case 37: return "Lanca-chamas";
            case 38: return "Minigun";
            case 39: return "Satchel";
            case 40: return "Detonador";
            case 41: return "Spray";
            case 42: return "Extintor";
            case 43: return "Camera";
            case 44: return "Visao noturna";
            case 45: return "Visao termica";
            case 46: return "Paraquedas";
            default: return "Arma " + weaponId;
        }
    }

    private static final class WeaponWheelEntry {
        int id;
        int ammo;
        final boolean current;
        final boolean empty;

        WeaponWheelEntry(int id, int ammo, boolean current) {
            this(id, ammo, current, false);
        }

        WeaponWheelEntry(int id, int ammo, boolean current, boolean empty) {
            this.id = id;
            this.ammo = ammo;
            this.current = current;
            this.empty = empty;
        }
    }

    private void ensureJavaWeaponWheelOverlay() {
        if (weaponWheelOverlay != null || hud_main == null) {
            return;
        }

        weaponWheelOverlay = new FrameLayout(this);
        weaponWheelOverlay.setClickable(true);
        weaponWheelOverlay.setFocusable(true);
        weaponWheelOverlay.setVisibility(View.GONE);
        weaponWheelOverlay.setBackgroundColor(Color.TRANSPARENT);
        weaponWheelOverlay.setElevation(dpToPx(18.0f));
        weaponWheelOverlay.setOnClickListener(v -> hideWeaponWheelOverlay());
        weaponWheelOverlay.setOnTouchListener((view, event) -> {
            if (event.getActionMasked() == MotionEvent.ACTION_UP) {
                hideWeaponWheelOverlay();
            }
            return true;
        });

        ConstraintLayout.LayoutParams params = new ConstraintLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
        );
        hud_main.addView(weaponWheelOverlay, params);
    }

    private void showJavaWeaponWheelOverlay() {
        ensureJavaWeaponWheelOverlay();
        if (weaponWheelOverlay == null) {
            return;
        }
        weaponWheelOverlayVisible = true;
        weaponWheelOverlay.setEnabled(true);
        weaponWheelOverlay.setVisibility(View.VISIBLE);
        weaponWheelOverlay.bringToFront();
        renderJavaWeaponWheelOverlay();
    }

    private void hideJavaWeaponWheelOverlay() {
        weaponWheelOverlayVisible = false;
        weaponWheelDragWeaponId = -1;
        weaponWheelDragMoved = false;
        weaponWheelNodeViews.clear();
        if (weaponWheelOverlay != null) {
            weaponWheelOverlay.setVisibility(View.GONE);
            weaponWheelOverlay.removeAllViews();
        }
    }

    private void renderJavaWeaponWheelOverlay() {
        if (!weaponWheelOverlayVisible || weaponWheelOverlay == null) {
            return;
        }
        int width = weaponWheelOverlay.getWidth();
        int height = weaponWheelOverlay.getHeight();
        if (width <= 0 || height <= 0) {
            weaponWheelOverlay.post(this::renderJavaWeaponWheelOverlay);
            return;
        }

        weaponWheelOverlay.removeAllViews();
        weaponWheelNodeViews.clear();

        View scrim = new View(this);
        scrim.setBackgroundColor(Color.parseColor("#99020611"));
        scrim.setClickable(false);
        weaponWheelOverlay.addView(scrim, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
        ));

        List<WeaponWheelEntry> entries = getOrderedWeaponWheelEntries();
        List<WeaponWheelEntry> slots = buildWeaponWheelSlots(entries);
        int count = WEAPON_WHEEL_VISIBLE_SLOT_COUNT;
        float centerX = width * 0.5f;
        float centerY = height * 0.52f;
        float minSide = Math.min(width, height);
        float ringRadius = minSide * 0.27f;
        int nodeSize = dpToPx(72.0f);

        View backplate = buildWeaponWheelBackplate(centerX, centerY, ringRadius, count);
        weaponWheelOverlay.addView(backplate, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
        ));

        TextView title = buildWeaponWheelText("ARMAS", 16.0f, Color.WHITE, true);
        FrameLayout.LayoutParams titleParams = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT,
                dpToPx(36.0f)
        );
        titleParams.leftMargin = dpToPx(26.0f);
        titleParams.topMargin = dpToPx(18.0f);
        weaponWheelOverlay.addView(title, titleParams);

        TextView close = buildWeaponWheelText("X", 14.0f, Color.WHITE, true);
        close.setGravity(Gravity.CENTER);
        close.setBackground(makeWeaponWheelRect(Color.parseColor("#221A2230"), Color.parseColor("#35FFFFFF"), 12.0f));
        close.setOnClickListener(v -> hideWeaponWheelOverlay());
        FrameLayout.LayoutParams closeParams = new FrameLayout.LayoutParams(dpToPx(42.0f), dpToPx(36.0f));
        closeParams.leftMargin = width - dpToPx(62.0f);
        closeParams.topMargin = dpToPx(20.0f);
        weaponWheelOverlay.addView(close, closeParams);

        FrameLayout centerCard = buildWeaponWheelCenter(entries);
        int centerSize = dpToPx(104.0f);
        FrameLayout.LayoutParams centerParams = new FrameLayout.LayoutParams(centerSize, centerSize);
        centerParams.leftMargin = Math.round(centerX - centerSize / 2.0f);
        centerParams.topMargin = Math.round(centerY - centerSize / 2.0f);
        weaponWheelOverlay.addView(centerCard, centerParams);

        for (int i = 0; i < slots.size(); i++) {
            WeaponWheelEntry entry = slots.get(i);
            double angle = -Math.PI / 2.0d + (Math.PI * 2.0d * i / count);
            float nodeCenterX = centerX + (float) Math.cos(angle) * ringRadius;
            float nodeCenterY = centerY + (float) Math.sin(angle) * ringRadius;
            FrameLayout node = buildWeaponWheelNode(entry);
            FrameLayout.LayoutParams nodeParams = new FrameLayout.LayoutParams(nodeSize, nodeSize);
            nodeParams.leftMargin = Math.round(nodeCenterX - nodeSize / 2.0f);
            nodeParams.topMargin = Math.round(nodeCenterY - nodeSize / 2.0f);
            weaponWheelOverlay.addView(node, nodeParams);
            if (!entry.empty) {
                weaponWheelNodeViews.put(entry.id, node);
            }
        }
    }

    private View buildWeaponWheelBackplate(float centerX, float centerY, float ringRadius, int count) {
        View view = new View(this) {
            private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);

            @Override
            protected void onDraw(Canvas canvas) {
                super.onDraw(canvas);
                float outerRadius = ringRadius + dpToPx(count <= 1 ? 24.0f : 54.0f);
                float innerRadius = dpToPx(62.0f);

                paint.setStyle(Paint.Style.FILL);
                paint.setColor(Color.parseColor("#2E101827"));
                canvas.drawCircle(centerX, centerY, outerRadius, paint);

                paint.setStyle(Paint.Style.STROKE);
                paint.setStrokeWidth(dpToPx(1.0f));
                paint.setColor(Color.parseColor("#2EFFFFFF"));
                canvas.drawCircle(centerX, centerY, outerRadius, paint);
                canvas.drawCircle(centerX, centerY, ringRadius, paint);

                paint.setColor(Color.parseColor("#22FFFFFF"));
                int spokes = Math.max(3, count);
                for (int i = 0; i < spokes; i++) {
                    double angle = -Math.PI / 2.0d + (Math.PI * 2.0d * i / spokes);
                    float startX = centerX + (float) Math.cos(angle) * innerRadius;
                    float startY = centerY + (float) Math.sin(angle) * innerRadius;
                    float endX = centerX + (float) Math.cos(angle) * outerRadius;
                    float endY = centerY + (float) Math.sin(angle) * outerRadius;
                    canvas.drawLine(startX, startY, endX, endY, paint);
                }

                paint.setStrokeWidth(dpToPx(2.0f));
                paint.setColor(Color.parseColor("#55FFB84D"));
                canvas.drawCircle(centerX, centerY, innerRadius, paint);
            }
        };
        view.setWillNotDraw(false);
        view.setClickable(false);
        return view;
    }

    private TextView buildWeaponWheelText(String text, float sp, int color, boolean bold) {
        TextView view = new TextView(this);
        view.setText(text);
        view.setTextColor(color);
        view.setTextSize(TypedValue.COMPLEX_UNIT_SP, sp);
        view.setIncludeFontPadding(false);
        view.setGravity(Gravity.CENTER_VERTICAL);
        if (bold) {
            view.setTypeface(view.getTypeface(), android.graphics.Typeface.BOLD);
        }
        return view;
    }

    private FrameLayout buildWeaponWheelCenter(List<WeaponWheelEntry> entries) {
        WeaponWheelEntry current = null;
        for (WeaponWheelEntry entry : entries) {
            if (entry.current) {
                current = entry;
                break;
            }
        }
        if (current == null && !entries.isEmpty()) {
            current = entries.get(0);
        }

        FrameLayout card = new FrameLayout(this);
        card.setClickable(true);
        final int currentWeaponId = current == null ? 0 : current.id;
        card.setOnClickListener(view -> selectRuntimeWeapon(currentWeaponId));
        card.setBackground(makeWeaponWheelOval(Color.parseColor("#F2161C2A"), Color.parseColor("#55FFFFFF"), 1.0f));

        ImageView icon = new ImageView(this);
        icon.setScaleType(ImageView.ScaleType.FIT_CENTER);
        icon.setImageResource(resolveWeaponDrawable(currentWeaponId));
        FrameLayout.LayoutParams iconParams = new FrameLayout.LayoutParams(dpToPx(56.0f), dpToPx(44.0f), Gravity.CENTER);
        iconParams.topMargin = -dpToPx(8.0f);
        card.addView(icon, iconParams);

        TextView label = buildWeaponWheelText(current == null ? "Punho" : weaponLabel(current.id), 10.0f, Color.WHITE, true);
        label.setGravity(Gravity.CENTER);
        FrameLayout.LayoutParams labelParams = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                dpToPx(24.0f),
                Gravity.BOTTOM
        );
        labelParams.leftMargin = dpToPx(10.0f);
        labelParams.rightMargin = dpToPx(10.0f);
        labelParams.bottomMargin = dpToPx(14.0f);
        card.addView(label, labelParams);

        TextView hint = buildWeaponWheelText("na mao", 7.2f, Color.parseColor("#FFFFB86B"), true);
        hint.setGravity(Gravity.CENTER);
        FrameLayout.LayoutParams hintParams = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                dpToPx(18.0f),
                Gravity.BOTTOM
        );
        hintParams.bottomMargin = dpToPx(5.0f);
        card.addView(hint, hintParams);
        return card;
    }

    private FrameLayout buildWeaponWheelNode(WeaponWheelEntry entry) {
        FrameLayout node = new FrameLayout(this);
        node.setClickable(!entry.empty);
        node.setFocusable(!entry.empty);
        int stroke = entry.empty ? Color.parseColor("#36FFFFFF") : (entry.current ? Color.parseColor("#FFFFB84D") : Color.parseColor("#3CFFFFFF"));
        int fill = entry.empty ? Color.parseColor("#90101824") : (entry.current ? Color.parseColor("#F226354E") : Color.parseColor("#F2121928"));
        node.setBackground(makeWeaponWheelOval(fill, stroke, entry.current ? 2.0f : 1.0f));
        if (!entry.empty) {
            node.setOnTouchListener((view, event) -> handleWeaponWheelNodeTouch(view, entry.id, event));
        }

        if (entry.empty) {
            TextView emptyMark = buildWeaponWheelText("+", 18.0f, Color.parseColor("#8AFFFFFF"), true);
            emptyMark.setGravity(Gravity.CENTER);
            FrameLayout.LayoutParams markParams = new FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    dpToPx(38.0f),
                    Gravity.TOP
            );
            markParams.topMargin = dpToPx(6.0f);
            node.addView(emptyMark, markParams);
        } else {
            ImageView icon = new ImageView(this);
            icon.setScaleType(ImageView.ScaleType.FIT_CENTER);
            icon.setAdjustViewBounds(true);
            icon.setImageResource(resolveWeaponDrawable(entry.id));
            icon.setContentDescription(weaponLabel(entry.id));
            FrameLayout.LayoutParams iconParams = new FrameLayout.LayoutParams(dpToPx(50.0f), dpToPx(38.0f), Gravity.TOP | Gravity.CENTER_HORIZONTAL);
            iconParams.topMargin = dpToPx(6.0f);
            node.addView(icon, iconParams);
        }

        TextView label = buildWeaponWheelText(entry.empty ? "Vazio" : weaponShortLabel(entry.id), 7.6f, entry.empty ? Color.parseColor("#A8FFFFFF") : Color.WHITE, true);
        label.setGravity(Gravity.CENTER);
        FrameLayout.LayoutParams labelParams = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                dpToPx(18.0f),
                Gravity.BOTTOM
        );
        labelParams.leftMargin = dpToPx(4.0f);
        labelParams.rightMargin = dpToPx(4.0f);
        labelParams.bottomMargin = dpToPx(4.0f);
        node.addView(label, labelParams);

        if (entry.id != 0 && entry.ammo > 0) {
            TextView ammo = buildWeaponWheelText(String.valueOf(Math.max(0, entry.ammo)), 7.0f, Color.WHITE, true);
            ammo.setGravity(Gravity.CENTER);
            ammo.setBackground(makeWeaponWheelRect(Color.parseColor("#DD020611"), Color.parseColor("#45FFFFFF"), 6.0f));
            FrameLayout.LayoutParams ammoParams = new FrameLayout.LayoutParams(dpToPx(36.0f), dpToPx(16.0f), Gravity.TOP | Gravity.RIGHT);
            ammoParams.topMargin = dpToPx(6.0f);
            ammoParams.rightMargin = dpToPx(6.0f);
            node.addView(ammo, ammoParams);
        }
        return node;
    }

    private boolean handleWeaponWheelNodeTouch(View view, int weaponId, MotionEvent event) {
        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                if (!isSupportedWeaponId(weaponId)) {
                    return true;
                }
                weaponWheelDragWeaponId = weaponId;
                weaponWheelDragMoved = false;
                view.bringToFront();
                return true;
            case MotionEvent.ACTION_MOVE:
                return true;
            case MotionEvent.ACTION_UP:
                weaponWheelDragWeaponId = -1;
                weaponWheelDragMoved = false;
                view.setTranslationX(0.0f);
                view.setTranslationY(0.0f);
                if (isSupportedWeaponId(weaponId)) {
                    selectRuntimeWeapon(weaponId);
                }
                return true;
            case MotionEvent.ACTION_CANCEL:
                weaponWheelDragWeaponId = -1;
                weaponWheelDragMoved = false;
                view.setTranslationX(0.0f);
                view.setTranslationY(0.0f);
                return true;
            default:
                return true;
        }
    }

    private int findWeaponWheelDropTarget(int draggedWeaponId, float rawX, float rawY) {
        if (weaponWheelOverlay == null) {
            return -1;
        }
        int[] overlayLocation = new int[2];
        weaponWheelOverlay.getLocationOnScreen(overlayLocation);
        float x = rawX - overlayLocation[0];
        float y = rawY - overlayLocation[1];
        int bestWeaponId = -1;
        float bestDistance = dpToPx(72.0f);
        for (Map.Entry<Integer, View> nodeEntry : weaponWheelNodeViews.entrySet()) {
            int targetWeaponId = nodeEntry.getKey();
            View node = nodeEntry.getValue();
            if (targetWeaponId == draggedWeaponId || node == null) {
                continue;
            }
            float centerX = node.getX() + node.getWidth() / 2.0f;
            float centerY = node.getY() + node.getHeight() / 2.0f;
            float distance = (float) Math.hypot(x - centerX, y - centerY);
            if (distance < bestDistance) {
                bestDistance = distance;
                bestWeaponId = targetWeaponId;
            }
        }
        return bestWeaponId;
    }

    private void moveWeaponWheelDisplayOrder(int draggedWeaponId, int targetWeaponId) {
        List<WeaponWheelEntry> currentEntries = getOrderedWeaponWheelEntries();
        ArrayList<Integer> nextOrder = new ArrayList<>();
        for (WeaponWheelEntry entry : currentEntries) {
            if (entry.id != draggedWeaponId) {
                nextOrder.add(entry.id);
            }
        }
        int targetIndex = nextOrder.indexOf(targetWeaponId);
        if (targetIndex < 0) {
            nextOrder.add(draggedWeaponId);
        } else {
            nextOrder.add(targetIndex, draggedWeaponId);
        }
        weaponWheelDisplayOrder.clear();
        weaponWheelDisplayOrder.addAll(nextOrder);
    }

    private List<WeaponWheelEntry> getOrderedWeaponWheelEntries() {
        ArrayList<WeaponWheelEntry> parsed = new ArrayList<>();
        try {
            JSONArray weapons = new JSONArray(sanitizeWeaponWheelJson(lastWeaponWheelJson));
            for (int i = 0; i < weapons.length(); i++) {
                JSONObject raw = weapons.optJSONObject(i);
                if (raw == null) {
                    continue;
                }
                int id = raw.optInt("id", -1);
                if (!isSupportedWeaponId(id) || containsWeaponId(parsed, id)) {
                    continue;
                }
                parsed.add(new WeaponWheelEntry(
                        id,
                        Math.max(0, raw.optInt("ammo", 0)),
                        raw.optBoolean("current", false)
                ));
            }
        } catch (JSONException ignored) {
            parsed.clear();
        }
        if (parsed.isEmpty()) {
            parsed.add(new WeaponWheelEntry(0, 0, true));
        }

        ArrayList<WeaponWheelEntry> ordered = new ArrayList<>();
        for (Integer orderedId : weaponWheelDisplayOrder) {
            WeaponWheelEntry entry = findWeaponEntry(parsed, orderedId == null ? -1 : orderedId);
            if (entry != null && findWeaponEntry(ordered, entry.id) == null) {
                ordered.add(entry);
            }
        }
        for (WeaponWheelEntry entry : parsed) {
            if (findWeaponEntry(ordered, entry.id) == null) {
                ordered.add(entry);
            }
        }

        ArrayList<WeaponWheelEntry> limited = limitWeaponWheelEntries(ordered);
        weaponWheelDisplayOrder.clear();
        for (WeaponWheelEntry entry : limited) {
            weaponWheelDisplayOrder.add(entry.id);
        }
        return limited;
    }

    private List<WeaponWheelEntry> buildWeaponWheelSlots(List<WeaponWheelEntry> entries) {
        ArrayList<WeaponWheelEntry> slots = new ArrayList<>();
        for (WeaponWheelEntry entry : entries) {
            if (entry == null || entry.empty) {
                continue;
            }
            if (slots.size() >= WEAPON_WHEEL_VISIBLE_SLOT_COUNT) {
                break;
            }
            slots.add(entry);
        }
        while (slots.size() < WEAPON_WHEEL_VISIBLE_SLOT_COUNT) {
            int index = slots.size();
            slots.add(new WeaponWheelEntry(WEAPON_WHEEL_EMPTY_SLOT_BASE_ID - index, 0, false, true));
        }
        return slots;
    }

    private ArrayList<WeaponWheelEntry> limitWeaponWheelEntries(List<WeaponWheelEntry> entries) {
        ArrayList<WeaponWheelEntry> limited = new ArrayList<>();
        WeaponWheelEntry fist = findWeaponEntry(entries, 0);
        if (fist != null) {
            limited.add(fist);
        }

        WeaponWheelEntry current = null;
        for (WeaponWheelEntry entry : entries) {
            if (entry.current) {
                current = entry;
                break;
            }
        }
        if (current != null && current.id != 0 && findWeaponEntry(limited, current.id) == null) {
            limited.add(current);
        }

        for (WeaponWheelEntry entry : entries) {
            if (limited.size() >= WEAPON_WHEEL_VISIBLE_SLOT_COUNT) {
                break;
            }
            if (entry != null && findWeaponEntry(limited, entry.id) == null) {
                limited.add(entry);
            }
        }

        if (limited.isEmpty()) {
            limited.add(new WeaponWheelEntry(0, 0, true));
        }
        return limited;
    }

    private boolean containsWeaponId(List<WeaponWheelEntry> entries, int weaponId) {
        return findWeaponEntry(entries, weaponId) != null;
    }

    private WeaponWheelEntry findWeaponEntry(List<WeaponWheelEntry> entries, int weaponId) {
        for (WeaponWheelEntry entry : entries) {
            if (entry.id == weaponId) {
                return entry;
            }
        }
        return null;
    }

    private GradientDrawable makeWeaponWheelOval(int fillColor, int strokeColor, float strokeDp) {
        GradientDrawable drawable = new GradientDrawable();
        drawable.setShape(GradientDrawable.OVAL);
        drawable.setColor(fillColor);
        drawable.setStroke(Math.max(1, dpToPx(strokeDp)), strokeColor);
        return drawable;
    }

    private GradientDrawable makeWeaponWheelRect(int fillColor, int strokeColor, float radiusDp) {
        GradientDrawable drawable = new GradientDrawable();
        drawable.setShape(GradientDrawable.RECTANGLE);
        drawable.setCornerRadius(dpToPx(radiusDp));
        drawable.setColor(fillColor);
        drawable.setStroke(dpToPx(1.0f), strokeColor);
        return drawable;
    }

    private String weaponShortLabel(int weaponId) {
        switch (weaponId) {
            case 0: return "Punho";
            case 22: return "Pistola";
            case 23: return "Silenc.";
            case 24: return "Desert";
            case 25: return "Shotgun";
            case 26: return "Serrada";
            case 27: return "Combat";
            case 28: return "Uzi";
            case 29: return "MP5";
            case 30: return "AK";
            case 31: return "M4";
            case 33: return "Rifle";
            case 34: return "Sniper";
            case 35: return "RPG";
            case 36: return "Heat";
            case 37: return "Chamas";
            case 38: return "Minigun";
            case 46: return "Paraq.";
            default: return weaponLabel(weaponId);
        }
    }

    public void showPhoneOverlay() {
        runOnUiThread(() -> {
            if (!canOpenExclusiveOverlay(NATIVE_OVERLAY_PHONE)) {
                showExclusiveOverlayBlockedMessage();
                return;
            }
            hideHudSettingsPanel();
            hideHudChatPanel();
            if (RadialMenu.menuVisible && mRadialMenu != null) {
                mRadialMenu.hide();
            }
            setNativeOverlayState(NATIVE_OVERLAY_PHONE);
            refreshRuntimeChrome();
        });
    }

    public void hidePhoneOverlay() {
        runOnUiThread(this::hidePhoneOverlayInternal);
    }

    public void showInventoryOverlay() {
        runOnUiThread(() -> {
            if (!canOpenExclusiveOverlay(NATIVE_OVERLAY_INVENTORY)) {
                showExclusiveOverlayBlockedMessage();
                return;
            }
            hideHudSettingsPanel();
            hideHudChatPanel();
            if (RadialMenu.menuVisible && mRadialMenu != null) {
                mRadialMenu.hide();
            }
            setNativeOverlayState(NATIVE_OVERLAY_INVENTORY);
            requestAppInventory();
            refreshRuntimeChrome();
        });
    }

    public void hideInventoryOverlay() {
        runOnUiThread(this::hideInventoryOverlayInternal);
    }

    public void toggleWeaponWheelOverlay() {
        if (isWeaponWheelOverlayVisible()) {
            hideWeaponWheelOverlay();
        } else {
            showWeaponWheelOverlay();
        }
    }

    public void showWeaponWheelOverlay() {
        runOnUiThread(() -> {
            if (!canOpenExclusiveOverlay(NATIVE_OVERLAY_WEAPON_WHEEL)) {
                showExclusiveOverlayBlockedMessage();
                return;
            }
            hideHudSettingsPanel();
            hideHudStatusPanel();
            hideHudChatPanel();
            if (RadialMenu.menuVisible && mRadialMenu != null) {
                mRadialMenu.hide();
            }
            if (Radinho.radinhoVisible && mRadinho != null) {
                mRadinho.hide();
            }
            refreshFallbackWeaponWheelIfNeeded();
            syncWeaponWheelPayload();
            setNativeOverlayState(NATIVE_OVERLAY_WEAPON_WHEEL);
            showJavaWeaponWheelOverlay();
            refreshRuntimeChrome();
            hideSystemUI();
        });
    }

    public void hideWeaponWheelOverlay() {
        runOnUiThread(this::hideWeaponWheelOverlayInternal);
    }

    private void hidePhoneOverlayInternal() {
        if (getNativeOverlayState() == NATIVE_OVERLAY_PHONE) {
            setNativeOverlayState(NATIVE_OVERLAY_NONE);
        }
        refreshRuntimeChrome();
    }

    private void hideInventoryOverlayInternal() {
        if (getNativeOverlayState() == NATIVE_OVERLAY_INVENTORY) {
            setNativeOverlayState(NATIVE_OVERLAY_NONE);
        }
        refreshRuntimeChrome();
    }

    private void hideWeaponWheelOverlayInternal() {
        hideJavaWeaponWheelOverlay();
        if (getNativeOverlayState() == NATIVE_OVERLAY_WEAPON_WHEEL) {
            setNativeOverlayState(NATIVE_OVERLAY_NONE);
        }
        refreshRuntimeChrome();
    }

    private boolean isPhoneOverlayVisible() {
        return getNativeOverlayState() == NATIVE_OVERLAY_PHONE;
    }

    private boolean isInventoryOverlayVisible() {
        return getNativeOverlayState() == NATIVE_OVERLAY_INVENTORY;
    }

    private boolean isWeaponWheelOverlayVisible() {
        return getNativeOverlayState() == NATIVE_OVERLAY_WEAPON_WHEEL;
    }

    private boolean isAnyNativeOverlayVisible() {
        return getNativeOverlayState() != NATIVE_OVERLAY_NONE || isNativeUserPauseActiveRuntime();
    }

    private boolean canOpenExclusiveOverlay(int overlayType) {
        int currentOverlay = getNativeOverlayState();
        return !hudOptionsPanelVisible
                && (currentOverlay == NATIVE_OVERLAY_NONE || currentOverlay == overlayType);
    }

    private void showExclusiveOverlayBlockedMessage() {
        Toast.makeText(this, "Feche a tela aberta primeiro.", Toast.LENGTH_SHORT).show();
        hideSystemUI();
    }

    private boolean isPickupCreatorVisible() {
        return mPickupCreatorOverlay != null && mPickupCreatorOverlay.isVisible();
    }

    private boolean handleRuntimeBack() {
        if (mKeyboard != null && mKeyboard.IsShowing()) {
            hideKeyboard();
            return true;
        }
        if (isHudChatPanelVisible()) {
            hideHudChatPanel();
            return true;
        }
        if (isHudHdMapVisible()) {
            hideHudHdMapOverlay();
            return true;
        }
        if (isHudSettingsVisible()) {
            hideHudSettingsPanel();
            return true;
        }
        if (isPickupCreatorVisible()) {
            mPickupCreatorOverlay.hide();
            return true;
        }
        if (isPhoneOverlayVisible()) {
            hidePhoneOverlay();
            return true;
        }
        if (isInventoryOverlayVisible()) {
            hideInventoryOverlay();
            return true;
        }
        if (isWeaponWheelOverlayVisible()) {
            hideWeaponWheelOverlay();
            return true;
        }
        if (RadialMenu.menuVisible) {
            mRadialMenu.hide();
            return true;
        }
        if (Radinho.radinhoVisible) {
            mRadinho.hide();
            return true;
        }
        if (isNativeUserPauseActiveRuntime()) {
            closeNativeMapOrPause();
            return true;
        }
        return false;
    }

    private void destroyRuntimeOverlays() {
        hideHudHdMapOverlay();
        hideJavaWeaponWheelOverlay();
        setNativeOverlayState(NATIVE_OVERLAY_NONE);
    }

    private void dispatchRuntimeCommand(String command) {
        if (command == null) {
            return;
        }
        String sanitized = command.trim();
        if (sanitized.isEmpty()) {
            return;
        }
        if (handleReconnectRuntimeCommand(sanitized)) {
            return;
        }
        String rconLoginCommand = normalizeRconLoginCommand(sanitized);
        if (rconLoginCommand != null) {
            sendCommandV(rconLoginCommand.getBytes(StandardCharsets.UTF_8));
            return;
        }
        if (handleLocalRuntimeCommand(sanitized)) {
            return;
        }
        sendCommandV(sanitized.getBytes(StandardCharsets.UTF_8));
    }

    private void scheduleRuntimeCommandExtra(Intent intent, long delayMs) {
        if (intent == null || handler == null || !intent.hasExtra(EXTRA_RUNTIME_COMMAND)) {
            return;
        }
        String command = intent.getStringExtra(EXTRA_RUNTIME_COMMAND);
        if (command == null) {
            return;
        }
        final String sanitized = command.trim();
        if (sanitized.isEmpty()) {
            return;
        }
        long safeDelay = Math.max(0L, Math.min(delayMs, 30000L));
        scheduleRuntimeCommandWhenReady(sanitized, safeDelay, 0);
        Log.i(TAG, "Scheduled runtime command from intent: " + sanitized + " delay=" + safeDelay);
    }

    private void scheduleRuntimeCommandWhenReady(String command, long delayMs, int attempt) {
        if (handler == null || command == null) {
            return;
        }
        final String sanitized = command.trim();
        if (sanitized.isEmpty()) {
            return;
        }
        long safeDelay = Math.max(0L, Math.min(delayMs, 30000L));
        handler.postDelayed(() -> {
            if (shouldWaitForNativeConnection(sanitized) && !isNativeRuntimeCommandReady()) {
                if (attempt < RUNTIME_COMMAND_READY_RETRY_MAX) {
                    Log.i(TAG, "Runtime command waiting native spawn-ready attempt="
                            + (attempt + 1) + " command=" + sanitized);
                    scheduleRuntimeCommandWhenReady(
                            sanitized,
                            RUNTIME_COMMAND_READY_RETRY_DELAY_MS,
                            attempt + 1
                    );
                    return;
                }
                Log.w(TAG, "Runtime command connection wait exhausted, dispatching anyway: " + sanitized);
            }
            dispatchRuntimeCommand(sanitized);
        }, safeDelay);
    }

    private boolean shouldWaitForNativeConnection(String command) {
        if (command == null) {
            return false;
        }
        String cleaned = command.trim();
        if (cleaned.isEmpty()) {
            return false;
        }
        String token = cleaned.split("\\s+")[0];
        int inlineSeparator = token.indexOf(':');
        if (inlineSeparator < 0) {
            inlineSeparator = token.indexOf('=');
        }
        if (inlineSeparator > 0) {
            token = token.substring(0, inlineSeparator);
        }
        String primary = normalizeWeaponAlias(token);
        return !isConnectionIndependentRuntimeCommand(primary);
    }

    private boolean isConnectionIndependentRuntimeCommand(String primary) {
        return "reconnect".equals(primary)
                || "reconectar".equals(primary)
                || "reconnectar".equals(primary)
                || "relog".equals(primary)
                || "relogar".equals(primary)
                || "rc".equals(primary)
                || "ak".equals(primary)
                || "ak47".equals(primary)
                || "roupa".equals(primary)
                || "roupas".equals(primary)
                || "cj".equals(primary)
                || "cjroupa".equals(primary)
                || "xyron_tp".equals(primary)
                || "xyrontp".equals(primary)
                || "xyronprobe".equals(primary)
                || "mapscanprobe".equals(primary)
                || "zoneprobe".equals(primary)
                || "probezona".equals(primary)
                || "mapscan_tp".equals(primary)
                || "mapscantp".equals(primary)
                || "zonetp".equals(primary)
                || "tpzona".equals(primary)
                || "hdmap".equals(primary)
                || "mapahd".equals(primary)
                || "gpshd".equals(primary)
                || "testtp".equals(primary);
    }

    private boolean isNativeGameConnectedRuntime() {
        try {
            return isNativeGameConnected();
        } catch (UnsatisfiedLinkError error) {
            Log.e(TAG, "Native connection checker indisponivel.", error);
            return false;
        }
    }

    private boolean isNativePlayerSpawnReadyRuntime() {
        try {
            return isNativePlayerSpawnReady();
        } catch (UnsatisfiedLinkError error) {
            Log.e(TAG, "Native spawn-ready checker indisponivel.", error);
            return false;
        }
    }

    private boolean isNativeRuntimeCommandReady() {
        return isNativeGameConnectedRuntime() && isNativePlayerSpawnReadyRuntime();
    }

    private String normalizeRconLoginCommand(String command) {
        if (command == null) {
            return null;
        }
        String cleaned = command.trim();
        if (cleaned.startsWith("/")) {
            cleaned = cleaned.substring(1).trim();
        }
        String[] parts = cleaned.split("\\s+");
        if (parts.length >= 3
                && "rcon".equalsIgnoreCase(parts[0])
                && "login".equalsIgnoreCase(parts[1])) {
            return "/" + cleaned;
        }
        return null;
    }

    private boolean handleReconnectRuntimeCommand(String command) {
        String cleaned = command == null ? "" : command.trim();
        if (cleaned.startsWith("/")) {
            cleaned = cleaned.substring(1).trim();
        }
        if (cleaned.isEmpty()) {
            return false;
        }

        String primary = cleaned.split("\\s+")[0].toLowerCase(Locale.US);
        boolean reconnectCommand = "reconnect".equals(primary)
                || "reconectar".equals(primary)
                || "reconnectar".equals(primary)
                || "relog".equals(primary)
                || "relogar".equals(primary)
                || "rc".equals(primary);
        if (!reconnectCommand) {
            return false;
        }

        try {
            requestReconnect();
        } catch (UnsatisfiedLinkError error) {
            Log.e(TAG, "Native requestReconnect indisponivel.", error);
            sendCommandV("/reconnect".getBytes(StandardCharsets.UTF_8));
        }
        return true;
    }

    private boolean handleLocalRuntimeCommand(String command) {
        if (command == null) {
            return false;
        }
        String sanitized = command.trim();
        if (sanitized.isEmpty()) {
            return false;
        }

        if (handleHdMapRuntimeCommand(sanitized)) {
            return true;
        }
        if (handleMapScanProbeRuntimeCommand(sanitized)) {
            return true;
        }
        if (handleMapScanTeleportRuntimeCommand(sanitized)) {
            return true;
        }
        if (handleFullSelfTestRuntimeCommand(sanitized)) {
            return true;
        }
        if (handleWorldCollisionSelfTestRuntimeCommand(sanitized)) {
            return true;
        }
        if (handleTextDrawSelfTestRuntimeCommand(sanitized)) {
            return true;
        }
        if (handleVehicleSelfTestRuntimeCommand(sanitized)) {
            return true;
        }
        if (handleAkTestRuntimeCommand(sanitized)) {
            return true;
        }
        if (handleCjOutfitRuntimeCommand(sanitized)) {
            return true;
        }
        return false;
    }

    private boolean handleHdMapRuntimeCommand(String command) {
        String cleaned = command == null ? "" : command.trim();
        if (cleaned.startsWith("/")) {
            cleaned = cleaned.substring(1).trim();
        }
        if (cleaned.isEmpty()) {
            return false;
        }
        String primary = normalizeWeaponAlias(cleaned.split("\\s+")[0]);
        boolean mapCommand = "hdmap".equals(primary)
                || "mapahd".equals(primary)
                || "gpshd".equals(primary);
        if (!mapCommand) {
            return false;
        }
        runOnUiThread(this::showHudHdMapOverlay);
        return true;
    }

    private boolean handleMapScanProbeRuntimeCommand(String command) {
        String cleaned = command == null ? "" : command.trim();
        if (cleaned.startsWith("/")) {
            cleaned = cleaned.substring(1).trim();
        }
        if (cleaned.isEmpty()) {
            return false;
        }

        String[] parts = cleaned.split("\\s+");
        String commandToken = parts[0];
        String inlineArgs = null;
        int inlineSeparator = commandToken.indexOf(':');
        if (inlineSeparator < 0) {
            inlineSeparator = commandToken.indexOf('=');
        }
        if (inlineSeparator > 0 && inlineSeparator < commandToken.length() - 1) {
            inlineArgs = commandToken.substring(inlineSeparator + 1);
            commandToken = commandToken.substring(0, inlineSeparator);
        }
        String primary = normalizeWeaponAlias(commandToken);
        boolean probeCommand = "xyronprobe".equals(primary)
                || "mapscanprobe".equals(primary)
                || "zoneprobe".equals(primary)
                || "probezona".equals(primary);
        if (!probeCommand) {
            return false;
        }

        String[] args;
        if (inlineArgs != null) {
            args = inlineArgs.split("[,;]");
        } else {
            args = new String[Math.max(0, parts.length - 1)];
            if (args.length > 0) {
                System.arraycopy(parts, 1, args, 0, args.length);
            }
        }

        int offset = 0;
        String zoneId = "manual";
        Float x = args.length >= 1 ? parseRuntimeFloat(args[0]) : null;
        if (x == null && args.length >= 4) {
            zoneId = sanitizeProbeId(args[0]);
            offset = 1;
            x = parseRuntimeFloat(args[offset]);
        }
        Float y = args.length > offset + 1 ? parseRuntimeFloat(args[offset + 1]) : null;
        Float z = args.length > offset + 2 ? parseRuntimeFloat(args[offset + 2]) : null;
        int area = 0;
        if (args.length > offset + 3) {
            try {
                area = Math.max(0, Math.min(255, Integer.parseInt(args[offset + 3].trim())));
            } catch (NumberFormatException error) {
                area = 0;
            }
        }

        if (x == null || y == null || z == null) {
            Log.w(TAG, "MAP_PROBE64 invalid args: " + cleaned);
            appendMapScanProbeResult(zoneId, "{\"status\":\"invalid_args\"}");
            return true;
        }

        final String safeZoneId = zoneId;
        final float targetX = x;
        final float targetY = y;
        final float targetZ = z;
        final int targetArea = area;
        boolean queued;
        try {
            queued = queueNativeMapScanProbe(safeZoneId, targetX, targetY, targetZ, targetArea);
        } catch (UnsatisfiedLinkError error) {
            Log.e(TAG, "Native queued map scan probe indisponivel.", error);
            appendMapScanProbeResult(safeZoneId, "{\"status\":\"native_unavailable\"}");
            return true;
        }
        if (!queued) {
            appendMapScanProbeResult(safeZoneId, "{\"status\":\"queue_failed\"}");
            Log.w(TAG, String.format(
                    Locale.US,
                    "MAP_PROBE64 queue failed zone=%s target=%.3f %.3f %.3f area=%d",
                    safeZoneId,
                    targetX,
                    targetY,
                    targetZ,
                    targetArea
            ));
            return true;
        }
        Log.i(TAG, String.format(
                Locale.US,
                "MAP_PROBE64 queued zone=%s target=%.3f %.3f %.3f area=%d",
                safeZoneId,
                targetX,
                targetY,
                targetZ,
                targetArea
        ));
        return true;
    }

    private String sanitizeProbeId(String value) {
        if (value == null) {
            return "manual";
        }
        String cleaned = value.trim().replaceAll("[^A-Za-z0-9_.-]", "_");
        return cleaned.isEmpty() ? "manual" : cleaned;
    }

    private void appendMapScanProbeResult(String zoneId, String nativeJson) {
        File root = getExternalFilesDir(null);
        if (root == null) {
            return;
        }
        File logFile = new File(root, "SAMP/xyron_monitor/map_probe_results.jsonl");
        File parent = logFile.getParentFile();
        if (parent != null && !parent.exists() && !parent.mkdirs()) {
            Log.w(TAG, "Could not create map probe result dir: " + parent.getAbsolutePath());
            return;
        }
        String payload = nativeJson == null ? "{}" : nativeJson.trim();
        if (!payload.startsWith("{")) {
            payload = "{\"status\":\"invalid_native_json\",\"raw\":\"" + escapeJson(payload) + "\"}";
        }
        String line = String.format(
                Locale.US,
                "{\"wallTimeMs\":%d,\"zoneId\":\"%s\",\"native\":%s}%n",
                System.currentTimeMillis(),
                escapeJson(zoneId == null ? "manual" : zoneId),
                payload
        );
        try (FileOutputStream output = new FileOutputStream(logFile, true)) {
            output.write(line.getBytes(StandardCharsets.UTF_8));
        } catch (IOException error) {
            Log.w(TAG, "Could not append map probe result.", error);
        }
    }

    private String escapeJson(String value) {
        if (value == null) {
            return "";
        }
        return value
                .replace("\\", "\\\\")
                .replace("\"", "\\\"")
                .replace("\n", "\\n")
                .replace("\r", "\\r");
    }

    private boolean handleMapScanTeleportRuntimeCommand(String command) {
        String cleaned = command == null ? "" : command.trim();
        if (cleaned.startsWith("/")) {
            cleaned = cleaned.substring(1).trim();
        }
        if (cleaned.isEmpty()) {
            return false;
        }

        String[] parts = cleaned.split("\\s+");
        String commandToken = parts[0];
        String inlineArgs = null;
        int inlineSeparator = commandToken.indexOf(':');
        if (inlineSeparator < 0) {
            inlineSeparator = commandToken.indexOf('=');
        }
        if (inlineSeparator > 0 && inlineSeparator < commandToken.length() - 1) {
            inlineArgs = commandToken.substring(inlineSeparator + 1);
            commandToken = commandToken.substring(0, inlineSeparator);
        }
        String primary = normalizeWeaponAlias(commandToken);
        boolean teleportCommand = "xyron_tp".equals(primary)
                || "xyrontp".equals(primary)
                || "mapscan_tp".equals(primary)
                || "mapscantp".equals(primary)
                || "zonetp".equals(primary)
                || "tpzona".equals(primary)
                || "testtp".equals(primary);
        if (!teleportCommand) {
            return false;
        }

        String[] args;
        if (inlineArgs != null) {
            args = inlineArgs.split("[,;]");
        } else {
            args = new String[Math.max(0, parts.length - 1)];
            if (args.length > 0) {
                System.arraycopy(parts, 1, args, 0, args.length);
            }
        }

        if (args.length < 3) {
            Toast.makeText(this, "Uso: xyron_tp x y z [interior]", Toast.LENGTH_SHORT).show();
            Log.w(TAG, "MAP_SCAN_TP64 missing args: " + cleaned);
            return true;
        }

        Float x = parseRuntimeFloat(args[0]);
        Float y = parseRuntimeFloat(args[1]);
        Float z = parseRuntimeFloat(args[2]);
        int area = 0;
        if (args.length >= 4) {
            try {
                area = Math.max(0, Math.min(255, Integer.parseInt(args[3].trim())));
            } catch (NumberFormatException error) {
                area = 0;
            }
        }

        if (x == null || y == null || z == null) {
            Toast.makeText(this, "xyron_tp recebeu coordenada invalida.", Toast.LENGTH_SHORT).show();
            Log.w(TAG, "MAP_SCAN_TP64 invalid args: " + cleaned);
            return true;
        }

        if (!isNativeGameConnectedRuntime()) {
            int retryCount = mapScanTeleportRetryCounts.containsKey(cleaned)
                    ? mapScanTeleportRetryCounts.get(cleaned)
                    : 0;
            if (retryCount < 45 && handler != null) {
                mapScanTeleportRetryCounts.put(cleaned, retryCount + 1);
                Log.i(TAG, "Map scan teleport direct retry waiting native connection attempt="
                        + (retryCount + 1) + " command=" + cleaned);
                final String retryCommand = cleaned;
                handler.postDelayed(() -> dispatchRuntimeCommand(retryCommand), RUNTIME_COMMAND_READY_RETRY_DELAY_MS);
            } else {
                mapScanTeleportRetryCounts.remove(cleaned);
                Log.w(TAG, "Map scan teleport retry exhausted: " + cleaned);
            }
            return true;
        }
        mapScanTeleportRetryCounts.remove(cleaned);

        final float targetX = x;
        final float targetY = y;
        final float targetZ = z;
        final int targetArea = area;
        runOnUiThread(() -> {
            boolean ok = false;
            try {
                ok = applyNativeMapScanTeleport(targetX, targetY, targetZ, targetArea);
            } catch (UnsatisfiedLinkError error) {
                Log.e(TAG, "Native map scan teleport indisponivel.", error);
            }
            Log.i(TAG, String.format(
                    Locale.US,
                    "MAP_SCAN_TP64 target=%.3f %.3f %.3f area=%d ok=%s",
                    targetX,
                    targetY,
                    targetZ,
                    targetArea,
                    ok
            ));
            if (!ok) {
                Toast.makeText(this, "Teleporte de varredura falhou.", Toast.LENGTH_SHORT).show();
            }
        });
        return true;
    }

    private Float parseRuntimeFloat(String value) {
        if (value == null) {
            return null;
        }
        try {
            return Float.parseFloat(value.trim().replace(',', '.'));
        } catch (NumberFormatException error) {
            return null;
        }
    }

    private boolean handleFullSelfTestRuntimeCommand(String command) {
        String cleaned = command == null ? "" : command.trim();
        if (cleaned.isEmpty()) {
            return false;
        }

        String[] parts = cleaned.split("\\s+");
        String primary = normalizeWeaponAlias(parts[0]);
        boolean fullCommand = "autotest".equals(primary)
                || "fulltest".equals(primary)
                || "testetudo".equals(primary)
                || "testesemhumano".equals(primary)
                || "diagnosticototal".equals(primary);
        if (!fullCommand) {
            return false;
        }

        if (!isNativeRuntimeCommandReady()) {
            Log.i(TAG, "Full self-test waiting native spawn-ready: " + cleaned);
            scheduleRuntimeCommandWhenReady(cleaned, RUNTIME_COMMAND_READY_RETRY_DELAY_MS, 0);
            return true;
        }

        runOnUiThread(() -> {
            runWorldCollisionSelfTestSafe();
            if (handler != null) {
                handler.postDelayed(this::runTextDrawSelfTest, 1500L);
                handler.postDelayed(() -> runVehicleSelfTestMatrix(new String[]{"vehmatrix"}), 4200L);
            }
        });
        return true;
    }

    private boolean handleWorldCollisionSelfTestRuntimeCommand(String command) {
        String cleaned = command == null ? "" : command.trim();
        if (cleaned.isEmpty()) {
            return false;
        }

        String[] parts = cleaned.split("\\s+");
        String primary = normalizeWeaponAlias(parts[0]);
        boolean collisionCommand = "coltest".equals(primary)
                || "collisiontest".equals(primary)
                || "worldtest".equals(primary)
                || "testecolisao".equals(primary)
                || "testemapa".equals(primary);
        if (!collisionCommand) {
            return false;
        }

        if (!isNativeRuntimeCommandReady()) {
            Log.i(TAG, "World collision self-test waiting native spawn-ready: " + cleaned);
            scheduleRuntimeCommandWhenReady(cleaned, RUNTIME_COMMAND_READY_RETRY_DELAY_MS, 0);
            return true;
        }

        runOnUiThread(this::runWorldCollisionSelfTestSafe);
        return true;
    }

    private boolean handleTextDrawSelfTestRuntimeCommand(String command) {
        String cleaned = command == null ? "" : command.trim();
        if (cleaned.isEmpty()) {
            return false;
        }

        String[] parts = cleaned.split("\\s+");
        String primary = normalizeWeaponAlias(parts[0]);
        boolean textDrawCommand = "tdtest".equals(primary)
                || "textdrawtest".equals(primary)
                || "testetextdraw".equals(primary)
                || "testelogin".equals(primary)
                || "logincliquetest".equals(primary)
                || "clicktest".equals(primary);
        if (!textDrawCommand) {
            return false;
        }

        if (!isNativeRuntimeCommandReady()) {
            Log.i(TAG, "Textdraw self-test waiting native spawn-ready: " + cleaned);
            scheduleRuntimeCommandWhenReady(cleaned, RUNTIME_COMMAND_READY_RETRY_DELAY_MS, 0);
            return true;
        }

        runOnUiThread(this::runTextDrawSelfTest);
        return true;
    }

    private void runTextDrawSelfTest() {
        if (handler == null) {
            return;
        }
        Log.i(TAG, "TD_TEST64 start");
        Toast.makeText(this, "Teste automatico de textdraw iniciado.", Toast.LENGTH_SHORT).show();
        sendCommandV("/tdtest".getBytes(StandardCharsets.UTF_8));
        handler.postDelayed(() -> performNativeMenuTap(0.50f, 0.46f), 1400L);
        handler.postDelayed(() -> {
            try {
                View decorView = getWindow() != null ? getWindow().getDecorView() : null;
                if (decorView == null) {
                    return;
                }
                sendSyntheticNativeTouch(
                        Math.round(decorView.getWidth() * 0.50f),
                        Math.round(decorView.getHeight() * 0.46f)
                );
            } catch (RuntimeException | UnsatisfiedLinkError error) {
                Log.e(TAG, "Falha no toque sintetico do TD_TEST64.", error);
            }
        }, 2100L);
    }

    private void runWorldCollisionSelfTestSafe() {
        try {
            runWorldCollisionSelfTest();
            Toast.makeText(this, "Teste automatico de colisao iniciado.", Toast.LENGTH_SHORT).show();
        } catch (UnsatisfiedLinkError error) {
            Log.e(TAG, "Native world collision self-test indisponivel.", error);
        }
    }

    private boolean handleVehicleSelfTestRuntimeCommand(String command) {
        String cleaned = command == null ? "" : command.trim();
        if (cleaned.isEmpty()) {
            return false;
        }

        String[] parts = cleaned.split("\\s+");
        String primary = normalizeWeaponAlias(parts[0]);
        boolean matrixCommand = "vehmatrix".equals(primary)
                || "vehiclematrix".equals(primary)
                || "veiculomatriz".equals(primary)
                || "matrizveiculo".equals(primary)
                || "testeveiculos".equals(primary);
        boolean singleCommand = "vehicletest".equals(primary)
                || "veiculoteste".equals(primary)
                || "testveiculo".equals(primary)
                || "testecarro".equals(primary)
                || "cartest".equals(primary)
                || "testcar".equals(primary)
                || "vehpilot".equals(primary);
        boolean visualCommand = "vehvisual".equals(primary)
                || "vehexit".equals(primary)
                || "testeveiculovisual".equals(primary)
                || "testevisualveiculo".equals(primary);
        if (!matrixCommand && !singleCommand && !visualCommand) {
            return false;
        }

        if (!isNativeRuntimeCommandReady()) {
            Log.i(TAG, "Vehicle self-test waiting native spawn-ready: " + cleaned);
            scheduleRuntimeCommandWhenReady(cleaned, RUNTIME_COMMAND_READY_RETRY_DELAY_MS, 0);
            return true;
        }

        if (matrixCommand) {
            runOnUiThread(() -> runVehicleSelfTestMatrix(parts));
            return true;
        }

        int model = 560;
        if (parts.length >= 2) {
            model = resolveVehicleSelfTestModel(parts[1], model);
        }
        final int safeModel = model;
        if (visualCommand) {
            runOnUiThread(() -> runVehicleVisualSelfTestModel(safeModel));
            return true;
        }
        runOnUiThread(() -> runVehicleSelfTestModel(safeModel, 0L, false));
        return true;
    }

    private void runVehicleSelfTestMatrix(String[] parts) {
        List<Integer> models = new ArrayList<>();
        if (parts != null && parts.length > 1) {
            for (int i = 1; i < parts.length; i++) {
                Integer parsed = resolveVehicleSelfTestModelOrNull(parts[i]);
                if (parsed != null) {
                    models.add(parsed);
                }
            }
        }
        if (models.isEmpty()) {
            for (int model : VEHICLE_SELF_TEST_DEFAULT_MODELS) {
                models.add(model);
            }
        }
        Log.i(TAG, "VEH_TEST64 matrix-start models=" + models);
        Toast.makeText(this, "Teste automatico de veiculos iniciado.", Toast.LENGTH_SHORT).show();
        for (int i = 0; i < models.size(); i++) {
            runVehicleSelfTestModel(models.get(i), i * VEHICLE_SELF_TEST_MODEL_SPACING_MS, true);
        }
    }

    private void runVehicleSelfTestModel(int model, long startDelayMs, boolean resetBeforeStart) {
        if (handler == null) {
            return;
        }
        final int safeModel = Math.max(400, Math.min(model, 611));
        final long safeDelay = Math.max(0L, startDelayMs);
        handler.postDelayed(() -> {
            if (resetBeforeStart) {
                Log.i(TAG, "VEH_TEST64 isolate-before-model model=" + safeModel);
                sendCommandV("/testspawn".getBytes(StandardCharsets.UTF_8));
                handler.postDelayed(() -> startVehicleSelfTestSequence(safeModel), 4600L);
                return;
            }
            startVehicleSelfTestSequence(safeModel);
        }, safeDelay);
    }

    private void startVehicleSelfTestSequence(int safeModel) {
        if (handler == null) {
            return;
        }
            Log.i(TAG, "VEH_TEST64 model-start model=" + safeModel);
            sendCommandV(("/veiculo " + safeModel).getBytes(StandardCharsets.UTF_8));
            handler.postDelayed(() -> startVehicleSelfTestSafe(4500, 0, true, false), 2200L);
            handler.postDelayed(() -> startVehicleSelfTestSafe(2500, -90, true, false), 7000L);
            handler.postDelayed(() -> startVehicleSelfTestSafe(2500, 90, true, false), 10000L);
            handler.postDelayed(() -> startVehicleSelfTestSafe(1400, 0, false, true), 13200L);
    }

    private void runVehicleVisualSelfTestModel(int model) {
        if (handler == null) {
            return;
        }
        final int safeModel = Math.max(400, Math.min(model, 611));
        Log.i(TAG, "VEH_TEST64 visual-start model=" + safeModel);
        Toast.makeText(this, "Teste visual de veiculo iniciado.", Toast.LENGTH_SHORT).show();
        sendCommandV(("/veiculo " + safeModel).getBytes(StandardCharsets.UTF_8));
        handler.postDelayed(() -> startVehicleSelfTestSafe(3000, 0, true, false), 2200L);
        handler.postDelayed(this::exitVehicleVisualSelfTestSafe, 6500L);
    }

    private void startVehicleSelfTestSafe(int durationMs, int steering, boolean throttle, boolean brake) {
        try {
            startVehicleSelfTest(durationMs, steering, throttle, brake);
        } catch (UnsatisfiedLinkError error) {
            Log.e(TAG, "Native vehicle self-test indisponivel.", error);
        }
    }

    private void exitVehicleVisualSelfTestSafe() {
        try {
            exitVehicleVisualSelfTest();
        } catch (UnsatisfiedLinkError error) {
            Log.e(TAG, "Native vehicle visual exit indisponivel.", error);
        }
    }

    private int resolveVehicleSelfTestModel(String value, int fallback) {
        Integer parsed = resolveVehicleSelfTestModelOrNull(value);
        return parsed != null ? parsed : fallback;
    }

    private Integer resolveVehicleSelfTestModelOrNull(String value) {
        if (value == null) {
            return null;
        }
        Integer numeric = parseInteger(value);
        if (numeric != null) {
            return Math.max(400, Math.min(numeric, 611));
        }
        String normalized = normalizeWeaponAlias(value);
        if ("carro".equals(normalized) || "car".equals(normalized) || "auto".equals(normalized)) {
            return 560;
        }
        if ("moto".equals(normalized) || "bike".equals(normalized) || "motorbike".equals(normalized)) {
            return 522;
        }
        if ("tank".equals(normalized) || "tanque".equals(normalized) || "rhino".equals(normalized)) {
            return 432;
        }
        if ("heli".equals(normalized) || "helicoptero".equals(normalized) || "helicopter".equals(normalized)) {
            return 487;
        }
        return null;
    }

    private boolean handleAkTestRuntimeCommand(String command) {
        String cleaned = command == null ? "" : command.trim();
        if (cleaned.startsWith("/")) {
            cleaned = cleaned.substring(1).trim();
        }
        if (cleaned.isEmpty()) {
            return false;
        }

        String[] parts = cleaned.split("\\s+");
        String primary = normalizeWeaponAlias(parts[0]);
        if (!"ak".equals(primary) && !"ak47".equals(primary)) {
            return false;
        }

        int ammo = 250;
        if (parts.length >= 2) {
            Integer parsedAmmo = parseInteger(parts[1]);
            if (parsedAmmo != null) {
                ammo = Math.max(1, Math.min(parsedAmmo, 9999));
            }
        }

        final int safeAmmo = ammo;
        runOnUiThread(() -> {
            hideKeyboard();
            putLocalWeaponForCarrySlot(30, safeAmmo);
            lastWeaponWheelJson = mergeLocalWeaponWheelInventory(lastWeaponWheelJson, 30);
            syncWeaponWheelPayload();
            try {
                giveWeaponToLocalPlayer(30, safeAmmo);
                Toast.makeText(this, "AK-47 liberada para teste.", Toast.LENGTH_SHORT).show();
            } catch (UnsatisfiedLinkError error) {
                Log.e(TAG, "Native giveWeaponToLocalPlayer indisponivel.", error);
                Toast.makeText(this, "Comando /ak indisponivel no nativo.", Toast.LENGTH_SHORT).show();
            }
        });
        return true;
    }

    private boolean handleCjOutfitRuntimeCommand(String command) {
        String cleaned = command == null ? "" : command.trim();
        if (cleaned.startsWith("/")) {
            cleaned = cleaned.substring(1).trim();
        }
        if (cleaned.isEmpty()) {
            return false;
        }

        String[] parts = cleaned.split("\\s+");
        String primary = normalizeWeaponAlias(parts[0]);
        boolean isOutfitCommand = "roupa".equals(primary)
                || "roupas".equals(primary)
                || "cj".equals(primary)
                || "cjroupa".equals(primary);
        if (!isOutfitCommand) {
            return false;
        }

        runOnUiThread(() -> {
            hideKeyboard();
            if (parts.length == 1) {
                cycleCjOutfit();
                return;
            }

            String arg = parts[1];
            String normalizedArg = normalizeWeaponAlias(arg);
            if ("reset".equals(normalizedArg) || "padrao".equals(normalizedArg) || "cj".equals(normalizedArg)) {
                applyCjOutfit(0, true);
                return;
            }

            Integer numeric = parseInteger(arg);
            if (numeric != null) {
                applyCjOutfit(numeric, true);
                return;
            }

            Integer namedIndex = resolveCjOutfitIndex(normalizedArg);
            if (namedIndex != null) {
                applyCjOutfit(namedIndex, true);
            } else {
                Toast.makeText(this, "Use /roupa, /roupa 0-7 ou /skin 0-311.", Toast.LENGTH_LONG).show();
            }
        });
        return true;
    }

    private Integer resolveCjOutfitIndex(String normalizedName) {
        if (normalizedName == null || normalizedName.isEmpty()) {
            return null;
        }
        for (int i = 0; i < CJ_OUTFIT_LABELS.length; i++) {
            if (normalizeWeaponAlias(CJ_OUTFIT_LABELS[i]).equals(normalizedName)) {
                return i;
            }
        }
        return null;
    }

    private void cycleCjOutfit() {
        applyCjOutfit(cjOutfitIndex + 1, true);
    }

    private void applyCjOutfit(int index, boolean notify) {
        int normalizedIndex = index % CJ_OUTFIT_SKINS.length;
        if (normalizedIndex < 0) {
            normalizedIndex += CJ_OUTFIT_SKINS.length;
        }
        cjOutfitIndex = normalizedIndex;
        applyCjSkinDirect(CJ_OUTFIT_SKINS[normalizedIndex], CJ_OUTFIT_LABELS[normalizedIndex]);
        if (notify) {
            Toast.makeText(this, "Roupa aplicada.", Toast.LENGTH_SHORT).show();
        }
    }

    private void applyCjSkinDirect(int skinId, String label) {
        try {
            setLocalPlayerSkin(skinId);
        } catch (UnsatisfiedLinkError error) {
            Log.e(TAG, "Native CJ outfit/skin setter unavailable.", error);
            Toast.makeText(this, "Sistema de roupa indisponivel no nativo.", Toast.LENGTH_SHORT).show();
            return;
        }
        syncCjOutfitButtons();
        Log.i(TAG, "CJ outfit test applied: " + label + " skin=" + skinId);
    }

    private static Integer parseInteger(String value) {
        try {
            return Integer.parseInt(value);
        } catch (NumberFormatException ignored) {
            return null;
        }
    }

    private void putLocalWeaponForCarrySlot(int weaponId, int ammo) {
        int carrySlot = weaponCarrySlot(weaponId);
        if (carrySlot <= 0) {
            return;
        }
        Integer previousWeaponId = null;
        for (Map.Entry<Integer, Integer> existing : localWeaponWheelInventory.entrySet()) {
            if (weaponCarrySlot(existing.getKey()) == carrySlot && existing.getKey() != weaponId) {
                previousWeaponId = existing.getKey();
                break;
            }
        }
        if (previousWeaponId != null) {
            localWeaponWheelInventory.remove(previousWeaponId);
        }
        if (localWeaponWheelInventory.containsKey(weaponId)) {
            localWeaponWheelInventory.remove(weaponId);
        }
        localWeaponWheelInventory.put(weaponId, Math.max(0, Math.min(ammo, 9999)));
        while (localWeaponWheelInventory.size() > WEAPON_WHEEL_MAX_CARRIED_WEAPONS) {
            Integer oldestWeaponId = localWeaponWheelInventory.keySet().iterator().next();
            localWeaponWheelInventory.remove(oldestWeaponId);
        }
    }

    private void selectRuntimeWeapon(int weaponId) {
        int safeWeaponId = isSupportedWeaponId(weaponId) ? weaponId : 0;
        Log.i(TAG, "Roleta selecionou " + weaponLabel(safeWeaponId) + " id=" + safeWeaponId);
        lastWeaponWheelJson = mergeLocalWeaponWheelInventory(lastWeaponWheelJson, safeWeaponId);
        syncWeaponWheelPayload();
        try {
            selectWeapon(safeWeaponId);
        } catch (UnsatisfiedLinkError error) {
            Log.e(TAG, "Native selectWeapon indisponivel.", error);
        }
        if (weaponWheelOverlay != null) {
            weaponWheelOverlay.setEnabled(true);
            weaponWheelOverlay.setClickable(true);
        }
        uiHandler.postDelayed(this::hideWeaponWheelOverlayInternal, 160L);
        Toast.makeText(this, weaponLabel(safeWeaponId) + " na mao.", Toast.LENGTH_SHORT).show();
    }

    private void openPickupCreatorOverlay() {
        hideKeyboard();
        hidePhoneOverlay();
        hideInventoryOverlay();
        hideWeaponWheelOverlay();
        hideSystemUI();
        if (mPickupCreatorOverlay != null) {
            mPickupCreatorOverlay.show();
        }
    }

    private String getSelectedServerName() {
        ServerConfigManager.ServerOption option = ServerConfigManager.getSelectedServer(this);
        return option.name;
    }

    private String getSelectedServerAddress() {
        ServerConfigManager.ServerOption option = ServerConfigManager.getSelectedServer(this);
        return option.getAddress();
    }

    private void refreshRuntimeChrome() {
        boolean modalVisible = isPhoneOverlayVisible()
                || isInventoryOverlayVisible()
                || isWeaponWheelOverlayVisible()
                || isPickupCreatorVisible()
                || isHudHdMapVisible()
                || isHudSettingsVisible()
                || isNativeUserPauseActiveRuntime();
        boolean javaModalNeedsScrim = isPickupCreatorVisible() || isHudSettingsVisible() || isHudHdMapVisible();
        applyClassicHudVisibilityFromQuickSettings();
        applyHudButtonVisibilityForRuntimeState();
        syncHudHdMapHotspot();
        setOverlayBlurEnabledInternal(javaModalNeedsScrim);
    }

    private void openNativePauseMenu() {
        openSafeHudSettings();
    }

    private void prepareHudForNativeMenu() {
        hideHudSettingsPanel();
        hideHudHdMapOverlay();
        setHudOptionsPanelVisible(false);
        hideHudStatusPanel();
        hideHudChatPanel();
        if (RadialMenu.menuVisible && mRadialMenu != null) {
            mRadialMenu.hide();
        }
        refreshRuntimeChrome();
        hideSystemUI();
    }

    private void openNativePauseMenuFromHud() {
        prepareHudForNativeMenu();
        openNativePauseMenu();
    }

    private void openNativeGameSettingsFromHud() {
        prepareHudForNativeMenu();
        openNativeGameSettings();
    }

    private void resumeGameplayFromHud() {
        hideHudSettingsPanel();
        setHudOptionsPanelVisible(false);
        requestNativeGameplayResume();
    }

    private void exitGameFromHud() {
        prepareHudForNativeMenu();
        exitGame();
    }

    public void exitGame() {
        runOnUiThread(() -> {
            destroyRuntimeOverlays();
            hideHudSettingsPanel();
            setHudOptionsPanelVisible(false);
            hideHudStatusPanel();
            hideHudChatPanel();
            if (RadialMenu.menuVisible && mRadialMenu != null) {
                mRadialMenu.hide();
            }
            if (Radinho.radinhoVisible && mRadinho != null) {
                mRadinho.hide();
            }
            finish();
        });
    }

    private void requestNativeGameplayResume() {
        try {
            setAllowNextNativePauseMenu(false);
            forceEndNativeUserPause();
            resumeEvent();
        } catch (UnsatisfiedLinkError error) {
            Log.e(TAG, "Nao foi possivel retomar o gameplay nativo.", error);
        }
        nativeMenuStateRecheckGeneration++;
        nativeUserPauseActive = false;
        refreshRuntimeChrome();
        hideSystemUI();
    }

    private boolean readNativeUserPauseActiveRaw() {
        try {
            return isNativeUserPauseActiveNative();
        } catch (UnsatisfiedLinkError error) {
            return false;
        }
    }

    private boolean isNativeUserPauseActiveRuntime() {
        return nativeUserPauseActive || readNativeUserPauseActiveRaw();
    }

    private void scheduleNativeMenuStateRecheck() {
        final int generation = ++nativeMenuStateRecheckGeneration;
        uiHandler.postDelayed(() -> reconcileNativeMenuState(generation), 350L);
    }

    private void reconcileNativeMenuState(int generation) {
        if (generation != nativeMenuStateRecheckGeneration || !nativeUserPauseActive) {
            return;
        }
        if (!readNativeUserPauseActiveRaw()) {
            nativeUserPauseActive = false;
            refreshRuntimeChrome();
            hideSystemUI();
            return;
        }
        refreshRuntimeChrome();
        uiHandler.postDelayed(() -> reconcileNativeMenuState(generation), 500L);
    }

    private boolean shouldHideGameplayHudForNativeMenu() {
        return isNativeUserPauseActiveRuntime();
    }

    private void applyHudButtonVisibilityForRuntimeState() {
        if (hud_main == null) {
            return;
        }
        cacheHudControlViews();
        boolean hideForNativeMenu = shouldHideGameplayHudForNativeMenu();

        if (hideForNativeMenu) {
            snapshotNativeMenuHudVisibility();
            hudOptionsPanelVisible = false;
            if (hudOptionsPanel != null) {
                setVisibilityIfChanged(hudOptionsPanel, View.GONE);
            }
            setVisibilityIfChanged(hudMenuButton, View.GONE);
            setVisibilityIfChanged(hudInteractionButton, View.GONE);
            setVisibilityIfChanged(hudOptionsButton, View.GONE);
            setVisibilityIfChanged(hudChatButton, View.GONE);
            setVisibilityIfChanged(hudChatClickArea, View.GONE);
            setVisibilityIfChanged(hudHdMapHotspot, View.GONE);
            setSourceHudControlsVisibility(View.GONE);
            resetNativeSourceControls();
            setVisibilityIfChanged(hudWeaponButton, View.GONE);
            setVisibilityIfChanged(hudFpsCounter, View.GONE);
            setVisibilityIfChanged(hudTopRightPanel, View.GONE);
            setVisibilityIfChanged(hudSettingsGearButton, View.GONE);
            setHudViewVisibility(R.id.btn_2, false);
            setHudViewVisibility(R.id.btn_status_toggle, false);
            setHudViewVisibility(R.id.enter_passenger, false);
            setHudViewVisibility(R.id.vehicle_lock_butt, false);
            setHudViewVisibility(R.id.hud_y, false);
            setHudViewVisibility(R.id.hud_f, false);
            if (hudStatusPanel != null) {
                setVisibilityIfChanged(hudStatusPanel, View.GONE);
            }
            if (hudChatPanel != null) {
                setVisibilityIfChanged(hudChatPanel, View.GONE);
            }
            if (hudChatAreaHint != null) {
                setVisibilityIfChanged(hudChatAreaHint, View.GONE);
            }
            stopHudFpsCounter();
            stopHudTopInfoTicker();
            return;
        }

        restoreNativeMenuHudVisibility();
        applyClassicHudVisibilityFromQuickSettings();
        setVisibilityIfChanged(hudMenuButton, View.VISIBLE);
        setVisibilityIfChanged(hudInteractionButton, hudShortcutButtonsVisible ? View.VISIBLE : View.INVISIBLE);
        setVisibilityIfChanged(hudOptionsButton, View.GONE);
        setVisibilityIfChanged(hudChatButton, hudNativeChatVisible ? View.VISIBLE : View.GONE);
        updateHudChatClickAreaVisibility();
        syncHudHdMapHotspot();
        setSourceHudControlsVisibility(View.VISIBLE);
        installSourceControlTouchHandlers();
        setVisibilityIfChanged(hudWeaponButton, View.GONE);
        setVisibilityIfChanged(hudFpsCounter, hudFpsCounterVisible ? View.VISIBLE : View.GONE);
        setVisibilityIfChanged(hudSettingsGearButton, View.GONE);
        if (hudFpsCounterVisible) {
            startHudFpsCounter();
        } else {
            stopHudFpsCounter();
        }
    }

    private void snapshotNativeMenuHudVisibility() {
        if (nativeMenuHudSnapshotActive || hud_main == null) {
            return;
        }
        nativeMenuHudVisibilitySnapshot.clear();
        int[] ids = {
                R.id.btn_0,
                R.id.btn_1,
                R.id.btn_2,
                R.id.btn_hud_options,
                R.id.btn_chat_toggle,
                R.id.hud_source_analog,
                R.id.hud_source_attack_button,
                R.id.hud_source_accelerate_button,
                R.id.hud_source_brake_button,
                R.id.hud_source_handbrake_button,
                R.id.hud_source_horn_button,
                R.id.hud_source_sprint_button,
                R.id.hud_source_jump_button,
                R.id.hud_source_vehicle_button,
                R.id.hud_source_lock_button,
                R.id.hud_source_camera_button,
                R.id.btn_weapon_wheel,
                R.id.btn_hud_settings_gear,
                R.id.hud_fps_counter,
                R.id.hud_top_right_panel,
                R.id.btn_status_toggle,
                R.id.WeaponShowLayout,
                R.id.hud_left_panel,
                R.id.hud_needs_panel,
                R.id.hud_options_panel,
                R.id.hud_status_panel
        };
        for (int id : ids) {
            View view = hud_main.findViewById(id);
            if (view != null) {
                nativeMenuHudVisibilitySnapshot.put(id, view.getVisibility());
            }
        }
        nativeMenuHudSnapshotActive = true;
    }

    private void restoreNativeMenuHudVisibility() {
        if (!nativeMenuHudSnapshotActive || hud_main == null) {
            return;
        }
        for (int i = 0; i < nativeMenuHudVisibilitySnapshot.size(); i++) {
            int id = nativeMenuHudVisibilitySnapshot.keyAt(i);
            View view = hud_main.findViewById(id);
            if (view != null) {
                setVisibilityIfChanged(view, nativeMenuHudVisibilitySnapshot.valueAt(i));
            }
        }
        nativeMenuHudVisibilitySnapshot.clear();
        nativeMenuHudSnapshotActive = false;
    }

    private void openSafeHudSettings() {
        if (isHudSettingsVisible()) {
            hideHudSettingsPanel();
            return;
        }
        showHudSettingsPanel();
    }

    private void openNativeGameSettings() {
        openSafeHudSettings();
    }

    private void performNativeMenuTap(float xRatio, float yRatio) {
        View decorView = getWindow() != null ? getWindow().getDecorView() : null;
        if (decorView == null) {
            Log.w(TAG, "performNativeMenuTap ignored: decorView null");
            return;
        }

        int width = decorView.getWidth();
        int height = decorView.getHeight();
        if (width <= 0 || height <= 0) {
            Log.w(TAG, "performNativeMenuTap ignored: invalid bounds " + width + "x" + height);
            return;
        }

        float x = width * xRatio;
        float y = height * yRatio;
        if (queueNativeMenuTap(x, y)) {
            Log.i(TAG, "performNativeMenuTap queued x=" + x + " y=" + y + " size=" + width + "x" + height);
            return;
        }
        if (injectTouchWithInputManager(x, y)) {
            return;
        }
        try {
            sendSyntheticNativeTouch(Math.round(x), Math.round(y));
            return;
        } catch (UnsatisfiedLinkError error) {
            Log.e(TAG, "Falha ao enviar toque nativo sintetico.", error);
        }
        Log.i(TAG, "performNativeMenuTap x=" + x + " y=" + y + " size=" + width + "x" + height);
        Thread injectorThread = new Thread(() -> {
            long downTime = SystemClock.uptimeMillis();
            MotionEvent downEvent = MotionEvent.obtain(
                    downTime,
                    downTime,
                    MotionEvent.ACTION_DOWN,
                    x,
                    y,
                    0
            );
            MotionEvent upEvent = MotionEvent.obtain(
                    downTime,
                    downTime + 24L,
                    MotionEvent.ACTION_UP,
                    x,
                    y,
                    0
            );
            downEvent.setSource(InputDevice.SOURCE_TOUCHSCREEN);
            upEvent.setSource(InputDevice.SOURCE_TOUCHSCREEN);

            try {
                Instrumentation instrumentation = new Instrumentation();
                instrumentation.sendPointerSync(downEvent);
                instrumentation.sendPointerSync(upEvent);
            } catch (RuntimeException error) {
                Log.e(TAG, "Falha ao injetar toque nativo.", error);
            } finally {
                downEvent.recycle();
                upEvent.recycle();
            }
        }, "xyron-native-menu-tap");
        injectorThread.start();
    }

    private boolean queueNativeMenuTap(float x, float y) {
        final int touchX = Math.round(x);
        final int touchY = Math.round(y);
        try {
            multiTouchEvent4Ex(
                    MotionEvent.ACTION_DOWN,
                    0,
                    touchX,
                    touchY,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0
            );
            uiHandler.postDelayed(() -> {
                try {
                    multiTouchEvent4Ex(
                            MotionEvent.ACTION_UP,
                            0,
                            touchX,
                            touchY,
                            0,
                            0,
                            0,
                            0,
                            0,
                            0
                    );
                } catch (UnsatisfiedLinkError error) {
                    Log.e(TAG, "Falha ao finalizar toque nativo enfileirado.", error);
                }
            }, 32L);
            return true;
        } catch (UnsatisfiedLinkError error) {
            Log.e(TAG, "Falha ao enfileirar toque nativo.", error);
            return false;
        }
    }

    private boolean injectTouchWithInputManager(float x, float y) {
        try {
            InputManager inputManager = (InputManager) getSystemService(INPUT_SERVICE);
            if (inputManager == null) {
                return false;
            }

            Method injectMethod = InputManager.class.getMethod(
                    "injectInputEvent",
                    android.view.InputEvent.class,
                    int.class
            );
            injectMethod.setAccessible(true);

            long downTime = SystemClock.uptimeMillis();
            MotionEvent downEvent = MotionEvent.obtain(
                    downTime,
                    downTime,
                    MotionEvent.ACTION_DOWN,
                    x,
                    y,
                    0
            );
            MotionEvent upEvent = MotionEvent.obtain(
                    downTime,
                    downTime + 24L,
                    MotionEvent.ACTION_UP,
                    x,
                    y,
                    0
            );
            downEvent.setSource(InputDevice.SOURCE_TOUCHSCREEN);
            upEvent.setSource(InputDevice.SOURCE_TOUCHSCREEN);

            boolean injectedDown;
            boolean injectedUp;
            try {
                injectedDown = (boolean) injectMethod.invoke(inputManager, downEvent, 0);
                injectedUp = (boolean) injectMethod.invoke(inputManager, upEvent, 0);
            } finally {
                downEvent.recycle();
                upEvent.recycle();
            }

            return injectedDown && injectedUp;
        } catch (Throwable ignored) {
            return false;
        }
    }

    private boolean shouldIgnoreTransientFocusLoss() {
        return !isFinishing()
                && !isChangingConfigurations()
                && !isPickupCreatorVisible()
                && !isPhoneOverlayVisible()
                && !isInventoryOverlayVisible()
                && !RadialMenu.menuVisible
                && !Radinho.radinhoVisible;
    }

    private boolean shouldHandleSystemUiMapMenu() {
        return !isFinishing()
                && !isChangingConfigurations()
                && !isPickupCreatorVisible()
                && !isPhoneOverlayVisible()
                && !isInventoryOverlayVisible()
                && !RadialMenu.menuVisible
                && !Radinho.radinhoVisible;
    }

    private boolean shouldSuppressAutomaticPauseMenu() {
        return shouldHandleSystemUiMapMenu();
    }

    private boolean shouldAutoDismissNativeMenu() {
        return !isFinishing() && !isChangingConfigurations();
    }

    @Override
    protected boolean shouldDispatchAutomaticPauseResume() {
        return !suppressAutomaticNativePauseResume;
    }

    private void scheduleAutomaticPauseResumeSuppressionRelease() {
        uiHandler.removeCallbacks(clearAutomaticPauseResumeSuppressionRunnable);
        uiHandler.postDelayed(
                clearAutomaticPauseResumeSuppressionRunnable,
                AUTOMATIC_PAUSE_RESUME_SUPPRESSION_RELEASE_DELAY_MS
        );
    }

    private void scheduleBackgroundMenuDismissSequence() {
        if (!pendingBackgroundMenuDismiss) {
            Log.i(TAG, "scheduleBackgroundMenuDismissSequence skipped: pending=false");
            return;
        }
        uiHandler.removeCallbacks(dismissMenuAfterBackgroundRunnable);
        backgroundMenuDismissStep = 0;
        Log.i(TAG, "scheduleBackgroundMenuDismissSequence start");
        uiHandler.postDelayed(
                dismissMenuAfterBackgroundRunnable,
                NATIVE_MENU_RESUME_AFTER_BACKGROUND_DELAYS_MS[0]
        );
    }

    private void setOverlayBlurEnabledInternal(boolean enabled) {
        boolean changed = overlayBlurActive != enabled;
        overlayBlurActive = enabled;

        if (overlayBlurScrim != null) {
            overlayBlurScrim.setVisibility(enabled ? View.VISIBLE : View.GONE);
            overlayBlurScrim.setAlpha(enabled ? 0.78f : 0.0f);
        }

        if (!changed) {
            return;
        }

        ViewGroup contentRoot = findViewById(android.R.id.content);
        if (contentRoot == null) {
            return;
        }

        for (int i = 0; i < contentRoot.getChildCount(); i += 1) {
            View child = contentRoot.getChildAt(i);
            if (child == null || child == hud_main) {
                continue;
            }
            child.setAlpha(enabled ? 0.94f : 1.0f);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                child.setRenderEffect(null);
            }
        }
    }
    private boolean detectLowRamMode() {
        ActivityManager activityManager = (ActivityManager) getSystemService(ACTIVITY_SERVICE);
        if (activityManager == null) {
            return false;
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT && activityManager.isLowRamDevice()) {
            return true;
        }
        return activityManager.getMemoryClass() <= 192;
    }
}
