package org.nephelium.phoneticipa;

import android.content.Context;
import android.view.MotionEvent;
import android.widget.Button;

/** A digit button: ordinary accessibility click selects; an upward touch gesture adds tone. */
final class ToneKey extends IpaKey {
    private final Runnable swipe;
    private float startX, startY;
    private boolean moved;
    private int pointer = -1;
    public ToneKey(Context context) { this(context, () -> {}); }
    ToneKey(Context context, Runnable swipe) { super(context); this.swipe = swipe; }
    @Override public boolean performClick() { return super.performClick(); }
    private float rawX(MotionEvent event, int index) { return event.getRawX() + (event.getX(index) - event.getX(0)); }
    private float rawY(MotionEvent event, int index) { return event.getRawY() + (event.getY(index) - event.getY(0)); }
    private void finishGesture(MotionEvent event, int index) {
        pointer = -1;
        setPressed(false);
        float dy = rawY(event, index) - startY, dx = rawX(event, index) - startX;
        if (dy < -UI.dp(getContext(), 20) && -dy > Math.abs(dx) * 1.3) swipe.run();
        else if (!moved) performClick();
    }
    @Override public boolean onTouchEvent(MotionEvent event) {
        if (!isEnabled()) { pointer = -1; setPressed(false); return false; }
        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                pointer = event.getPointerId(0);
                startX = event.getRawX(); startY = event.getRawY(); moved = false; setPressed(true); return true;
            case MotionEvent.ACTION_MOVE: {
                int index = event.findPointerIndex(pointer);
                if (index < 0) return true;
                moved |= Math.hypot(rawX(event, index) - startX, rawY(event, index) - startY) > UI.dp(getContext(), 10);
                return true;
            }
            case MotionEvent.ACTION_POINTER_UP: {
                int actionIndex = event.getActionIndex();
                if (event.getPointerId(actionIndex) == pointer) finishGesture(event, actionIndex);
                return true;
            }
            case MotionEvent.ACTION_UP: {
                int index = event.findPointerIndex(pointer);
                if (index >= 0) finishGesture(event, index);
                else { pointer = -1; setPressed(false); }
                return true;
            }
            case MotionEvent.ACTION_CANCEL: pointer = -1; setPressed(false); return true;
            default: return super.onTouchEvent(event);
        }
    }
}
