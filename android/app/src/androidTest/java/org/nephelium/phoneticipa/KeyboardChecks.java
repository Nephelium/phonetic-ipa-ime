package org.nephelium.phoneticipa;

import android.accessibilityservice.AccessibilityServiceInfo;
import android.app.*;
import android.content.Intent;
import android.graphics.*;
import android.os.SystemClock;
import android.view.*;
import android.view.accessibility.*;
import java.io.*;
import java.util.*;

/** Interacts through Android accessibility and injected touch, never through the Session API. */
final class KeyboardChecks {
    final Instrumentation test;
    final UiAutomation ui;
    KeyboardChecks(Instrumentation test) { this.test = test; ui = test.getUiAutomation(); }
    AccessibilityNodeInfo find(String value, boolean description) {
        ui.clearCache();
        for (AccessibilityWindowInfo window : ui.getWindows()) {
            AccessibilityNodeInfo result = search(window.getRoot(), value, description);
            if (result != null) return result;
        }
        return null;
    }
    AccessibilityNodeInfo search(AccessibilityNodeInfo node, String value, boolean description) {
        if (node == null) return null;
        CharSequence label = description ? node.getContentDescription() : node.getText();
        if (value.contentEquals(label == null ? "" : label) && node.isVisibleToUser()) return node;
        for (int i = 0; i < node.getChildCount(); i++) { AccessibilityNodeInfo result = search(node.getChild(i), value, description); if (result != null) return result; }
        return null;
    }
    AccessibilityNodeInfo scrollable(AccessibilityNodeInfo node) {
        if (node == null) return null;
        if (node.isScrollable()) return node;
        for (int index = 0; index < node.getChildCount(); index++) {
            AccessibilityNodeInfo result = scrollable(node.getChild(index)); if (result != null) return result;
        }
        return null;
    }
    AccessibilityNodeInfo waitFor(String value, boolean description) {
        for (int i = 0; i < 40; i++) { AccessibilityNodeInfo node = find(value, description); if (node != null) return node; SystemClock.sleep(150); }
        throw new AssertionError("Missing UI: " + value);
    }
    void tap(String value, boolean description) {
        int screenHeight = ui.takeScreenshot().getHeight();
        Rect bounds = new Rect(), previous = new Rect(); boolean stable = false;
        for (int retry = 0; retry < 40; retry++) {
            AccessibilityNodeInfo node = waitFor(value, description); node.refresh(); node.getBoundsInScreen(bounds);
            if (!bounds.isEmpty() && bounds.top >= 0 && bounds.bottom <= screenHeight && bounds.equals(previous)) { stable = true; break; }
            previous.set(bounds); SystemClock.sleep(100);
        }
        if (!stable) throw new AssertionError("UI is not touchable on screen: " + value + " " + bounds);
        android.util.Log.i("IPATouchQA", value + " " + bounds);
        touch(bounds.centerX(), bounds.centerY(), bounds.centerY());
        try { ui.waitForIdle(150, 3000); } catch (java.util.concurrent.TimeoutException e) { throw new AssertionError("UI did not settle", e); } SystemClock.sleep(180);
    }
    void touch(float x, float y, float endY) {
        long start = SystemClock.uptimeMillis();
        if (y == endY) {
            // A short tap must not become a held key while the emulator renders.
            MotionEvent down = MotionEvent.obtain(start, start, MotionEvent.ACTION_DOWN, x, y, 0);
            down.setSource(InputDevice.SOURCE_TOUCHSCREEN); ui.injectInputEvent(down, true); down.recycle();
            SystemClock.sleep(35);
            long released = SystemClock.uptimeMillis();
            MotionEvent up = MotionEvent.obtain(start, released, MotionEvent.ACTION_UP, x, y, 0);
            up.setSource(InputDevice.SOURCE_TOUCHSCREEN); ui.injectInputEvent(up, true); up.recycle();
            android.util.Log.i("IPATouchQA", "short tap duration=" + (released - start));
            if (released - start >= ViewConfiguration.getLongPressTimeout())
                throw new AssertionError("Short tap exceeded the long-press threshold: " + (released - start));
            return;
        }
        for (int step = 0; step <= 8; step++) {
            int action = step == 0 ? MotionEvent.ACTION_DOWN : step == 8 ? MotionEvent.ACTION_UP : MotionEvent.ACTION_MOVE;
            MotionEvent event = MotionEvent.obtain(start, SystemClock.uptimeMillis(), action, x, y + (endY-y)*step/8, 0);
            event.setSource(InputDevice.SOURCE_TOUCHSCREEN); ui.injectInputEvent(event, true); event.recycle(); SystemClock.sleep(25);
        }
    }
    void swipe(float x, float y, float endX, float endY) {
        long start = SystemClock.uptimeMillis();
        for (int step = 0; step <= 12; step++) {
            int action = step == 0 ? MotionEvent.ACTION_DOWN : step == 12 ? MotionEvent.ACTION_UP : MotionEvent.ACTION_MOVE;
            MotionEvent event = MotionEvent.obtain(start, SystemClock.uptimeMillis(), action, x + (endX-x)*step/12, y + (endY-y)*step/12, 0);
            event.setSource(InputDevice.SOURCE_TOUCHSCREEN); ui.injectInputEvent(event, true); event.recycle(); SystemClock.sleep(25);
        }
        SystemClock.sleep(250);
    }
    void scrollChoice(String label) {
        for (int i = 0; i < 18; i++) {
            if (find(label, true) != null) return;
            Rect panel = new Rect(); waitFor("symbol-picker-list", true).getBoundsInScreen(panel);
            swipe(panel.centerX(), panel.bottom - 20, panel.centerX(), panel.top + 90);
        }
        throw new AssertionError("Unreachable symbol: " + label);
    }
    String hostText() { CharSequence value = waitFor("typing-area", true).getText(); return value == null ? "" : value.toString(); }
    void visibleGlyphs(String... descriptions) {
        Bitmap screen = ui.takeScreenshot();
        for (String description : descriptions) {
            Rect bounds = new Rect(); waitFor(description, true).getBoundsInScreen(bounds);
            int ink = 0;
            for (int y = Math.max(0, bounds.top + 5); y < Math.min(screen.getHeight(), bounds.bottom - 5); y++)
                for (int x = Math.max(0, bounds.left + 5); x < Math.min(screen.getWidth(), bounds.right - 5); x++)
                    if ((screen.getPixel(x, y) & 0xffffff) == (UI.INK & 0xffffff)) ink++;
            if (ink < 6) throw new AssertionError("Key is present but its glyph is invisible: " + description + " pixels=" + ink);
        }
        screen.recycle();
    }
    void expect(String expected) { String actual = hostText(); if (!expected.equals(actual)) throw new AssertionError("Host text: " + actual + " expected: " + expected); }
    void visibleChoiceBadge(String description, boolean expected) {
        Bitmap screen = ui.takeScreenshot(); Rect bounds = new Rect(); waitFor(description, true).getBoundsInScreen(bounds);
        int accent = 0;
        for (int y = bounds.centerY(); y < Math.min(screen.getHeight(), bounds.bottom); y++)
            for (int x = bounds.centerX(); x < Math.min(screen.getWidth(), bounds.right); x++)
                if ((screen.getPixel(x, y) & 0xffffff) == (UI.ACCENT & 0xffffff)) accent++;
        screen.recycle();
        if ((accent > 2) != expected) throw new AssertionError("Multiple-choice badge visibility: " + description + " pixels=" + accent);
    }
    void code(String code) { for (char c : code.toCharArray()) tap("key-" + c, true); }
    void candidate(String value) {
        for (int i = 0; i < 30; i++) { if (find(value, true) != null) { tap(value, true); return; } tap("›", false); }
        throw new AssertionError("Candidate missing: " + value);
    }
    void screenshot(String name) throws Exception {
        File folder = new File(test.getTargetContext().getExternalFilesDir(null), "qa"); folder.mkdirs();
        try (FileOutputStream out = new FileOutputStream(new File(folder, name + ".png"))) { ui.takeScreenshot().compress(Bitmap.CompressFormat.PNG, 100, out); }
    }
    void run() throws Exception {
        try { runChecks(); } catch (Throwable error) { screenshot("android-failure"); throw error; }
    }
    void candidateAboveDigits(String... labels) {
        Rect digit = new Rect(); waitFor("digit-1", true).getBoundsInScreen(digit);
        for (String label : labels) {
            Rect bounds = new Rect(); waitFor(label, true).getBoundsInScreen(bounds);
            if (bounds.height() < 20 || bounds.bottom + 3 > digit.top) throw new AssertionError(label + " overlaps digit row: " + bounds + " / " + digit);
        }
    }
    void clearTrial() {
        android.os.Bundle args = new android.os.Bundle(); args.putCharSequence(AccessibilityNodeInfo.ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE, "");
        waitFor("typing-area", true).performAction(AccessibilityNodeInfo.ACTION_SET_TEXT, args); tap("typing-area", true);
    }
    void runLayout() throws Exception {
        AccessibilityServiceInfo info = ui.getServiceInfo(); info.flags |= AccessibilityServiceInfo.FLAG_RETRIEVE_INTERACTIVE_WINDOWS | AccessibilityServiceInfo.FLAG_REPORT_VIEW_IDS; ui.setServiceInfo(info);
        try {
            for (int rotation : new int[] {UiAutomation.ROTATION_FREEZE_0, UiAutomation.ROTATION_FREEZE_90}) {
                ui.setRotation(rotation); SystemClock.sleep(800);
                test.startActivitySync(new Intent(test.getTargetContext(), SettingsActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK));
                tap("试打", false); tap("typing-area", true);
                if (!"拼音".contentEquals(waitFor("keyboard-mode", true).getText())) tap("keyboard-mode", true);
                if ("汉字".contentEquals(waitFor("output-mode", true).getText())) tap("output-mode", true);
                clearTrial(); code("kuaile"); candidateAboveDigits("hanzi-快乐-kuai'le", "ipa-快乐-kuai'le"); screenshot("android-layout-" + rotation + "-word"); candidate("ipa-快乐-kuai'le");
                code("hao"); candidate("ipa-好-hao"); candidateAboveDigits("reading-hao3", "reading-hao4"); screenshot("android-layout-" + rotation + "-readings"); tap("reading-hao4", true);
                Rect two = new Rect(); waitFor("digit-2", true).getBoundsInScreen(two); touch(two.centerX(), two.bottom - 8, two.top - 24); SystemClock.sleep(300);
                tap("digit-1", true); tap("digit-4", true); candidateAboveDigits("hanzi-˨˩˦-"); screenshot("android-layout-" + rotation + "-tone"); candidate("hanzi-˨˩˦-");
            }
        } finally { ui.setRotation(UiAutomation.ROTATION_UNFREEZE); }
    }
    void runChecks() throws Exception {
        AccessibilityServiceInfo info = ui.getServiceInfo(); info.flags |= AccessibilityServiceInfo.FLAG_RETRIEVE_INTERACTIVE_WINDOWS | AccessibilityServiceInfo.FLAG_REPORT_VIEW_IDS; ui.setServiceInfo(info);
        Intent launch = new Intent(test.getTargetContext(), SettingsActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
        test.startActivitySync(launch);
        for (int i = 0; i < 40 && find("键位设置", false) == null; i++) SystemClock.sleep(150);
        if (find("下一步", false) != null) {
            screenshot("android-first-launch");
            for (int page = 0; page < 4; page++) {
                waitFor(Onboarding.TITLES[page], false);
                screenshot("android-guide-page-" + (page + 1));
                AccessibilityNodeInfo body = scrollable(ui.getRootInActiveWindow());
                if (body != null) { body.performAction(AccessibilityNodeInfo.ACTION_SCROLL_FORWARD); SystemClock.sleep(250); }
                screenshot("android-guide-bottom-" + (page + 1));
                tap(page == 3 ? "开始使用" : "下一步", false);
            }
        }
        tap("试打", false); tap("typing-area", true); waitFor("key-y", true);
        if (!"拼音".contentEquals(waitFor("keyboard-mode", true).getText())) tap("keyboard-mode", true);
        screenshot("android-initial-mode");
        java.util.List<String> labels = new java.util.ArrayList<>();
        for (char key : "qwertyuiopasdfghjklzxcvbnm".toCharArray()) labels.add("key-" + key);
        for (char digit : "1234567890".toCharArray()) labels.add("digit-" + digit);
        visibleGlyphs(labels.toArray(new String[0]));
        code("yanwu"); screenshot("android-pinyin-composing"); candidate("hanzi-厌恶-yan'wu"); expect("厌恶");
        code("wu"); candidate("ipa-恶-wu");
        if (find("reading-e3", true) != null) throw new AssertionError("e reading leaked into wu");
        tap("reading-wu4", true); expect("厌恶wu˥˩");
        tap("keyboard-mode", true); waitFor("IPA", false); visibleChoiceBadge("key-a", true); code("a"); expect("厌恶wu˥˩"); screenshot("android-short-tap-picker"); tap("symbol-choice-a-0", true); expect("厌恶wu˥˩a");
        String tone = "";
        for (char digit : "214".toCharArray()) {
            Rect bounds = new Rect(); waitFor("digit-" + digit, true).getBoundsInScreen(bounds);
            touch(bounds.centerX(), bounds.centerY(), bounds.centerY()-120); tone += digit;
            String expected = "厌恶wu˥˩atone:" + tone;
            for (int retry = 0; retry < 30 && !hostText().equals(expected); retry++) SystemClock.sleep(100);
            expect(expected);
            ui.waitForIdle(150, 3000);
        }
        screenshot("android-tone-composing"); expect("厌恶wu˥˩atone:214");
        candidate("hanzi-˨˩˦-"); tap("digit-6", true); expect("厌恶wu˥˩a˨˩˦6"); screenshot("android-actual-input");
        // Pick the IME help button by its window rather than the host app's tab.
        for (AccessibilityWindowInfo window : ui.getWindows()) if (window.getType() == AccessibilityWindowInfo.TYPE_INPUT_METHOD) {
            AccessibilityNodeInfo help = search(window.getRoot(), "说明", false); if (help != null) help.performAction(AccessibilityNodeInfo.ACTION_CLICK);
        }
        waitFor("键盘使用说明", false); screenshot("android-keyboard-help"); tap("返回键盘", false); expect("厌恶wu˥˩a˨˩˦6");
        tap("符号", false); visibleChoiceBadge("key-\\", true); visibleChoiceBadge("key-/", false); tap("key-\\", true); tap("symbol-choice-\\-0", true); tap("key-/", true);
        expect("厌恶wu˥˩a˨˩˦6˥/"); screenshot("android-symbol-keys");
        tap("ABC", false);
        tap("key-g", true); expect("厌恶wu˥˩a˨˩˦6˥/");
        screenshot("android-g-picker"); tap("symbol-picker-close", true); expect("厌恶wu˥˩a˨˩˦6˥/");
        Rect g = new Rect(); waitFor("key-g", true).getBoundsInScreen(g);
        long hold = SystemClock.uptimeMillis(); MotionEvent down = MotionEvent.obtain(hold, hold, MotionEvent.ACTION_DOWN, g.centerX(), g.centerY(), 0); down.setSource(InputDevice.SOURCE_TOUCHSCREEN); ui.injectInputEvent(down, true); down.recycle(); SystemClock.sleep(650);
        MotionEvent up = MotionEvent.obtain(hold, SystemClock.uptimeMillis(), MotionEvent.ACTION_UP, g.centerX(), g.centerY(), 0); up.setSource(InputDevice.SOURCE_TOUCHSCREEN); ui.injectInputEvent(up, true); up.recycle();
        tap("symbol-choice-g-1", true); expect("厌恶wu˥˩a˨˩˦6˥/ɠ");
        tap("key-v", true); expect("厌恶wu˥˩a˨˩˦6˥/ɠ");
        tap("symbol-code", true); tap("组合输入 · voqs", false); scrollChoice("symbol-choice-voqs-57"); screenshot("android-combination-picker"); tap("symbol-choice-voqs-57", true);
        expect("厌恶wu˥˩a˨˩˦6˥/ɠΘ");
        tap("digit-0", true); expect("厌恶wu˥˩a˨˩˦6˥/ɠΘ∅");
        screenshot("android-picked-symbols");
        tap("keyboard-plus", true); tap("plus-ipa-pulmonic-001", true);
        expect("厌恶wu˥˩a˨˩˦6˥/ɠΘ∅p");
        tap("extIPA", false); tap("plus-extipa-consonants-001", true);
        expect("厌恶wu˥˩a˨˩˦6˥/ɠΘ∅pp̪"); screenshot("android-keyboard-plus-extipa");
        tap("VoQS", false); screenshot("android-keyboard-plus-voqs"); tap("plus-close", true);
        expect("厌恶wu˥˩a˨˩˦6˥/ɠΘ∅pp̪");
        // A real held key deletes repeatedly, then stops immediately on release.
        String beforeRepeat = hostText();
        for (int i = 0; i < 14; i++) tap("digit-6", true);
        Rect delete = new Rect(); waitFor("backspace", true).getBoundsInScreen(delete);
        long heldAt = SystemClock.uptimeMillis();
        MotionEvent deleteDown = MotionEvent.obtain(heldAt, heldAt, MotionEvent.ACTION_DOWN, delete.centerX(), delete.centerY(), 0);
        deleteDown.setSource(InputDevice.SOURCE_TOUCHSCREEN); ui.injectInputEvent(deleteDown, true); deleteDown.recycle();
        SystemClock.sleep(900);
        MotionEvent deleteUp = MotionEvent.obtain(heldAt, SystemClock.uptimeMillis(), MotionEvent.ACTION_UP, delete.centerX(), delete.centerY(), 0);
        deleteUp.setSource(InputDevice.SOURCE_TOUCHSCREEN); ui.injectInputEvent(deleteUp, true); deleteUp.recycle();
        SystemClock.sleep(180); String afterRepeat = hostText();
        int removed = (beforeRepeat.length() + 14) - afterRepeat.length();
        if (removed < 3 || removed > 13 || !afterRepeat.startsWith(beforeRepeat)) throw new AssertionError("Held delete count: " + removed);
        SystemClock.sleep(500); expect(afterRepeat);
        while (hostText().length() > beforeRepeat.length()) tap("backspace", true);
        expect(beforeRepeat); screenshot("android-held-delete");
        // Moving off a key cancels it; releasing must not cause a stray delete.
        tap("digit-6", true); tap("digit-6", true);
        waitFor("backspace", true).getBoundsInScreen(delete);
        touch(delete.centerX(), delete.centerY(), delete.top - 50); expect(beforeRepeat + "66");
        android.os.Bundle selection = new android.os.Bundle();
        selection.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_START_INT, beforeRepeat.length());
        selection.putInt(AccessibilityNodeInfo.ACTION_ARGUMENT_SELECTION_END_INT, beforeRepeat.length() + 2);
        if (!waitFor("typing-area", true).performAction(AccessibilityNodeInfo.ACTION_SET_SELECTION, selection)) throw new AssertionError("Cannot select trial text");
        tap("backspace", true); expect(beforeRepeat);
        tap("keyboard-mode", true); String base = hostText();
        code("yanwu"); tap("digit-1", true); expect(base + "jan˥˩ wu˥˩");
        tap("output-mode", true); code("yanwu"); tap("digit-1", true); expect(base + "jan˥˩ wu˥˩厌恶");
        tap("output-mode", true); tap("transcription-rules", true); tap("UntPhesoca 严", false);
        code("yanwu"); tap("digit-1", true); expect(base + "jan˥˩ wu˥˩厌恶jɛ̠n̚˥˩ uᵝ˥˩");
        tap("transcription-rules", true); tap("Standard Chinese Beijing 宽", false);
        screenshot("android-default-ipa-rules");
        long now = SystemClock.uptimeMillis(); ui.injectInputEvent(new KeyEvent(now, now, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_BACK, 0), true); ui.injectInputEvent(new KeyEvent(now, now, KeyEvent.ACTION_UP, KeyEvent.KEYCODE_BACK, 0), true);
        tap("键位设置", false); screenshot("android-settings");
        for (int i = 0; i < 6 && find("IPA Plus", false) == null; i++) { Rect settings = new Rect(); waitFor("settings-page", true).getBoundsInScreen(settings); swipe(settings.centerX(), settings.top + settings.height() * .8f, settings.centerX(), settings.top + settings.height() * .4f); }
        tap("IPA Plus", false); waitFor("IPA Plus · 点选音标", false); screenshot("android-plus-ipa");
        waitFor("50%", false); visibleGlyphs("plus-ipa-pulmonic-001");
        tap("plus-ipa-pulmonic-001", true);
        Rect matrix = new Rect(); waitFor("plus-matrix-extended", true).getBoundsInScreen(matrix);
        for (int i = 0; i < 14 && find("plus-ipa-pulmonic-013", true) == null; i++) swipe(matrix.left + matrix.width() * 0.8f, matrix.top + 100, matrix.left + matrix.width() * 0.25f, matrix.top + 100);
        tap("plus-ipa-pulmonic-013", true);
        screenshot("android-plus-ipa-scrolled");
        tap("extIPA", false); waitFor("plus-extipa-consonants-001", true); screenshot("android-plus-extipa");
        tap("字号 ＋", false); screenshot("android-plus-extipa-large");
        tap("VoQS", false); screenshot("android-plus-voqs"); tap("完成", false);
        if (!"ʔ".contentEquals(waitFor("output-0", true).getText())) throw new AssertionError("Plus selection did not reach editor");
        for (int i = 0; i < 6 && find("output-preview-6", true) == null; i++) {
            Rect editor = new Rect(); waitFor("output-list", true).getBoundsInScreen(editor);
            swipe(editor.left + editor.width() * .8f, editor.top + editor.height() / 3, editor.left + editor.width() * .25f, editor.top + editor.height() / 3);
        }
        if (!"ᵃ".contentEquals(waitFor("output-preview-6", true).getText())) throw new AssertionError("Modifier preview disappeared");
        screenshot("android-editor-modifiers");
    }
}
