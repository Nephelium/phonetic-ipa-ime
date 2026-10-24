from pathlib import Path
import hashlib,html,importlib.util,json,re,shutil,struct
root=Path(__file__).resolve().parent;repo=root.parents[1]
destination=Path(globals()['destination']);article=globals().get('text_article','')
spec=importlib.util.spec_from_file_location('guide_lessons',root/'lessons.py');mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
images=json.loads((root/'images.json').read_text(encoding='utf-8'));data={'lessons':{},'images':{}}
manual_windows={'mode':[55,30,48,31],'output':[55,30,48,31],'composition':[78,403,230,38],'word-ipa':[81,476,180,42],'word-hanzi':[81,435,180,40],'quick-ipa':[82,554,305,46],'quick-reading-4':[82,464,245,77],'reading-4':[83,546,245,77],'result':[43,153,640,75],'symbol-first':[80,437,220,53],'tone-first':[80,437,220,53],'rule':[124,28,222,36],'plus':[36,1325,1320,43],'plus-symbol':[118,273,28,40],'save':[35,1245,90,42]}
manual_mac={'mode':[26,224,220,29],'output':[504,224,53,30],'composition':[26,223,220,31],'word-ipa':[34,290,110,42],'word-hanzi':[34,253,110,40],'quick-ipa':[150,290,150,42],'quick-reading-4':[26,286,92,70],'reading-4':[108,286,70,70],'result':[28,67,680,105],'symbol-first':[30,254,92,65],'tone-first':[32,254,100,65],'rule':[562,224,76,30],'plus':[34,860,119,28],'plus-symbol':[85,160,40,40],'save':[34,899,77,27]}
ids={'mode':['keyboard-mode','function-IPA','function-拼音'],'output':['output-mode'],'composition':['composition-code'],'word-ipa':["ipa-快乐-kuai'le","ipa-好-hao"],'word-hanzi':["hanzi-快乐-kuai'le"],'quick-ipa':["ipa-快了-kuai'le"],'quick-reading-4':['reading-kuai4'],'reading-4':['reading-hao4'],'symbol-choice':['symbol-choice-g-1'],'tone-first':['hanzi-˨˩˦-'],'plus':['keyboard-plus'],'plus-symbol':['plus-ipa-pulmonic-001'],'rule':['transcription-rules','rule-beijing','Standard Chinese Beijing 宽'],'digit-1':['digit-1'],'digit-2':['digit-2'],'digit-4':['digit-4'],'key-g':['key-g']}
meta_path=root/'capture-frames.json';frames=json.loads(meta_path.read_text(encoding='utf-8')) if meta_path.exists() else {}
assets=destination/'visual-v1';assets.mkdir(parents=True,exist_ok=True)
def brect(b):return [b[0],b[1],b[2]-b[0],b[3]-b[1]]
for platform in ['windows','macos','android','ios']:
    phone=platform in ['android','ios'];chapters=mod.lessons(phone,platform=='macos');data['lessons'][platform]=chapters;data['images'][platform]={}
    wanted={step['image'] for chapter in chapters for step in chapter['steps']}
    for name in wanted:
        source_name=name
        if source_name not in images[platform]:raise ValueError('Missing screenshot: '+platform+'/'+source_name)
        record=dict(images[platform][source_name]);w,h=record['w'],record['h'];shutil.copy2(root/'screens'/record['file'],assets/record['file']);record['url']='visual-v1/'+record['file']
        targets=[]
        for chapter in chapters:
            for step in chapter['steps']:
                if step['image']==name:
                    for t in step['targets']:
                        if t not in targets:targets.append(t)
        marks={};capture=frames.get(platform,{}).get(source_name,{});boxes=capture.get('boxes',{});scale=w/capture.get('width',w)
        for target in targets:
            if platform=='windows':b=manual_windows.get(target)
            elif platform=='macos':b=manual_mac.get(target)
            elif target=='result':b=[w*.04,h*(.25 if platform=='ios' else .17),w*.88,h*.065]
            else:
                raw=next((boxes[k] for k in ids.get(target,[target]) if k in boxes),None)
                b=[n*scale for n in brect(raw)] if raw else None
            if b:marks[target]=[round(n,1) for n in b]
        record['marks']=marks
        if phone:
            if name.endswith('result'):crop=[0,int(h*.21 if platform=='ios' else h*.14),w,int(h*.22)]
            elif name in ['plus-chart','ipa-picker']:
                close=boxes.get('plus-close' if name=='plus-chart' else 'symbol-picker-close')
                top=max(0,int(close[1]*scale-30)) if close else max(0,int(min((b[1] for b in marks.values()),default=h*.66)-220))
                crop=[0,top,w,h-top]
            elif name=='rules':
                top=max(0,int(min((b[1] for b in marks.values()),default=h*.5)-100));crop=[0,top,w,h-top]
            else:
                cy=min((b[1] for b in marks.values() if b[1]>h*.45),default=h*.56);top=max(int(h*.45),int(cy-70));crop=[0,top,w,h-top]
        elif source_name=='settings':crop=[0,880,w,min(h-880,780)] if platform=='windows' else [15,390,1150,560]
        elif source_name=='plus-chart':crop=[0,0,w,min(h,950)]
        elif name.endswith('result'):crop=[25,105,710,160] if platform=='windows' else [20,30,720,185]
        elif name in ['start','hanzi-mode','ipa-mode','rule-button']:crop=[20,18,500,65] if platform=='windows' else [18,210,725,75]
        else:crop=[65,295,440,370 if source_name=='readings' or source_name.startswith('quick-') else 265] if platform=='windows' else [18,210,725,180]
        record['crop']=crop;record['caption']=('原生候选窗与控件示例。黄色编号标出操作位置。' if platform in ['windows','macos'] else '系统键盘截图，已聚焦本步操作区域。点开放大可看完整截图。')
        data['images'][platform][name]=record
