package org.nephelium.phoneticipa;

import android.app.*;
import android.content.Context;
import android.graphics.Typeface;
import android.widget.*;

/** Offline first-launch walkthrough; it does not change the active keyboard. */
final class Onboarding {
    private static final String PREF = "onboarding-v2";
    static final String[] TITLES = {"先认清当前输入状态",
"用拼音输入音标或汉字",
"切换到直接音标键盘",
"输入声调，查找更多符号"};
    static final String[] DETAILS = {"① 启用并切换到国际音标输入法。Android 在试打页点“启用键盘”后点“切换键盘”；iPhone/iPad 在系统设置→通用→键盘中添加，输入时长按地球键选择。\n\n② 看左下角：显示“拼音”表示当前在拼音查字，点一下变成“IPA”，进入直接音标键盘；再点一次返回。按钮显示的是当前模式。\n\n③ 拼音模式下，上方“音标／汉字”是另一个开关。显示“音标”时，数字／空格默认选 IPA；点成“汉字”后默认选汉字。空闲时左侧会显示“拼音 → 音标”或“拼音 → 汉字”。新安装默认是拼音查字、输出音标，已有偏好会保留。",
"① 先让左下角显示“拼音”，上方输出按钮显示“音标”。依次输入 hao，候选上行显示汉字，下行显示对应音标。\n\n② 选择“好”。可点下行音标，或按“好”前面的数字。出现 hao3、hao4 时，再按读音前的数字，或直接点所需读音。选 hao4 会输入 xau˥˩。候选编号以当前页为准。单一读音直接输入，有多个读音才再选一次。\n\n③ 想输入汉字：把上方按钮切成“汉字”，再输入 hao 并选“好”，结果是汉字“好”。无论默认输出是什么，点候选上行汉字都能输入汉字，点下行音标都能输入 IPA。\n\n④ 可先试完整词语 yanwu，再点“厌恶”下方音标，输入 jan˥˩ wu˥˩。候选确认后才上屏，空格先确认候选，再按一次才输入空格。",
"① 点左下角的“拼音”，直到它显示“IPA”。此时按键用于选择音标，输入普通 a、g 等键就能查找相应符号。回到拼音查字时，再点左下角，直到显示“拼音”。\n\n② 带 ··· 的键有多个音标。短按一次即打开选择面板，再点想要的音标，例如点 g 键后选择 ɠ。打开或关闭面板本身不会输入字母。没有 ··· 的单项键直接输入。\n\n③ 也可用上方 IPA Plus：打开完整 IPA／extIPA／VoQS 音标表，搜索或滑动后点符号，可以连续输入。先确认已有候选，再打开音标表。\n\n④ 点“符号”查看 /、\\ 等标点键。\\ 的候选收录声调符号，/ 默认输入斜杠。按住退格可连续删除，松手停止。",
"① 先确认正在输入的汉字或音标，再开始加声调。数字 1 最低、5 最高。依次在 2、1、4 三个数字键上向上划动，上方出现调值候选。\n\n② 看到 tone:214 时，表示调值还在组合。点选 ˨˩˦、²¹⁴ 或 214 中想要的一项，也可点空格确认首项。确认后声调才上屏。输入声调时轻点 1–5 可继续追加调值。\n\n③ 普通候选出现时，轻点 1–9 选当前页候选；无候选时，普通数字键输入数字。IPA 模式的 0 保留键位方案中的映射。\n\n④ “规则”按钮切换 Standard Chinese Beijing、UntPhesoca 等十套转写方案。规则决定拼音查字的转写结果，直接音标键盘按自己的键位方案输入。设置中的“说明”可随时重看教学。"};
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
        String[] titles = TITLES;
        String[] figures = {"左下角：拼音 ↔ IPA\n上方：音标 ↔ 汉字", "hao → 选好 → 选 hao4\nxau˥˩", "ɡ ··· → 选择 ɠ\nIPA Plus → 全部符号", "2 ↑  1 ↑  4 ↑\n˨˩˦   ²¹⁴   214"};
        String[] details = DETAILS;
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
