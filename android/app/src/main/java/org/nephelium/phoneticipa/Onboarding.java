package org.nephelium.phoneticipa;

import android.app.*;
import android.content.Context;
import android.graphics.Typeface;
import android.widget.*;

/** Offline first-launch walkthrough; it does not change the active keyboard. */
final class Onboarding {
    private static final String PREF = "onboarding-v1";
    static Dialog show(Activity activity, boolean force) {
        if (activity.isFinishing() || activity.isDestroyed() || (!force && activity.getPreferences(Context.MODE_PRIVATE).getBoolean(PREF, false))) return null;
        Dialog dialog = new Dialog(activity);
        LinearLayout root = UI.column(activity); int pad = UI.dp(activity, 18);
        root.setPadding(pad, pad, pad, pad); root.setBackground(UI.background(0xfff5f8fa, UI.dp(activity, 18)));
        TextView title = UI.text(activity, "", 21); title.setTypeface(Typeface.DEFAULT_BOLD); UI.full(root, title, -2);
        TextView count = UI.text(activity, "", 12); count.setTextColor(UI.MUTED); UI.full(root, count, -2);
        ScrollView scroll = new ScrollView(activity); LinearLayout page = UI.column(activity); page.setPadding(0, pad, 0, pad); scroll.addView(page);
        root.addView(scroll, new LinearLayout.LayoutParams(-1, 0, 1));
        LinearLayout actions = UI.row(activity); UI.full(root, actions, -2);
        String[] titles = {"拼音，也能输入音标", "遇到多音字，再选读音", "点按音标，滑动输入声调", "随时打开完整音标表"};
        String[] figures = {"yanwu  →  厌恶\njan˥˩ wu˥˩", "恶 · wu\n① wu3   ② wu4\nwu˥˩", "ɡ  ···  →  ɠ\n2 ↑   1 ↑   4 ↑  →  ˨˩˦", "IPA Plus\nIPA   ·   extIPA   ·   VoQS\np   t   k   ʔ   ◌̃"};
        String[] details = {
            "默认输入拼音，数字键或空格选择候选音标。点击上面的汉字输入汉字，点击下面的音标输入 IPA。上方音标／汉字按钮切换默认输出。",
            "词语能确定读音时直接使用词语读音。仍有歧义时，先选字，再点选带声调的读音，不会自动取第一条。",
            "左下角切到 IPA，可直接输入音标。带 ··· 的键短按弹出候选，点哪个输入哪个。上划数字 1–5 组合调值，再点上方候选确认。按住退格可连续删除。",
            "键盘上方的 IPA Plus 打开分类音标表，可以连续点选输入。规则按钮可切换转写标准。先确认当前拼音或声调候选，再打开音标表。设置中的说明可重新查看教学。"
        };
        int[] index = {0};
        Runnable[] render = new Runnable[1];
        render[0] = () -> {
            title.setText(titles[index[0]]); count.setText((index[0] + 1) + " / 4"); page.removeAllViews(); actions.removeAllViews();
            TextView figure = UI.text(activity, figures[index[0]], 25); figure.setTypeface(UI.ipa(activity)); figure.setGravity(android.view.Gravity.CENTER);
            figure.setPadding(pad, pad, pad, pad); figure.setBackground(UI.background(UI.SURFACE, UI.dp(activity, 12))); UI.full(page, figure, -2);
            TextView detail = UI.text(activity, details[index[0]], 15); detail.setPadding(0, pad, 0, 0); UI.full(page, detail, -2);
            if (index[0] > 0) UI.weighted(actions, UI.button(activity, "上一步", () -> { index[0]--; render[0].run(); scroll.scrollTo(0, 0); }), 1, 44);
            UI.weighted(actions, UI.button(activity, index[0] == 3 ? "开始使用" : "下一步", () -> {
                if (index[0] < 3) { index[0]++; render[0].run(); scroll.scrollTo(0, 0); }
                else { activity.getPreferences(Context.MODE_PRIVATE).edit().putBoolean(PREF, true).apply(); dialog.dismiss(); }
            }), 1, 44);
        };
        render[0].run(); dialog.setContentView(root); dialog.setCanceledOnTouchOutside(false); dialog.show();
        android.util.DisplayMetrics metrics = activity.getResources().getDisplayMetrics();
        dialog.getWindow().setBackgroundDrawableResource(android.R.color.transparent);
        dialog.getWindow().setLayout(Math.min(UI.dp(activity, 480), metrics.widthPixels - UI.dp(activity, 24)), Math.min(UI.dp(activity, 500), metrics.heightPixels - UI.dp(activity, 100)));
        return dialog;
    }
}
