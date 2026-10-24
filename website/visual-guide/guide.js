(() => {
 const data=JSON.parse(document.querySelector('#guide-data').textContent), names={windows:'Windows',macos:'macOS',android:'Android',ios:'iOS'};
 const $=s=>document.querySelector(s), esc=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
 let platform=new URLSearchParams(location.search).get('platform');if(!names[platform])platform=/iPhone|iPad/.test(navigator.userAgent)?'ios':/Android/.test(navigator.userAgent)?'android':/Mac/.test(navigator.platform)?'macos':'windows';
 let chapter=0,index=0,activePhoto=null;
 function saveHash(){history.replaceState(null,'',location.pathname+'?platform='+platform+'#'+data.lessons[platform][chapter].id)}
 function move(n){const chapters=data.lessons[platform];index+=n;if(index>=chapters[chapter].steps.length){if(chapter<chapters.length-1){chapter++;index=0}else index--}else if(index<0){if(chapter>0){chapter--;index=chapters[chapter].steps.length-1}else index=0}render();saveHash()}
 function imageMarkup(photo,full=false){const [x,y,w,h]=full?[0,0,photo.w,photo.h]:photo.crop;const marks=photo.activeMarks.map((b,i)=>{
   const [bx,by,bw,bh]=b;const r=Math.max(14,photo.w/60),cx=Math.min(x+w-r-5,bx+bw+r+8),cy=by+bh/2;
   return `<rect class="target" x="${bx}" y="${by}" width="${bw}" height="${bh}" rx="${Math.min(10,bh/4)}"/><circle class="tag" cx="${cx}" cy="${cy}" r="${r}"/><text class="tag-text" x="${cx}" y="${cy}" style="font-size:${r*1.25}px">${i+1}</text>`;
 }).join('');return `<svg viewBox="${x} ${y} ${w} ${h}" role="img" aria-label="${esc(activePhoto.title)}，黄色编号标出操作位置"><image href="${esc(photo.url)}" width="${photo.w}" height="${photo.h}"/>${marks}</svg>`}
 function render(){const list=data.lessons[platform],lesson=list[chapter],step=lesson.steps[index],photo=data.images[platform][step.image];activePhoto={...photo,title:step.title,activeMarks:[...new Set(step.targets)].filter(t=>photo.marks[t]).map(t=>photo.marks[t])};
  document.querySelectorAll('.platforms button').forEach(b=>b.setAttribute('aria-selected',String(b.dataset.platform===platform)));
  $('#platform-note').textContent=platform==='windows'?'Windows 原生候选窗与控件示例':platform==='macos'?'macOS 原生界面示例':names[platform]+' 模拟器界面截图';
  $('#chapters').innerHTML=list.map((l,i)=>`<button type="button" data-chapter="${i}" class="${i===chapter?'active':''}" aria-current="${i===chapter?'step':'false'}"><span class="num">${String(i+1).padStart(2,'0')}</span>${esc(l.name)}</button>`).join('')+'<div class="rail-note">跟着黄色编号操作。<br>每一步都可以放大查看。</div>';
  $('#chapter-title').textContent=lesson.name;$('#chapter-note').textContent=lesson.note;$('#chapter-mark').textContent=String(chapter+1).padStart(2,'0');
  $('#step-tabs').innerHTML=lesson.steps.map((s,i)=>`<button type="button" data-step="${i}" class="${i===index?'active':''}" aria-current="${i===index?'step':'false'}"><span>${i+1}</span>${esc(s.title)}</button>`).join('');
  $('#step-count').textContent=`第 ${index+1} / ${lesson.steps.length} 步`;$('#platform-label').textContent=names[platform]+' · 对照操作';
  $('#picture').innerHTML=imageMarkup(activePhoto)+'<button type="button" id="zoom-open">放大查看 ↗</button>';
  $('#step-title').textContent=step.title;$('#action').textContent=step.action;$('#watch').textContent=step.watch;
  $('#keys').innerHTML=step.keys.map(k=>'<kbd>'+esc(k)+'</kbd>').join('');$('#keys').hidden=!step.keys.length;
  $('#result').hidden=!step.result;$('#result-text').textContent=step.result;$('#tip').textContent=step.tip;$('#tip').hidden=!step.tip;
  $('#picture-caption').textContent=photo.caption||'黄色框对应本步要点的位置；按键顺序见右侧提示。';
  $('#previous').disabled=chapter===0&&index===0;$('#next').disabled=chapter===list.length-1&&index===lesson.steps.length-1;
  const total=list.reduce((n,l)=>n+l.steps.length,0),done=list.slice(0,chapter).reduce((n,l)=>n+l.steps.length,0)+index+1;$('#progress-text').textContent=done+' / '+total;$('#progress-fill').style.width=(done/total*100)+'%';
 }
 const hash=location.hash.slice(1);if(hash){const c=data.lessons[platform].findIndex(l=>l.id===hash);if(c>=0)chapter=c}
 document.querySelector('.platforms').addEventListener('click',e=>{const b=e.target.closest('button[data-platform]');if(b){platform=b.dataset.platform;index=0;render();saveHash()}});
 $('#chapters').addEventListener('click',e=>{const b=e.target.closest('[data-chapter]');if(b){chapter=Number(b.dataset.chapter);index=0;render();saveHash()}});
 $('#step-tabs').addEventListener('click',e=>{const b=e.target.closest('[data-step]');if(b){index=Number(b.dataset.step);render();saveHash()}});
 $('#previous').addEventListener('click',()=>move(-1));$('#next').addEventListener('click',()=>move(1));
 $('#picture').addEventListener('click',()=>{if(activePhoto){$('#zoom-picture').innerHTML=imageMarkup(activePhoto,true);$('#zoom-title').textContent=names[platform]+' · '+activePhoto.title;$('#zoom').showModal()}});
 $('#zoom-close').addEventListener('click',()=>$('#zoom').close());$('#zoom').addEventListener('click',e=>{if(e.target===$('#zoom'))$('#zoom').close()});
 document.addEventListener('keydown',e=>{if($('#zoom').open||e.altKey||e.ctrlKey||e.metaKey||/INPUT|TEXTAREA|SELECT/.test(e.target.tagName))return;if(e.key==='ArrowRight'){e.preventDefault();move(1)}if(e.key==='ArrowLeft'){e.preventDefault();move(-1)}});
 render();
})();