font=repo/'prototype/resources/ipa/DoulosSIL-Regular.ttf'
if not font.is_file():font=repo/'apple/Generated/IPA/DoulosSIL-Regular.ttf'
if not font.is_file():font=Path('C:/Users/13680/ipa-prototype-workspace/output/runtime/data/ipa/DoulosSIL-Regular.ttf')
shutil.copy2(repo/'prototype/resources/ipa/Doulos-OFL.txt',assets/'Doulos-OFL.txt')
font_name='DoulosSIL-'+hashlib.sha256(font.read_bytes()).hexdigest()[:10]+'.ttf';shutil.copy2(font,assets/font_name)
css=(root/'style.css').read_text(encoding='utf-8').replace('src:var(--ipa-font)','src:url('+font_name+')');js=(root/'guide.js').read_text(encoding='utf-8')
css_name='style-'+hashlib.sha256(css.encode()).hexdigest()[:10]+'.css';js_name='guide-'+hashlib.sha256(js.encode()).hexdigest()[:10]+'.js'
(assets/css_name).write_text(css,encoding='utf-8');(assets/js_name).write_text(js,encoding='utf-8')
article=re.sub(r'<p[^>]*>\s*<img.*?</p>','',article,flags=re.S)
article=re.sub(r'<p><strong>上图展示候选窗布局。</strong>.*?</p>','',article,flags=re.S)
page='''<!doctype html><html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><link rel="icon" type="image/png" href="assets/icons/ipa-bigfish-v1.png"><title>国际音标输入法 · 一步一图使用教程</title><meta name="description" content="选择 Windows、macOS、Android 或 iOS，跟着标注截图学习拼音输入音标、多音字选读音、Caps Lock、Shift、声调和 IPA Plus。"><link rel="stylesheet" href="visual-v1/CSS"></head><body><div class="shell"><header class="topbar"><a class="brand" href="/">PhoneticToolbox</a><small>国际音标输入法 · 使用教程</small><a class="back" href="/#ipa-ime-download">返回下载 ↗</a></header><section class="hero"><div><div class="eyebrow">PHONETIC IPA / FOLLOW ALONG</div><h1>跟着截图，<br>把第一个<em>音标打出来。</em></h1><p>选好平台，一次跟着做一步。黄色标记告诉你点哪里，按键卡片告诉你按什么，结果也放在旁边。</p></div><div class="hero-art"><img src="assets/icons/ipa-bigfish-v1.png" alt="大肥鱼抱着 ə 音标键帽"><span>从「快乐」开始练习</span></div></section><section class="platform-bar" aria-label="选择教程平台"><div class="platforms" role="tablist" aria-label="平台"><button type="button" role="tab" data-platform="windows">Windows</button><button type="button" role="tab" data-platform="macos">macOS</button><button type="button" role="tab" data-platform="android">Android</button><button type="button" role="tab" data-platform="ios">iOS</button></div><p id="platform-note" class="platform-note"></p></section><main class="workspace"><nav id="chapters" class="chapters" aria-label="教程章节"></nav><section class="lesson" aria-label="分步截图教程"><header class="chapter-heading"><div id="chapter-mark" class="chapter-mark"></div><div><h2 id="chapter-title"></h2><p id="chapter-note"></p></div></header><div id="step-tabs" class="step-tabs" aria-label="当前章节步骤"></div><article class="step-card"><div class="card-top"><b id="platform-label"></b><span id="step-count"></span></div><div class="card-body"><div class="picture-panel"><div id="picture" class="picture"></div><p id="picture-caption" class="picture-caption"></p></div><div class="instructions" aria-live="polite"><div class="step-index">DO THIS / 跟着做</div><h3 id="step-title"></h3><p id="action" class="action"></p><div id="keys" class="keys" aria-label="本步按键或点击顺序"></div><div class="watch"><b>接下来会看到</b><span id="watch"></span></div><div id="result" class="result" hidden><small>输入到正文的结果</small><strong id="result-text"></strong></div><p id="tip" class="tip"></p></div></div></article><div class="lesson-controls"><button type="button" id="previous">← 上一步</button><div class="progress"><div class="progress-track"><i id="progress-fill"></i></div><span id="progress-text"></span></div><button type="button" id="next">下一步 →</button></div><noscript><p>分步图解需要开启 JavaScript。也可以展开下方的完整文字教程。</p></noscript></section></main><details class="reference" id="fulltext"><summary>展开完整文字教程、安装说明与常见问题</summary><article class="text-article">ARTICLE</article></details><footer>基于水杉输入法开发，感谢原作者与贡献者。 · <a href="https://github.com/Nephelium/phonetic-ipa-ime">开源项目</a> · <a href="/#ipa-ime-download">下载输入法</a> · <a href="visual-v1/Doulos-OFL.txt">字体许可</a></footer></div><dialog class="zoom" id="zoom"><div class="zoom-top"><span id="zoom-title"></span><button type="button" id="zoom-close">关闭 ✕</button></div><div id="zoom-picture"></div></dialog><script type="application/json" id="guide-data">DATA</script><script src="visual-v1/JS" defer></script></body></html>'''
page=page.replace('visual-v1/CSS','visual-v1/'+css_name).replace('visual-v1/JS','visual-v1/'+js_name).replace('ARTICLE',article).replace('>DATA<','>'+json.dumps(data,ensure_ascii=False).replace('</','<\\/')+'<')
(destination/'index.html').write_text(page,encoding='utf-8')
print('Visual tutorial generated with',sum(len(v) for v in data['images'].values()),'platform-specific screenshot states')
