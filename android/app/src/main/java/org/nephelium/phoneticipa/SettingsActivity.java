package org.nephelium.phoneticipa;

import android.app.*;
import android.content.*;
import android.os.Bundle;
import android.provider.Settings;
import android.text.*;
import android.view.*;
import android.view.inputmethod.InputMethodManager;
import android.widget.*;
import org.json.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.*;

public final class SettingsActivity extends Activity {
    private Engine engine;
    private LinearLayout root, body, editor;
    private EditText code, trial;
    private LinearLayout trialControls;
    private Spinner pages;
    private final List<String> outputs = new ArrayList<>();
    private int selected;
    private boolean appendSymbol;
    private byte[] exporting;
    private TextView status, updateStatus;
    private LinearLayout updateLinks;
    private Dialog onboarding;
    private void showOnboarding(boolean force) {
        if (onboarding == null || !onboarding.isShowing()) onboarding = Onboarding.show(this, force);
    }
    @Override protected void onDestroy() { if (onboarding != null) onboarding.dismiss(); super.onDestroy(); }
    @Override public void onCreate(Bundle saved) {
        super.onCreate(saved);
        root = UI.column(this); root.setBackgroundColor(0xfff5f8fa);
        root.setOnApplyWindowInsetsListener((v, insets) -> {
            if (android.os.Build.VERSION.SDK_INT >= 30) { if (trialControls != null && getResources().getConfiguration().orientation == android.content.res.Configuration.ORIENTATION_LANDSCAPE) trialControls.setVisibility(insets.isVisible(WindowInsets.Type.ime()) ? View.GONE : View.VISIBLE); android.graphics.Insets system = insets.getInsets(WindowInsets.Type.systemBars()); v.setPadding(system.left, system.top, system.right, system.bottom); }
            else v.setPadding(insets.getSystemWindowInsetLeft(), insets.getSystemWindowInsetTop(), insets.getSystemWindowInsetRight(), insets.getSystemWindowInsetBottom());
            return insets;
        });
        setContentView(root); root.addView(UI.text(this, "正在准备音标资料…", 17));
        Engine.get(this).whenComplete((value, error) -> runOnUiThread(() -> {
            if (isDestroyed()) return;
            root.removeAllViews();
            if (error != null) { root.addView(UI.text(this, "资料载入失败：" + error.getMessage(), 15)); return; }
            engine = value; LinearLayout nav = UI.row(this);
            UI.weighted(nav, UI.button(this, "键位设置", this::settings), 1, 44); UI.weighted(nav, UI.button(this, "试打", this::typing), 1, 44); UI.weighted(nav, UI.button(this, "说明", () -> { body.removeAllViews(); UI.full(body, UI.button(this, "打开完整图文教程（网页） ↗", () -> download("https://www.phonetictoolbox.com/ipa-ime-guide/?platform=android")), 44); UI.full(body, UI.button(this, "重新查看上手教学", () -> showOnboarding(true)), 44); UI.full(body, UI.guide(this), -1); }), 1, 44); UI.full(root, nav, -2);
            body = UI.column(this); UI.full(root, body, -1); settings(); showOnboarding(false);
        }));
    }
    private void settings() {
        body.removeAllViews(); ScrollView scroll = new ScrollView(this); scroll.setContentDescription("settings-page"); LinearLayout content = UI.column(this); int pad = UI.dp(this, 16); content.setPadding(pad, pad, pad, pad); scroll.addView(content); UI.full(body, scroll, -1);
        TextView title = UI.text(this, "让每个音，都有自己的键位。", 23); title.setPadding(0, 0, 0, pad); UI.full(content, title, -2);
        UI.full(content, UI.text(this, "点击按键编辑，长按查看组合输入。数字 1–9 用于选词，上划 1–5 输入调值。", 13), -2);
        LinearLayout map = UI.column(this);
        for (String row : new String[]{"1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm\\"}) {
            LinearLayout keys = UI.row(this);
            for (char c : row.toCharArray()) { String key = "" + c; Button button = UI.button(this, key, () -> choose(key)); button.setTypeface(UI.ipa(this)); button.setTextSize(23); button.setContentDescription("edit-key-" + key); button.setOnLongClickListener(v -> { combinations(key); return true; }); UI.weighted(keys, button, 1, 42); }
            UI.full(map, keys, -2);
        }
        UI.full(content, map, -2);
        LinearLayout pageRow = UI.row(this); UI.weighted(pageRow, UI.text(this, "每页候选", 14), 1, 40);
        pages = new Spinner(this); pages.setAdapter(new ArrayAdapter<>(this, android.R.layout.simple_spinner_dropdown_item, new String[]{"3 个", "4 个", "5 个", "6 个", "7 个", "8 个", "9 个"}));
        try { pages.setSelection(engine.profile().getInt("pageSize") - 3); } catch (Exception e) { report(e); }
        UI.weighted(pageRow, pages, 1, 40); UI.full(content, pageRow, -2);
        UI.full(content, UI.text(this, "输入编码", 15), -2); LinearLayout codeRow = UI.row(this);
        code = new EditText(this); code.setSingleLine(); code.setHint("例如 a、voqs"); code.setContentDescription("mapping-code"); UI.weighted(codeRow, code, 3, 44); UI.weighted(codeRow, UI.button(this, "载入", () -> choose(code.getText().toString())), 1, 40); UI.full(content, codeRow, -2);
        UI.full(content, UI.text(this, "按从左到右的顺序编辑候选。点选一个候选后，可打开 IPA Plus 替换或追加符号。", 13), -2);
        editor = UI.column(this); UI.full(content, editor, -2);
        LinearLayout editActions = UI.row(this);
        UI.weighted(editActions, UI.button(this, "＋ 候选", () -> { outputs.add(""); selected = outputs.size() - 1; renderOutputs(); }), 1, 40);
        UI.weighted(editActions, UI.button(this, "IPA Plus", () -> plus("ipa")), 1, 40);
        UI.weighted(editActions, UI.button(this, "移除", () -> { if (!outputs.isEmpty()) { outputs.remove(selected); selected = Math.max(0, Math.min(selected, outputs.size() - 1)); renderOutputs(); } }), 1, 40); UI.full(content, editActions, -2);
        CheckBox append = new CheckBox(this); append.setText("追加到当前候选"); append.setChecked(appendSymbol); append.setOnCheckedChangeListener((button, checked) -> appendSymbol = checked); UI.full(content, append, -2);
        LinearLayout saveRow = UI.row(this); UI.weighted(saveRow, UI.button(this, "保存方案", this::save), 1, 44); UI.weighted(saveRow, UI.button(this, "撤销编辑", () -> choose(code.getText().toString())), 1, 44); UI.full(content, saveRow, -2);
        LinearLayout files = UI.row(this); UI.weighted(files, UI.button(this, "导入", this::importFile), 1, 42); UI.weighted(files, UI.button(this, "导出 JSON", () -> exportFile(true)), 1, 42); UI.weighted(files, UI.button(this, "导出 IPA", () -> exportFile(false)), 1, 42); UI.full(content, files, -2);
        updateCard(content);
        status = UI.text(this, engine.profileError, 13); status.setPadding(0, pad, 0, 0); UI.full(content, status, -2);
        UI.full(content, UI.button(this, "PhoneticToolbox 语音工具箱官网 ↗", () -> download("https://www.phonetictoolbox.com/")), 42);
        UI.full(content, UI.button(this, "来源与许可", this::licenses), 40); choose("a");
    }
    @Override protected void onResume() {
        super.onResume();
        Updates.check(this, false, message -> { if (!isDestroyed()) { refreshUpdates(message); notice(); } });
    }
    private void updateCard(LinearLayout content) {
        LinearLayout card = UI.column(this); int pad = UI.dp(this, 12); card.setPadding(pad, pad, pad, pad);
        card.setBackground(UI.background(0xffe9f1f4, UI.dp(this, 12)));
        UI.full(card, UI.text(this, "软件更新 · " + Updates.VERSION, 17), -2);
        CheckBox automatic = new CheckBox(this); automatic.setText("自动检查新版本"); automatic.setChecked(Updates.automatic(this));
        automatic.setOnCheckedChangeListener((button, checked) -> Updates.automatic(this, checked)); UI.full(card, automatic, -2);
        updateStatus = UI.text(this, "", 13); updateStatus.setContentDescription("update-status"); UI.full(card, updateStatus, -2);
        UI.full(card, UI.button(this, "检查更新", () -> Updates.check(this, true, message -> { if (!isDestroyed()) refreshUpdates(message); })), 40);
        updateLinks = UI.row(this); UI.full(card, updateLinks, -2); UI.full(content, card, -2); refreshUpdates(Updates.status(this));
    }
    private void refreshUpdates(String message) {
        if (updateStatus == null) return; updateStatus.setText(message); updateLinks.removeAllViews(); Updates.Feed feed = Updates.available(this);
        if (feed != null) {
            UI.weighted(updateLinks, UI.button(this, "官网下载", () -> download(feed.url)), 1, 42);
            UI.weighted(updateLinks, UI.button(this, "GitHub 下载", () -> download(feed.githubUrl)), 1, 42);
        }
    }
    private void download(String url) {
        try { startActivity(new Intent(Intent.ACTION_VIEW, android.net.Uri.parse(url))); }
        catch (ActivityNotFoundException e) { Toast.makeText(this, "请先安装浏览器，再打开此链接", Toast.LENGTH_LONG).show(); }
    }
    private void notice() {
        Updates.Feed feed = Updates.available(this);
        if (!hasWindowFocus() || isFinishing() || !Updates.takeNotice(this, feed)) return;
        new AlertDialog.Builder(this).setTitle("发现新版本 " + feed.version).setMessage(feed.notes)
            .setPositiveButton("官网下载", (dialog, which) -> download(feed.url))
            .setNeutralButton("GitHub 下载", (dialog, which) -> download(feed.githubUrl)).setNegativeButton("稍后", null).show();
    }
    private void choose(String key) {
        code.setText(key); outputs.clear(); selected = 0;
        try { JSONArray values = engine.profile().getJSONObject("sequences").optJSONArray(key); if (values != null) for (int i = 0; i < values.length(); i++) outputs.add(values.getString(i)); }
        catch (Exception e) { report(e); } renderOutputs();
    }
    private void renderOutputs() {
        editor.removeAllViews(); HorizontalScrollView scroll = new HorizontalScrollView(this); scroll.setContentDescription("output-list"); LinearLayout row = UI.row(this); scroll.addView(row);
        for (int i = 0; i < outputs.size(); i++) {
            int index = i; LinearLayout card = UI.column(this); card.setPadding(UI.dp(this, 6), UI.dp(this, 3), UI.dp(this, 6), 0); card.setBackground(UI.background(selected == i ? 0xffdcebef : 0xffedf2f4, UI.dp(this, 8)));
            card.addView(UI.text(this, "候选 " + (i + 1), 11));
            TextView preview = UI.text(this, UI.displaySymbol(outputs.get(i)), 32); preview.setTypeface(UI.ipa(this)); preview.setGravity(Gravity.CENTER); preview.setMinHeight(UI.dp(this, UI.fontHeight(this, 32, 64))); preview.setPadding(UI.dp(this, 6), UI.dp(this, 4), UI.dp(this, 6), UI.dp(this, 4)); preview.setContentDescription("output-preview-" + i); UI.full(card, preview, -2);
            TextView points = UI.text(this, UI.codePoints(outputs.get(i)), 10); points.setTextColor(UI.MUTED); points.setContentDescription("output-codepoints-" + i); UI.full(card, points, -2);
            EditText output = new EditText(this); output.setSingleLine(); output.setText(outputs.get(i)); output.setTextSize(24); output.setTypeface(UI.ipa(this)); output.setContentDescription("output-" + i); output.setSelectAllOnFocus(true);
            output.setOnFocusChangeListener((v, focused) -> { if (focused) selected = index; });
            output.addTextChangedListener(new TextWatcher() { public void beforeTextChanged(CharSequence s, int start, int count, int after) {} public void onTextChanged(CharSequence s, int start, int before, int count) { if (index < outputs.size()) outputs.set(index, s.toString()); preview.setText(UI.displaySymbol(s.toString())); points.setText(UI.codePoints(s.toString())); } public void afterTextChanged(Editable e) {} });
            card.addView(output, new LinearLayout.LayoutParams(UI.dp(this, 116), -2)); LinearLayout.LayoutParams p = new LinearLayout.LayoutParams(-2, -2); p.setMargins(UI.dp(this, 3), UI.dp(this, 5), UI.dp(this, 3), UI.dp(this, 5)); row.addView(card, p);
        }
        UI.full(editor, scroll, -2); if (outputs.isEmpty()) UI.full(editor, UI.text(this, "此编码尚无候选，点击「＋ 候选」或 IPA Plus 添加。", 13), 44);
    }
    private void save() {
        try {
            JSONObject profile = engine.profile(); JSONObject entries = profile.getJSONObject("sequences"); JSONArray values = new JSONArray(); for (String text : outputs) if (!text.isEmpty()) values.put(text);
            String key = code.getText().toString(); if (values.length() == 0) entries.remove(key); else entries.put(key, values);
            profile.put("pageSize", pages.getSelectedItemPosition() + 3); engine.loadProfile(profile, true); choose(key); status.setText("方案已保存");
        } catch (Exception e) { report(e); }
    }
    private void combinations(String prefix) {
        try {
            JSONObject entries = engine.profile().getJSONObject("sequences"); List<String> keys = new ArrayList<>(); entries.keys().forEachRemaining(key -> { if (key.startsWith(prefix)) keys.add(key); }); Collections.sort(keys);
            String[] labels = new String[keys.size()]; for (int i = 0; i < keys.size(); i++) { String key = keys.get(i); labels[i] = key + (key.equals(prefix) ? "　" : " · 组合输入　") + entries.getJSONArray(key).join("  ").replace("\"", ""); }
            new AlertDialog.Builder(this).setTitle(prefix + " · 按键与组合输入").setItems(labels, (dialog, which) -> choose(keys.get(which))).setNegativeButton("关闭", null).show();
        } catch (Exception e) { report(e); }
    }
    private void typing() {
        body.removeAllViews(); LinearLayout panel = UI.column(this); panel.setPadding(UI.dp(this, 16), UI.dp(this, 16), UI.dp(this, 16), 0); UI.full(body, panel, -1);
        trialControls = UI.column(this); UI.full(panel, trialControls, -2);
        UI.full(trialControls, UI.text(this, "点选汉字或下方音标分别输入。", 14), -2);
        LinearLayout actions = UI.row(this); UI.weighted(actions, UI.button(this, "启用键盘", () -> startActivity(new Intent(Settings.ACTION_INPUT_METHOD_SETTINGS))), 1, 42); UI.weighted(actions, UI.button(this, "切换键盘", () -> ((InputMethodManager)getSystemService(INPUT_METHOD_SERVICE)).showInputMethodPicker()), 1, 42); UI.full(trialControls, actions, -2);
        trial = new EditText(this); trial.setGravity(Gravity.TOP); trial.setTextSize(getResources().getConfiguration().orientation == android.content.res.Configuration.ORIENTATION_LANDSCAPE ? 20 : 27); trial.setTypeface(UI.ipa(this)); trial.setHint("在这里输入"); trial.setContentDescription("typing-area"); trial.setInputType(android.text.InputType.TYPE_CLASS_TEXT | android.text.InputType.TYPE_TEXT_FLAG_MULTI_LINE | android.text.InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS); panel.addView(trial, new LinearLayout.LayoutParams(-1, 0, 1));
    }
    private void importFile() { Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE).setType("*/*"); startActivityForResult(intent, 10); }
    private void exportFile(boolean json) {
        try { exporting = (json ? engine.profile().toString(2) : Engine.cin(engine.profile())).getBytes(StandardCharsets.UTF_8); startActivityForResult(new Intent(Intent.ACTION_CREATE_DOCUMENT).addCategory(Intent.CATEGORY_OPENABLE).setType(json ? "application/json" : "text/plain").putExtra(Intent.EXTRA_TITLE, "音标键位方案." + (json ? "json" : "cin")), 11); }
        catch (Exception e) { report(e); }
    }
    @Override protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data); if (result != RESULT_OK || data == null || data.getData() == null) return;
        try {
            if (request == 10) {
                byte[] bytes; try (InputStream in = getContentResolver().openInputStream(data.getData())) { bytes = readLimited(in, 8 * 1024 * 1024); }
                String text = new String(bytes, StandardCharsets.UTF_8); engine.importBytes(bytes, text.trim().startsWith("{")); settings(); status.setText("方案已导入");
            } else if (request == 11 && exporting != null) { try (OutputStream out = getContentResolver().openOutputStream(data.getData(), "wt")) { out.write(exporting); } status.setText("方案已导出"); }
        } catch (Exception e) { report(e); }
    }
    static byte[] readLimited(InputStream input, int limit) throws IOException {
        if (input == null) throw new IOException("无法打开文件"); ByteArrayOutputStream output = new ByteArrayOutputStream(); byte[] buffer = new byte[8192]; int n;
        while ((n = input.read(buffer)) != -1) { if (output.size() + n > limit) throw new IOException("文件过大"); output.write(buffer, 0, n); } return output.toByteArray();
    }
    private void report(Exception error) { if (status != null) status.setText(error.getMessage()); else Toast.makeText(this, error.getMessage(), Toast.LENGTH_LONG).show(); }
    private void licenses() {
        try { TextView text = UI.text(this, new String(Files.readAllBytes(new File(engine.directory, "THIRD_PARTY.md").toPath()), StandardCharsets.UTF_8), 13); ScrollView scroll = new ScrollView(this); scroll.addView(text); new AlertDialog.Builder(this).setTitle("来源与许可").setView(scroll).setPositiveButton("关闭", null).show(); }
        catch (Exception e) { report(e); }
    }
    private void plus(String initial) {
        try {
            android.app.Dialog dialog = new android.app.Dialog(this);
            PlusPanel panel = new PlusPanel(this, engine.directory, false, value -> {
                if (outputs.isEmpty()) { outputs.add(""); selected = 0; }
                selected = Math.min(selected, outputs.size() - 1); outputs.set(selected, appendSymbol ? outputs.get(selected) + value : value); renderOutputs();
            }, dialog::dismiss);
            dialog.setContentView(panel); dialog.show();
            dialog.getWindow().setBackgroundDrawable(UI.background(0xffffffff, UI.dp(this, 14)));
            dialog.getWindow().setLayout(getResources().getDisplayMetrics().widthPixels - UI.dp(this, 16), Math.round(getResources().getDisplayMetrics().heightPixels * 0.88f));
        } catch (Exception e) { report(e); }
    }
}
