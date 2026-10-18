package com.bda.controller;

public final class MotionEvent {
    public static final int AXIS_X = 0;
    public static final int AXIS_Y = 1;
    public static final int AXIS_Z = 11;
    public static final int AXIS_RZ = 14;

    private final int controllerId;
    private final float[] axisValues = new float[16];

    public MotionEvent(int controllerId) {
        this.controllerId = controllerId;
    }

    public int getControllerId() {
        return controllerId;
    }

    public float getAxisValue(int axis) {
        if (axis < 0 || axis >= axisValues.length) {
            return 0.0f;
        }

        return axisValues[axis];
    }

    public void setAxisValue(int axis, float value) {
        if (axis >= 0 && axis < axisValues.length) {
            axisValues[axis] = value;
        }
    }
}
