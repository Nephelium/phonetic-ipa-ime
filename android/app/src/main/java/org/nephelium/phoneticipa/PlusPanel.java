package org.nephelium.phoneticipa;
import android.content.Context;
import android.text.*;
import android.view.*;
import android.widget.*;
import org.json.*;
import java.io.*;
import java.nio.file.Files;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.function.Consumer;

final class PlusPanel extends LinearLayout {
    private final Consumer<String> select;
    private final boolean compact;
    private String query = "";
    PlusPanel(Context context, File directory, boolean compact, Consumer<String> select, Runnable close) throws Exception {
        super(context); this.select = select; this.compact = compact; setOrientation(VERTICAL);
        setPadding(UI.dp(context, 8), UI.dp(context, 6), UI.dp(context, 8), UI.dp(context, 6)); setBackgroundColor(0xffffffff);
        JSONObject catalog = new JSONObject(new String(Files.readAllBytes(new File(directory, "plus-catalog.json").toPath()), StandardCharsets.UTF_8));
        Map<String, JSONObject> entries = new LinkedHashMap<>(); JSONArray list = catalog.getJSONArray("entries");
        for (int i = 0; i < list.length(); i++) { JSONObject e = list.getJSONObject(i); entries.put(e.getString("id"), e); }
        LinearLayout header = UI.row(context); UI.weighted(header, label(compact ? "IPA Plus" : "IPA Plus · 点选音标", 15), 3, 30);
        Button done = control(compact ? "返回键盘" : "完成", close); done.setContentDescription("plus-close"); UI.weighted(header, done, 1.5f, 30); UI.full(this, header, -2);
        LinearLayout tabs = UI.row(context); UI.full(this, tabs, -2);
        EditText search = new EditText(context); search.setSingleLine(); search.setTextSize(android.util.TypedValue.COMPLEX_UNIT_DIP, 12); search.setPadding(4, 0, 4, 0); search.setHint("搜索音标或中文名称"); search.setContentDescription("plus-search");
        if (compact) { search.setFocusable(false); search.setFocusableInTouchMode(false); search.setVisibility(GONE); }
        UI.full(this, search, 32);
        LinearLayout tools = UI.row(context); if (!compact) UI.full(this, tools, -2);
        TextView hint = label(compact ? "上下滑动分类 · 点选直接输入" : "左右滑动表格 · 上下浏览分类", 10); hint.setTextColor(UI.MUTED); UI.full(this, hint, -2);
        ScrollView scroll = new ScrollView(context); scroll.setContentDescription("plus-page"); LinearLayout charts = UI.column(context); scroll.addView(charts); addView(scroll, new LinearLayout.LayoutParams(-1, 0, 1));
        String[] system = {"ipa"}; float[] scale = {0.5f}; Runnable redraw = () -> drawCharts(charts, catalog, entries, system[0], scale[0]);
        for (String name : new String[]{"ipa", "extipa", "voqs"}) UI.weighted(tabs, control(name.equals("ipa") ? "IPA" : name.equals("extipa") ? "extIPA" : "VoQS", () -> { system[0] = name; scroll.scrollTo(0, 0); redraw.run(); }), 1, 30);
        TextView size = label("50%", 11); size.setGravity(Gravity.CENTER);
        UI.weighted(tools, control("字号 −", () -> { scale[0] = Math.max(0.4f, scale[0] - 0.1f); size.setText(Math.round(scale[0] * 100) + "%"); redraw.run(); }), 1, 28);
        UI.weighted(tools, size, 1, 28);
        UI.weighted(tools, control("字号 ＋", () -> { scale[0] = Math.min(1.3f, scale[0] + 0.1f); size.setText(Math.round(scale[0] * 100) + "%"); redraw.run(); }), 1, 28);
        search.addTextChangedListener(new TextWatcher() { public void beforeTextChanged(CharSequence s, int start, int count, int after) {} public void onTextChanged(CharSequence s, int start, int before, int count) { query = s.toString(); redraw.run(); } public void afterTextChanged(Editable e) {} });
        redraw.run();
    }
    private void drawCharts(LinearLayout target, JSONObject catalog, Map<String, JSONObject> entries, String system, float scale) {
        target.removeAllViews(); Set<String> shown = new HashSet<>();
        int font = Math.round(22 * scale), symbolHeight = Math.max(24, Math.round(44 * scale));
        try {
            JSONArray charts = catalog.getJSONObject("charts").getJSONArray(system);
            for (int i = 0; i < charts.length(); i++) {
                JSONObject chart = charts.getJSONObject(i); if (compact || !query.isEmpty() || !chart.optString("kind").equals("matrix")) continue;
                TextView title = label(chart.getString("title"), 12); title.setPadding(0, UI.dp(getContext(), 6), 0, UI.dp(getContext(), 3)); UI.full(target, title, -2);
                // TableRow MATCH_PARENT heights and an unspecified GridLayout row spec
                // previously let the table shrink to label height, clipping IPA glyphs.
                int unit = UI.dp(getContext(), Math.max(18, 36 * scale));
                android.graphics.Paint measure = new android.graphics.Paint(); measure.setTypeface(UI.ipa(getContext())); measure.setTextSize(UI.dp(getContext(), font));
                JSONArray rows = chart.getJSONArray("rows");
                for (int r = 0; r < rows.length(); r++) {
                    JSONArray cells = rows.getJSONObject(r).getJSONArray("cells");
                    for (int c = 0; c < cells.length(); c++) { JSONArray ids = cells.getJSONObject(c).getJSONArray("ids");
                        for (int n = 0; n < ids.length(); n++) { JSONObject e = entries.get(ids.getString(n)); if (e != null) unit = Math.max(unit, (int)Math.ceil(measure.measureText(e.getString("display"))) + UI.dp(getContext(), 8)); }
                    }
                }
                int cellWidth = unit * 2 + UI.dp(getContext(), 4);
                TableLayout table = new TableLayout(getContext()); table.setShrinkAllColumns(false); table.setStretchAllColumns(false);
                TableRow head = new TableRow(getContext()); head.addView(cellTitle("方式 / 部位", 56)); JSONArray columns = chart.getJSONArray("columns");
                for (int c = 0; c < columns.length(); c++) { TextView label = cellTitle(columns.getString(c), 1); head.addView(label, new TableRow.LayoutParams(cellWidth, -2)); }
                table.addView(head);
                for (int r = 0; r < rows.length(); r++) {
                    JSONObject row = rows.getJSONObject(r); JSONArray cells = row.getJSONArray("cells"); int lines = 1;
                    for (int c = 0; c < cells.length(); c++) lines = Math.max(lines, (cells.getJSONObject(c).getJSONArray("ids").length() + 1) / 2);
                    int rowHeight = UI.dp(getContext(), lines * symbolHeight + 4);
                    TableRow tableRow = new TableRow(getContext()); tableRow.addView(cellTitle(row.getString("label"), 56), new TableRow.LayoutParams(UI.dp(getContext(), 56), rowHeight));
                    for (int c = 0; c < cells.length(); c++) {
                        GridLayout cell = new GridLayout(getContext()); cell.setColumnCount(2); cell.setPadding(UI.dp(getContext(), 2), UI.dp(getContext(), 2), UI.dp(getContext(), 2), UI.dp(getContext(), 2)); cell.setBackgroundColor(0xffeff4f5);
                        JSONArray ids = cells.getJSONObject(c).getJSONArray("ids");
                        for (int n = 0; n < ids.length(); n++) {
                            String id = ids.getString(n); JSONObject entry = entries.get(id); if (entry == null) continue; shown.add(id);
                            Button button = symbol(entry); button.setTextSize(android.util.TypedValue.COMPLEX_UNIT_DIP, font);
                            GridLayout.LayoutParams bounds = new GridLayout.LayoutParams(GridLayout.spec(n / 2), GridLayout.spec(n % 2)); bounds.width = unit; bounds.height = UI.dp(getContext(), symbolHeight); cell.addView(button, bounds);
                        }
                        TableRow.LayoutParams bounds = new TableRow.LayoutParams(cellWidth, rowHeight); bounds.setMargins(0, 0, UI.dp(getContext(), 1), UI.dp(getContext(), 1)); tableRow.addView(cell, bounds);
                    }
                    table.addView(tableRow);
                }
                HorizontalScrollView horizontal = new HorizontalScrollView(getContext()); horizontal.setContentDescription("plus-matrix-" + chart.getString("id")); horizontal.setHorizontalScrollBarEnabled(true); horizontal.setScrollbarFadingEnabled(false); horizontal.addView(table, new HorizontalScrollView.LayoutParams(-2, -2)); UI.full(target, horizontal, -2);
            }
            Map<String, List<JSONObject>> groups = new LinkedHashMap<>();
            for (JSONObject entry : entries.values()) if (entry.getString("system").equals(system) && !shown.contains(entry.getString("id")) && (query.isEmpty() || entry.getString("display").contains(query) || entry.optString("nameZh").contains(query))) groups.computeIfAbsent(entry.getString("section"), k -> new ArrayList<>()).add(entry);
            Map<String, String> titles = new HashMap<>();
            for (int i = 0; i < charts.length(); i++) titles.put(charts.getJSONObject(i).getString("id"), charts.getJSONObject(i).getString("title"));
            String[] names = {"vowels", "元音", "diacritics", "附加符号", "suprasegmentals", "超音段", "other", "其他符号", "tones", "声调", "nonpulmonic", "非肺部气流辅音", "combinations", "组合符号"};
            for (int i = 0; i < names.length; i += 2) titles.put(names[i], names[i + 1]);
            int available = getResources().getDisplayMetrics().widthPixels - UI.dp(getContext(), 42);
            int count = Math.max(3, Math.min(10, available / UI.dp(getContext(), Math.max(30, Math.round(46 * scale)))));
            for (Map.Entry<String, List<JSONObject>> group : groups.entrySet()) {
                TextView heading = label(titles.getOrDefault(group.getKey(), group.getKey()), 12); heading.setPadding(0, UI.dp(getContext(), 6), 0, UI.dp(getContext(), 3)); UI.full(target, heading, -2);
                LinearLayout row = null; int index = 0;
                for (JSONObject entry : group.getValue()) {
                    if (index % count == 0) { row = UI.row(getContext()); UI.full(target, row, -2); }
                    Button button = symbol(entry); button.setTextSize(android.util.TypedValue.COMPLEX_UNIT_DIP, font); UI.weighted(row, button, 1, symbolHeight); index++;
                }
                if (row != null) for (int blank = index % count; blank > 0 && blank < count; blank++) UI.weighted(row, new Space(getContext()), 1, symbolHeight);
            }
        } catch (Exception e) { target.addView(UI.text(getContext(), e.getMessage(), 13)); }
    }
    private TextView label(String title, int font) { TextView text = UI.text(getContext(), title, font); text.setTextSize(android.util.TypedValue.COMPLEX_UNIT_DIP, font); return text; }
    private Button control(String title, Runnable action) { Button button = UI.button(getContext(), title, action); button.setTextSize(android.util.TypedValue.COMPLEX_UNIT_DIP, 12); return button; }
    private TextView cellTitle(String title, int width) { TextView text = label(title, 9); text.setGravity(Gravity.CENTER); text.setPadding(2, 4, 2, 4); if (width > 1) text.setWidth(UI.dp(getContext(), width)); return text; }
    private Button symbol(JSONObject entry) throws JSONException {
        String output = entry.getString("insertText");
        Button button = UI.ipaButton(getContext(), entry.getString("display"), () -> select.accept(output));
        button.setTypeface(UI.ipa(getContext())); button.setTextSize(23); button.setElevation(0); button.setBackgroundColor(0x00000000); button.setTooltipText(entry.optString("nameZh")); button.setContentDescription("plus-" + entry.getString("id")); return button;
    }
}
