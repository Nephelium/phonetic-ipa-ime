def lessons(phone, mac=False):
    def step(image,title,action,watch,keys=(),targets=(),result='',tip=''):
        return dict(image=image,title=title,action=action,watch=watch,keys=list(keys),targets=list(targets),result=result,tip=tip)
    return [
      dict(id='start',name='先看当前状态',note='拼音、汉字、单个音标，先选对入口。',steps=[
        step('start','从拼音输出音标开始',
          '点进试打框，切到国际音标输入法。左下角应显示「拼音」，上方应显示「音标」。' if phone else '先在系统中选择国际音标输入法，再点进要打字的文本框。保持拼音输入、音标输出。',
          '① 拼音决定如何查找，② 音标决定选中后输出什么。两处按钮分别切换。' if phone else ('候选栏显示「拼音 → 音标」。按 Caps Lock 可以切换为输出汉字。' if mac else '工具栏显示 IPA。拼音候选上行是汉字，下行是音标；直接音标模式也显示 IPA，要结合候选内容区分。'),
          ['点「拼音」','点「音标」'] if phone else ['选择输入法','点文本框'],(['mode','output'] if phone or mac else ['mode']))]),
      dict(id='happy',name='拼音输入「快乐」',note='乐在快乐里读 lè，整词读音帮助确定多音字。',steps=[
        step('word','输入完整拼音 kuaile','依次输入 k、u、a、i、l、e，先不要确认候选。','组合区显示 kuaile 或 kuai’le，候选中出现快乐。现在还没有把音标输入正文。',list('kuaile'),['composition']),
        step('word','选快乐下方的音标','点快乐下方的音标。电脑也可按快乐前的数字，图中快乐是第 1 项。','选的是音标这一行。快乐的读音为 kuai4 le4，通常不需要再选一次。',['点下行音标'] if phone else ['1 / 空格','或点下行音标'],['word-ipa']),
        step('word-result','看正文里的结果','确认后看输入框，音标应已进入正文。','拼音组合消失。这个结果已经带声调，不必再重复补调值。',[],['result'],'kʰuai˥˩ lɤ˥˩','本文示例使用 Standard Chinese Beijing 宽；其他规则的音标写法会不同。'),
        step('quick-word','对比：这次选快了','同样输入 kuaile，这次点快了下方的选择读音。电脑也可按快了前的数字，图中是第 2 项。','快乐和快了的拼音字母相同，先选中自己要表达的词语，再确认读音。',['点快了下行'] if phone else ['k u a i l e','2'],['quick-ipa']),
        step('quick-readings','确认快读 kuai4','在实际出现的读音页中选择 kuai4。图中它排第 1 项。','这里待选的是快的读音。了按输入的 le 匹配为轻声，不会列出 liao 的读音。',['点 kuai4'] if phone else ['1'],['quick-reading-4']),
        step('quick-result','对照两个词的结果','看正文中的快了音标，再和前面的快乐对照。','快乐中的乐读 lè，音标带第四声；快了中的了读轻声 le，示例写作 lɤ，不添加第四声。',[],['result'],'kʰuai˥˩ lɤ')]),
      dict(id='polyphone',name='「好」按两次数字',note='第一次选汉字，第二次选读音。每一页都重新看编号。',steps=[
        step('hao','输入 hao，先选好','输入 hao，找到好，再按它前面的数字。图中好排第 1，按 1。','这一步在选汉字。好仍有 hao3、hao4 两种读音，需要继续选择。',['h','a','o','1'],['word-ipa']),
        step('readings','再选 hao4','重新查看读音页编号。图中 hao4 排第 2，所以再按 2；手机也能直接点 hao4。','hao3 和 hao4 是两个不同读音，不要沿用上一页的编号。',['点 hao4'] if phone else ['2'],['reading-4']),
        step('reading-result','确认好读 hào 的音标','读音确认后，看正文中刚输入的音标。','选择 hao4 得到 xau˥˩；如果选 hao3，则得到 xau˨˩˦。',[],['result'],'xau˥˩','只有读音仍有歧义时才出现第二次选择。单一读音可以直接输出。')]),
      dict(id='hanzi',name='改为输出汉字',note='输入方式仍是拼音，改变的是最终输出内容。',steps=[
        step('hanzi-mode','切到汉字输出','在拼音模式点上方「音标」，使它显示「汉字」。' if phone else '在拼音模式按一次 Caps Lock（大写锁定键），不需要按住。','输出按钮显示汉字，之后选候选默认输入汉字。',['点「音标」'] if phone else ['Caps Lock'],['output']),
        step('hanzi-word','再输入 kuaile 并选快乐','依次输入 kuaile，再选快乐的编号，或直接点上行汉字。','继续输入的是拼音，不需要改打大写字母。',['k u a i l e','点上行汉字'] if phone else ['k u a i l e','1'],['word-hanzi']),
        step('hanzi-result','正文出现快乐','查看正文，确认输入的是汉字快乐。','想恢复拼音输出音标，再点「汉字」切回音标。' if phone else '想恢复拼音输出音标，再按一次 Caps Lock。',[],['result'],'快乐')]),
      dict(id='direct',name='直接选单个音标',note='跳过拼音查字，用键位查找音标。',steps=[
        step('ipa-mode','切到 IPA 直接音标模式','先确认已有候选，再点左下角「拼音」，直到显示 IPA。' if phone else '先确认或取消已有候选，再单独按下 Shift 并松开。','带 ··· 的按键可以短按打开音标选择面板。' if phone else ('候选栏显示「IPA · 点选音标」，接下来字母用作音标编码。' if mac else '切到直接音标后，字母用作音标编码。下一步按 a 看对应的音标列表。'),['点「拼音」'] if phone else ['Shift','松开'],['mode','key-g' if phone else 'mode']),
        step('ipa-picker','短按 g，再选一个音标' if phone else '按 a，再选首项','短按 g 一次，松手后在面板中点选想要的音标。本图选择第二项 ɡ。' if phone else '按 a 查看候选，再按 1 或空格确认首项 a。','打开或关闭面板本身不会输入 g。' if phone else '选择之前仍在组合；确认候选后音标才进入正文。',['g','点第二项 ɡ'] if phone else ['a','1 / 空格'],['symbol-choice'] if phone else ['symbol-first']),
        step('ipa-result','音标已输入','看正文中的单个音标。可以继续输入其他音标。','点左下角 IPA 切回拼音。' if phone else '再次单按并松开 Shift，返回拼音输入。',[],['result'],'ɡ' if phone else 'a')]),
      dict(id='tone',name='输入 214 调值',note='1 最低、5 最高，最后再选择声调的写法。',steps=[
        step('tone','先组合 214','先在数字 2 上向上划，再轻点 1、4。进入声调组合后，轻点数字可继续追加调值。' if phone else '按住 Shift，依次按 2、1、4，全部按完再松开 Shift。','组合区显示 tone:214，表示声调还在等待确认。',['2 ↑','1','4'] if phone else ['按住 Shift','2','1','4','松开'],['digit-2','digit-1','digit-4','composition']),
        step('tone','选择五度调符号','直接点第一项 ˨˩˦。这里要点候选，不是再点数字 1。' if phone else '按空格确认首项 ˨˩˦，也可按候选编号选择其他写法。','还可以选择上标数字 ²¹⁴ 或普通数字 214。',['点首项 ˨˩˦'] if phone else ['空格'],['tone-first']),
        step('tone-result','声调进入正文','看刚输入的音标后面，已经加上 ˨˩˦。','确认声调后，再按空格才会插入普通空格。',[],['result'],'ɡ˨˩˦' if phone else 'a˨˩˦')]),
      dict(id='plus',name='打开 IPA Plus',note='手机直接向正文输入，电脑用于查看音标和编辑键位。',steps=[
        step('plus-open' if phone else 'settings','打开完整音标表','先确认已有候选，再点键盘上方的 IPA Plus。' if phone else '打开设置的音标键盘页，点要编辑的键位，选中一个候选，再打开 IPA Plus。','音标表提供 IPA、extIPA 和 VoQS 分类。',['IPA Plus'] if phone else ['设置','音标键盘','IPA Plus'],['plus']),
        step('plus-chart','点选需要的符号','切换分类，搜索或滑动查找，再点需要的符号。' if phone else '在 IPA Plus 中点一个符号，将它放入当前编辑的键位候选。','手机会把符号直接输入正文，可以连续点选。' if phone else '电脑是在编辑键位，修改后需要保存方案。',['点符号'],['plus-symbol']),
        step('plus-result' if phone else 'settings','返回键盘继续输入' if phone else '保存键位方案','点返回键盘，再看正文中的符号。' if phone else ('完成候选编辑后点保存方案，之后使用该键位选择音标。' if mac else '完成候选编辑后点应用方案，之后使用该键位选择音标。'),'表格可以左右滑，页面可以上下滑。手机默认字号 50%，最低可调至 20%。Windows 默认适合宽度，放大后可用左右移动按钮查看。',[],['result'] if phone else ['save'])]),
      dict(id='rules',name='快捷切换转写规则',note='先选规则，再输入拼音；正文中已经输入的音标不会被改写。',steps=[
        step('rules' if phone else 'rule-button','打开规则菜单并选择方案','点键盘上方的规则，再点需要的方案。' if phone else ('点候选栏的规则菜单，选择需要的方案。' if mac else '点悬浮工具栏的规则名称，菜单会向上展开，再选择需要的方案。'),'共有十套规则，包括 Standard Chinese Beijing 宽／严和 UntPhesoca 宽／严。',['规则'],['rule']),
        step('word','用相同的词再试一次','回到文本框，重新输入 kuaile，再选快乐的音标。','新输入使用刚选中的规则。直接音标键位方案不会随转写规则切换。',['k u a i l e','选快乐'],['word-ipa'])])
    ]
