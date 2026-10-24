package org.nephelium.phoneticipa;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;

/** A held delete key repeats only while its original touch stays active. */
final class RepeatKey extends IpaKey {
    private final Handler timer = new Handler(Looper.getMainLooper());
    private boolean touching, repeated;
    private int pointer = -1;
    private final int slop;
    private final Runnable repeat = new Runnable() {
        @Override public void run() {
            if (!touching || !isEnabled() || !isAttachedToWindow() || !isShown()) { cancelRepeat(); return; }
            repeated = true;
            performClick();
            if (touching) timer.postDelayed(this, 65);
        }
    };
    RepeatKey(Context context) { super(context); slop = ViewConfiguration.get(context).getScaledTouchSlop(); }
    void cancelRepeat() { timer.removeCallbacks(repeat); touching = false; pointer = -1; setPressed(false); }
    private boolean inside(float x, float y) { return x >= -slop && y >= -slop && x < getWidth() + slop && y < getHeight() + slop; }
    @Override public boolean onTouchEvent(MotionEvent event) {
        if (!isEnabled()) { cancelRepeat(); return false; }
        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                cancelRepeat(); touching = true; repeated = false; pointer = event.getPointerId(0); setPressed(true);
                timer.postDelayed(repeat, ViewConfiguration.getLongPressTimeout()); return true;
            case MotionEvent.ACTION_MOVE:
                int index = event.findPointerIndex(pointer);
                if (index < 0 || !inside(event.getX(index), event.getY(index))) cancelRepeat();
                return true;
            case MotionEvent.ACTION_POINTER_UP:
                if (event.getPointerId(event.getActionIndex()) == pointer) cancelRepeat();
                return true;
            case MotionEvent.ACTION_UP:
                boolean click = touching && !repeated && inside(event.getX(), event.getY());
                cancelRepeat(); if (click) performClick(); return true;
            case MotionEvent.ACTION_CANCEL: cancelRepeat(); return true;
            default: return touching;
        }
    }
    @Override public boolean performClick() { return super.performClick(); }
    @Override protected void onDetachedFromWindow() { cancelRepeat(); super.onDetachedFromWindow(); }
    @Override protected void onWindowVisibilityChanged(int visibility) { super.onWindowVisibilityChanged(visibility); if (visibility != View.VISIBLE) cancelRepeat(); }
}
