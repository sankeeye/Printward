// The web control single-page app, as a C++ raw string literal. Included by
// webctl.cpp as the initializer of PAGE_HTML. Kept in its own file so the HTML/
// CSS/JS is editable on its own. Self-contained: inline CSS + vanilla JS, dark,
// mobile-first. Mirrors the tablet: Dashboard, Filament, Files, Move, Settings.
R"PAGE(<!doctype html>
<html lang="nl"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>PandaTouch</title>
<style>
:root{--bg:#0e1216;--panel:#1c2229;--panel2:#161b21;--btn:#2c3e50;--txt:#eceff2;--muted:#93a0ad;--accent:#3465a4}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{margin:0;background:var(--bg);color:var(--txt);font-family:system-ui,Segoe UI,Roboto,Arial,sans-serif}
header{display:flex;justify-content:space-between;align-items:center;padding:12px 16px;border-bottom:1px solid #262c33}
header .t{font-size:19px;font-weight:600}
#conn{font-size:14px}
nav{display:flex;gap:6px;padding:8px 10px;overflow-x:auto;border-bottom:1px solid #262c33;position:sticky;top:0;background:var(--bg);z-index:5}
nav button{flex:0 0 auto;background:var(--panel);color:var(--muted);border:0;border-radius:8px;padding:9px 15px;font-size:14px;cursor:pointer}
nav button.on{background:var(--accent);color:#fff}
section{display:none;padding:16px;max-width:820px;margin:0 auto}
section.on{display:block}
.card{background:var(--panel);border-radius:12px;padding:14px;margin-bottom:14px}
.muted{color:var(--muted)}
#state{font-size:24px;font-weight:600}
#task{margin-top:2px;font-size:14px}
.bar{height:16px;background:#333b44;border-radius:8px;overflow:hidden;margin:12px 0 6px}
#fill{height:100%;width:0;background:#2ecc71;transition:width .5s}
.temps{display:flex;flex-wrap:wrap;gap:8px 22px;font-size:16px}
.temps b{color:var(--txt)}
.ctrls{display:flex;flex-wrap:wrap;gap:10px;margin-top:14px}
.ctrls button,.ctrls select{flex:1 1 120px;min-height:52px;background:var(--btn);color:#fff;border:0;border-radius:10px;font-size:16px;cursor:pointer}
.b-blue{background:#3465a4!important}.b-red{background:#a40000!important}
.ams{display:flex;flex-wrap:wrap;gap:10px}
.amsbox{background:var(--panel2);border-radius:10px;padding:10px;min-width:150px}
.trays{display:flex;gap:8px;margin-top:8px}
.tray{text-align:center;font-size:12px;min-width:52px}
.sw{width:46px;height:30px;border-radius:6px;border:1px solid #555;margin:0 auto 4px}
.sw.empty{background:#333;opacity:.5}
h3{margin:0 0 10px;font-size:15px;color:var(--muted);font-weight:600}
/* move */
.step{display:flex;gap:8px;align-items:center;color:var(--muted);margin-bottom:14px}
.step button{width:56px;height:38px;font-size:15px;background:#333;color:#fff;border:0;border-radius:8px;cursor:pointer}
.step button.sel{background:var(--accent)}
.movewrap{display:flex;flex-wrap:wrap;gap:16px}
.pad{display:grid;grid-template-columns:repeat(3,74px);grid-template-rows:repeat(3,74px);gap:8px}
.pad button,.zcol button,.ecol button{background:var(--btn);color:#fff;border:0;border-radius:10px;font-size:18px;cursor:pointer}
.pad button{width:74px;height:74px}
.pad .home{background:#c0392b;font-size:24px}
.zcol{display:flex;flex-direction:column;gap:10px}
.zcol button{width:100px;height:92px}
.ecol{display:flex;flex-direction:column;gap:10px;min-width:180px}
.ecol .big{height:56px}
.ext{background:#27ae60!important}.ret{background:#d35400!important}
.heat{display:flex;gap:8px}.heat button{flex:1;height:46px;font-size:15px}
.ph{background:#e67e22!important}.cl{background:#2980b9!important}
#hint{min-height:20px;margin-top:12px;color:var(--muted);font-size:14px}
/* files */
.fbar{display:flex;gap:10px;align-items:center;margin-bottom:12px}
.fbar button{background:var(--panel);color:#fff;border:0;border-radius:8px;padding:8px 12px;cursor:pointer}
#fpath{color:var(--muted);word-break:break-all}
.fitem{display:flex;align-items:center;gap:10px;justify-content:space-between;background:var(--panel);border-radius:8px;padding:10px 12px;margin-bottom:6px;cursor:pointer}
.fitem[data-dir]{cursor:pointer}
.fstart{background:#27ae60;color:#fff;border:0;border-radius:6px;padding:6px 12px;cursor:pointer}
/* settings */
.frow{display:flex;flex-direction:column;gap:4px;margin-bottom:12px}
.frow input[type=text],.frow input[type=password]{padding:11px;border-radius:8px;border:1px solid #333b44;background:var(--panel2);color:#fff;font-size:16px}
.frow input[type=range]{width:100%}
#cView{background:var(--btn);color:#fff;border:0;border-radius:8px;padding:11px;font-size:16px;cursor:pointer}
#cSave{background:#27ae60;color:#fff;border:0;border-radius:10px;padding:13px;font-size:17px;width:100%;cursor:pointer;margin-top:6px}
#cMsg{margin-top:10px;color:var(--muted)}
</style></head><body>
<header><div class="t">PandaTouch</div><div id="conn" class="muted">verbinden…</div></header>
<nav>
 <button data-tab="dash" class="on">Dashboard</button>
 <button data-tab="fil">Filament</button>
 <button data-tab="files">Files</button>
 <button data-tab="move">Move</button>
 <button data-tab="scale">Scale</button>
 <button data-tab="set">Settings</button>
</nav>

<section id="dash" class="on">
 <div class="card"><div id="state">–</div><div id="task" class="muted"></div>
  <div class="bar"><div id="fill"></div></div><div id="prog" class="muted"></div></div>
 <div class="card temps"><div>Nozzle <b id="noz">–</b></div><div>Bed <b id="bed">–</b></div><div>Kamer <b id="cham">–</b></div></div>
 <div class="card"><h3>Bediening</h3><div class="ctrls">
   <button id="bPause" class="b-blue">Pause</button>
   <button id="bStop" class="b-red">Stop</button>
   <button id="bLight">Licht</button>
   <button id="bFan">Fan</button>
   <select id="speed"><option value="1">Silent</option><option value="2">Standard</option><option value="3">Sport</option><option value="4">Ludicrous</option></select>
 </div></div>
 <div class="card"><h3>AMS</h3><div id="amsStrip" class="ams"></div></div>
</section>

<section id="fil"><div class="card"><h3>Filament / AMS</h3><div id="amsDetail" class="ams"></div></div></section>

<section id="files">
 <div class="fbar"><button id="fUp">⬆ Up</button><span id="fpath">/</span><button id="fRef">↻</button></div>
 <div id="flist" class="muted">…</div>
</section>

<section id="move">
 <div class="step">Stap:
  <button data-s="0.1">0.1</button><button data-s="1" class="sel">1</button><button data-s="10">10</button> mm</div>
 <div class="movewrap">
  <div class="card"><h3>X / Y</h3><div class="pad">
   <span></span><button data-a="yp">Y+</button><span></span>
   <button data-a="xm">X-</button><button class="home" data-a="home">&#8962;</button><button data-a="xp">X+</button>
   <span></span><button data-a="ym">Y-</button><span></span>
  </div></div>
  <div class="card"><h3>Z</h3><div class="zcol"><button data-a="zp">Z+</button><button data-a="zm">Z-</button></div></div>
  <div class="card"><h3 id="movenoz">Extruder</h3><div class="ecol">
   <button class="ext big" data-a="ext">Extrude</button>
   <button class="ret big" data-a="ret">Retract</button>
   <div class="heat"><button class="ph" data-a="preheat">Preheat</button><button class="cl" data-a="cool">Cooldown</button></div>
  </div></div>
 </div>
 <div id="hint"></div>
</section>

<section id="scale">
 <div class="card" style="text-align:center"><div id="swt" style="font-size:52px;font-weight:700">– g</div><div id="sst" class="muted">…</div></div>
 <div class="card"><h3>Kalibratie</h3>
  <button id="sTare">Tarra (nulstellen)</button>
  <input type="number" id="sKnown" value="500" style="width:110px">
  <button id="sCal" class="ext">Kalibreer</button>
  <div id="sMsg" class="muted" style="margin-top:8px"></div>
 </div>
 <div class="card"><h3>Netwerk</h3><div id="sInfo" class="muted" style="margin-bottom:10px">…</div>
  <div class="frow"><label class="muted">Scale IP (waar de tablet de schaal zoekt)</label><input type="text" id="sIp"></div>
  <div style="display:flex;gap:8px;flex-wrap:wrap"><button id="sIpSave">Opslaan op tablet</button><button id="sIpFix">Vast IP op schaal</button></div>
  <div class="frow" style="margin-top:12px"><label class="muted">WiFi van de schaal wijzigen</label><input type="text" id="sSsid" placeholder="WiFi naam"><input type="password" id="sPass" placeholder="wachtwoord"></div>
  <button id="sWifi" class="ph">WiFi wijzigen</button>
 </div>
</section>

<section id="set">
 <div class="card"><h3>Printer</h3>
  <div class="frow"><label class="muted">Printer IP</label><input type="text" id="cIp"></div>
  <div class="frow"><label class="muted">Serial</label><input type="text" id="cSerial"></div>
  <div class="frow"><label class="muted">Access code</label><input type="password" id="cCode" placeholder="laat leeg = ongewijzigd"></div>
  <button id="cView">Screensaver: 2D</button>
  <div class="frow" style="margin-top:12px"><label class="muted">Helderheid</label><input type="range" id="cBri" min="5" max="100"></div>
  <button id="cSave">Opslaan &amp; verbinden</button>
  <div id="cMsg"></div>
 </div>
</section>

<script>
var step="1",curState="",curLight=false,curFan=0,cfgFilled=false,curPath="/",scaleHost="",lowG=100;
function $(id){return document.getElementById(id);}
function tab(n){
 document.querySelectorAll('nav button').forEach(function(b){b.classList.toggle('on',b.dataset.tab===n);});
 document.querySelectorAll('section').forEach(function(s){s.classList.toggle('on',s.id===n);});
 if(n==='files')loadFiles(curPath);
}
document.querySelectorAll('nav button').forEach(function(b){b.onclick=function(){tab(b.dataset.tab);};});
document.querySelectorAll('.step button').forEach(function(b){b.onclick=function(){step=b.dataset.s;
 document.querySelectorAll('.step button').forEach(function(x){x.classList.remove('sel');});b.classList.add('sel');};});
document.querySelectorAll('.pad button,.zcol button,.ecol button').forEach(function(b){b.onclick=function(){
 fetch('/cmd?a='+b.dataset.a+'&s='+step).then(function(r){return r.text();}).then(function(t){$('hint').textContent=t;}).catch(function(){});};});
$('bPause').onclick=function(){fetch('/ctl?a='+(curState==='PAUSE'?'resume':'pause'));};
$('bStop').onclick=function(){if(confirm('Print stoppen?'))fetch('/ctl?a=stop');};
$('bLight').onclick=function(){fetch('/ctl?a=light&v='+(curLight?0:1));};
$('bFan').onclick=function(){fetch('/ctl?a=fan&v='+(curFan>0?0:1));};
$('speed').onchange=function(){fetch('/ctl?a=speed&v='+$('speed').value);};
$('cView').onclick=function(){var b=$('cView');b.dataset.v=(b.dataset.v==='1'?'0':'1');b.textContent='Screensaver: '+(b.dataset.v==='1'?'3D':'2D');};
$('cSave').onclick=function(){
 var q='ip='+encodeURIComponent($('cIp').value)+'&serial='+encodeURIComponent($('cSerial').value)+'&view3d='+($('cView').dataset.v||'0')+'&bri='+$('cBri').value;
 if($('cCode').value)q+='&code='+encodeURIComponent($('cCode').value);
 fetch('/setcfg?'+q).then(function(r){return r.text();}).then(function(t){$('cMsg').textContent=t+' – verbinden…';$('cCode').value='';});
};
function joinPath(b,n){return (b==='/'?'':b)+'/'+n;}
function fmtSize(b){return b>1048576?(b/1048576).toFixed(1)+' MB':b>1024?(b/1024).toFixed(0)+' KB':b+' B';}
function loadFiles(path){curPath=path;$('fpath').textContent=path;$('flist').innerHTML='<div class=muted>laden…</div>';
 fetch('/files?path='+encodeURIComponent(path)).then(function(r){return r.json();}).then(function(d){
  if(!d.ok){$('flist').innerHTML='<div class=muted>fout: '+(d.err||'')+'</div>';return;}
  var h='';
  d.items.forEach(function(it){
   if(it.dir)h+='<div class="fitem" data-dir="'+it.name+'"><b>📁 '+it.name+'</b><span></span></div>';
   else h+='<div class="fitem"><span>📄 '+it.name+'</span><span class=muted>'+fmtSize(it.size)+'</span><button class="fstart" data-f="'+it.name+'">Start</button></div>';
  });
  $('flist').innerHTML=h||'<div class=muted>leeg</div>';
  document.querySelectorAll('.fitem[data-dir]').forEach(function(el){el.onclick=function(){loadFiles(joinPath(path,el.dataset.dir));};});
  document.querySelectorAll('.fstart').forEach(function(el){el.onclick=function(e){e.stopPropagation();
   if(confirm('Print starten: '+el.dataset.f+' ?\n(kan door printer-firmware geweigerd worden)'))
    fetch('/start?path='+encodeURIComponent(joinPath(path,el.dataset.f))).then(function(r){return r.text();}).then(function(t){alert(t);});};});
 }).catch(function(){$('flist').innerHTML='<div class=muted>geen verbinding</div>';});
}
$('fUp').onclick=function(){var p=curPath.replace(/\/[^\/]*\/?$/,'');loadFiles(p||'/');};
$('fRef').onclick=function(){loadFiles(curPath);};
function amt(t){
 if(t.gram>=0){return '<div class="muted"'+(t.gram<lowG?' style="color:#e74c3c;font-weight:600"':'')+'>'+t.gram+' g</div>';}
 return '<div class="muted">'+(t.remain>=0?t.remain+'%':'–')+'</div>';
}
function amsHtml(ams,ext){
 var h='';
 (ams||[]).forEach(function(u){
  h+='<div class="amsbox"><div class="muted">AMS '+u.id+(u.humidity>0?(' · vocht '+u.humidity):'')+'</div><div class="trays">';
  u.trays.forEach(function(t){
   if(t.present)h+='<div class="tray"><div class="sw" style="background:'+t.rgb+'"></div><div>'+t.type+'</div>'+amt(t)+'</div>';
   else h+='<div class="tray"><div class="sw empty"></div><div class="muted">leeg</div><div></div></div>';
  });
  h+='</div></div>';
 });
 if(ext&&ext.present)h+='<div class="amsbox"><div class="muted">Externe spoel</div><div class="trays"><div class="tray"><div class="sw" style="background:'+ext.rgb+'"></div><div>'+ext.type+'</div>'+amt(ext)+'</div></div></div>';
 return h||'<div class="muted">geen AMS</div>';
}
function poll(){
 fetch('/status').then(function(r){return r.json();}).then(function(s){
  $('conn').textContent=s.conn?'● verbonden':'○ offline';$('conn').style.color=s.conn?'#2ecc71':'#e74c3c';
  curState=s.state;curLight=s.light;curFan=s.fan;
  $('state').textContent=s.state||'–';$('task').textContent=s.name||'';
  $('fill').style.width=(s.pct||0)+'%';
  $('prog').textContent=(s.pct||0)+'%'+(s.total?('  laag '+s.layer+'/'+s.total):'')+(s.remain?('  ~'+s.remain+' min'):'');
  $('noz').textContent=s.nozzle+'/'+s.nozzle_t+'°';$('bed').textContent=s.bed+'/'+s.bed_t+'°';$('cham').textContent=s.chamber+'°';
  $('bPause').textContent=(s.state==='PAUSE')?'Resume':'Pause';
  $('bLight').textContent='Licht: '+(s.light?'aan':'uit');
  $('bFan').textContent='Fan '+s.fan+'%';
  if(document.activeElement!==$('speed'))$('speed').value=s.speed;
  var a=amsHtml(s.ams,s.ext);$('amsStrip').innerHTML=a;$('amsDetail').innerHTML=a;
  $('movenoz').textContent='Nozzle '+s.nozzle+'/'+s.nozzle_t+'°C';
  if(s.cfg&&s.cfg.scale_ip){scaleHost=s.cfg.scale_ip;var si=$('sIp');if(si&&!si.value)si.value=s.cfg.scale_ip;}
  if(s.cfg&&s.cfg.low)lowG=s.cfg.low;
  if(!cfgFilled&&s.cfg){cfgFilled=true;
   $('cIp').value=s.cfg.ip;$('cSerial').value=s.cfg.serial;$('cBri').value=s.cfg.bri;
   var b=$('cView');b.dataset.v=s.cfg.view3d?'1':'0';b.textContent='Screensaver: '+(s.cfg.view3d?'3D':'2D');
   if(s.cfg.code_set)$('cCode').placeholder='•••••••• (ingesteld, laat leeg = ongewijzigd)';
  }
 }).catch(function(){$('conn').textContent='○ geen tablet';$('conn').style.color='#e74c3c';});
}
setInterval(poll,1500);poll();
function sMsg(t){$('sMsg').textContent=t;}
function scalePoll(){
 if(!$('scale').classList.contains('on')||!scaleHost)return;
 fetch('http://'+scaleHost+'/weight').then(function(r){return r.json();}).then(function(d){
  $('swt').textContent=d.g.toFixed(0)+' g';$('sst').textContent=d.stable?'stabiel':'…meten';
 }).catch(function(){$('sst').textContent='geen verbinding met schaal';});
 fetch('http://'+scaleHost+'/info').then(function(r){return r.json();}).then(function(d){
  $('sInfo').textContent=(d.connected?'verbonden':'AP-modus')+' · '+d.ip+' · '+d.ssid+' · '+d.rssi+'dBm'+(d.static?' · vast IP':'');
 }).catch(function(){});
}
function sGet(p){fetch('http://'+scaleHost+p).then(function(r){return r.text();}).then(sMsg).catch(function(){sMsg('geen verbinding met schaal');});}
$('sTare').onclick=function(){sGet('/tare');};
$('sCal').onclick=function(){sGet('/cal?known='+$('sKnown').value);};
$('sIpFix').onclick=function(){sGet('/setip?ip='+encodeURIComponent($('sIp').value));};
$('sWifi').onclick=function(){sGet('/setwifi?ssid='+encodeURIComponent($('sSsid').value)+'&pass='+encodeURIComponent($('sPass').value));};
$('sIpSave').onclick=function(){var v=$('sIp').value;fetch('/setcfg?scale_ip='+encodeURIComponent(v)).then(function(){scaleHost=v;sMsg('opgeslagen op tablet');});};
setInterval(scalePoll,1200);
</script></body></html>)PAGE"
