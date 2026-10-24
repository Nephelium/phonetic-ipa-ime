package org.nephelium.phoneticipa;

import android.inputmethodservice.InputMethodService;
import android.view.*;
import android.view.inputmethod.*;
import android.widget.*;
import org.json.*;
import java.util.*;

public final class PhoneticIME extends InputMethodService {
    private LinearLayout root, candidatePanel, keyPanel;
    private TextView updateNotice;
    private Session model;
    private boolean uppercase, symbols, alive = true;
    @Override public View onCreateInputView() {
        root = UI.column(this); root.setPadding(UI.dp(this, 4), UI.dp(this, 5), UI.dp(this, 4), UI.dp(this, 7)); root.setBackgroundColor(UI.SURFACE);
        root.setOnApplyWindowInsetsListener((view, insets) -> {
            int bottom = android.os.Build.VERSION.SDK_INT >= 30 ? insets.getInsets(WindowInsets.Type.navigationBars()).bottom : insets.getSystemWindowInsetBottom();
            view.setPadding(UI.dp(this, 4), UI.dp(this, 5), UI.dp(this, 4), UI.dp(this, 7) + bottom); return insets;
        });
        root.addView(UI.text(this, "正在准备音标资料…", 14));
        Engine.get(this).whenComplete((engine, error) -> new android.os.Handler(getMainLooper()).post(() -> {
            if (!alive || root == null) return;
            root.removeAllViews();
            if (error != null) { root.addView(UI.text(this, "资料载入失败：" + error.getMessage(), 14)); return; }
            model = new Session(engine); model.commit = text -> { InputConnection input = getCurrentInputConnection(); if (input != null) input.commitText(text, 1); };
            model.changed = this::candidates;
            updateNotice = UI.text(this, "", 12); updateNotice.setTextColor(UI.ACCENT);
            updateNotice.setPadding(UI.dp(this, 8), UI.dp(this, 4), UI.dp(this, 8), UI.dp(this, 4));
            updateNotice.setOnClickListener(v -> startActivity(new android.content.Intent(this, SettingsActivity.class).addFlags(android.content.Intent.FLAG_ACTIVITY_NEW_TASK)));
            UI.full(root, updateNotice, -2); updateNotice();
            candidatePanel = UI.column(this); keyPanel = UI.column(this); UI.full(root, candidatePanel, -2); UI.full(root, keyPanel, -2);
            keys(); candidates();
        }));
        return root;
    }
    @Override public boolean onEvaluateFullscreenMode() { return false; }
    @Override public void onStartInput(EditorInfo info, boolean restarting) { super.onStartInput(info, restarting); if (model != null) model.reload();
        Updates.check(this, false, message -> { if (alive) updateNotice(); });
    }
    private void updateNotice() {
        if (updateNotice == null) return; Updates.Feed feed = Updates.available(this);
        updateNotice.setVisibility(feed == null ? View.GONE : View.VISIBLE);
        if (feed != null) updateNotice.setText("新版本 " + feed.version + " 可用 · 点此下载");
    }
    @Override public void onFinishInput() { if (model != null) model.clear(); super.onFinishInput(); }
    @Override public void onDestroy() { alive = false; super.onDestroy(); }
    @Override public void onUpdateSelection(int oldStart, int oldEnd, int newStart, int newEnd, int composingStart, int composingEnd) {
        super.onUpdateSelection(oldStart, oldEnd, newStart, newEnd, composingStart, composingEnd);
        if (model != null && !model.raw.isEmpty() && (newStart != composingEnd || newEnd != composingEnd)) model.clear();
    }
    private void candidates() {
        if (candidatePanel == null || model == null) return;
        candidatePanel.removeAllViews();
        LinearLayout header = UI.row(this);
        TextView code = UI.text(this, model.raw.isEmpty() ? (model.pinyin ? "拼音 → 汉字 / 音标" : "IPA · 输入编码") : model.raw, 13);
        code.setTextColor(UI.MUTED); UI.weighted(header, code, 5, 28);
        if (!model.candidates.isEmpty()) { UI.weighted(header, UI.button(this, "‹", () -> model.page(-1)), 1, 28); UI.weighted(header, UI.text(this, "" + (model.page + 1), 12), 1, 28); UI.weighted(header, UI.button(this, "›", () -> model.page(1)), 1, 28); }
        UI.full(candidatePanel, header, -2);
        LinearLayout choices = UI.row(this); HorizontalScrollView scroll = new HorizontalScrollView(this); scroll.setHorizontalScrollBarEnabled(false); scroll.addView(choices);
        if (model.pending != null) {
            try {
                JSONArray options = model.options();
                for (int i = 0; i < options.length(); i++) {
                    int index = i; JSONObject option = options.getJSONObject(i);
                    Button choice = UI.button(this, option.getString("pinyin") + "\n" + option.getString("ipa"), () -> model.chooseReading(index));
                    choice.setTypeface(UI.ipa(this)); choice.setTextSize(22); choice.setContentDescription("reading-" + option.getString("pinyin")); addChoice(choices, choice, 76);
                }
                addChoice(choices, UI.button(this, "返回", () -> { model.pending = null; candidates(); }), 76);
            } catch (JSONException e) { model.status = e.getMessage(); }
        } else {
            List<Session.Candidate> visible = model.visible();
            for (int i = 0; i < visible.size(); i++) {
                int index = i; Session.Candidate item = visible.get(i); LinearLayout card = UI.column(this); card.setBackground(UI.background(0xfff8fbfc, UI.dp(this, 8)));
                Button word = UI.button(this, (i + 1) + "  " + item.word, () -> model.choose(index, false));
                word.setTextSize(21); if (item.symbol()) word.setTypeface(UI.ipa(this)); word.setContentDescription("hanzi-" + item.word + "-" + item.canonical); UI.full(card, word, 34);
                if (!item.symbol()) {
                    String value = item.ipa().isEmpty() ? (item.reading.optBoolean("ambiguous") ? "选择读音…" : "暂无音标") : item.ipa();
                    Button ipa = UI.button(this, value, () -> model.choose(index, true)); ipa.setTextSize(23); ipa.setTypeface(UI.ipa(this)); ipa.setTextColor(UI.ACCENT); ipa.setContentDescription("ipa-" + item.word + "-" + item.canonical);
                    ipa.setOnLongClickListener(v -> { model.reopen(index); return true; }); UI.full(card, ipa, 36);
                }
                addChoice(choices, card, 76);
            }
        }
        if (!model.raw.isEmpty()) UI.full(candidatePanel, scroll, 80);
        if (!model.status.isEmpty()) { TextView status = UI.text(this, model.status, 11); status.setTextColor(UI.MUTED); UI.full(candidatePanel, status, -2); }
        InputConnection input = getCurrentInputConnection();
        if (input != null) { if (model.raw.isEmpty()) input.finishComposingText(); else input.setComposingText(model.raw, 1); }
    }
    private void addChoice(LinearLayout row, View child, int height) {
        LinearLayout.LayoutParams p = new LinearLayout.LayoutParams(-2, UI.dp(this, height)); p.setMargins(UI.dp(this, 3), 0, UI.dp(this, 3), 0); child.setMinimumWidth(UI.dp(this, 70)); row.addView(child, p);
    }
    private void keys() {
        keyPanel.removeAllViews();
        LinearLayout digits = UI.row(this);
        for (char c : "1234567890".toCharArray()) {
            String digit = "" + c;
            Button key = c >= '1' && c <= '5' ? UI.styleButton(new ToneKey(this, () -> model.tone(digit)), digit, () -> model.digit(digit)) : UI.button(this, digit, () -> model.digit(digit));
            key.setTextSize(22); key.setContentDescription("digit-" + digit);
            UI.weighted(digits, key, 1, 35);
        }
        UI.full(keyPanel, digits, -2);
        JSONObject mappings;
        try { mappings = model.engine.profile().getJSONObject("sequences"); } catch (Exception e) { mappings = new JSONObject(); }
        for (String row : symbols ? new String[]{"-/:;()$&@", "[]{}#%^*+=", ".,?!'\\_~<>"} : new String[]{"qwertyuiop", "asdfghjkl", "zxcvbnm"}) {
            LinearLayout keys = UI.row(this);
            if (row.equals("asdfghjkl")) keys.setPadding(UI.dp(this, 16), 0, UI.dp(this, 16), 0);
            if (row.equals("zxcvbnm")) UI.weighted(keys, shiftButton(), 1.35f, 44);
            for (char c : row.toCharArray()) {
                String key = uppercase ? ("" + c).toUpperCase(Locale.ROOT) : "" + c;
                JSONArray values = mappings.optJSONArray(key); String output = !model.pinyin && values != null ? values.optString(0, key) : key;
                Button b = UI.button(this, output, () -> model.append(key)); b.setTypeface(UI.ipa(this)); b.setTextSize(25); b.setContentDescription("key-" + c);
                b.setOnLongClickListener(v -> { preview(key); return true; });
                if (!output.equals(key)) {
                    FrameLayout cap = new FrameLayout(this); cap.addView(b, new FrameLayout.LayoutParams(-1, -1));
                    TextView label = UI.text(this, key, 9); label.setTextColor(UI.MUTED); label.setPadding(UI.dp(this, 3), UI.dp(this, 2), 0, 0); label.setElevation(b.getElevation() + UI.dp(this, 1)); label.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO); cap.addView(label, new FrameLayout.LayoutParams(-2, -2, Gravity.TOP | Gravity.START)); UI.weighted(keys, cap, 1, 44);
                } else UI.weighted(keys, b, 1, 44);
            }
            if (row.equals("zxcvbnm")) UI.weighted(keys, backspaceButton(), 1.35f, 44);
            UI.full(keyPanel, keys, -2);
        }
        LinearLayout actions = UI.row(this);
        UI.weighted(actions, UI.button(this, "🌐", () -> switchToNextInputMethod(false)), 0.8f, 39);
        UI.weighted(actions, UI.button(this, model.pinyin ? "中" : "IPA", () -> { model.toggle(); keys(); }), 1, 39);
        UI.weighted(actions, UI.button(this, "说明", this::help), 1, 39);
        if (symbols) UI.weighted(actions, shiftButton(), 0.8f, 39);
        UI.weighted(actions, UI.button(this, symbols ? "ABC" : "符号", () -> { symbols = !symbols; keys(); }), 1, 39);
        UI.weighted(actions, UI.button(this, "空格", () -> { if (model.raw.isEmpty()) model.commit.accept(" "); else model.choose(0, false); }), 3, 39);
        if (symbols) UI.weighted(actions, backspaceButton(), 1, 39);
        Button enter = UI.button(this, "↵", () -> { if (model.raw.isEmpty()) { if (!sendDefaultEditorAction(true)) model.commit.accept("\n"); } else if (model.candidates.isEmpty()) { model.commit.accept(model.raw); model.clear(); } else model.choose(0, false); });
        enter.setTextSize(25); enter.setTextColor(0xffffffff); enter.setBackground(UI.background(UI.ACCENT, UI.dp(this, 7))); UI.weighted(actions, enter, 1.35f, 39);
        UI.full(keyPanel, actions, -2);
    }
    private Button shiftButton() { Button key = UI.button(this, uppercase ? "⇪" : "⇧", () -> { uppercase = !uppercase; keys(); }); key.setTextSize(25); key.setContentDescription("shift"); return key; }
    private Button backspaceButton() { Button key = UI.button(this, "⌫", () -> { if (!model.raw.isEmpty()) model.backspace(); else if (getCurrentInputConnection() != null) getCurrentInputConnection().deleteSurroundingTextInCodePoints(1, 0); }); key.setTextSize(22); key.setContentDescription("backspace"); return key; }
    private void preview(String key) {
        try {
            JSONObject entries = model.engine.profile().getJSONObject("sequences"); List<String> lines = new ArrayList<>();
            List<String> codes = new ArrayList<>(); entries.keys().forEachRemaining(codes::add); Collections.sort(codes);
            for (String code : codes) if (code.startsWith(key)) { JSONArray values = entries.getJSONArray(code); List<String> outputs = new ArrayList<>(); for (int i = 0; i < values.length(); i++) outputs.add(values.getString(i)); lines.add(code + (code.equals(key) ? "　" : " · 组合输入　") + String.join("  ", outputs)); }
            candidatePanel.removeAllViews(); TextView text = UI.text(this, String.join("\n", lines), 22); text.setTypeface(UI.ipa(this)); ScrollView scroll = new ScrollView(this); scroll.addView(text); UI.full(candidatePanel, scroll, 150); UI.full(candidatePanel, UI.button(this, "关闭预览", this::candidates), 32);
        } catch (Exception e) { model.status = e.getMessage(); candidates(); }
    }
    private void help() {
        root.removeAllViews(); LinearLayout header = UI.row(this); UI.weighted(header, UI.text(this, "键盘使用说明", 16), 3, 38);
        UI.weighted(header, UI.button(this, "返回键盘", () -> { root.removeAllViews(); UI.full(root, candidatePanel, -2); UI.full(root, keyPanel, -2); candidates(); }), 2, 38); UI.full(root, header, -2); UI.full(root, UI.guide(this), 240);
    }
}
