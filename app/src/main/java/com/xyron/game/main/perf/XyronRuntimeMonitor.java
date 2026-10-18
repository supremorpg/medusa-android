package com.xyron.game.main.perf;

import android.app.Activity;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.util.Log;

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

public final class XyronRuntimeMonitor {
    private static final String TAG = "XyronRuntimeMonitor";
    private static final long SAMPLE_INTERVAL_MS = 1000L;
    private static final long SAMPLE_LOG_INTERVAL_MS = 5000L;
    private static final long EVENT_DEBOUNCE_MS = 4500L;
    private static final long FIRST_NATIVE_SAMPLE_DELAY_MS = 8000L;
    private static final long STARTUP_IO_GRACE_MS = 15_000L;
    private static final float LIMBO_Z = -80.0f;
    private static final float BELOW_GROUND_DZ = -12.0f;
    private static final float FLOATING_DZ = 2.75f;
    private static final float STILL_SPEED2 = 0.0025f;
    private static final int LOW_FPS = 18;

    public interface NativeSnapshotProvider {
        String getNativeRuntimeMonitorSnapshotJson();
    }

    public interface PerfSnapshotProvider {
        XyronPerformanceMonitor.Snapshot getSnapshot();
    }

    private final Activity activity;
    private final NativeSnapshotProvider nativeSnapshotProvider;
    private final PerfSnapshotProvider perfSnapshotProvider;
    private final Handler mainHandler = new Handler(Looper.getMainLooper());
    private final ExecutorService writer = Executors.newSingleThreadExecutor(runnable -> {
        Thread thread = new Thread(runnable, "XyronRuntimeMonitorLog");
        thread.setDaemon(true);
        return thread;
    });

    private volatile boolean running;
    private long lastSampleLogMs;
    private long lastFallEventMs;
    private long lastFloatingEventMs;
    private long lastCollisionFlagsEventMs;
    private long lastPassThroughEventMs;
    private long lastObjectProbeEventMs;
    private long lastFpsEventMs;
    private long nativeSnapshotEnabledAfterUptimeMs;
    private long monitorIoEnabledAfterUptimeMs;
    private int floatingStableSamples;

    private final Runnable sampleRunnable = new Runnable() {
        @Override
        public void run() {
            if (!running) {
                return;
            }
            sampleNow();
            mainHandler.postDelayed(this, SAMPLE_INTERVAL_MS);
        }
    };

    public XyronRuntimeMonitor(
            Activity activity,
            NativeSnapshotProvider nativeSnapshotProvider,
            PerfSnapshotProvider perfSnapshotProvider
    ) {
        this.activity = activity;
        this.nativeSnapshotProvider = nativeSnapshotProvider;
        this.perfSnapshotProvider = perfSnapshotProvider;
    }

    public void start() {
        if (running) {
            return;
        }
        running = true;
        lastSampleLogMs = 0L;
        long nowMs = SystemClock.uptimeMillis();
        nativeSnapshotEnabledAfterUptimeMs = nowMs + FIRST_NATIVE_SAMPLE_DELAY_MS;
        monitorIoEnabledAfterUptimeMs = nowMs + STARTUP_IO_GRACE_MS;
        mainHandler.post(sampleRunnable);
        Log.i(TAG, "Runtime monitor started.");
    }

    public void stop() {
        if (!running) {
            return;
        }
        running = false;
        mainHandler.removeCallbacks(sampleRunnable);
        Log.i(TAG, "Runtime monitor stopped.");
    }

    public void shutdown() {
        writer.shutdown();
    }

    private void sampleNow() {
        long nowMs = System.currentTimeMillis();
        JSONObject nativeSnapshot = readNativeSnapshot(nowMs);
        XyronPerformanceMonitor.Snapshot perfSnapshot = perfSnapshotProvider != null
                ? perfSnapshotProvider.getSnapshot()
                : XyronPerformanceMonitor.Snapshot.empty();
        JSONObject sample = mergeSample(nowMs, nativeSnapshot, perfSnapshot);

        long uptime = SystemClock.uptimeMillis();
        boolean ioEnabled = uptime >= monitorIoEnabledAfterUptimeMs || nativeSnapshot.optBoolean("nativeReady", false);
        if (ioEnabled) {
            writer.execute(() -> writeLatest(sample));
            if (uptime - lastSampleLogMs >= SAMPLE_LOG_INTERVAL_MS) {
                lastSampleLogMs = uptime;
                writer.execute(() -> appendLine("samples.jsonl", sample));
            }
        }

        detectEvents(sample, uptime);
    }

    private JSONObject readNativeSnapshot(long nowMs) {
        if (SystemClock.uptimeMillis() < nativeSnapshotEnabledAfterUptimeMs) {
            return createFallbackSnapshot(nowMs, "native_snapshot_warmup");
        }
        try {
            String raw = nativeSnapshotProvider != null
                    ? nativeSnapshotProvider.getNativeRuntimeMonitorSnapshotJson()
                    : null;
            if (raw != null && !raw.trim().isEmpty()) {
                return new JSONObject(raw);
            }
        } catch (Throwable error) {
            Log.w(TAG, "Native runtime snapshot unavailable.", error);
        }
        return createFallbackSnapshot(nowMs, "native_snapshot_unavailable");
    }

