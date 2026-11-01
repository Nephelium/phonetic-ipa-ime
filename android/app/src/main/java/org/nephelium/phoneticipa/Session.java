package org.nephelium.phoneticipa;

import org.json.*;
import java.util.*;
import java.util.function.Consumer;

/** Composition state only. All IPA derivation stays in the shared C++ core. */
final class Session {
    static final class Candidate {
        final String word, canonical;
        private final boolean symbol;
        JSONObject reading;
        boolean resolved;
        Candidate(String word, String canonical, boolean symbol) { this.word = word; this.canonical = canonical; this.symbol = symbol; this.resolved = symbol; }
        boolean symbol() { return symbol; }
        boolean ambiguous() { return reading != null && reading.optBoolean("ambiguous"); }
        String ipa() { return reading == null ? "" : reading.optString("text"); }
    }
    final Engine engine;
    String raw = "", status = "";
    boolean pinyin = true, outputIpa = true;
    private boolean commitAfterReading;
    int page = 0, pageSize = 6;
    List<Candidate> candidates = new ArrayList<>();
    Candidate pending;
    Consumer<String> commit = value -> {};
    Runnable changed = () -> {};
    Session(Engine engine) { this.engine = engine; reload(); }
    void reload() { clear(); try { pageSize = engine.profile().getInt("pageSize"); status = engine.profileError; } catch (Exception e) { status = e.getMessage(); } changed.run(); }
    /** The visible page; only these candidates query readings (instead of all 180 on every key). */
    List<Candidate> visible() {
        int start = Math.min(page * pageSize, candidates.size());
        List<Candidate> shown = candidates.subList(start, Math.min(start + pageSize, candidates.size()));
        for (Candidate item : shown) if (!item.resolved) {
            item.resolved = true;
            try { item.reading = engine.reading(item.word, item.canonical); } catch (Exception e) { item.reading = null; }
        }
        return shown;
    }
    void refresh() {
        pending = null; page = 0;
        try { candidates = engine.candidates(raw, pinyin); } catch (Exception e) { candidates = new ArrayList<>(); status = "查询失败：" + e.getMessage(); }
        changed.run();
    }
    void append(String text) { if (raw.length() + text.length() > 128) return; raw += text; status = ""; refresh(); }
    void tone(String digit) {
        if (!Arrays.asList("1", "2", "3", "4", "5").contains(digit)) return;
        // Direct IPA: confirm the highlighted symbol first, then start the tone.
        if (!pinyin && !raw.isEmpty() && !raw.startsWith("tone:") && !visible().isEmpty()) choose(0, true);
        if (!raw.isEmpty() && !raw.startsWith("tone:")) { status = "先确认当前候选，再输入声调"; changed.run(); return; }
        if (raw.isEmpty()) raw = "tone:"; append(digit);
    }
    void digit(String digit) {
        if (raw.startsWith("tone:")) { tone(digit); return; }
        if (raw.startsWith("code:")) { append(digit); return; }
        if (digit.equals("0")) { if (pinyin && raw.isEmpty()) commit.accept(digit); else append(digit); }
        else if (!raw.isEmpty()) chooseDefault(Integer.parseInt(digit) - 1);
        else commit.accept(digit);
    }
    void clear() { raw = ""; page = 0; candidates = new ArrayList<>(); pending = null; status = ""; changed.run(); }
    void backspace() {
        if (pending != null) { pending = null; changed.run(); return; }
        if (!raw.isEmpty()) { raw = raw.substring(0, raw.offsetByCodePoints(raw.length(), -1)); refresh(); }
    }
    void toggle() { pinyin = !pinyin; clear(); }
    void page(int delta) { page = Math.max(0, Math.min(page + delta, Math.max(0, (candidates.size() - 1) / pageSize))); changed.run(); }
    JSONArray options() throws JSONException {
        if (pending == null) return new JSONArray();
        JSONArray parts = pending.reading.getJSONArray("parts");
        for (int i = 0; i < parts.length(); i++) { JSONObject part = parts.getJSONObject(i); if (part.getInt("selected") < 0) return part.getJSONArray("options"); }
        return new JSONArray();
    }
    void chooseDefault(int index) { choose(index, outputIpa); }
    void choose(int index, boolean ipa) {
        if (pending != null) { chooseReading(index); return; }
        if (index < 0 || index >= visible().size()) return;
        Candidate item = visible().get(index);
        if (ipa && !item.symbol()) {
            if (item.ambiguous()) { pending = item; commitAfterReading = true; changed.run(); return; }
            // No complete IPA: commit the Hanzi instead of ignoring the key, as on the desktop.
            if (item.ipa().isEmpty()) { commit.accept(item.word); clear(); return; }
        }
        commit.accept(ipa && !item.symbol() ? item.ipa() : item.word); clear();
    }
    void chooseReading(int index) {
        try {
            if (index < 0 || index >= options().length()) return;
            JSONArray parts = pending.reading.getJSONArray("parts");
            for (int i = 0; i < parts.length(); i++) if (parts.getJSONObject(i).getInt("selected") < 0) { parts.getJSONObject(i).put("selected", index); break; }
            StringJoiner text = new StringJoiner(" ");
            for (int i = 0; i < parts.length(); i++) {
                JSONObject part = parts.getJSONObject(i); if (part.getInt("selected") < 0) { changed.run(); return; }
                text.add(part.getJSONArray("options").getJSONObject(part.getInt("selected")).getString("ipa"));
            }
            pending.reading.put("text", text.toString()).put("ambiguous", false); pending = null;
            if (commitAfterReading) { commit.accept(text.toString()); clear(); return; }
            status = "读音已选择，点击音标上屏"; changed.run();
        } catch (JSONException e) { status = e.getMessage(); changed.run(); }
    }
    void reopen(int index) {
        if (index < 0 || index >= visible().size() || visible().get(index).symbol()) return;
        try {
            Candidate item = visible().get(index); JSONObject reading = engine.reading(item.word, item.canonical);
            JSONArray parts = reading.getJSONArray("parts"); boolean hasChoice = false;
            for (int i = 0; i < parts.length(); i++) if (parts.getJSONObject(i).getJSONArray("options").length() > 1) { parts.getJSONObject(i).put("selected", -1); hasChoice = true; }
            if (hasChoice) { commitAfterReading = false; item.reading = reading; pending = item; changed.run(); }
        } catch (Exception e) { status = e.getMessage(); changed.run(); }
    }
}
