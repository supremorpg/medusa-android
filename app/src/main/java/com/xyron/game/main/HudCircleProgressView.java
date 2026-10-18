package com.xyron.game.main;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.util.AttributeSet;
import android.view.View;

public class HudCircleProgressView extends View {
    private final Paint fillPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint haloPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint progressPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF arcBounds = new RectF();

    private int progress = 100;
    private int progressColor = Color.parseColor("#F59E0B");
    private float density = 1.0f;

    public HudCircleProgressView(Context context) {
        super(context);
        init();
    }

    public HudCircleProgressView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    public HudCircleProgressView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }

    private void init() {
        density = getResources().getDisplayMetrics().density;

        fillPaint.setStyle(Paint.Style.FILL);
        fillPaint.setColor(Color.parseColor("#D9000000"));

        haloPaint.setStyle(Paint.Style.STROKE);
        haloPaint.setStrokeCap(Paint.Cap.ROUND);
        haloPaint.setColor(withAlpha(progressColor, 82));

        progressPaint.setStyle(Paint.Style.STROKE);
        progressPaint.setStrokeCap(Paint.Cap.ROUND);
        progressPaint.setColor(progressColor);
    }

    public void setProgressValue(int value) {
        int safeValue = Math.max(0, Math.min(100, value));
        progress = safeValue;
        invalidate();
    }

    public void setProgressColor(int color) {
        progressColor = color;
        haloPaint.setColor(withAlpha(progressColor, 82));
        progressPaint.setColor(progressColor);
        invalidate();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);

        float size = Math.min(getWidth(), getHeight());
        if (size <= 0.0f) {
            return;
        }

        float stroke = dp(4.1f);
        float centerX = getWidth() / 2.0f;
        float centerY = getHeight() / 2.0f;
        float radius = (size - stroke) / 2.0f - dp(1.0f);
        if (radius <= 0.0f) {
            return;
        }

        haloPaint.setStrokeWidth(stroke);
        progressPaint.setStrokeWidth(stroke);

        canvas.drawCircle(centerX, centerY, radius - dp(1.5f), fillPaint);
        canvas.drawCircle(centerX, centerY, radius, haloPaint);

        arcBounds.set(centerX - radius, centerY - radius, centerX + radius, centerY + radius);
        if (progress >= 99) {
            canvas.drawCircle(centerX, centerY, radius, progressPaint);
        } else if (progress > 0) {
            canvas.drawArc(arcBounds, -90.0f, 360.0f * progress / 100.0f, false, progressPaint);
        }
    }

    private float dp(float value) {
        return value * density;
    }

    private int withAlpha(int color, int alpha) {
        return Color.argb(
                Math.max(0, Math.min(255, alpha)),
                Color.red(color),
                Color.green(color),
                Color.blue(color)
        );
    }
}
