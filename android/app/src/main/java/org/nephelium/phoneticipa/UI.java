package org.nephelium.phoneticipa;

import android.content.Context;
import android.graphics.Typeface;
import android.graphics.fonts.*;
import android.graphics.drawable.GradientDrawable;
import android.view.*;
import android.widget.*;
import java.io.IOException;

final class UI {
    static final int INK = 0xff172f36, ACCENT = 0xff215c66, SURFACE = 0xffe7eef2, MUTED = 0xff62777f;
    static int dp(Context c, float size) { return Math.round(c.getResources().getDisplayMetrics().density * size); }
    private static Typeface ipaTypeface;
    static synchronized Typeface ipa(Context c) {
        if (ipaTypeface != null) return ipaTypeface;
        try {
            Font doulos = new Font.Builder(c.getAssets(), "IPA/DoulosSIL-Regular.ttf").build();
            Font plus = new Font.Builder(c.getAssets(), "IPA/PTBIPAPlus-Regular.ttf").build();
            ipaTypeface = new Typeface.CustomFallbackBuilder(new FontFamily.Builder(doulos).build()).addCustomFallback(new FontFamily.Builder(plus).build()).setSystemFallback("sans-serif").build();
        } catch (IOException e) { ipaTypeface = Typeface.SERIF; }
        return ipaTypeface;
    }
    static GradientDrawable background(int color, float radius) { GradientDrawable b = new GradientDrawable(); b.setColor(color); b.setCornerRadius(radius); return b; }
    static LinearLayout column(Context c) { LinearLayout v = new LinearLayout(c); v.setOrientation(LinearLayout.VERTICAL); return v; }
    static LinearLayout row(Context c) { LinearLayout v = new LinearLayout(c); v.setOrientation(LinearLayout.HORIZONTAL); v.setGravity(Gravity.CENTER_VERTICAL); return v; }
    static TextView text(Context c, String value, int size) { TextView v = new TextView(c); v.setText(value); v.setTextSize(size); v.setTextColor(INK); return v; }
    static Button button(Context c, String label, Runnable action) {
        return styleButton(new Button(c), label, action);
    }
    static Button styleButton(Button b, String label, Runnable action) {
        Context c = b.getContext(); b.setText(label); b.setAllCaps(false); b.setTextColor(INK); b.setTextSize(14);
        b.setMinWidth(0); b.setMinimumWidth(0); b.setMinHeight(0); b.setMinimumHeight(0); b.setPadding(dp(c, 3), 0, dp(c, 3), 0);
        b.setBackground(background(0xffffffff, dp(c, 7))); b.setElevation(dp(c, 1)); b.setStateListAnimator(null);
        b.setOnClickListener(v -> action.run()); return b;
    }
    static void weighted(LinearLayout parent, View view, float weight, int height) {
        LinearLayout.LayoutParams p = new LinearLayout.LayoutParams(0, dp(parent.getContext(), height), weight); p.setMargins(dp(parent.getContext(), 2), dp(parent.getContext(), 3), dp(parent.getContext(), 2), dp(parent.getContext(), 3)); parent.addView(view, p);
    }
    static void full(LinearLayout parent, View view, int height) { parent.addView(view, new LinearLayout.LayoutParams(-1, height < 0 ? height : dp(parent.getContext(), height))); }
    static ScrollView guide(Context c) {
        ScrollView scroll = new ScrollView(c); LinearLayout content = column(c); content.setPadding(dp(c, 14), dp(c, 10), dp(c, 14), dp(c, 16)); scroll.addView(content);
        String[][] sections = {
            {"开始使用", "在设置页点启用键盘，启用国际音标输入法，再点切换键盘。输入时也可用地球键换回其他输入法。"},
            {"汉字与音标分别上屏", "中模式输入拼音，点击上面的汉字输入汉字，点击下面的音标输入 IPA。点击中 / IPA 切换模式。"},
            {"多音字", "优先按词语和输入拼音筛选读音。出现多个带调读音时，先选择读音，再点音标上屏。长按音标可以重新选择。"},
            {"数字与五度调值", "轻点 1–9 选择当前页候选，没有候选时输入数字。上划 1–5 输入五度调值，1 最低、5 最高。依次上划 2、1、4，再点上方候选，可输入 ˨˩˦、²¹⁴ 或 214。"},
            {"先确认当前候选", "输入声调前先让当前汉字或音标上屏。声调输入期间轻点 1–5 可追加调值，空格确认。0 在 IPA 模式保留原映射。"},
            {"音标和组合编码", "IPA 模式按 CIN 编码输入音标。长按字母可查看该键和以它开头的组合编码。含数字的编码可加 code: 前缀。"},
            {"斜杠等符号键", "先切到 IPA，再点符号。点编码为 / 或 \\ 的键，从上方候选选择输出。\\ 收录原数字键上的声调音标。默认 / 输出斜杠，可以在设置中修改。长按可查看对应候选。"},
            {"修改键位", "在设置页点击按键，候选从左到右排列。可以直接编辑或用 IPA Plus 点选符号，然后保存方案。每页候选可设 3–9 个，支持导入和导出 CIN / JSON。"}
        };
        for (String[] section : sections) { TextView title = text(c, section[0], 16); title.setTypeface(Typeface.DEFAULT_BOLD); title.setPadding(0, dp(c, 12), 0, dp(c, 4)); full(content, title, -2); TextView detail = text(c, section[1], 14); detail.setTextColor(MUTED); full(content, detail, -2); }
        scroll.setContentDescription("usage-guide"); return scroll;
    }
}
