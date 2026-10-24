"""Render this repository's illustrated README as a self-contained static guide.

This deliberately supports only the Markdown constructs used in README.md.
No remote content is fetched and no executable HTML is accepted from users.
"""
from pathlib import Path
import html, re, shutil, sys

repo=Path(__file__).resolve().parents[1]
destination=Path(sys.argv[1]).resolve()
destination.mkdir(parents=True,exist_ok=True)
source=(repo/'README.md').read_text(encoding='utf-8')
base='https://github.com/Nephelium/phonetic-ipa-ime/blob/ipa-v0.1.6/'
def inline(text):
    text=html.escape(text)
    text=re.sub(r'`([^`]+)`',r'<code>\1</code>',text)
    text=re.sub(r'\*\*([^*]+)\*\*',r'<strong>\1</strong>',text)
    return re.sub(r'\[([^\]]+)\]\(([^)]+)\)',lambda m:'<a href="'+(m[2] if m[2].startswith(('https://','#')) else base+m[2])+'">'+m[1]+'</a>',text)
body=[]; toc=[]; lines=source.splitlines(); i=1
while i<len(lines):
    line=lines[i].strip()
    if not line:i+=1;continue
    if line.startswith('## '):
        title=line[3:]; anchor='step-'+str(len(toc)+1);toc.append((anchor,title))
        body.append(f'<h2 id="{anchor}">{html.escape(title)}</h2>');i+=1
    elif line.startswith('|'):
        rows=[]
        while i<len(lines) and lines[i].strip().startswith('|'):
            row=lines[i].strip().strip('|').split('|');i+=1
            if all(re.fullmatch(r'[\s:-]+',c) for c in row):continue
            tag='th' if not rows else 'td'
            rows.append('<tr>'+''.join(f'<{tag}>'+inline(c.strip())+f'</{tag}>' for c in row)+'</tr>')
        body.append('<div class="table-wrap" tabindex="0" role="region" aria-label="操作对照表，可左右滚动"><table><thead>'+rows[0]+'</thead><tbody>'+''.join(rows[1:])+'</tbody></table></div>')
    elif re.match(r'(?:\d+\. |\- )',line):
        tag='ol' if line[0].isdigit() else 'ul';items=[]
        while i<len(lines) and re.match(r'(?:\d+\. |\- )',lines[i].strip()):
            items.append('<li>'+inline(re.sub(r'^(?:\d+\. |\- )','',lines[i].strip()))+'</li>');i+=1
        body.append('<'+tag+'>'+''.join(items)+'</'+tag+'>')
    elif line.startswith('<'):
        raw=line.replace('src="docs/images/','src="images/').replace('src="assets/','src="assets/')
        body.append(raw);i+=1
    else:
        paragraph=[line];i+=1
        while i<len(lines) and lines[i].strip() and not lines[i].lstrip().startswith(('#','|','<')):
            paragraph.append(lines[i].strip());i+=1
        body.append('<p>'+inline(' '.join(paragraph))+'</p>')
