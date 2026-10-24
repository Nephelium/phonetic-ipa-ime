package org.nephelium.phoneticipa;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Handler;
import android.os.Looper;
import org.json.*;
import java.net.*;
import javax.net.ssl.HttpsURLConnection;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.concurrent.*;
import java.util.function.Consumer;

/** Fetches public version metadata only. It never receives the input session or text. */
final class Updates {
    static final String VERSION = "0.1.1";
    static final String[] SOURCES = {
        "https://www.phonetictoolbox.com/releases/phonetic-ipa-ime/update.json",
        "https://raw.githubusercontent.com/Nephelium/phonetic-ipa-ime/ipa-prototype/updates/latest.json"
    };
    private static final ExecutorService workers = Executors.newFixedThreadPool(3);
    private static final Handler main = new Handler(Looper.getMainLooper());
    private static boolean checking;
    static final class Feed {
        final String version, notes, url, githubUrl, json;
        Feed(JSONObject root) throws JSONException {
            version = root.getString("version"); notes = root.getString("notes");
            JSONObject item = root.getJSONObject("platforms").getJSONObject("android");
            url = item.getString("url"); githubUrl = item.getString("githubUrl"); json = root.toString();
        }
    }
    static int[] version(String text) {
        if (text == null || !text.matches("(0|[1-9][0-9]{0,4})\\.(0|[1-9][0-9]{0,4})\\.(0|[1-9][0-9]{0,4})")) return null;
        return Arrays.stream(text.split("\\.")).mapToInt(Integer::parseInt).toArray();
    }
    static int compare(String a, String b) {
        int[] left = version(a), right = version(b);
        if (left == null || right == null) throw new IllegalArgumentException("Invalid version");
        for (int i = 0; i < 3; i++) if (left[i] != right[i]) return Integer.compare(left[i], right[i]);
        return 0;
    }
    static boolean trusted(String text) {
        try {
            URI url = new URI(text); String path = url.getPath();
            if (!"https".equals(url.getScheme()) || url.getUserInfo() != null || url.getPort() != -1 || url.getQuery() != null || url.getFragment() != null || path.contains("..") || path.contains("\\")) return false;
            return ("www.phonetictoolbox.com".equals(url.getHost()) && path.startsWith("/releases/phonetic-ipa-ime/")) ||
                ("github.com".equals(url.getHost()) && path.startsWith("/Nephelium/phonetic-ipa-ime/releases/download/"));
        } catch (Exception e) { return false; }
    }
    static Feed parse(String json) {
        try {
            if (json.getBytes(StandardCharsets.UTF_8).length > 65536) return null;
            JSONObject root = new JSONObject(json); Feed feed = new Feed(root);
            JSONObject item = root.getJSONObject("platforms").getJSONObject("android");
            if (!(root.get("schema") instanceof Integer) || root.getInt("schema") != 1 || !root.getString("product").equals("phonetic-ipa-ime") || version(feed.version) == null || feed.notes.length() > 4000 || !trusted(feed.url) || !trusted(feed.githubUrl) || !item.getString("sha256").matches("[0-9a-f]{64}")) return null;
            Object size = item.get("bytes");
            if (!(size instanceof Integer || size instanceof Long) || item.getLong("bytes") <= 0 || item.getLong("bytes") > 2147483648L) return null;
            return feed;
        } catch (Exception e) { return null; }
    }
    static Feed newest(List<Feed> feeds) {
        Feed best = null; for (Feed feed : feeds) if (feed != null && (best == null || compare(feed.version, best.version) > 0)) best = feed; return best;
    }
    private static SharedPreferences preferences(Context context) { return context.getSharedPreferences("updates", Context.MODE_PRIVATE); }
    static boolean automatic(Context context) { return preferences(context).getBoolean("automatic", true); }
    static void automatic(Context context, boolean value) { preferences(context).edit().putBoolean("automatic", value).apply(); }
    static Feed available(Context context) {
        Feed feed = parse(preferences(context).getString("latest", ""));
        return feed != null && compare(feed.version, VERSION) > 0 ? feed : null;
    }
    static boolean takeNotice(Context context, Feed feed) {
        SharedPreferences p = preferences(context);
        if (feed == null || feed.version.equals(p.getString("notified", ""))) return false;
        p.edit().putString("notified", feed.version).apply(); return true;
    }
    private static Feed fetch(String endpoint) {
        HttpsURLConnection connection = null;
        try {
            connection = (HttpsURLConnection)new URL(endpoint).openConnection();
            connection.setConnectTimeout(8000); connection.setReadTimeout(10000); connection.setInstanceFollowRedirects(false);
            connection.setRequestProperty("Accept", "application/json"); connection.setRequestProperty("User-Agent", "PhoneticIPA/" + VERSION);
            if (connection.getResponseCode() != 200 || connection.getContentLengthLong() > 65536) return null;
            try (java.io.InputStream input = connection.getInputStream()) { return parse(new String(SettingsActivity.readLimited(input, 65536), StandardCharsets.UTF_8)); }
        } catch (Exception e) { return null; } finally { if (connection != null) connection.disconnect(); }
    }
    static synchronized void check(Context context, boolean manual, Consumer<String> callback) {
        SharedPreferences p = preferences(context.getApplicationContext()); long now = System.currentTimeMillis();
        if (checking) { callback.accept("正在检查官网与 GitHub…"); return; }
        if (!manual && (!p.getBoolean("automatic", true) || now - p.getLong("success", 0) < 86400000L || now - p.getLong("attempt", 0) < 600000L)) { callback.accept(status(context)); return; }
        checking = true; p.edit().putLong("attempt", now).apply(); callback.accept("正在检查官网与 GitHub…");
        workers.execute(() -> {
            List<Feed> feeds = new ArrayList<>(); List<Future<Feed>> requests = new ArrayList<>();
            for (String endpoint : SOURCES) requests.add(workers.submit(() -> fetch(endpoint)));
            for (Future<Feed> request : requests) { try { Feed feed = request.get(20, TimeUnit.SECONDS); if (feed != null) feeds.add(feed); } catch (Exception e) { request.cancel(true); } }
            main.post(() -> {
                checking = false; Feed best = newest(feeds), cached = parse(p.getString("latest", ""));
                if (feeds.size() == SOURCES.length) p.edit().putLong("success", now).apply();
                if (best != null && compare(best.version, VERSION) > 0 && (cached == null || compare(best.version, cached.version) >= 0)) p.edit().putString("latest", best.json).apply();
                Feed current = available(context);
                String message = current != null ? "发现新版本 " + current.version : feeds.isEmpty() ? "暂时无法连接更新服务，请稍后重试" : feeds.size() == 2 ? "已是最新版本" : "已检查一个来源，另一来源暂时无法连接";
                p.edit().putString("status", message).apply(); callback.accept(message);
            });
        });
    }
    static String status(Context context) { Feed feed = available(context); return feed != null ? "发现新版本 " + feed.version : preferences(context).getString("status", "自动检查官网与 GitHub 的新版本"); }
}
