package org.nephelium.phoneticipa;

import org.json.*;
import java.util.*;
import java.util.function.Consumer;

/** Composition state only. All IPA derivation stays in the shared C++ core. */
final class Session {
    static final class Candidate {
        final String word, canonical;
        JSONObject reading;
        Candidate(String word, String canonical, JSONObject reading) { this.word = word; this.canonical = canonical; this.reading = reading; }
        boolean symbol() { return reading == null; }
        String ipa() { return reading == null ? "" : reading.optString("text"); }
    }
    final Engine engine;
    String raw = "", status = "";
    boolean pinyin = true;
    int page = 0, pageSize = 6;
    List<Candidate> candidates = new ArrayList<>();
    Candidate pending;
    Consumer<String> commit = value -> {};
    Runnable changed = () -> {};
    Session(Engine engine) { this.engine = engine; reload(); }
    void reload() { try { pageSize = engine.profile().getInt("pageSize"); status = engine.profileError; } catch (Exception e) { status = e.getMessage(); } clear(); }
    List<Candidate> visible() { int start = Math.min(page * pageSize, candidates.size()); return candidates.subList(start, Math.min(start + pageSize, candidates.size())); }
    void refresh() {
        pending = null; page = 0;
        try { candidates = engine.candidates(raw, pinyin); } catch (Exception e) { candidates = new ArrayList<>(); status = "查询失败：" + e.getMessage(); }
        changed.run();
    }
    void append(String text) { if (raw.length() + text.length() > 128) return; raw += text; status = ""; refresh(); }
    void tone(String digit) {
        if (!Arrays.asList("1", "2", "3", "4", "5").contains(digit)) return;
        if (!raw.isEmpty() && !raw.startsWith("tone:")) { status = "先确认当前候选，再输入声调"; changed.run(); return; }
        if (raw.isEmpty()) raw = "tone:"; append(digit);
    }
    void digit(String digit) {
        if (raw.startsWith("tone:")) { tone(digit); return; }
        if (raw.startsWith("code:")) { append(digit); return; }
        if (digit.equals("0")) { if (pinyin && raw.isEmpty()) commit.accept(digit); else append(digit); }
        else if (!raw.isEmpty()) choose(Integer.parseInt(digit) - 1, false);
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
    void choose(int index, boolean ipa) {
        if (pending != null) { chooseReading(index); return; }
        if (index < 0 || index >= visible().size()) return;
        Candidate item = visible().get(index);
        if (ipa && !item.symbol()) {
            if (item.reading.optBoolean("ambiguous")) { pending = item; changed.run(); return; }
            if (item.ipa().isEmpty()) { status = "当前字表没有完整读音"; changed.run(); return; }
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
            status = "读音已选择，点击音标上屏"; changed.run();
        } catch (JSONException e) { status = e.getMessage(); changed.run(); }
    }
    void reopen(int index) {
        if (index < 0 || index >= visible().size() || visible().get(index).symbol()) return;
        try {
            Candidate item = visible().get(index); JSONObject reading = engine.reading(item.word, item.canonical);
            JSONArray parts = reading.getJSONArray("parts"); boolean hasChoice = false;
            for (int i = 0; i < parts.length(); i++) if (parts.getJSONObject(i).getJSONArray("options").length() > 1) { parts.getJSONObject(i).put("selected", -1); hasChoice = true; }
            if (hasChoice) { item.reading = reading; pending = item; changed.run(); }
        } catch (Exception e) { status = e.getMessage(); changed.run(); }
    }
}
