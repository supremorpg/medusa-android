package com.xyron.game.main.perf;

import android.app.Activity;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.util.Log;
import android.view.Choreographer;

import com.xyron.game.BuildConfig;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.Locale;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.RejectedExecutionException;

public final class XyronPerformanceMonitor implements Choreographer.FrameCallback {
    private static final String TAG = "XyronPerf";
    private static final long SLOW_FRAME_NS = 24_000_000L;
    private static final long FROZEN_FRAME_NS = 700_000_000L;
    private static final long SNAPSHOT_INTERVAL_MS = 5_000L;
    private static final long STARTUP_JANK_GRACE_MS = 18_000L;
    private static final long STARTUP_IO_GRACE_MS = 25_000L;

    private final Activity activity;
    private final NativeFpsProvider nativeFpsProvider;
    private final Handler mainHandler = new Handler(Looper.getMainLooper());
    private final ExecutorService writer = Executors.newSingleThreadExecutor(runnable -> {
        Thread thread = new Thread(runnable, "XyronPerfLog");
        thread.setDaemon(true);
        return thread;
    });

    private volatile boolean running;
    private volatile int nativeFps;
    private volatile Snapshot lastSnapshot = Snapshot.empty();

    private long lastFrameNanos;
    private long lastSnapshotMs;
    private int frameCount;
    private int slowFrameCount;
    private int frozenFrameCount;
    private long totalFrameNanos;
    private long maxFrameNanos;
    private long startupJankGraceUntilMs;
    private long startupIoQuietUntilMs;

    public interface NativeFpsProvider {
        int getNativeFps();
    }

    public XyronPerformanceMonitor(Activity activity) {
        this(activity, null);
    }

    public XyronPerformanceMonitor(Activity activity, NativeFpsProvider nativeFpsProvider) {
        this.activity = activity;
        this.nativeFpsProvider = nativeFpsProvider;
    }

    public void start() {
        if (running) {
            return;
        }
        running = true;
        resetWindow();
        long nowMs = SystemClock.uptimeMillis();
        startupJankGraceUntilMs = nowMs + STARTUP_JANK_GRACE_MS;
        startupIoQuietUntilMs = nowMs + STARTUP_IO_GRACE_MS;
        lastSnapshotMs = nowMs;
        mainHandler.post(() -> Choreographer.getInstance().postFrameCallback(this));
    }

    public void stop() {
        if (!running) {
            return;
        }
        running = false;
        mainHandler.post(() -> {
            Choreographer.getInstance().removeFrameCallback(this);
            flushSnapshot("stop");
        });
    }

    public void shutdown() {
        writer.shutdown();
    }

    public void setNativeFps(int fps) {
        nativeFps = clamp(fps, 0, 240);
    }

    public Snapshot snapshot() {
        return lastSnapshot;
    }

    @Override
    public void doFrame(long frameTimeNanos) {
        if (!running) {
            return;
        }

        long nowMs = SystemClock.uptimeMillis();
        if (lastFrameNanos != 0L && frameTimeNanos > lastFrameNanos) {
            long duration = frameTimeNanos - lastFrameNanos;
            if (nowMs >= startupJankGraceUntilMs) {
                frameCount++;
                totalFrameNanos += duration;
                if (duration > maxFrameNanos) {
                    maxFrameNanos = duration;
                }
                if (duration >= SLOW_FRAME_NS) {
                    slowFrameCount++;
                }
                if (duration >= FROZEN_FRAME_NS) {
                    frozenFrameCount++;
                }
            }
        }
        lastFrameNanos = frameTimeNanos;

        if (nowMs - lastSnapshotMs >= SNAPSHOT_INTERVAL_MS) {
            flushSnapshot("interval");
            lastSnapshotMs = nowMs;
        }

        Choreographer.getInstance().postFrameCallback(this);
    }

