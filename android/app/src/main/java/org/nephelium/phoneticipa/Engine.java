package org.nephelium.phoneticipa;

import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.util.AtomicFile;
import org.json.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.security.MessageDigest;
import java.util.*;
import java.util.concurrent.*;

/** Shared native phonetic rules with Android's read-only SQLite lookup. */
final class Engine implements AutoCloseable {
    static { System.loadLibrary("phoneticipa"); }
    private static CompletableFuture<Engine> shared;
    static synchronized CompletableFuture<Engine> get(Context context) {
        if (shared == null) {
            Context app = context.getApplicationContext();
            shared = CompletableFuture.supplyAsync(() -> {
                try { return new Engine(app); } catch (Exception e) { throw new CompletionException(e); }
            });
        }
        return shared;
    }
    private static native long nativeOpen(String directory);
    private static native void nativeClose(long handle);
    private static native String nativeSymbols(long handle, String code);
    private static native String nativeProfile(long handle);
    private static native String nativeReading(long handle, String word, String canonical, String[] readings);
    private static native void nativeLoadCIN(long handle, String path);
    private static native String nativeReadCIN(String path);
    private static native String nativeRules(long handle);
    private static native void nativeSetRule(long handle, String id);
    private final android.content.SharedPreferences preferences;
    private String ruleId = "beijing", ruleError = "";
    private long handle;
    private final SQLiteDatabase db;
    final File directory, profileFile;
    private JSONObject profile;
    String profileError = "";

