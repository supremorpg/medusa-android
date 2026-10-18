package com.bda.controller;

public final class StateEvent {
    public static final int STATE_CONNECTION = 1;

    private final int controllerId;
    private final int state;
    private final int action;

    public StateEvent(int controllerId, int state, int action) {
        this.controllerId = controllerId;
        this.state = state;
        this.action = action;
    }

    public int getControllerId() {
        return controllerId;
    }

    public int getState() {
        return state;
    }

    public int getAction() {
        return action;
    }
}
