package org.nephelium.phoneticipa;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Rect;
import android.widget.Button;

/** Fit the actual IPA ink to a key, without using a font's oversized line box. */
class IpaKey extends Button {
    private final Rect ink = new Rect();
    private final Paint glyphPaint = new Paint(Paint.ANTI_ALIAS_FLAG | Paint.SUBPIXEL_TEXT_FLAG);
    IpaKey(Context context) { super(context); setWillNotDraw(false); }
    @Override protected void onDraw(Canvas canvas) {
        String text = getText().toString();
        if (text.isEmpty()) return;
        Paint paint = glyphPaint; paint.setTypeface(getTypeface()); paint.setTextSize(getTextSize()); paint.setColor(getCurrentTextColor()); paint.setTextAlign(Paint.Align.LEFT);
        paint.getTextBounds(text, 0, text.length(), ink);
        if (ink.isEmpty()) return;
        float width = Math.max(1, getWidth() - getPaddingLeft() - getPaddingRight() - UI.dp(getContext(), 4));
        float height = Math.max(1, getHeight() - getPaddingTop() - getPaddingBottom() - UI.dp(getContext(), 6));
        float scale = Math.min(1f, Math.min(width / ink.width(), height / ink.height()));
        int saved = canvas.save();
        // Single-line TextView may center its very wide internal layout by scrolling
        // more than 500,000 px. Draw in the visible key coordinates, not that layout.
        canvas.translate(getScrollX() + getWidth() / 2f, getScrollY() + getHeight() / 2f); canvas.scale(scale, scale);
        canvas.drawText(text, -ink.exactCenterX(), -ink.exactCenterY(), paint);
        canvas.restoreToCount(saved);
    }
}
