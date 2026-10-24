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
    private FrameLayout inputFrame;
    private View symbolPicker;
    private RepeatKey deleteKey;
    private int inputViewGeneration;
    private Session model;
    private boolean uppercase, symbols, alive = true;
    @Override public View onCreateInputView() {
        if (deleteKey != null) deleteKey.cancelRepeat();
        final int generation = ++inputViewGeneration;
        inputFrame = new FrameLayout(this);
        root = UI.column(this); root.setPadding(UI.dp(this, 4), UI.dp(this, 5), UI.dp(this, 4), UI.dp(this, 7)); root.setBackgroundColor(UI.SURFACE);
        root.setOnApplyWindowInsetsListener((view, insets) -> {
            int bottom = android.os.Build.VERSION.SDK_INT >= 30 ? insets.getInsets(WindowInsets.Type.navigationBars()).bottom : insets.getSystemWindowInsetBottom();
            view.setPadding(UI.dp(this, 4), UI.dp(this, 5), UI.dp(this, 4), UI.dp(this, 7) + bottom); return insets;
        });
        root.addView(UI.text(this, "正在准备音标资料…", 14));
        Engine.get(this).whenComplete((engine, error) -> new android.os.Handler(getMainLooper()).post(() -> {
            if (!alive || root == null || generation != inputViewGeneration) return;
            root.removeAllViews();
            if (error != null) { root.addView(UI.text(this, "资料载入失败：" + error.getMessage(), 14)); return; }
            model = new Session(engine); model.pinyin = getSharedPreferences("keyboard", MODE_PRIVATE).getBoolean("pinyin-mode", true); model.outputIpa = getSharedPreferences("keyboard", MODE_PRIVATE).getBoolean("output-ipa", true); model.commit = text -> { InputConnection input = getCurrentInputConnection(); if (input != null) input.commitText(text, 1); };
            model.changed = this::candidates;
            updateNotice = UI.text(this, "", 12); updateNotice.setTextColor(UI.ACCENT);
            updateNotice.setPadding(UI.dp(this, 8), UI.dp(this, 4), UI.dp(this, 8), UI.dp(this, 4));
            updateNotice.setOnClickListener(v -> startActivity(new android.content.Intent(this, SettingsActivity.class).addFlags(android.content.Intent.FLAG_ACTIVITY_NEW_TASK)));
            UI.full(root, updateNotice, -2); updateNotice();
            candidatePanel = UI.column(this); keyPanel = UI.column(this); UI.full(root, candidatePanel, -2); UI.full(root, keyPanel, -2);
            keys(); candidates();
        }));
        inputFrame.addView(root, new FrameLayout.LayoutParams(-1, -2));
        return inputFrame;
    }
    @Override public boolean onEvaluateFullscreenMode() { return false; }
    @Override public void onStartInput(EditorInfo info, boolean restarting) { super.onStartInput(info, restarting); if (deleteKey != null) deleteKey.cancelRepeat(); dismissSymbols(); if (model != null) model.reload();
        Updates.check(this, false, message -> { if (alive) updateNotice(); });
    }
    private void updateNotice() {
        if (updateNotice == null) return; Updates.Feed feed = Updates.available(this);
        updateNotice.setVisibility(feed == null ? View.GONE : View.VISIBLE);
        if (feed != null) updateNotice.setText("新版本 " + feed.version + " 可用 · 点此下载");
    }
    @Override public void onFinishInput() { if (deleteKey != null) deleteKey.cancelRepeat(); dismissSymbols(); if (model != null) model.clear(); super.onFinishInput(); }
    @Override public void onFinishInputView(boolean finishingInput) { if (deleteKey != null) deleteKey.cancelRepeat(); super.onFinishInputView(finishingInput); }
    @Override public void onDestroy() { if (deleteKey != null) deleteKey.cancelRepeat(); alive = false; super.onDestroy(); }
    @Override public void onUpdateSelection(int oldStart, int oldEnd, int newStart, int newEnd, int composingStart, int composingEnd) {
        super.onUpdateSelection(oldStart, oldEnd, newStart, newEnd, composingStart, composingEnd);
        if (model != null && !model.raw.isEmpty() && (newStart != composingEnd || newEnd != composingEnd)) model.clear();
    }
    private void candidates() {
        if (candidatePanel == null || model == null) return;
        candidatePanel.removeAllViews();
        LinearLayout header = UI.row(this);
        TextView code = UI.text(this, model.raw.isEmpty() ? (model.pinyin ? (model.outputIpa ? "拼音 → 音标" : "拼音 → 汉字") : "IPA · 输入编码") : model.raw, 13);
        code.setContentDescription("composition-code");
        code.setSingleLine(true); code.setEllipsize(android.text.TextUtils.TruncateAt.START);
        code.setTextColor(UI.MUTED); UI.weighted(header, code, 5, keyHeight(32, 24));
        if (model.pinyin) {
            Button output = UI.button(this, model.outputIpa ? "音标" : "汉字", () -> {
                model.outputIpa = !model.outputIpa;
                getSharedPreferences("keyboard", MODE_PRIVATE).edit().putBoolean("output-ipa", model.outputIpa).apply();
                candidates();
            });
            output.setContentDescription("output-mode"); output.setTextSize(android.util.TypedValue.COMPLEX_UNIT_DIP, 12);
            UI.weighted(header, output, 2, keyHeight(32, 24));
            Button rules = UI.button(this, "规则", () -> {});
            rules.setTextSize(android.util.TypedValue.COMPLEX_UNIT_DIP, 12); rules.setContentDescription("transcription-rules");
            rules.setOnClickListener(v -> {
                try {
                    JSONArray choices = model.engine.rules();
                    android.widget.PopupMenu menu = new android.widget.PopupMenu(this, rules);
                    for (int i=0; i<choices.length(); i++) { JSONObject choice = choices.getJSONObject(i);
                        menu.getMenu().add(0,i,i,choice.getString("name")).setCheckable(true).setChecked(choice.getString("id").equals(model.engine.ruleId())); }
                    menu.setOnMenuItemClickListener(item -> {
                        try { model.engine.selectRule(choices.getJSONObject(item.getItemId()).getString("id")); model.refresh(); }
                        catch (Exception error) { model.status = error.getMessage(); candidates(); }
                        return true;
                    }); menu.show();
                } catch (Exception error) { model.status = error.getMessage(); candidates(); }
            });
            UI.weighted(header, rules, 2, keyHeight(32,24));
        }
        Button plus = UI.button(this, "IPA Plus", this::showPlus); plus.setContentDescription("keyboard-plus"); plus.setTextSize(android.util.TypedValue.COMPLEX_UNIT_DIP, 12); UI.weighted(header, plus, 2, keyHeight(32, 24));
        if (!model.candidates.isEmpty()) { UI.weighted(header, UI.button(this, "‹", () -> model.page(-1)), 1, 28); UI.weighted(header, UI.text(this, "" + (model.page + 1), 12), 1, 28); UI.weighted(header, UI.button(this, "›", () -> model.page(1)), 1, 28); }
        UI.full(candidatePanel, header, -2);
        LinearLayout choices = UI.row(this); HorizontalScrollView scroll = new HorizontalScrollView(this); scroll.setHorizontalScrollBarEnabled(false); scroll.addView(choices);
        if (model.pending != null) {
            try {
                JSONArray options = model.options();
                for (int i = 0; i < options.length(); i++) {
                    int index = i; JSONObject option = options.getJSONObject(i);
                    Button choice = UI.button(this, option.getString("pinyin") + "\n" + option.getString("ipa"), () -> model.chooseReading(index));
                    choice.setTypeface(UI.ipa(this)); choice.setTextSize(android.util.TypedValue.COMPLEX_UNIT_DIP, 22); choice.setContentDescription("reading-" + option.getString("pinyin")); addChoice(choices, choice, 76);
                }
                addChoice(choices, UI.button(this, "返回", () -> { model.pending = null; candidates(); }), 76);
            } catch (JSONException e) { model.status = e.getMessage(); }
        } else {
            List<Session.Candidate> visible = model.visible();
            for (int i = 0; i < visible.size(); i++) {
                int index = i; Session.Candidate item = visible.get(i); LinearLayout card = UI.column(this); card.setBackground(UI.background(0xfff8fbfc, UI.dp(this, 8)));
                Button word = item.symbol() ? UI.ipaButton(this, (i + 1) + "  " + UI.displaySymbol(item.word), () -> model.choose(index, false)) : UI.button(this, (i + 1) + "  " + item.word, () -> model.choose(index, false));
                word.setTextSize(android.util.TypedValue.COMPLEX_UNIT_DIP, item.symbol() ? 21 : 17); if (item.symbol()) word.setTypeface(UI.ipa(this)); word.setContentDescription("hanzi-" + item.word + "-" + item.canonical); UI.full(card, word, item.symbol() ? keyHeight(34, 28) : keyHeight(30, 26));
                if (!item.symbol()) {
                    String value = item.ipa().isEmpty() ? (item.reading.optBoolean("ambiguous") ? "选择读音…" : "暂无音标") : item.ipa();
                    Button ipa = UI.ipaButton(this, value, () -> model.choose(index, true)); ipa.setTextSize(android.util.TypedValue.COMPLEX_UNIT_DIP, 23); ipa.setTypeface(UI.ipa(this)); ipa.setTextColor(UI.ACCENT); ipa.setContentDescription("ipa-" + item.word + "-" + item.canonical);
                    ipa.setOnLongClickListener(v -> { model.reopen(index); return true; }); UI.full(card, ipa, keyHeight(36, 26));
                }
                addChoice(choices, card, item.symbol() ? 42 : 64);
            }
        }
        if (!model.raw.isEmpty()) { boolean twoRows = model.pending != null || model.visible().stream().anyMatch(item -> !item.symbol()); UI.full(candidatePanel, scroll, twoRows ? (model.pending != null ? 84 : keyHeight(76, 60)) : keyHeight(48, 36)); }
        if (!model.status.isEmpty()) { TextView status = UI.text(this, model.status, 11); status.setTextColor(UI.MUTED); UI.full(candidatePanel, status, -2); }
        InputConnection input = getCurrentInputConnection();
        if (input != null) { if (model.raw.isEmpty()) input.finishComposingText(); else input.setComposingText(model.raw, 1); }
    }
    private void addChoice(LinearLayout row, View child, int height) {
        LinearLayout.LayoutParams p = new LinearLayout.LayoutParams(-2, -2); p.setMargins(UI.dp(this, 3), 0, UI.dp(this, 3), 0); child.setMinimumHeight(UI.dp(this, height)); child.setMinimumWidth(UI.dp(this, 70)); row.addView(child, p);
    }
    private int keyHeight(int portrait, int landscape) { return getResources().getConfiguration().orientation == android.content.res.Configuration.ORIENTATION_LANDSCAPE ? landscape : portrait; }
    private void keys() {
        if (deleteKey != null) deleteKey.cancelRepeat();
        keyPanel.removeAllViews();
        LinearLayout digits = UI.row(this);
        for (char c : "1234567890".toCharArray()) {
            String digit = "" + c;
            Button key = c >= '1' && c <= '5' ? UI.styleButton(new ToneKey(this, () -> model.tone(digit)), digit, () -> typeDigit(digit)) : UI.ipaButton(this, digit, () -> typeDigit(digit));
            key.setTextSize(android.util.TypedValue.COMPLEX_UNIT_DIP, 22); key.setContentDescription("digit-" + digit);
            UI.weighted(digits, key, 1, keyHeight(32, 22));
        }
        UI.full(keyPanel, digits, -2);
        JSONObject mappings;
        try { mappings = model.engine.profile().getJSONObject("sequences"); } catch (Exception e) { mappings = new JSONObject(); }
        for (String row : symbols ? new String[]{"-/:;()$&@", "[]{}#%^*+=", ".,?!'\\_~<>"} : new String[]{"qwertyuiop", "asdfghjkl", "zxcvbnm"}) {
            LinearLayout keys = UI.row(this);
            if (row.equals("asdfghjkl")) keys.setPadding(UI.dp(this, 16), 0, UI.dp(this, 16), 0);
            if (row.equals("zxcvbnm")) UI.weighted(keys, shiftButton(), 1.35f, keyHeight(42, 26));
            for (char c : row.toCharArray()) {
                String key = uppercase ? ("" + c).toUpperCase(Locale.ROOT) : "" + c;
                JSONArray values = mappings.optJSONArray(key); String output = !model.pinyin && values != null ? values.optString(0, key) : key;
                boolean chooser = !model.pinyin && directOutput(key, mappings) == null;
                Button b = UI.ipaButton(this, UI.displaySymbol(output), () -> typeKey(key)); b.setTypeface(UI.ipa(this)); b.setTextSize(android.util.TypedValue.COMPLEX_UNIT_DIP, 25); b.setContentDescription("key-" + c);
                b.setOnLongClickListener(v -> { showSymbols(key); return true; });
                if (android.os.Build.VERSION.SDK_INT >= 30) b.setStateDescription(model.pinyin ? "输入拼音" : chooser ? "多个音标，点按选择" : "单个符号，点按直接输入");
                if (!output.equals(key) || chooser) {
                    FrameLayout cap = new FrameLayout(this); cap.addView(b, new FrameLayout.LayoutParams(-1, -1));
                    if (!output.equals(key) && !key.equalsIgnoreCase("g")) { TextView label = UI.text(this, key, 9); label.setTextColor(UI.MUTED); label.setPadding(UI.dp(this, 3), UI.dp(this, 2), 0, 0); label.setElevation(b.getElevation() + UI.dp(this, 1)); label.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO); cap.addView(label, new FrameLayout.LayoutParams(-2, -2, Gravity.TOP | Gravity.START)); }
                    if (chooser) { TextView badge = UI.text(this, "···", 10); badge.setTextColor(UI.ACCENT); badge.setPadding(0, 0, UI.dp(this, 3), 0); badge.setElevation(b.getElevation() + UI.dp(this, 1)); badge.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO); cap.addView(badge, new FrameLayout.LayoutParams(-2, -2, Gravity.BOTTOM | Gravity.END)); }
                    UI.weighted(keys, cap, 1, keyHeight(42, 26));
                } else UI.weighted(keys, b, 1, keyHeight(42, 26));
            }
            if (row.equals("zxcvbnm")) UI.weighted(keys, backspaceButton(), 1.35f, keyHeight(42, 26));
            UI.full(keyPanel, keys, -2);
        }
        LinearLayout actions = UI.row(this);
        UI.weighted(actions, UI.button(this, "🌐", () -> switchToNextInputMethod(false)), 0.8f, keyHeight(39, 26));
        Button mode = UI.button(this, model.pinyin ? "拼音" : "IPA", () -> { model.toggle(); getSharedPreferences("keyboard", MODE_PRIVATE).edit().putBoolean("pinyin-mode", model.pinyin).apply(); keys(); });
        mode.setContentDescription("keyboard-mode");
        if (android.os.Build.VERSION.SDK_INT >= 30) mode.setStateDescription(model.pinyin ? "当前为拼音查字，点按进入直接音标键盘" : "当前为直接音标键盘，点按进入拼音查字");
        UI.weighted(actions, mode, 1, keyHeight(39, 26));
        UI.weighted(actions, UI.button(this, "说明", this::help), 1, keyHeight(39, 26));
        if (symbols) UI.weighted(actions, shiftButton(), 0.8f, keyHeight(39, 26));
        UI.weighted(actions, UI.button(this, symbols ? "ABC" : "符号", () -> { symbols = !symbols; keys(); }), 1, keyHeight(39, 26));
        UI.weighted(actions, UI.button(this, "空格", () -> { if (model.raw.isEmpty()) model.commit.accept(" "); else model.chooseDefault(0); }), 3, keyHeight(39, 26));
        if (symbols) UI.weighted(actions, backspaceButton(), 1, keyHeight(39, 26));
        Button enter = UI.ipaButton(this, "↵", () -> { if (model.raw.isEmpty()) { if (!sendDefaultEditorAction(true)) model.commit.accept("\n"); } else if (model.candidates.isEmpty()) { model.commit.accept(model.raw); model.clear(); } else model.chooseDefault(0); });
        enter.setTextSize(android.util.TypedValue.COMPLEX_UNIT_DIP, 25); enter.setTextColor(0xffffffff); enter.setBackground(UI.background(UI.ACCENT, UI.dp(this, 7))); UI.weighted(actions, enter, 1.35f, keyHeight(39, 26));
        for (int i = 0; i < actions.getChildCount(); i++) if (actions.getChildAt(i) instanceof Button && actions.getChildAt(i) != enter) ((Button)actions.getChildAt(i)).setTextSize(android.util.TypedValue.COMPLEX_UNIT_DIP, 13);
        UI.full(keyPanel, actions, -2);
    }
    private Button shiftButton() { Button key = UI.ipaButton(this, uppercase ? "⇪" : "⇧", () -> { uppercase = !uppercase; keys(); }); key.setTextSize(android.util.TypedValue.COMPLEX_UNIT_DIP, 25); key.setContentDescription("shift"); return key; }
    private Button backspaceButton() { deleteKey = new RepeatKey(this); Button key = UI.styleButton(deleteKey, "⌫", () -> {
        if (!model.raw.isEmpty()) model.backspace();
        else { InputConnection input = getCurrentInputConnection(); if (input != null) {
            CharSequence selection = input.getSelectedText(0);
            if (selection != null && selection.length() > 0) { input.finishComposingText(); input.commitText("", 1); }
            else input.deleteSurroundingTextInCodePoints(1, 0);
        } }
    }); key.setTypeface(UI.ipa(this)); key.setTextSize(android.util.TypedValue.COMPLEX_UNIT_DIP, 22); key.setContentDescription("backspace"); return key; }
    private void typeKey(String key) {
        if (model.pinyin) model.append(key);
        else if (!model.raw.isEmpty()) { model.status = "先确认当前候选，再输入音标"; candidates(); }
        else {
            try { String direct = directOutput(key, model.engine.profile().getJSONObject("sequences"));
                if (direct != null) { model.commit.accept(direct); model.clear(); } else showSymbols(key);
            } catch (Exception error) { model.status = error.getMessage(); candidates(); }
        }
    }
    private void typeDigit(String digit) {
        if (digit.equals("0") && !model.pinyin && model.raw.isEmpty()) typeKey(digit);
        else model.digit(digit);
    }
    private String directOutput(String key, JSONObject mappings) {
        JSONArray values = mappings.optJSONArray(key);
        if (values != null && values.length() != 1) return null;
        for (Iterator<String> it = mappings.keys(); it.hasNext();) { String code = it.next(); if (!code.equals(key) && code.startsWith(key)) return null; }
        return values == null ? key : values.optString(0, key);
    }
    private void showPlus() {
        if (!model.raw.isEmpty()) { model.status = "先确认当前候选，再打开音标表"; candidates(); return; }
        try {
            dismissSymbols(); symbolPicker = new PlusPanel(this, model.engine.directory, true, output -> { model.commit.accept(output); model.clear(); }, this::dismissSymbols);
            symbolPicker.setPadding(UI.dp(this, 8), UI.dp(this, 6), UI.dp(this, 8), root.getPaddingBottom());
            root.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS);
            inputFrame.addView(symbolPicker, new FrameLayout.LayoutParams(-1, root.getHeight()));
        } catch (Exception error) { model.status = error.getMessage(); candidates(); }
    }
    private void dismissSymbols() {
        if (symbolPicker != null && inputFrame != null) inputFrame.removeView(symbolPicker);
        symbolPicker = null;
        if (root != null) root.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_AUTO);
    }
    private void showSymbols(String key) {
        if (!model.raw.isEmpty()) { model.status = "先确认当前候选，再选择音标"; candidates(); return; }
        try {
            dismissSymbols();
            symbolPicker = SymbolPicker.create(this, key, model.engine.profile().getJSONObject("sequences"), output -> {
                dismissSymbols(); model.commit.accept(output); model.clear();
            }, this::dismissSymbols);
            symbolPicker.setPadding(UI.dp(this, 8), UI.dp(this, 6), UI.dp(this, 8), root.getPaddingBottom());
            root.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS);
            inputFrame.addView(symbolPicker, new FrameLayout.LayoutParams(-1, root.getHeight()));
            symbolPicker.announceForAccessibility(key + "，选择音标");
        } catch (Exception e) { model.status = e.getMessage(); candidates(); }
    }
    @Override public boolean onKeyDown(int code, KeyEvent event) {
        if (code == KeyEvent.KEYCODE_BACK && symbolPicker != null) { dismissSymbols(); return true; }
        return super.onKeyDown(code, event);
    }
    private void help() {
        root.removeAllViews(); LinearLayout header = UI.row(this); UI.weighted(header, UI.text(this, "键盘使用说明", 16), 3, 38);
        UI.weighted(header, UI.button(this, "返回键盘", () -> { root.removeAllViews(); UI.full(root, updateNotice, -2); UI.full(root, candidatePanel, -2); UI.full(root, keyPanel, -2); candidates(); }), 2, 38); UI.full(root, header, -2); UI.full(root, UI.guide(this), 240);
    }
}
