package com.bda.controller;

public final class KeyEvent {
    public static final int ACTION_DOWN = 0;
    public static final int ACTION_UP = 1;

    private final int controllerId;
    private final int action;
    private final int keyCode;

    public KeyEvent(int controllerId, int action, int keyCode) {
        this.controllerId = controllerId;
        this.action = action;
        this.keyCode = keyCode;
    }

    public int getControllerId() {
        return controllerId;
    }

    public int getAction() {
        return action;
    }

    public int getKeyCode() {
        return keyCode;
    }
}