    Engine(Context context) throws Exception {
        preferences = context.getSharedPreferences("input-rules", Context.MODE_PRIVATE);
        directory = prepareResources(context);
        profileFile = new File(context.getFilesDir(), "profile.json");
        handle = nativeOpen(directory.getPath());
        try { String wanted = preferences.getString("rule", "beijing"); nativeSetRule(handle, wanted); ruleId = wanted; }
        catch (RuntimeException e) { ruleError = "转写规则读取失败，请重新选择规则"; }
        try { db = SQLiteDatabase.openDatabase(new File(directory, "lookup.sqlite").getPath(), null, SQLiteDatabase.OPEN_READONLY); }
        catch (RuntimeException e) { nativeClose(handle); handle = 0; throw e; }
        profile = normalize(new JSONObject(nativeProfile(handle)));
        if (profileFile.exists()) {
            try (InputStream input = new FileInputStream(profileFile)) { loadProfile(new JSONObject(new String(SettingsActivity.readLimited(input, 8 * 1024 * 1024), StandardCharsets.UTF_8)), false); }
            catch (Exception e) { profileError = "方案读取失败：" + e.getMessage(); }
        }
    }
    private static File prepareResources(Context context) throws Exception {
        byte[] manifestBytes;
        try (InputStream in = context.getAssets().open("IPA/apple-resource-manifest.json")) { manifestBytes = SettingsActivity.readLimited(in, 1024 * 1024); }
        JSONObject files = new JSONObject(new String(manifestBytes, StandardCharsets.UTF_8)).getJSONObject("files");
        String version = hex(MessageDigest.getInstance("SHA-256").digest(manifestBytes)).substring(0, 16);
        File target = new File(context.getFilesDir(), "ipa-" + version);
        if (!target.isDirectory() && !target.mkdirs()) throw new IOException("无法创建音标资料目录");
        File ready = new File(target, "ready");
        if (ready.exists()) return target;
        for (Iterator<String> it = files.keys(); it.hasNext();) {
            String name = it.next();
            if (name.contains("/") || name.contains("\\") || name.startsWith(".")) throw new IOException("无效资料名称");
            MessageDigest digest = MessageDigest.getInstance("SHA-256");
            File part = new File(target, name + ".part");
            try (InputStream in = context.getAssets().open("IPA/" + name); OutputStream out = new FileOutputStream(part)) {
                byte[] bytes = new byte[65536]; int n;
                while ((n = in.read(bytes)) != -1) { digest.update(bytes, 0, n); out.write(bytes, 0, n); }
            }
            if (!hex(digest.digest()).equals(files.getString(name))) throw new IOException("音标资料校验失败：" + name);
            if (!part.renameTo(new File(target, name))) throw new IOException("无法保存音标资料：" + name);
        }
        Files.write(ready.toPath(), manifestBytes);
        return target;
    }
    private static String hex(byte[] bytes) {
        StringBuilder result = new StringBuilder(); for (byte b : bytes) result.append(String.format(Locale.ROOT, "%02x", b & 255)); return result.toString();
    }
    synchronized JSONArray rules() throws JSONException { return new JSONArray(nativeRules(handle)); }
    synchronized String ruleId() { return ruleId; }
    synchronized void selectRule(String id) throws Exception {
        nativeSetRule(handle, id);
        if (!preferences.edit().putString("rule", id).commit()) { nativeSetRule(handle, ruleId); throw new IOException("无法保存转写规则"); }
        ruleId = id; ruleError = "";
    }
    synchronized JSONObject profile() throws JSONException { return new JSONObject(profile.toString()); }
    synchronized JSONArray symbols(String code) throws JSONException { return new JSONArray(nativeSymbols(handle, code)); }
    synchronized JSONObject reading(String word, String canonical) throws JSONException {
        List<String> variants = new ArrayList<>();
        try (Cursor rows = db.rawQuery("SELECT reading FROM readings WHERE word=?", new String[]{word})) {
            while (rows.moveToNext()) variants.add(rows.getString(0));
        }
        return new JSONObject(nativeReading(handle, word, canonical, variants.toArray(new String[0])));
    }
    synchronized List<Session.Candidate> candidates(String code, boolean pinyin) throws JSONException {
        if (pinyin && !ruleError.isEmpty()) throw new JSONException(ruleError);
        List<Session.Candidate> result = new ArrayList<>();
        if (!pinyin || code.startsWith("tone:") || code.startsWith("code:")) {
            JSONArray symbols = symbols(code);
            for (int i = 0; i < symbols.length(); i++) result.add(new Session.Candidate(symbols.getString(i), "", null));
        } else if (!code.isEmpty()) {
            String normalized = code.toLowerCase(Locale.ROOT).replace("'", "").replace("ü", "v").replace("u:", "v");
            try (Cursor rows = db.rawQuery("SELECT word,canonical FROM candidates WHERE code=? ORDER BY weight DESC,word,canonical LIMIT 180", new String[]{normalized})) {
                while (rows.moveToNext()) result.add(new Session.Candidate(rows.getString(0), rows.getString(1), reading(rows.getString(0), rows.getString(1))));
            }
        }
        return result;
    }
    static JSONObject normalize(JSONObject source) throws JSONException {
        JSONObject result = new JSONObject(source.toString());
        if (!result.getString("format").equals("phonetic-ipa-keymap") || result.getInt("version") != 1) throw new JSONException("不支持的方案版本");
        if (!Arrays.asList("104", "87", "75", "65", "60").contains(result.optString("layout", "104"))) throw new JSONException("键盘规模无效");
        int pageSize = result.optInt("pageSize", 6);
        if (pageSize < 3 || pageSize > 9) throw new JSONException("每页候选应为 3–9");
        result.put("pageSize", pageSize);
        JSONObject entries = result.getJSONObject("sequences");
        if (entries.length() == 0 || entries.length() > 10000) throw new JSONException("码表为空或过大");
        for (Iterator<String> it = entries.keys(); it.hasNext();) {
            String key = it.next();
            if (key.isEmpty() || key.getBytes(StandardCharsets.UTF_8).length > 128 || key.codePoints().anyMatch(c -> c <= 32 || c == 127)) throw new JSONException("编码不能包含空白或控制字符");
            JSONArray outputs = entries.getJSONArray(key);
            if (outputs.length() == 0 || outputs.length() > 200) throw new JSONException("候选数量无效");
            for (int i = 0; i < outputs.length(); i++) validateOutput(outputs.getString(i));
        }
        for (int i = 1; i <= 9; i++) {
            JSONArray values = entries.optJSONArray("" + i);
            if (values != null) {
                JSONArray moved = entries.optJSONArray("\\"); if (moved == null) moved = new JSONArray();
                for (int j = 0; j < values.length(); j++) moved.put(values.getString(j));
                entries.put("\\", moved); entries.remove("" + i);
            }
        }
        if (entries.optJSONArray("\\") != null && entries.getJSONArray("\\").length() > 200) throw new JSONException("整理数字键后候选超过 200 项，请先拆分编码");
        JSONArray hotkeys = result.optJSONArray("hotkeys"); if (hotkeys == null) hotkeys = new JSONArray();
        if (hotkeys.length() > 1000) throw new JSONException("组合键过多");
        Set<String> seen = new HashSet<>();
        for (int i = 0; i < hotkeys.length(); i++) {
            JSONObject chord = hotkeys.getJSONObject(i); int mods = chord.getInt("modifiers"), key = chord.getInt("virtualKey");
            if (mods < 1 || mods > 7 || key < 1 || key > 255 || !seen.add(mods + ":" + key)) throw new JSONException("组合键无效或重复");
            validateOutput(chord.getString("output"));
        }
        result.put("hotkeys", hotkeys);
        return result;
    }
    private static void validateOutput(String output) throws JSONException {
        if (output.isEmpty() || output.getBytes(StandardCharsets.UTF_8).length > 1024 || output.indexOf('\n') >= 0 || output.indexOf('\r') >= 0 || output.indexOf('\t') >= 0 || output.indexOf('\0') >= 0) throw new JSONException("候选不能为空或包含换行");
    }
    static String cin(JSONObject profile) throws JSONException {
        StringBuilder out = new StringBuilder("%gen_inp\n%ename PhoneticIPA\n%selkey 123456789\n%ipa_layout " + profile.optString("layout", "104") + "\n");
        JSONArray hotkeys = profile.optJSONArray("hotkeys");
        if (hotkeys != null) for (int i = 0; i < hotkeys.length(); i++) { JSONObject c = hotkeys.getJSONObject(i); out.append("%ipa_hotkey ").append(c.getInt("modifiers")).append(' ').append(c.getInt("virtualKey")).append('\t').append(c.getString("output")).append('\n'); }
        out.append("%chardef begin\n");
        JSONObject entries = profile.getJSONObject("sequences"); List<String> keys = new ArrayList<>(); entries.keys().forEachRemaining(keys::add); Collections.sort(keys);
        for (String key : keys) { JSONArray values = entries.getJSONArray(key); for (int i = 0; i < values.length(); i++) out.append(key).append('\t').append(values.getString(i)).append('\n'); }
        return out.append("%chardef end\n").toString();
    }
    synchronized void loadProfile(JSONObject next, boolean persist) throws Exception {
        JSONObject validated = normalize(next);
        byte[] serialized = validated.toString(2).getBytes(StandardCharsets.UTF_8);
        if (serialized.length > 8 * 1024 * 1024) throw new IOException("方案不能超过 8 MB");
        File temporary = new File(profileFile.getParentFile(), "validated.cin");
        Files.write(temporary.toPath(), cin(validated).getBytes(StandardCharsets.UTF_8));
        nativeReadCIN(temporary.getPath()); // Validate before publishing or changing the active engine.
        if (persist) {
            AtomicFile file = new AtomicFile(profileFile); FileOutputStream out = null;
            try { out = file.startWrite(); out.write(serialized); file.finishWrite(out); }
            catch (IOException e) { file.failWrite(out); throw e; }
        }
        nativeLoadCIN(handle, temporary.getPath()); profile = validated; profileError = "";
    }
    synchronized void importBytes(byte[] bytes, boolean json) throws Exception {
        if (bytes.length > 8 * 1024 * 1024) throw new IOException("方案不能超过 8 MB");
        JSONObject next;
        if (json) next = new JSONObject(new String(bytes, StandardCharsets.UTF_8));
        else {
            File input = new File(profileFile.getParentFile(), "import.cin"); Files.write(input.toPath(), bytes);
            next = new JSONObject(nativeReadCIN(input.getPath())); next.put("pageSize", profile.getInt("pageSize"));
        }
        loadProfile(next, true);
    }
    @Override public synchronized void close() { if (handle != 0) { nativeClose(handle); handle = 0; db.close(); } }
}
