package com.osrbot.open32drone.controller;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;

public final class JoystickView extends View {
    interface Listener {
        void onMove(float x, float y);
    }

    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF bounds = new RectF();
    private Listener listener;
    private float axisX;
    private float axisY;

    public JoystickView(Context context, AttributeSet attrs) {
        super(context, attrs);
        setFocusable(true);
    }

    void setListener(Listener listener) {
        this.listener = listener;
    }

    void reset() {
        if (axisX != 0.0f || axisY != 0.0f) setAxes(0.0f, 0.0f);
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        float centerX = getWidth() * 0.5f;
        float centerY = getHeight() * 0.5f;
        float radius = Math.max(1.0f, Math.min(getWidth(), getHeight()) * 0.43f);
        bounds.set(centerX - radius, centerY - radius, centerX + radius, centerY + radius);

        paint.setStyle(Paint.Style.FILL);
        paint.setColor(Color.rgb(17, 30, 49));
        canvas.drawOval(bounds, paint);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(2.0f);
        paint.setColor(Color.rgb(40, 64, 95));
        canvas.drawOval(bounds, paint);
        paint.setStrokeWidth(1.0f);
        canvas.drawLine(centerX - radius, centerY, centerX + radius, centerY, paint);
        canvas.drawLine(centerX, centerY - radius, centerX, centerY + radius, paint);

        float knobRadius = radius * 0.23f;
        float knobX = centerX + axisX * (radius - knobRadius);
        float knobY = centerY - axisY * (radius - knobRadius);
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(Color.rgb(55, 214, 176));
        canvas.drawCircle(knobX, knobY, knobRadius, paint);

        CharSequence label = getContentDescription();
        if (label != null) {
            paint.setColor(Color.rgb(158, 179, 204));
            paint.setTextAlign(Paint.Align.CENTER);
            paint.setTextSize(12.0f * getResources().getDisplayMetrics().scaledDensity);
            canvas.drawText(label.toString(), centerX, getHeight() - 6.0f, paint);
        }
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (!isEnabled()) return false;
        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_MOVE:
                updateFromTouch(event.getX(), event.getY());
                return true;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                setAxes(0.0f, 0.0f);
                performClick();
                return true;
            default:
                return false;
        }
    }

    @Override
    public boolean performClick() {
        super.performClick();
        return true;
    }

    private void updateFromTouch(float touchX, float touchY) {
        float centerX = getWidth() * 0.5f;
        float centerY = getHeight() * 0.5f;
        float radius = Math.max(1.0f, Math.min(getWidth(), getHeight()) * 0.43f);
        float x = (touchX - centerX) / radius;
        float y = (centerY - touchY) / radius;
        float magnitude = (float) Math.hypot(x, y);
        if (magnitude > 1.0f) {
            x /= magnitude;
            y /= magnitude;
        }
        setAxes(x, y);
    }

    private void setAxes(float x, float y) {
        axisX = x;
        axisY = y;
        invalidate();
        if (listener != null) listener.onMove(axisX, axisY);
    }
}