    private void flushSnapshot(String reason) {
        Snapshot snapshot = buildSnapshot(reason);
        lastSnapshot = snapshot;
        resetWindow();
        if (snapshot.nativeFps > 0 || snapshot.avgFrameMs > 0L) {
            Log.i(TAG, snapshot.toCompactLog());
            writeSnapshotAsync(snapshot);
        }
    }

    private Snapshot buildSnapshot(String reason) {
        long nowUptimeMs = SystemClock.uptimeMillis();
        boolean warmup = nowUptimeMs < startupJankGraceUntilMs;
        long avgFrameMs = frameCount > 0
                ? (totalFrameNanos / frameCount) / 1_000_000L
                : 0L;
        long maxFrameMs = maxFrameNanos / 1_000_000L;
        int measuredFps = totalFrameNanos > 0L && frameCount > 0
                ? (int) ((frameCount * 1_000_000_000L + totalFrameNanos / 2L) / totalFrameNanos)
                : 0;
        Runtime runtime = Runtime.getRuntime();
        long javaUsedMb = (runtime.totalMemory() - runtime.freeMemory()) / (1024L * 1024L);
        long javaMaxMb = runtime.maxMemory() / (1024L * 1024L);
        float javaPressure = javaMaxMb > 0L ? (float) javaUsedMb / (float) javaMaxMb : 0.0f;
        return new Snapshot(
                System.currentTimeMillis(),
                reason,
                warmup,
                frameCount,
                slowFrameCount,
                frozenFrameCount,
                avgFrameMs,
                maxFrameMs,
                resolveNativeFps(measuredFps),
                javaUsedMb,
                javaMaxMb,
                javaPressure
        );
    }

    private int resolveNativeFps(int measuredFpsFallback) {
        int fps = nativeFps;
        if (nativeFpsProvider != null) {
            try {
                int sampledFps = nativeFpsProvider.getNativeFps();
                if (sampledFps > 0) {
                    fps = sampledFps;
                }
            } catch (Throwable ignored) {
            }
        }
        if (fps <= 0 && measuredFpsFallback > 0) {
            fps = measuredFpsFallback;
        }
        nativeFps = clamp(fps, 0, 240);
        return nativeFps;
    }

    private void resetWindow() {
        lastFrameNanos = 0L;
        frameCount = 0;
        slowFrameCount = 0;
        frozenFrameCount = 0;
        totalFrameNanos = 0L;
        maxFrameNanos = 0L;
    }

    private void writeSnapshotAsync(Snapshot snapshot) {
        if (snapshot.warmup && SystemClock.uptimeMillis() < startupIoQuietUntilMs) {
            return;
        }
        try {
            writer.execute(() -> writeSnapshot(snapshot));
        } catch (RejectedExecutionException ignored) {
        }
    }

    private void writeSnapshot(Snapshot snapshot) {
        File root = activity.getExternalFilesDir(null);
        if (root == null) {
            return;
        }
        File dir = new File(root, "SAMP/perf");
        if (!dir.exists() && !dir.mkdirs()) {
            return;
        }

        JSONObject payload = snapshot.toJson();
        try {
            payload.put("device", Build.MANUFACTURER + " " + Build.MODEL);
            payload.put("packageName", BuildConfig.APPLICATION_ID);
        } catch (JSONException ignored) {
        }

        byte[] latest = payload.toString().getBytes(StandardCharsets.UTF_8);
        byte[] line = (payload.toString() + "\n").getBytes(StandardCharsets.UTF_8);
        try (FileOutputStream output = new FileOutputStream(new File(dir, "latest.json"), false)) {
            output.write(latest);
        } catch (IOException error) {
            Log.w(TAG, "Could not write latest performance snapshot.", error);
        }
        try (FileOutputStream output = new FileOutputStream(new File(dir, "perf.jsonl"), true)) {
            output.write(line);
        } catch (IOException error) {
            Log.w(TAG, "Could not append performance snapshot.", error);
        }
    }

