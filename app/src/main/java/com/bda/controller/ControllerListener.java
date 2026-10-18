package com.bda.controller;

public interface ControllerListener {
    void onKeyEvent(KeyEvent event);

    void onMotionEvent(MotionEvent event);

    void onStateEvent(StateEvent event);
}
