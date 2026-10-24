package org.nephelium.phoneticipa;

import android.content.Context;
import android.graphics.Typeface;
import android.graphics.Paint;
import android.graphics.fonts.*;
import android.graphics.drawable.GradientDrawable;
import android.view.*;
import android.widget.*;
import java.io.IOException;
import java.util.StringJoiner;

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
    static String displaySymbol(String value) {
        if (value.equals(" ")) return "␠";
        if (value.isEmpty()) return "";
        int type = Character.getType(value.codePointAt(0));
        return type == Character.NON_SPACING_MARK || type == Character.COMBINING_SPACING_MARK || type == Character.ENCLOSING_MARK ? "◌" + value : value;
    }
    static String codePoints(String value) {
        StringJoiner result = new StringJoiner(" "); value.codePoints().forEach(code -> result.add(String.format(java.util.Locale.ROOT, "U+%04X", code))); return result.toString();
    }
    static GradientDrawable background(int color, float radius) { GradientDrawable b = new GradientDrawable(); b.setColor(color); b.setCornerRadius(radius); return b; }
    static LinearLayout column(Context c) { LinearLayout v = new LinearLayout(c); v.setOrientation(LinearLayout.VERTICAL); return v; }
    static LinearLayout row(Context c) { LinearLayout v = new LinearLayout(c); v.setOrientation(LinearLayout.HORIZONTAL); v.setGravity(Gravity.CENTER_VERTICAL); return v; }
    static TextView text(Context c, String value, int size) { TextView v = new TextView(c); v.setText(value); v.setTextSize(size); v.setTextColor(INK); return v; }
    static Button ipaButton(Context c, String label, Runnable action) {
        Button button = styleButton(new IpaKey(c), label, action); button.setTypeface(ipa(c)); return button;
    }
    static Button button(Context c, String label, Runnable action) {
        return styleButton(new Button(c), label, action);
    }
    static Button styleButton(Button b, String label, Runnable action) {
        Context c = b.getContext(); b.setText(label); b.setAllCaps(false); b.setSingleLine(!label.contains("\n")); b.setTextColor(INK); b.setTextSize(14);
        b.setMinWidth(0); b.setMinimumWidth(0); b.setMinHeight(0); b.setMinimumHeight(0); b.setPadding(dp(c, 3), 0, dp(c, 3), 0);
        b.setBackground(background(0xffffffff, dp(c, 7))); b.setElevation(dp(c, 1)); b.setStateListAnimator(null);
        b.setOnClickListener(v -> action.run()); return b;
    }
    static int fontHeight(Context c, int sp, int minimum) {
        Paint paint = new Paint(); paint.setTypeface(ipa(c));
        paint.setTextSize(sp * c.getResources().getDisplayMetrics().scaledDensity);
        Paint.FontMetrics metrics = paint.getFontMetrics();
        return Math.max(minimum, (int)Math.ceil((metrics.bottom - metrics.top) / c.getResources().getDisplayMetrics().density) + 10);
    }
    static int ipaHeight(Context c, int dip, int minimum) {
        Paint paint = new Paint(); paint.setTypeface(ipa(c)); paint.setTextSize(dip * c.getResources().getDisplayMetrics().density);
        Paint.FontMetrics metrics = paint.getFontMetrics();
        return Math.max(minimum, (int)Math.ceil((metrics.bottom - metrics.top) / c.getResources().getDisplayMetrics().density) + 10);
    }
    private static int controlHeight(View view, int height) {
        if (height < 0 || view instanceof IpaKey || !(view instanceof TextView)) return height;
        TextView text = (TextView)view;
        Paint.FontMetrics metrics = text.getPaint().getFontMetrics();
        int lines = Math.max(1, text.getText().toString().split("\n", -1).length);
        int needed = (int)Math.ceil(((metrics.bottom - metrics.top) * lines + text.getPaddingTop() + text.getPaddingBottom()) / view.getResources().getDisplayMetrics().density) + 8;
        return Math.max(height, needed);
    }
    static void weighted(LinearLayout parent, View view, float weight, int height) {
        LinearLayout.LayoutParams p = new LinearLayout.LayoutParams(0, height < 0 ? height : dp(parent.getContext(), controlHeight(view, height)), weight); p.setMargins(dp(parent.getContext(), 2), dp(parent.getContext(), 3), dp(parent.getContext(), 2), dp(parent.getContext(), 3)); parent.addView(view, p);
    }
    static void full(LinearLayout parent, View view, int height) { parent.addView(view, new LinearLayout.LayoutParams(-1, height < 0 ? height : dp(parent.getContext(), controlHeight(view, height)))); }
    static ScrollView guide(Context c) {
        ScrollView scroll = new ScrollView(c); LinearLayout content = column(c); content.setPadding(dp(c, 14), dp(c, 10), dp(c, 14), dp(c, 16)); scroll.addView(content);
        for (int index = 0; index < Onboarding.TITLES.length; index++) {
            TextView heading = text(c, Onboarding.TITLES[index], 16); heading.setTypeface(Typeface.DEFAULT_BOLD); heading.setPadding(0, dp(c, 12), 0, dp(c, 4)); full(content, heading, -2);
            TextView detail = text(c, Onboarding.DETAILS[index], 14); detail.setTextColor(MUTED); full(content, detail, -2);
        }
        String[][] sections = {
            {"开始使用", "在设置页点启用键盘，启用国际音标输入法，再点切换键盘。输入时也可用地球键换回其他输入法。"},
            {"汉字与音标分别上屏", "拼音模式输入拼音，点击上面的汉字输入汉字，点击下面的音标输入 IPA。默认数字键和空格选音标，上方音标／汉字按钮可切换输出。点左下角的拼音／IPA 切换输入方式。"},
            {"多音字", "优先按词语和输入拼音筛选读音。出现多个带调读音时，选定读音后直接上屏。长按音标可以重新选择。"},
            {"数字与五度调值", "轻点 1–9 选择当前页候选，没有候选时输入数字。上划 1–5 输入五度调值，1 最低、5 最高。依次上划 2、1、4，再点上方候选，可输入 ˨˩˦、²¹⁴ 或 214。"},
            {"先确认当前候选", "输入声调前先让当前汉字或音标上屏。声调输入期间轻点 1–5 可追加调值，空格确认。0 在 IPA 模式保留原映射。"},
            {"音标和组合编码", "首次启用用拼音输入音标，切到 IPA 可按键位点选符号。带 ··· 的键轻点后弹出音标，点哪个就输入哪个；没有角标的单候选键直接输入。带有组合输入标记的分区收录以此键开头的编码，例如 v 下的 voqs。打开或关闭面板不会输入字母。"},
            {"斜杠等符号键", "先切到 IPA，再点符号。点编码为 / 或 \\ 的键，在弹出的面板中点选输出。\\ 收录原数字键上的声调音标。默认 / 输出斜杠，可以在设置中修改。面板可上下滑动，关闭时不输入。键盘上方的 IPA Plus 可随时打开分类音标表并连续点选输入；先确认已有的拼音或声调候选。"},
            {"修改键位", "在设置页点击按键，候选从左到右排列。可以直接编辑或用 IPA Plus 点选符号，然后保存方案。每页候选可设 3–9 个，支持导入和导出 IPA / JSON。"}
        };
        for (String[] section : sections) { TextView title = text(c, section[0], 16); title.setTypeface(Typeface.DEFAULT_BOLD); title.setPadding(0, dp(c, 12), 0, dp(c, 4)); full(content, title, -2); TextView detail = text(c, section[1], 14); detail.setTextColor(MUTED); full(content, detail, -2); }
        scroll.setContentDescription("usage-guide"); return scroll;
    }
}
