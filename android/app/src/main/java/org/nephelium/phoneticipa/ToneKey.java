package org.nephelium.phoneticipa;

import android.content.Context;
import android.view.MotionEvent;
import android.widget.Button;

/** A digit button: ordinary accessibility click selects; an upward touch gesture adds tone. */
final class ToneKey extends IpaKey {
    private final Runnable swipe;
    private float startX, startY;
    private boolean moved;
    public ToneKey(Context context) { this(context, () -> {}); }
    ToneKey(Context context, Runnable swipe) { super(context); this.swipe = swipe; }
    @Override public boolean performClick() { return super.performClick(); }
    @Override public boolean onTouchEvent(MotionEvent event) {
        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                startX = event.getRawX(); startY = event.getRawY(); moved = false; setPressed(true); return true;
            case MotionEvent.ACTION_MOVE:
                moved |= Math.hypot(event.getRawX() - startX, event.getRawY() - startY) > UI.dp(getContext(), 10); return true;
            case MotionEvent.ACTION_UP:
                setPressed(false);
                float dy = event.getRawY() - startY, dx = event.getRawX() - startX;
                if (dy < -UI.dp(getContext(), 20) && -dy > Math.abs(dx) * 1.3) swipe.run();
                else if (!moved) performClick();
                return true;
            case MotionEvent.ACTION_CANCEL: setPressed(false); return true;
            default: return super.onTouchEvent(event);
        }
    }
}
