package com.xyron.game.main.ui;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.RectF;
import android.util.AttributeSet;
import android.util.LruCache;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;
import android.view.View;
import android.view.ViewConfiguration;

import java.io.IOException;
import java.io.InputStream;
import java.util.Collections;
import java.util.HashSet;
import java.util.Locale;
import java.util.Set;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public final class HdMapView extends View {
    private static final int TILE_SIZE = 500;
    private static final int MIN_ZOOM = 0;
    private static final int MAX_ZOOM = 7;
    private static final int DEFAULT_ZOOM = 3;
    private static final int DEFAULT_MAX_INTERACTIVE_ZOOM = 4;
    private static final float MIN_TILE_COVERAGE_FOR_DEEP_ZOOM = 0.90f;
    private static final float GTA_WORLD_MIN = -3000.0f;
    private static final float GTA_WORLD_MAX = 3000.0f;
    private static final String BASE_MAP_ASSET = "maps/gta_sa_map.jpg";
    private static final int[] MIN_X = {0, 0, 0, 0, 0, 1, 10, 26};
    private static final int[] MAX_X = {0, 1, 2, 5, 11, 23, 46, 87};
    private static final int[] MIN_Y = {0, 0, 0, 0, 0, 0, 0, 0};
    private static final int[] MAX_Y = {0, 1, 2, 5, 11, 23, 46, 91};
    private static final int MAX_PENDING_LOADS = 48;

    private final Paint bitmapPaint = new Paint(Paint.FILTER_BITMAP_FLAG | Paint.DITHER_FLAG);
    private final Paint fallbackPaint = new Paint(Paint.FILTER_BITMAP_FLAG | Paint.DITHER_FLAG);
    private final Paint placeholderPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint gridPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint routeGlowPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint routePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint markerFillPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint markerStrokePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint destinationFillPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint destinationStrokePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF tileRect = new RectF();
    private final RectF viewportRect = new RectF();
    private final Rect sourceRect = new Rect();
    private final LruCache<String, Bitmap> tileCache;
    private final Set<String> pending = Collections.synchronizedSet(new HashSet<>());
    private final Set<String> missing = Collections.synchronizedSet(new HashSet<>());
    private final ExecutorService decodeExecutor = Executors.newFixedThreadPool(2);
    private final ScaleGestureDetector scaleDetector;
    private final float tapSlop;

    private String tileRoot = "gtag-satellite";
    private volatile int cacheGeneration;
    private volatile int manifestGeneration;
    private volatile boolean tileManifestLoaded;
    private volatile Set<String> availableTiles = Collections.emptySet();
    private volatile Bitmap baseMapBitmap;
    private volatile boolean baseMapRequested;
    private volatile int maxAvailableInteractiveZoom = DEFAULT_MAX_INTERACTIVE_ZOOM;
    private int zoom = DEFAULT_ZOOM;
    private float originX;
    private float originY;
    private float focusWorldX;
    private float focusWorldY;
    private boolean hasWorldFocus;
    private float destinationWorldX;
    private float destinationWorldY;
    private boolean hasDestination;
    private float lastTouchX;
    private float lastTouchY;
    private float downTouchX;
    private float downTouchY;
    private boolean dragging;
    private boolean movedDuringGesture;

    public HdMapView(Context context) {
        this(context, null);
    }

    public HdMapView(Context context, AttributeSet attrs) {
        super(context, attrs);
        int maxKb = (int) Math.min(Runtime.getRuntime().maxMemory() / 1024 / 8, 65536);
        tileCache = new LruCache<String, Bitmap>(Math.max(24576, maxKb)) {
            @Override
            protected int sizeOf(String key, Bitmap value) {
                return value == null ? 0 : value.getByteCount() / 1024;
            }

            @Override
            protected void entryRemoved(boolean evicted, String key, Bitmap oldValue, Bitmap newValue) {
                if (oldValue != null && oldValue != newValue && !oldValue.isRecycled()) {
                    oldValue.recycle();
                }
            }
        };
        scaleDetector = new ScaleGestureDetector(context, new ScaleGestureDetector.SimpleOnScaleGestureListener() {
            private float accumulatedScale = 1.0f;

            @Override
            public boolean onScaleBegin(ScaleGestureDetector detector) {
                accumulatedScale = 1.0f;
                return true;
            }

            @Override
            public boolean onScale(ScaleGestureDetector detector) {
                accumulatedScale *= detector.getScaleFactor();
                if (accumulatedScale >= 1.16f) {
                    zoomIn(detector.getFocusX(), detector.getFocusY());
                    accumulatedScale = 1.0f;
                } else if (accumulatedScale <= 0.86f) {
                    zoomOut(detector.getFocusX(), detector.getFocusY());
                    accumulatedScale = 1.0f;
                }
                return true;
            }
        });
        tapSlop = ViewConfiguration.get(context).getScaledTouchSlop();
        fallbackPaint.setAlpha(220);
        placeholderPaint.setStyle(Paint.Style.FILL);
        placeholderPaint.setColor(Color.argb(28, 15, 18, 22));
        gridPaint.setStyle(Paint.Style.STROKE);
        gridPaint.setStrokeWidth(1.0f);
        gridPaint.setColor(Color.argb(28, 255, 255, 255));
        routeGlowPaint.setStyle(Paint.Style.STROKE);
        routeGlowPaint.setStrokeWidth(12.0f);
        routeGlowPaint.setStrokeCap(Paint.Cap.ROUND);
        routeGlowPaint.setStrokeJoin(Paint.Join.ROUND);
        routeGlowPaint.setColor(Color.argb(80, 0, 0, 0));
        routePaint.setStyle(Paint.Style.STROKE);
        routePaint.setStrokeWidth(5.0f);
        routePaint.setStrokeCap(Paint.Cap.ROUND);
        routePaint.setStrokeJoin(Paint.Join.ROUND);
        routePaint.setColor(Color.argb(235, 29, 163, 255));
        markerFillPaint.setStyle(Paint.Style.FILL);
        markerFillPaint.setColor(Color.argb(235, 42, 200, 255));
        markerStrokePaint.setStyle(Paint.Style.STROKE);
        markerStrokePaint.setStrokeWidth(3.0f);
        markerStrokePaint.setColor(Color.WHITE);
        destinationFillPaint.setStyle(Paint.Style.FILL);
        destinationFillPaint.setColor(Color.argb(245, 255, 69, 79));
        destinationStrokePaint.setStyle(Paint.Style.STROKE);
        destinationStrokePaint.setStrokeWidth(4.0f);
        destinationStrokePaint.setColor(Color.WHITE);
        setWillNotDraw(false);
        setFocusable(true);
        setClickable(true);
        setLayerType(View.LAYER_TYPE_HARDWARE, null);
        loadTileManifest();
        requestBaseMap();
    }

    public void setTileRoot(String tileRoot) {
        if (tileRoot == null || tileRoot.trim().isEmpty()) {
            return;
        }
        String normalized = tileRoot.trim();
        if (normalized.equals(this.tileRoot) && (tileManifestLoaded || !availableTiles.isEmpty())) {
            invalidate();
            return;
        }
        this.tileRoot = normalized;
        clearTiles();
        loadTileManifest();
        invalidate();
    }

    public void resetView() {
        zoom = defaultZoomForViewport();
        centerOnFocusOrMap();
    }

    public void setWorldFocus(float worldX, float worldY, boolean resetZoom) {
        if (!Float.isFinite(worldX) || !Float.isFinite(worldY)) {
            return;
        }
        focusWorldX = clampFloat(worldX, GTA_WORLD_MIN, GTA_WORLD_MAX);
        focusWorldY = clampFloat(worldY, GTA_WORLD_MIN, GTA_WORLD_MAX);
        hasWorldFocus = true;
        if (resetZoom) {
            zoom = defaultZoomForViewport();
        } else {
            zoom = clampZoom(zoom);
        }
        centerOnWorldFocus();
    }

    public void setRouteDestination(float worldX, float worldY, boolean centerOnDestination) {
        if (!Float.isFinite(worldX) || !Float.isFinite(worldY)) {
            return;
        }
        destinationWorldX = clampFloat(worldX, GTA_WORLD_MIN, GTA_WORLD_MAX);
        destinationWorldY = clampFloat(worldY, GTA_WORLD_MIN, GTA_WORLD_MAX);
        hasDestination = true;
        if (centerOnDestination && getWidth() > 0 && getHeight() > 0) {
            centerOnWorld(destinationWorldX, destinationWorldY);
        } else {
            invalidate();
        }
    }

    public void clearRouteDestination() {
        hasDestination = false;
        invalidate();
    }

    public void zoomIn() {
        zoomIn(getWidth() * 0.5f, getHeight() * 0.5f);
    }

    public void zoomOut() {
        zoomOut(getWidth() * 0.5f, getHeight() * 0.5f);
    }

    public void releaseMemory() {
        clearTiles();
        missing.clear();
        Bitmap currentBaseMap = baseMapBitmap;
        baseMapBitmap = null;
        baseMapRequested = false;
        if (currentBaseMap != null && !currentBaseMap.isRecycled()) {
            currentBaseMap.recycle();
        }
    }

    private void clearTiles() {
        cacheGeneration++;
        synchronized (pending) {
            pending.clear();
        }
        missing.clear();
        tileCache.evictAll();
    }

    private void zoomIn(float focusX, float focusY) {
        setZoomKeepingFocus(clampZoom(zoom + 1), focusX, focusY);
    }

    private void zoomOut(float focusX, float focusY) {
        setZoomKeepingFocus(clampZoom(zoom - 1), focusX, focusY);
    }

    private void setZoomKeepingFocus(int nextZoom, float focusX, float focusY) {
        if (nextZoom == zoom || getWidth() <= 0 || getHeight() <= 0) {
            return;
        }
        float ratioX = (originX + focusX) / Math.max(1.0f, contentWidth(zoom));
        float ratioY = (originY + focusY) / Math.max(1.0f, contentHeight(zoom));
        zoom = nextZoom;
        originX = ratioX * contentWidth(zoom) - focusX;
        originY = ratioY * contentHeight(zoom) - focusY;
        clampOrigin();
        invalidate();
    }

    @Override
    protected void onSizeChanged(int width, int height, int oldWidth, int oldHeight) {
        super.onSizeChanged(width, height, oldWidth, oldHeight);
        zoom = clampZoom(zoom);
        if (oldWidth == 0 || oldHeight == 0) {
            centerOnFocusOrMap();
        } else {
            clampOrigin();
        }
    }

    private void centerOnMap() {
        originX = (contentWidth(zoom) - getWidth()) * 0.5f;
        originY = (contentHeight(zoom) - getHeight()) * 0.5f;
        clampOrigin();
        invalidate();
    }

    private void centerOnFocusOrMap() {
        if (hasWorldFocus) {
            centerOnWorldFocus();
        } else {
            centerOnMap();
        }
    }

    private void centerOnWorldFocus() {
        centerOnWorld(focusWorldX, focusWorldY);
    }

    private void centerOnWorld(float worldX, float worldY) {
        float contentX = worldToContentX(worldX, zoom);
        float contentY = worldToContentY(worldY, zoom);
        originX = contentX - getWidth() * 0.5f;
        originY = contentY - getHeight() * 0.5f;
        clampOrigin();
        invalidate();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        canvas.drawColor(Color.argb(24, 5, 7, 10));
        clampOrigin();
        boolean drewBaseMap = drawBaseMap(canvas);

        int startX = clampTile((int) Math.floor(originX / TILE_SIZE) - 1, MIN_X[zoom], MAX_X[zoom]);
        int endX = clampTile((int) Math.floor((originX + getWidth()) / TILE_SIZE) + 1, MIN_X[zoom], MAX_X[zoom]);
        int startY = clampTile((int) Math.floor(originY / TILE_SIZE) - 1, MIN_Y[zoom], MAX_Y[zoom]);
        int endY = clampTile((int) Math.floor((originY + getHeight()) / TILE_SIZE) + 1, MIN_Y[zoom], MAX_Y[zoom]);

        for (int y = startY; y <= endY; y++) {
            for (int x = startX; x <= endX; x++) {
                float left = (x - MIN_X[zoom]) * TILE_SIZE - originX;
                float top = (y - MIN_Y[zoom]) * TILE_SIZE - originY;
                tileRect.set(left, top, left + TILE_SIZE, top + TILE_SIZE);
                String key = keyFor(zoom, x, y);
                Bitmap tile = tileCache.get(key);
                if (tile != null && !tile.isRecycled()) {
                    canvas.drawBitmap(tile, null, tileRect, bitmapPaint);
                } else {
                    boolean drewFallback = drawFallbackTile(canvas, zoom, x, y, tileRect);
                    if (!drewFallback && !drewBaseMap) {
                        canvas.drawRoundRect(tileRect, 2.0f, 2.0f, placeholderPaint);
                    }
                    if (!isKnownMissing(key)) {
                        requestTile(key, zoom, x, y);
                    }
                }
                if ((tile == null || tile.isRecycled()) && !drewBaseMap) {
                    canvas.drawRect(tileRect, gridPaint);
                }
            }
        }
        drawRoute(canvas);
        drawWorldFocusMarker(canvas);
        drawDestinationMarker(canvas);
    }

    private boolean drawBaseMap(Canvas canvas) {
        Bitmap base = baseMapBitmap;
        if (base == null || base.isRecycled()) {
            requestBaseMap();
            return false;
        }

        float contentWidth = contentWidth(zoom);
        float contentHeight = contentHeight(zoom);
        float visibleLeft = clampFloat(originX, 0.0f, contentWidth);
        float visibleTop = clampFloat(originY, 0.0f, contentHeight);
        float visibleRight = clampFloat(originX + getWidth(), 0.0f, contentWidth);
        float visibleBottom = clampFloat(originY + getHeight(), 0.0f, contentHeight);
        if (visibleRight <= visibleLeft || visibleBottom <= visibleTop) {
            return false;
        }

        sourceRect.set(
                clampInt(Math.round(visibleLeft * base.getWidth() / contentWidth), 0, base.getWidth() - 1),
                clampInt(Math.round(visibleTop * base.getHeight() / contentHeight), 0, base.getHeight() - 1),
                clampInt(Math.round(visibleRight * base.getWidth() / contentWidth), 1, base.getWidth()),
                clampInt(Math.round(visibleBottom * base.getHeight() / contentHeight), 1, base.getHeight())
        );
        viewportRect.set(
                visibleLeft - originX,
                visibleTop - originY,
                visibleRight - originX,
                visibleBottom - originY
        );
        canvas.drawBitmap(base, sourceRect, viewportRect, fallbackPaint);
        return true;
    }

    private void drawRoute(Canvas canvas) {
        if (!hasDestination || !hasWorldFocus || getWidth() <= 0 || getHeight() <= 0) {
            return;
        }
        float startX = worldToContentX(focusWorldX, zoom) - originX;
        float startY = worldToContentY(focusWorldY, zoom) - originY;
        float endX = worldToContentX(destinationWorldX, zoom) - originX;
        float endY = worldToContentY(destinationWorldY, zoom) - originY;
        canvas.drawLine(startX, startY, endX, endY, routeGlowPaint);
        canvas.drawLine(startX, startY, endX, endY, routePaint);
    }

    private void drawWorldFocusMarker(Canvas canvas) {
        if (!hasWorldFocus || getWidth() <= 0 || getHeight() <= 0) {
            return;
        }
        float x = worldToContentX(focusWorldX, zoom) - originX;
        float y = worldToContentY(focusWorldY, zoom) - originY;
        if (x < -32.0f || y < -32.0f || x > getWidth() + 32.0f || y > getHeight() + 32.0f) {
            return;
        }
        canvas.drawCircle(x, y, 10.0f, markerFillPaint);
        canvas.drawCircle(x, y, 15.0f, markerStrokePaint);
    }

    private void drawDestinationMarker(Canvas canvas) {
        if (!hasDestination || getWidth() <= 0 || getHeight() <= 0) {
            return;
        }
        float x = worldToContentX(destinationWorldX, zoom) - originX;
        float y = worldToContentY(destinationWorldY, zoom) - originY;
        if (x < -40.0f || y < -40.0f || x > getWidth() + 40.0f || y > getHeight() + 40.0f) {
            return;
        }
        canvas.drawCircle(x, y, 12.0f, destinationFillPaint);
        canvas.drawCircle(x, y, 18.0f, destinationStrokePaint);
    }

    private void requestTile(String key, int tileZoom, int tileX, int tileY) {
        if (isKnownMissing(key) || tileCache.get(key) != null || pending.size() >= MAX_PENDING_LOADS) {
            return;
        }
        if (!pending.add(key)) {
            return;
        }
        final int requestGeneration = cacheGeneration;
        decodeExecutor.execute(() -> {
            Bitmap bitmap = decodeTile(tileZoom, tileX, tileY);
            pending.remove(key);
            if (requestGeneration != cacheGeneration) {
                if (bitmap != null && !bitmap.isRecycled()) {
                    bitmap.recycle();
                }
                return;
            }
            if (bitmap == null) {
                missing.add(key);
            } else {
                tileCache.put(key, bitmap);
            }
            postInvalidateOnAnimation();
        });
    }

    private boolean drawFallbackTile(Canvas canvas, int tileZoom, int tileX, int tileY, RectF dest) {
        float currentLeft = (tileX - MIN_X[tileZoom]) * TILE_SIZE;
        float currentTop = (tileY - MIN_Y[tileZoom]) * TILE_SIZE;
        float currentRight = currentLeft + TILE_SIZE;
        float currentBottom = currentTop + TILE_SIZE;
        float currentWidth = contentWidth(tileZoom);
        float currentHeight = contentHeight(tileZoom);

        for (int parentZoom = tileZoom - 1; parentZoom >= MIN_ZOOM; parentZoom--) {
            float parentLeft = currentLeft * contentWidth(parentZoom) / currentWidth;
            float parentTop = currentTop * contentHeight(parentZoom) / currentHeight;
            float parentRight = currentRight * contentWidth(parentZoom) / currentWidth;
            float parentBottom = currentBottom * contentHeight(parentZoom) / currentHeight;

            int parentLocalX = clampTile((int) Math.floor(parentLeft / TILE_SIZE), 0, MAX_X[parentZoom] - MIN_X[parentZoom]);
            int parentLocalY = clampTile((int) Math.floor(parentTop / TILE_SIZE), 0, MAX_Y[parentZoom] - MIN_Y[parentZoom]);
            int parentX = MIN_X[parentZoom] + parentLocalX;
            int parentY = MIN_Y[parentZoom] + parentLocalY;
            String parentKey = keyFor(parentZoom, parentX, parentY);
            Bitmap parentTile = tileCache.get(parentKey);
            if (parentTile != null && !parentTile.isRecycled()) {
                int srcLeft = clampInt(Math.round(parentLeft - parentLocalX * TILE_SIZE), 0, TILE_SIZE - 1);
                int srcTop = clampInt(Math.round(parentTop - parentLocalY * TILE_SIZE), 0, TILE_SIZE - 1);
                int srcRight = clampInt(Math.round(parentRight - parentLocalX * TILE_SIZE), srcLeft + 1, TILE_SIZE);
                int srcBottom = clampInt(Math.round(parentBottom - parentLocalY * TILE_SIZE), srcTop + 1, TILE_SIZE);
                sourceRect.set(srcLeft, srcTop, srcRight, srcBottom);
                canvas.drawBitmap(parentTile, sourceRect, dest, fallbackPaint);
                return true;
            }
            if (!isKnownMissing(parentKey)) {
                requestTile(parentKey, parentZoom, parentX, parentY);
            }
        }
        return false;
    }

    private void loadTileManifest() {
        final String root = tileRoot;
        final int requestGeneration = ++manifestGeneration;
        tileManifestLoaded = false;
        availableTiles = Collections.emptySet();
        decodeExecutor.execute(() -> {
            Set<String> discovered = new HashSet<>();
            int[] discoveredByZoom = new int[MAX_ZOOM + 1];
            for (int z = MIN_ZOOM; z <= MAX_ZOOM; z++) {
                try {
                    String[] names = getContext().getAssets().list(root + "/" + z);
                    if (names == null) {
                        continue;
                    }
                    for (String name : names) {
                        if (name == null || !name.endsWith(".jpg")) {
                            continue;
                        }
                        String base = name.substring(0, name.length() - 4);
                        int split = base.indexOf('_');
                        if (split <= 0 || split >= base.length() - 1) {
                            continue;
                        }
                        try {
                            int x = Integer.parseInt(base.substring(0, split));
                            int y = Integer.parseInt(base.substring(split + 1));
                            if (discovered.add(keyFor(z, x, y))) {
                                discoveredByZoom[z]++;
                            }
                        } catch (NumberFormatException ignored) {
                            // Ignore non-tile files in the same asset folder.
                        }
                    }
                } catch (IOException ignored) {
                    // Some builds keep assets compressed; direct decode remains the fallback path.
                }
            }
            if (requestGeneration != manifestGeneration || !root.equals(tileRoot)) {
                return;
            }
            if (!discovered.isEmpty()) {
                availableTiles = Collections.unmodifiableSet(discovered);
                tileManifestLoaded = true;
                maxAvailableInteractiveZoom = resolveCoveredInteractiveZoom(discoveredByZoom);
            } else {
                maxAvailableInteractiveZoom = DEFAULT_MAX_INTERACTIVE_ZOOM;
            }
            post(() -> {
                zoom = clampZoom(zoom);
                clampOrigin();
                invalidate();
            });
        });
    }

    private static int resolveCoveredInteractiveZoom(int[] discoveredByZoom) {
        int coveredZoom = DEFAULT_MAX_INTERACTIVE_ZOOM;
        for (int z = MIN_ZOOM; z <= MAX_ZOOM; z++) {
            int expected = Math.max(1, (MAX_X[z] - MIN_X[z] + 1) * (MAX_Y[z] - MIN_Y[z] + 1));
            float coverage = discoveredByZoom[z] / (float) expected;
            if (coverage >= MIN_TILE_COVERAGE_FOR_DEEP_ZOOM) {
                coveredZoom = z;
            }
        }
        return clampTile(coveredZoom, MIN_ZOOM, MAX_ZOOM);
    }

    private boolean isKnownMissing(String key) {
        if (missing.contains(key)) {
            return true;
        }
        return tileManifestLoaded && !availableTiles.contains(key);
    }

    private Bitmap decodeTile(int tileZoom, int tileX, int tileY) {
        String path = String.format(Locale.US, "%s/%d/%d_%d.jpg", tileRoot, tileZoom, tileX, tileY);
        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inPreferredConfig = Bitmap.Config.RGB_565;
        options.inDither = false;
        try (InputStream input = getContext().getAssets().open(path)) {
            return BitmapFactory.decodeStream(input, null, options);
        } catch (IOException ignored) {
            return null;
        } catch (OutOfMemoryError error) {
            tileCache.evictAll();
            return null;
        }
    }

    private void requestBaseMap() {
        if (baseMapRequested || (baseMapBitmap != null && !baseMapBitmap.isRecycled())) {
            return;
        }
        baseMapRequested = true;
        decodeExecutor.execute(() -> {
            BitmapFactory.Options options = new BitmapFactory.Options();
            options.inPreferredConfig = Bitmap.Config.RGB_565;
            options.inDither = false;
            Bitmap decoded = null;
            try (InputStream input = getContext().getAssets().open(BASE_MAP_ASSET)) {
                decoded = BitmapFactory.decodeStream(input, null, options);
            } catch (IOException ignored) {
                // HD tiles remain the primary source when the compact base map is absent.
            } catch (OutOfMemoryError error) {
                tileCache.evictAll();
            }
            baseMapBitmap = decoded;
            postInvalidateOnAnimation();
        });
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        scaleDetector.onTouchEvent(event);
        if (event.getPointerCount() > 1) {
            dragging = false;
            return true;
        }
        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                dragging = true;
                movedDuringGesture = false;
                downTouchX = event.getX();
                downTouchY = event.getY();
                lastTouchX = event.getX();
                lastTouchY = event.getY();
                return true;
            case MotionEvent.ACTION_MOVE:
                if (dragging) {
                    float x = event.getX();
                    float y = event.getY();
                    if (Math.abs(x - downTouchX) > tapSlop || Math.abs(y - downTouchY) > tapSlop) {
                        movedDuringGesture = true;
                    }
                    originX -= x - lastTouchX;
                    originY -= y - lastTouchY;
                    lastTouchX = x;
                    lastTouchY = y;
                    clampOrigin();
                    invalidate();
                }
                return true;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                if (event.getActionMasked() == MotionEvent.ACTION_UP && dragging && !movedDuringGesture) {
                    float worldX = contentToWorldX(originX + event.getX(), zoom);
                    float worldY = contentToWorldY(originY + event.getY(), zoom);
                    setRouteDestination(worldX, worldY, false);
                }
                dragging = false;
                return true;
            default:
                return true;
        }
    }

    private void clampOrigin() {
        float width = contentWidth(zoom);
        float height = contentHeight(zoom);
        if (width <= getWidth()) {
            originX = (width - getWidth()) * 0.5f;
        } else {
            originX = clampFloat(originX, 0.0f, width - getWidth());
        }
        if (height <= getHeight()) {
            originY = (height - getHeight()) * 0.5f;
        } else {
            originY = clampFloat(originY, 0.0f, height - getHeight());
        }
    }

    private static int clampTile(int value, int min, int max) {
        return Math.max(min, Math.min(max, value));
    }

    private static float clampFloat(float value, float min, float max) {
        return Math.max(min, Math.min(max, value));
    }

    private int clampZoom(int value) {
        return clampTile(value, minZoomForViewport(), maxInteractiveZoom());
    }

    private int defaultZoomForViewport() {
        return clampTile(Math.max(DEFAULT_ZOOM, minZoomForViewport()), MIN_ZOOM, maxInteractiveZoom());
    }

    private int minZoomForViewport() {
        if (getWidth() <= 0 || getHeight() <= 0) {
            return 3;
        }
        float minWidth = getWidth() * 1.12f;
        float minHeight = getHeight() * 1.12f;
        int maxZoom = maxInteractiveZoom();
        for (int z = MIN_ZOOM; z <= maxZoom; z++) {
            if (contentWidth(z) >= minWidth && contentHeight(z) >= minHeight) {
                return z;
            }
        }
        return maxZoom;
    }

    private int maxInteractiveZoom() {
        return clampTile(maxAvailableInteractiveZoom, MIN_ZOOM, MAX_ZOOM);
    }

    private static float worldToContentX(float worldX, int zoom) {
        float normalized = (clampFloat(worldX, GTA_WORLD_MIN, GTA_WORLD_MAX) - GTA_WORLD_MIN)
                / (GTA_WORLD_MAX - GTA_WORLD_MIN);
        return normalized * contentWidth(zoom);
    }

    private static float worldToContentY(float worldY, int zoom) {
        float normalized = (GTA_WORLD_MAX - clampFloat(worldY, GTA_WORLD_MIN, GTA_WORLD_MAX))
                / (GTA_WORLD_MAX - GTA_WORLD_MIN);
        return normalized * contentHeight(zoom);
    }

    private static float contentToWorldX(float contentX, int zoom) {
        float normalized = clampFloat(contentX, 0.0f, contentWidth(zoom)) / contentWidth(zoom);
        return GTA_WORLD_MIN + normalized * (GTA_WORLD_MAX - GTA_WORLD_MIN);
    }

    private static float contentToWorldY(float contentY, int zoom) {
        float normalized = clampFloat(contentY, 0.0f, contentHeight(zoom)) / contentHeight(zoom);
        return GTA_WORLD_MAX - normalized * (GTA_WORLD_MAX - GTA_WORLD_MIN);
    }

    private static int clampInt(int value, int min, int max) {
        return Math.max(min, Math.min(max, value));
    }

    private static String keyFor(int zoom, int x, int y) {
        return zoom + "/" + x + "_" + y;
    }

    private static float contentWidth(int zoom) {
        return (MAX_X[zoom] - MIN_X[zoom] + 1) * TILE_SIZE;
    }

    private static float contentHeight(int zoom) {
        return (MAX_Y[zoom] - MIN_Y[zoom] + 1) * TILE_SIZE;
    }

    @Override
    protected void onDetachedFromWindow() {
        releaseMemory();
        decodeExecutor.shutdownNow();
        super.onDetachedFromWindow();
    }
}
