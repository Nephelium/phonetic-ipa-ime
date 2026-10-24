package org.nephelium.phoneticipa;
import org.json.*;
import java.util.*;
import java.nio.charset.StandardCharsets;
final class UpdateChecks {
    static void run(android.content.Context context) throws Exception {
        JSONObject vectors;
        try (java.io.InputStream in = context.getAssets().open("update-test-vectors.json")) { vectors = new JSONObject(new String(SettingsActivity.readLimited(in, 200000), StandardCharsets.UTF_8)); }
        JSONArray cases = vectors.getJSONArray("cases");
        for (int i = 0; i < cases.length(); i++) {
            JSONObject test = cases.getJSONObject(i);
            if ((Updates.parse(test.getJSONObject("feed").toString()) != null) != test.getBoolean("valid")) throw new AssertionError(test.getString("name"));
        }
        JSONArray versions = vectors.getJSONArray("versions");
        for (int i = 0; i < versions.length(); i++) { JSONArray pair = versions.getJSONArray(i); if (Integer.signum(Updates.compare(pair.getString(0), pair.getString(1))) != pair.getInt(2)) throw new AssertionError("Version comparison"); }
        JSONObject first = cases.getJSONObject(0).getJSONObject("feed"); Updates.Feed server = Updates.parse(first.toString());
        first.put("version", "0.1.10"); Updates.Feed github = Updates.parse(first.toString());
        if (!Updates.newest(Arrays.asList(server, github)).version.equals("0.1.10") || !Updates.newest(Arrays.asList(github, server)).version.equals("0.1.10") || Updates.newest(Arrays.asList(null, server)) != server || Updates.newest(Collections.emptyList()) != null) throw new AssertionError("Source independence");
        if (Updates.parse(" ".repeat(65537)) != null || Updates.parse("{") != null) throw new AssertionError("Size/JSON");
    }
}
