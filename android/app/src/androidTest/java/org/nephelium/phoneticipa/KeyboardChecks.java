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
    AccessibilityNodeInfo waitFor(String value, boolean description) {
        for (int i = 0; i < 40; i++) { AccessibilityNodeInfo node = find(value, description); if (node != null) return node; SystemClock.sleep(150); }
        throw new AssertionError("Missing UI: " + value);
    }
    void tap(String value, boolean description) {
        AccessibilityNodeInfo node = waitFor(value, description);
        if (!node.performAction(AccessibilityNodeInfo.ACTION_CLICK)) {
            Rect bounds = new Rect(); node.getBoundsInScreen(bounds); touch(bounds.centerX(), bounds.centerY(), bounds.centerY());
        }
        SystemClock.sleep(180);
    }
    void touch(float x, float y, float endY) {
        long start = SystemClock.uptimeMillis();
        for (int step = 0; step <= 8; step++) {
            int action = step == 0 ? MotionEvent.ACTION_DOWN : step == 8 ? MotionEvent.ACTION_UP : MotionEvent.ACTION_MOVE;
            MotionEvent event = MotionEvent.obtain(start, SystemClock.uptimeMillis(), action, x, y + (endY-y)*step/8, 0);
            event.setSource(InputDevice.SOURCE_TOUCHSCREEN); ui.injectInputEvent(event, true); event.recycle(); SystemClock.sleep(25);
        }
    }
    String hostText() { CharSequence value = waitFor("typing-area", true).getText(); return value == null ? "" : value.toString(); }
    void expect(String expected) { String actual = hostText(); if (!expected.equals(actual)) throw new AssertionError("Host text: " + actual + " expected: " + expected); }
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
        AccessibilityServiceInfo info = ui.getServiceInfo(); info.flags |= AccessibilityServiceInfo.FLAG_RETRIEVE_INTERACTIVE_WINDOWS | AccessibilityServiceInfo.FLAG_REPORT_VIEW_IDS; ui.setServiceInfo(info);
        Intent launch = new Intent(test.getTargetContext(), SettingsActivity.class).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TASK);
        test.startActivitySync(launch); tap("试打", false); tap("typing-area", true); waitFor("key-y", true);
        code("yanwu"); candidate("hanzi-厌恶-yan'wu"); expect("厌恶");
        code("wu"); candidate("ipa-恶-wu");
        if (find("reading-e3", true) != null) throw new AssertionError("e reading leaked into wu");
        String before = hostText(); tap("reading-wu4", true); expect(before); candidate("ipa-恶-wu"); expect("厌恶wu˥˩");
        tap("中", false); code("a"); candidate("hanzi-a-"); expect("厌恶wu˥˩a");
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
        tap("符号", false); tap("key-\\", true); candidate("hanzi-˥-"); tap("key-/", true); candidate("hanzi-/-");
        expect("厌恶wu˥˩a˨˩˦6˥/"); screenshot("android-symbol-keys");
        long now = SystemClock.uptimeMillis(); ui.injectInputEvent(new KeyEvent(now, now, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_BACK, 0), true); ui.injectInputEvent(new KeyEvent(now, now, KeyEvent.ACTION_UP, KeyEvent.KEYCODE_BACK, 0), true);
        tap("键位设置", false); screenshot("android-settings"); tap("IPA Plus", false); waitFor("IPA Plus · 点选音标", false); screenshot("android-plus-ipa"); tap("extIPA", false); screenshot("android-plus-extipa"); tap("VoQS", false); screenshot("android-plus-voqs"); tap("完成", false);
    }
}