    private JSONObject createFallbackSnapshot(long nowMs, String reason) {
        JSONObject fallback = new JSONObject();
        try {
            fallback.put("nativeReady", false);
            fallback.put("reason", reason);
            fallback.put("wallTimeMs", nowMs);
        } catch (JSONException ignored) {
        }
        return fallback;
    }

    private JSONObject mergeSample(
            long nowMs,
            JSONObject nativeSnapshot,
            XyronPerformanceMonitor.Snapshot perfSnapshot
    ) {
        JSONObject sample = new JSONObject();
        try {
            sample.put("wallTimeMs", nowMs);
            sample.put("device", Build.MANUFACTURER + " " + Build.MODEL);
            sample.put("packageName", BuildConfig.APPLICATION_ID);
            sample.put("native", nativeSnapshot);
            JSONObject perf = perfSnapshot != null ? perfSnapshot.toJson() : XyronPerformanceMonitor.Snapshot.empty().toJson();
            sample.put("perf", perf);
        } catch (JSONException ignored) {
        }
        return sample;
    }

    private void detectEvents(JSONObject sample, long uptimeMs) {
        JSONObject nativeSnapshot = sample.optJSONObject("native");
        JSONObject perf = sample.optJSONObject("perf");
        if (nativeSnapshot == null || !nativeSnapshot.optBoolean("nativeReady", false)) {
            return;
        }

        float z = (float) nativeSnapshot.optDouble("z", 0.0);
        float dz = (float) nativeSnapshot.optDouble("dz", 0.0);
        float speed2 = (float) nativeSnapshot.optDouble("speed2", 0.0);
        float vz = (float) nativeSnapshot.optDouble("vz", 0.0);
        boolean inVehicle = nativeSnapshot.optBoolean("inVehicle", false);
        boolean verticalHit = nativeSnapshot.optBoolean("verticalHit", false);
        boolean onGround = nativeSnapshot.optBoolean("onGround", false);
        boolean inAir = nativeSnapshot.optBoolean("inAir", false);
        boolean sweptHit = nativeSnapshot.optBoolean("sweptHit", false);
        boolean visualProbeHit = nativeSnapshot.optBoolean("visualProbeHit", false);
        boolean forwardHit = nativeSnapshot.optBoolean("forwardHit", false);

        boolean severeFall = z < LIMBO_Z || dz < BELOW_GROUND_DZ || (!verticalHit && z < -20.0f);
        if (severeFall && uptimeMs - lastFallEventMs >= EVENT_DEBOUNCE_MS) {
            lastFallEventMs = uptimeMs;
            emitEvent("fall_or_missing_floor", "critical", sample,
                    String.format(Locale.US, "z=%.2f dz=%.2f verticalHit=%s", z, dz, verticalHit));
        }

        boolean floating = !inVehicle && dz > FLOATING_DZ && speed2 <= STILL_SPEED2 && Math.abs(vz) < 0.015f && (onGround || !inAir);
        floatingStableSamples = floating ? floatingStableSamples + 1 : 0;
        if (floatingStableSamples >= 3 && uptimeMs - lastFloatingEventMs >= EVENT_DEBOUNCE_MS) {
            lastFloatingEventMs = uptimeMs;
            emitEvent("floating_invisible_support", "critical", sample,
                    String.format(Locale.US, "dz=%.2f speed2=%.4f onGround=%s inAir=%s", dz, speed2, onGround, inAir));
        }

        boolean collisionFlagsBad =
                !nativeSnapshot.optBoolean("usesCollision", true)
                        || !nativeSnapshot.optBoolean("collidable", true)
                        || !nativeSnapshot.optBoolean("canBeCollidedWith", true)
                        || !nativeSnapshot.optBoolean("simpleCollision", true)
                        || !nativeSnapshot.optBoolean("applyGravity", true);
        if (collisionFlagsBad && uptimeMs - lastCollisionFlagsEventMs >= EVENT_DEBOUNCE_MS) {
            lastCollisionFlagsEventMs = uptimeMs;
            emitEvent("collision_flags_bad", "warn", sample, "entity collision/gravity flags are not normal");
        }

        float passThroughSpeed2 = inVehicle ? 0.0045f : 0.0015f;
        float sweptDistance = (float) nativeSnapshot.optDouble("sweptDistance", 0.0);
        boolean sweptObjectInPath =
                sweptHit
                        && nativeSnapshot.optInt("sweptModel", -1) >= 0
                        && sweptDistance >= 0.0f
                        && sweptDistance <= (inVehicle ? 14.0f : 4.5f);
        if (sweptObjectInPath && speed2 >= passThroughSpeed2 && uptimeMs - lastPassThroughEventMs >= EVENT_DEBOUNCE_MS) {
            lastPassThroughEventMs = uptimeMs;
            emitEvent("object_pass_through_suspect", "warn", sample, describeProbe(nativeSnapshot, "swept"));
        }

        boolean visualProbeBad =
                visualProbeHit
                        && (
                                !nativeSnapshot.optBoolean("visualProbeUsesCollision", true)
                                        || !nativeSnapshot.optBoolean("visualProbeHasColData", true)
                                        || (nativeSnapshot.optBoolean("visualProbeBreakable", false) && speed2 >= passThroughSpeed2)
                        );
        boolean forwardProbeBad =
                forwardHit
                        && (
                                !nativeSnapshot.optBoolean("forwardUsesCollision", true)
                                        || !nativeSnapshot.optBoolean("forwardHasColData", true)
                        );
        if ((visualProbeBad || forwardProbeBad) && uptimeMs - lastObjectProbeEventMs >= EVENT_DEBOUNCE_MS) {
            lastObjectProbeEventMs = uptimeMs;
            String prefix = visualProbeBad ? "visualProbe" : "forward";
            emitEvent("object_collision_probe_bad", "warn", sample, describeProbe(nativeSnapshot, prefix));
        }

        if (perf != null && !perf.optBoolean("warmup", false)) {
            int nativeFps = perf.optInt("nativeFps", nativeSnapshot.optInt("nativeFps", 0));
            double slowPercent = perf.optDouble("slowFramePercent", 0.0);
            long maxFrameMs = perf.optLong("maxFrameMs", 0L);
            int frozen = perf.optInt("frozenFrameCount", 0);
            boolean fpsDrop = nativeFps > 0 && nativeFps <= LOW_FPS;
            boolean severeJank = frozen > 0 || maxFrameMs >= 700L || slowPercent >= 45.0;
            if ((fpsDrop || severeJank) && uptimeMs - lastFpsEventMs >= EVENT_DEBOUNCE_MS) {
                lastFpsEventMs = uptimeMs;
                emitEvent("fps_or_jank_drop", fpsDrop && severeJank ? "critical" : "warn", sample,
                        String.format(Locale.US, "fps=%d slow=%.1f max=%d frozen=%d", nativeFps, slowPercent, maxFrameMs, frozen));
            }
        }
    }

