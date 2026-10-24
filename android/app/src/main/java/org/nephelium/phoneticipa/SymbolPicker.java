package org.nephelium.phoneticipa;

import android.content.Context;
import android.view.*;
import android.widget.*;
import org.json.*;
import java.util.*;
import java.util.function.Consumer;

/** A touchable panel inside the IME window. Opening it never edits the host. */
final class SymbolPicker {
    static View create(Context context, String key, JSONObject mappings, Consumer<String> select, Runnable close) throws JSONException {
        LinearLayout panel = UI.column(context);
        panel.setPadding(UI.dp(context, 8), UI.dp(context, 6), UI.dp(context, 8), UI.dp(context, 6));
        panel.setBackground(UI.background(UI.SURFACE, UI.dp(context, 12)));
        panel.setClickable(true);
        panel.setContentDescription("symbol-picker");
        LinearLayout header = UI.row(context);
        TextView title = UI.text(context, key + " · 选择音标", 16);
        UI.weighted(header, title, 3, 42);
        Button cancel = UI.button(context, "关闭", close);
        cancel.setContentDescription("symbol-picker-close");
        UI.weighted(header, cancel, 1, 42);
        UI.full(panel, header, -2);
        ScrollView scroll = new ScrollView(context);
        scroll.setFillViewport(false); scroll.setContentDescription("symbol-picker-list");
        LinearLayout content = UI.column(context);
        scroll.addView(content);
        panel.addView(scroll, new LinearLayout.LayoutParams(-1, 0, 1));
        List<String> codes = new ArrayList<>();
        mappings.keys().forEachRemaining(code -> { if (code.startsWith(key)) codes.add(code); });
        codes.sort(Comparator.comparingInt((String code) -> code.equals(key) ? 0 : 1).thenComparing(code -> code));
        if (codes.isEmpty()) { codes.add(key); mappings = new JSONObject().put(key, new JSONArray().put(key)); }
        int width = context.getResources().getDisplayMetrics().widthPixels - UI.dp(context, 28);
        int columns = Math.max(3, Math.min(6, width / UI.dp(context, 64)));
        // Keep a long family of combination codes from expanding the IME.
        // Only the selected code creates buttons (at most 200 validated outputs).
        if (codes.remove("voqs")) codes.add(Math.min(1, codes.size()), "voqs");
        final JSONObject source = mappings;
        Spinner chooser = new Spinner(context); chooser.setContentDescription("symbol-code");
        List<String> labels = new ArrayList<>(); for (String code : codes) labels.add((code.equals(key) ? "单键 · " : "组合输入 · ") + code);
        ArrayAdapter<String> adapter = new ArrayAdapter<>(context, android.R.layout.simple_spinner_item, labels); adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item); chooser.setAdapter(adapter);
        panel.addView(chooser, 1, new LinearLayout.LayoutParams(-1, UI.dp(context, 44)));
        chooser.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            public void onNothingSelected(AdapterView<?> parent) {}
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                content.removeAllViews(); scroll.scrollTo(0, 0); String code = codes.get(position);
                try {
                    JSONArray values = source.getJSONArray(code); LinearLayout row = null;
                    for (int i = 0; i < values.length(); i++) {
                        if (i % columns == 0) { row = UI.row(context); UI.full(content, row, -2); }
                        String output = values.getString(i);
                        Button button = UI.button(context, UI.displaySymbol(output), () -> select.accept(output));
                        button.setTypeface(UI.ipa(context)); button.setTextSize(android.util.TypedValue.COMPLEX_UNIT_DIP, 27); button.setSingleLine(false); button.setMinHeight(UI.dp(context, 54));
                        button.setContentDescription("symbol-choice-" + code + "-" + i);
                        button.setTooltipText(output); UI.weighted(row, button, 1, -2);
                        if (i == values.length() - 1) for (int blank = (i % columns) + 1; blank < columns; blank++) UI.weighted(row, new Space(context), 1, 54);
                    }
                } catch (JSONException error) { UI.full(content, UI.text(context, "无法读取该编码", 14), -2); }
            }
        });
        return panel;
    }
}