css='''
:root{--cream:#fffaf0;--paper:#fffef9;--ink:#35213e;--muted:#7d6c87;--line:#e6daca;--lilac:#eee2fa;--yellow:#ffe75c;scroll-behavior:smooth}
*{box-sizing:border-box}body{margin:0;background:var(--cream);color:var(--ink);font-family:"Microsoft YaHei","PingFang SC",sans-serif;font-size:16px;line-height:1.9}a{color:#79539b;text-underline-offset:4px}a:hover{color:#35213e}a:focus-visible,summary:focus-visible,.table-wrap:focus-visible{outline:3px solid #b697d5;outline-offset:4px}header{max-width:1180px;margin:28px auto 0;padding:20px 28px;border:1px solid var(--line);border-radius:26px;background:var(--paper);box-shadow:0 6px 0 #ede6d6;display:flex;justify-content:space-between;align-items:center;gap:16px}header a{text-decoration:none;font-weight:700}header .home{font-size:22px}header small{color:var(--muted)}.masthead{max-width:1180px;margin:64px auto 38px;padding:0 24px}.eyebrow{letter-spacing:.12em;font-size:13px;color:var(--muted)}h1{font-size:clamp(30px,4vw,52px);line-height:1.35;margin:16px 0 22px;letter-spacing:-.025em}.lead{max-width:740px;font-size:18px}.actions{display:flex;gap:14px;flex-wrap:wrap}.button{display:inline-block;border:2px solid var(--ink);border-radius:24px;padding:9px 22px;background:var(--yellow);color:var(--ink);box-shadow:0 4px 0 var(--ink);font-weight:700;text-decoration:none}.button.secondary{background:var(--lilac)}.layout{max-width:1180px;margin:0 auto;padding:0 24px 70px;display:grid;grid-template-columns:215px minmax(0,1fr);gap:38px}nav{position:sticky;top:24px;align-self:start;font-size:14px}nav a{display:block;padding:6px 0;text-decoration:none}article{min-width:0;background:var(--paper);border:1px solid var(--line);border-radius:28px;padding:30px 36px;box-shadow:0 8px 0 #eee7da}h2{font-size:25px;line-height:1.5;margin:44px 0 20px;scroll-margin-top:24px;border-top:1px dashed #dcc9ae;padding-top:28px}h2:first-of-type{border-top:0;margin-top:26px}p{margin:0 0 20px}strong{font-weight:700}code{font-family:"Cascadia Code",Consolas,monospace;background:#f2eaf8;border-radius:5px;padding:2px 5px;overflow-wrap:anywhere}li{margin:10px 0}.table-wrap{overflow-x:auto;max-width:100%;margin:22px 0;border:1px solid var(--line);border-radius:14px}table{border-collapse:collapse;width:100%;font-size:14px;min-width:560px}th,td{text-align:left;vertical-align:top;padding:13px 15px;border-bottom:1px solid var(--line);border-right:1px solid var(--line)}th{background:var(--lilac);font-weight:700}tr:last-child td{border-bottom:0}td:last-child,th:last-child{border-right:0}img{max-width:100%;height:auto;vertical-align:top;border-radius:16px;margin:6px 6px 12px 0;border:1px solid #eadfec}details{border:1px solid var(--line);border-radius:16px;padding:14px 18px;margin:24px 0}summary{cursor:pointer;font-weight:700}details p{margin-top:18px}footer{max-width:1180px;margin:0 auto;padding:24px;color:var(--muted);border-top:1px dashed #d6c4a3;font-size:14px}@media(max-width:900px){.layout{grid-template-columns:1fr;gap:24px}nav{position:static;display:flex;flex-wrap:wrap;gap:7px 18px;padding:16px;border-radius:18px;background:#f1e8f7}nav a{padding:0}.masthead{margin-top:44px}article{padding:22px}}@media(max-width:500px){body{font-size:15px}header{margin:14px 14px 0;padding:16px 18px;border-radius:20px}header .home{font-size:18px}header small{display:none}.layout,.masthead{padding-left:16px;padding-right:16px}article{padding:18px;border-radius:22px}h2{font-size:22px}.lead{font-size:16px}img{max-width:100%;margin-right:0}}
'''
page='''<!doctype html><html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>国际音标输入法 · 新手图文教程 | PhoneticToolbox</title><meta name="description" content="国际音标输入法四端安装与使用：Shift 切换拼音与直接音标，Caps Lock 切换音标和汉字，两次数字选读音，输入声调与 IPA Plus。"><style>'''+css+'''</style></head><body id="top"><header><a class="home" href="/">PhoneticToolbox</a><small>国际音标输入法 · 使用指南</small><a href="/#ipa-ime-download">返回下载 ↗</a></header><div class="masthead"><div class="eyebrow">PHONETIC IPA · GET STARTED</div><h1>从第一个音标开始。</h1><p class="lead">先看当前状态，再按步骤试一次。拼音查字、单个音标和声调，都可以在这页找到对应的操作。</p><div class="actions"><a class="button" href="#step-1">先认清两个开关 ↓</a><a class="button secondary" href="/#ipa-ime-download">选择平台下载 ↗</a></div></div><main class="layout"><nav aria-label="教程目录">'''+''.join(f'<a href="#{a}">{html.escape(t)}</a>' for a,t in toc)+'''</nav><article>'''+''.join(body)+'''</article></main><footer>基于水杉输入法开发，感谢原作者与贡献者。 · <a href="https://github.com/Nephelium/phonetic-ipa-ime">开源项目</a> · <a href="#top">回到顶部 ↑</a></footer></body></html>'''
(destination/'index.html').write_text(page,encoding='utf-8')
for directory in ['docs/images/0.1.4','docs/images/0.1.6','assets/icons']:
    for p in (repo/directory).rglob('*'):
        if p.is_file() and (directory!='assets/icons' or p.name=='ipa-bigfish-v1.png'):
            dest=destination/(p.relative_to(repo/'docs') if directory.startswith('docs/') else p.relative_to(repo))
            dest.parent.mkdir(exist_ok=True,parents=True);shutil.copy2(p,dest)
print('Static illustrated guide generated:',destination)