    private String describeProbe(JSONObject nativeSnapshot, String prefix) {
        return String.format(Locale.US,
                "%s model=%d name=%s type=%d pos=%.2f,%.2f,%.2f dist=%.2f usesCol=%s visible=%s rw=%s hasCol=%s colData=%s slot=%d breakable=%s",
                prefix,
                nativeSnapshot.optInt(prefix + "Model", -1),
                nativeSnapshot.optString(prefix + "Name", "?"),
                nativeSnapshot.optInt(prefix + "Type", -1),
                nativeSnapshot.optDouble(prefix + "X", 0.0),
                nativeSnapshot.optDouble(prefix + "Y", 0.0),
                nativeSnapshot.optDouble(prefix + "Z", 0.0),
                nativeSnapshot.optDouble(prefix + "Distance", 0.0),
                nativeSnapshot.optBoolean(prefix + "UsesCollision", false),
                nativeSnapshot.optBoolean(prefix + "Visible", false),
                nativeSnapshot.optBoolean(prefix + "HasRw", false),
                nativeSnapshot.optBoolean(prefix + "HasCol", false),
                nativeSnapshot.optBoolean(prefix + "HasColData", false),
                nativeSnapshot.optInt(prefix + "ColSlot", -1),
                nativeSnapshot.optBoolean(prefix + "Breakable", false));
    }

    private void emitEvent(String kind, String severity, JSONObject sample, String detail) {
        JSONObject event = new JSONObject();
        try {
            event.put("wallTimeMs", System.currentTimeMillis());
            event.put("kind", kind);
            event.put("severity", severity);
            event.put("detail", detail);
            event.put("sample", sample);
        } catch (JSONException ignored) {
        }
        Log.w(TAG, "event kind=" + kind + " severity=" + severity + " detail=" + detail);
        writer.execute(() -> appendLine("events.jsonl", event));
    }

    private void writeLatest(JSONObject sample) {
        File dir = getOutputDir();
        if (dir == null) {
            return;
        }
        byte[] latest = sample.toString().getBytes(StandardCharsets.UTF_8);
        try (FileOutputStream output = new FileOutputStream(new File(dir, "latest.json"), false)) {
            output.write(latest);
        } catch (IOException error) {
            Log.w(TAG, "Could not write runtime latest snapshot.", error);
        }
    }

    private void appendLine(String name, JSONObject payload) {
        File dir = getOutputDir();
        if (dir == null) {
            return;
        }
        byte[] line = (payload.toString() + "\n").getBytes(StandardCharsets.UTF_8);
        try (FileOutputStream output = new FileOutputStream(new File(dir, name), true)) {
            output.write(line);
        } catch (IOException error) {
            Log.w(TAG, "Could not append runtime monitor " + name + ".", error);
        }
    }

    private File getOutputDir() {
        File root = activity.getExternalFilesDir(null);
        if (root == null) {
            return null;
        }
        File dir = new File(root, "SAMP/xyron_monitor");
        if (!dir.exists() && !dir.mkdirs()) {
            return null;
        }
        return dir;
    }
}