    private static int clamp(int value, int min, int max) {
        return Math.max(min, Math.min(max, value));
    }

    public static final class Snapshot {
        public final long wallTimeMs;
        public final String reason;
        public final boolean warmup;
        public final int frameCount;
        public final int slowFrameCount;
        public final int frozenFrameCount;
        public final long avgFrameMs;
        public final long maxFrameMs;
        public final int nativeFps;
        public final long javaUsedMb;
        public final long javaMaxMb;
        public final float javaPressure;

        private Snapshot(
                long wallTimeMs,
                String reason,
                boolean warmup,
                int frameCount,
                int slowFrameCount,
                int frozenFrameCount,
                long avgFrameMs,
                long maxFrameMs,
                int nativeFps,
                long javaUsedMb,
                long javaMaxMb,
                float javaPressure
        ) {
            this.wallTimeMs = wallTimeMs;
            this.reason = reason == null ? "" : reason;
            this.warmup = warmup;
            this.frameCount = frameCount;
            this.slowFrameCount = slowFrameCount;
            this.frozenFrameCount = frozenFrameCount;
            this.avgFrameMs = avgFrameMs;
            this.maxFrameMs = maxFrameMs;
            this.nativeFps = nativeFps;
            this.javaUsedMb = javaUsedMb;
            this.javaMaxMb = javaMaxMb;
            this.javaPressure = javaPressure;
        }

        public static Snapshot empty() {
            return new Snapshot(0L, "empty", false, 0, 0, 0, 0L, 0L, 0, 0L, 0L, 0.0f);
        }

        public float slowFramePercent() {
            return frameCount > 0 ? (slowFrameCount * 100.0f) / frameCount : 0.0f;
        }

        public boolean hasUsefulData() {
            return frameCount > 0 || effectiveNativeFps() > 0;
        }

        public int effectiveNativeFps() {
            if (nativeFps > 0) {
                return nativeFps;
            }
            if (avgFrameMs > 0L) {
                return Math.max(0, Math.min(240, Math.round(1000.0f / (float) avgFrameMs)));
            }
            return 0;
        }

        public JSONObject toJson() {
            JSONObject payload = new JSONObject();
            try {
                int outputFps = nativeFps;
                if (outputFps <= 0 && avgFrameMs > 0L) {
                    outputFps = Math.max(0, Math.min(240, Math.round(1000.0f / (float) avgFrameMs)));
                }
                payload.put("wallTimeMs", wallTimeMs);
                payload.put("reason", reason);
                payload.put("warmup", warmup);
                payload.put("frameCount", frameCount);
                payload.put("slowFrameCount", slowFrameCount);
                payload.put("slowFramePercent", slowFramePercent());
                payload.put("frozenFrameCount", frozenFrameCount);
                payload.put("avgFrameMs", avgFrameMs);
                payload.put("maxFrameMs", maxFrameMs);
                payload.put("nativeFps", outputFps);
                payload.put("javaUsedMb", javaUsedMb);
                payload.put("javaMaxMb", javaMaxMb);
                payload.put("javaPressure", javaPressure);
            } catch (JSONException ignored) {
            }
            return payload;
        }

        public String toCompactLog() {
            int outputFps = nativeFps;
            if (outputFps <= 0 && avgFrameMs > 0L) {
                outputFps = Math.max(0, Math.min(240, Math.round(1000.0f / (float) avgFrameMs)));
            }
            return String.format(
                    Locale.US,
                    "native=%dfps source=perf phase=%s uiAvg=%dms uiMax=%dms slow=%.1f%% frozen=%d java=%d/%dMB",
                    outputFps,
                    warmup ? "warmup" : "game",
                    avgFrameMs,
                    maxFrameMs,
                    slowFramePercent(),
                    frozenFrameCount,
                    javaUsedMb,
                    javaMaxMb
            );
        }
    }
}
