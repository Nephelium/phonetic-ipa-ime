package org.nephelium.phoneticipa;

import android.app.Instrumentation;
import android.os.Bundle;
import org.json.*;
import java.nio.charset.StandardCharsets;
import java.util.*;

/** Runs on Android, including the packaged JNI libraries and actual fixed resources. */
public final class CoreTests extends Instrumentation {
    private boolean keyboard;
    private boolean layout;
    private boolean updates;
    private boolean updateProxy;
    @Override public void onCreate(Bundle arguments) { super.onCreate(arguments); keyboard = "keyboard".equals(arguments.getString("suite")); layout = "layout".equals(arguments.getString("suite")); updates = "updates".equals(arguments.getString("suite")); updateProxy = "true".equals(arguments.getString("updateProxy")); start(); }
    private static void require(boolean value, String message) { if (!value) throw new AssertionError(message); }
    @Override public void onStart() {
        Bundle result = new Bundle();
        try {
            if (layout) { new KeyboardChecks(this).runLayout(); result.putString("layout", "passed"); finish(-1, result); return; }
            if (updates) {
                // Optional process-local proxy for this QA emulator's GitHub connectivity.
                if (updateProxy) { System.setProperty("https.proxyHost", "10.0.2.2"); System.setProperty("https.proxyPort", "7892"); }
                java.util.concurrent.CountDownLatch done = new java.util.concurrent.CountDownLatch(1);
                java.util.concurrent.atomic.AtomicReference<String> message = new java.util.concurrent.atomic.AtomicReference<>();
                runOnMainSync(() -> Updates.check(getTargetContext(), true, text -> { if (!text.startsWith("正在")) { message.set(text); done.countDown(); } }));
                require(done.await(50, java.util.concurrent.TimeUnit.SECONDS), "Online check timed out");
                if (!"已是最新版本".equals(message.get())) {
                    for (String endpoint : Updates.SOURCES) {
                        java.net.HttpURLConnection probe = null;
                        try {
                            probe = (java.net.HttpURLConnection)new java.net.URL(endpoint).openConnection();
                            probe.setConnectTimeout(8000); probe.setReadTimeout(8000); probe.setInstanceFollowRedirects(false);
                            android.util.Log.i("IPAUpdateQA", endpoint + " status=" + probe.getResponseCode());
                        } catch (Exception error) { android.util.Log.i("IPAUpdateQA", endpoint + " " + error); }
                        finally { if (probe != null) probe.disconnect(); }
                    }
                }
                require("已是最新版本".equals(message.get()), "Both live sources required: " + message.get());
                result.putString("stream", "Both live Android update sources current: passed\n"); finish(-1, result); return;
            }
            Updates.automatic(getTargetContext(), false);
            require(Updates.VERSION.equals(getTargetContext().getPackageManager().getPackageInfo(getTargetContext().getPackageName(), 0).versionName), "Update version must match the installed package");
            UpdateChecks.run(getContext());
            if (keyboard) { new KeyboardChecks(this).run(); result.putString("stream", "Android real keyboard UI checks passed\n"); finish(-1, result); return; }
            runOnMainSync(() -> {
                android.widget.Button key = UI.ipaButton(getTargetContext(), "ɡ", () -> {}); key.setTextSize(android.util.TypedValue.COMPLEX_UNIT_DIP, 25);
                int width = UI.dp(getTargetContext(), 36), height = UI.dp(getTargetContext(), 42);
                key.measure(android.view.View.MeasureSpec.makeMeasureSpec(width, android.view.View.MeasureSpec.EXACTLY), android.view.View.MeasureSpec.makeMeasureSpec(height, android.view.View.MeasureSpec.EXACTLY)); key.layout(0, 0, width, height);
                android.graphics.Bitmap image = android.graphics.Bitmap.createBitmap(width, height, android.graphics.Bitmap.Config.ARGB_8888); image.eraseColor(android.graphics.Color.WHITE); key.draw(new android.graphics.Canvas(image));
                int ink = 0; for (int y = 2; y < height - 2; y++) for (int x = 2; x < width - 2; x++) if ((image.getPixel(x, y) & 0xffffff) == (UI.INK & 0xffffff)) ink++;
                android.graphics.Rect bounds = new android.graphics.Rect(); key.getPaint().getTextBounds("ɡ", 0, 1, bounds); android.util.Log.i("IPAGlyphQA", "Static g bounds=" + bounds + " size=" + key.getTextSize() + " ink=" + ink);
                require(ink > 10, "Compact key must render visible IPA ink");
                key.scrollTo(524247, 0); image.eraseColor(android.graphics.Color.WHITE);
                android.graphics.Canvas translated = new android.graphics.Canvas(image); translated.translate(-key.getScrollX(), 0); key.draw(translated);
                int scrolledInk = 0;
                for (int y = 2; y < height - 2; y++) for (int x = 2; x < width - 2; x++) if ((image.getPixel(x, y) & 0xffffff) == (UI.INK & 0xffffff)) scrolledInk++;
                require(scrolledInk > 10, "Compact IPA must remain visible after TextView's single-line scroll");
                image.recycle();
            });
            Engine engine = Engine.get(getTargetContext()).get();
            require(engine.symbols("a").getString(0).equals("a"), "CIN a");
            require(engine.symbols("tone:214").getString(0).equals("˨˩˦"), "five-level tone");
            JSONObject profile = engine.profile(); require(!profile.getJSONObject("sequences").has("1"), "bare digit must not retain IPA");
            require(profile.getJSONObject("sequences").getJSONArray("\\").length() >= 15, "moved digit mappings");
            JSONObject wu = engine.reading("恶", "wu"); require(wu.getBoolean("ambiguous"), "wu requires tone choice");
            JSONArray options = wu.getJSONArray("parts").getJSONObject(0).getJSONArray("options");
            for (int i = 0; i < options.length(); i++) require(options.getJSONObject(i).getString("pinyin").startsWith("wu"), "wu must never include e");
            JSONObject phrase = engine.reading("厌恶", "yan'wu"); require(!phrase.getBoolean("ambiguous") && phrase.getString("text").endsWith("wu˥˩"), "word-level tone");
            require(engine.reading("好学", "hao'xue").getBoolean("ambiguous"), "ambiguous phrase stays explicit");
            Session session = new Session(engine); List<String> committed = new ArrayList<>(); session.commit = committed::add;
            session.append("wu"); int position = -1; for (int i = 0; i < session.candidates.size(); i++) if (session.candidates.get(i).word.equals("恶")) { position = i; break; }
            require(position >= 0, "factory candidate index"); session.page = position / session.pageSize; session.choose(position % session.pageSize, true); require(session.pending != null && committed.isEmpty(), "IPA opens choice without commit");
            JSONArray readings = session.options(); int choice = -1; for (int i = 0; i < readings.length(); i++) if (readings.getJSONObject(i).getString("pinyin").equals("wu4")) choice = i;
            session.chooseReading(choice); require(session.pending == null && session.raw.isEmpty() && committed.size() == 1, "reading choice must commit exactly once"); require(committed.get(0).equals("wu˥˩"), "selected IPA commits");
            session.tone("2"); session.tone("1"); session.tone("4"); session.digit("9"); require(session.raw.equals("tone:214"), "invalid tone digit ignored"); session.choose(0, false); require(committed.get(1).equals("˨˩˦"), "tone session");
            session.digit("6"); require(committed.get(2).equals("6"), "empty digit literal");
            for (int size = 3; size <= 9; size++) { session.pageSize = size; session.append("a"); require(session.visible().size() <= size, "page limit"); session.clear(); }
            JSONObject unicode = new JSONObject(profile.toString()); unicode.getJSONObject("sequences").put("zz", new JSONArray().put("ãː").put("𝼀").put(" "));
            engine.loadProfile(unicode, false); require(engine.symbols("zz").getString(1).equals("𝼀"), "supplementary Unicode JNI"); require(engine.symbols("zz").getString(2).equals(" "), "significant space");
            JSONObject invalid = new JSONObject(unicode.toString()); invalid.put("version", 99); boolean rejected = false; try { engine.loadProfile(invalid, false); } catch (Exception expected) { rejected = true; }
            require(rejected && engine.symbols("zz").getString(0).equals("ãː"), "invalid import preserves active profile");
            JSONObject oversized = new JSONObject(profile.toString()); JSONArray moved = new JSONArray(); for (int i = 0; i < 200; i++) moved.put("a"); oversized.getJSONObject("sequences").put("1", moved);
            rejected = false; try { engine.loadProfile(oversized, false); } catch (Exception expected) { rejected = true; }
            require(rejected && engine.symbols("zz").getString(0).equals("ãː"), "Oversized digit migration preserves the active profile");
            require(engine.rules().length() == 10, "ten rules missing");
            engine.selectRule("unt-narrow");
            require(engine.reading("厌恶", "yan'wu").getString("text").contains("jɛ̠n̚"), "narrow rule did not change actual IPA");
            boolean badRule = false; try { engine.selectRule("../beijing"); } catch (Exception e) { badRule = true; }
            require(badRule && engine.ruleId().equals("unt-narrow"), "invalid rule replaced the active one");
            engine.selectRule("beijing");
            engine.profileError = "方案读取失败"; Session failedProfile = new Session(engine); require(failedProfile.status.equals("方案读取失败"), "Profile load error must remain visible"); engine.profileError = "";
            engine.loadProfile(profile, false);
            result.putString("stream", "Android JNI, Unicode, factory lookup, word tones, manual disambiguation, pagination, numeric input and atomic profile checks passed\n"); finish(-1, result);
        } catch (Throwable e) { result.putString("stream", "FAILED: " + android.util.Log.getStackTraceString(e)); finish(0, result); }
    }
}
