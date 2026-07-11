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
.rolbtn{background:var(--accent);color:#fff;border:0;border-radius:6px;padding:4px 12px;font-size:12px;cursor:pointer;margin-top:5px}
.modal{display:none;position:fixed;inset:0;background:rgba(0,0,0,.6);z-index:30;align-items:center;justify-content:center;padding:16px}
.modalbox{background:var(--panel);border:2px solid var(--accent);border-radius:12px;padding:16px;width:100%;max-width:460px;max-height:82vh;overflow:auto}
.modalhead{display:flex;justify-content:space-between;align-items:center;margin-bottom:12px}
.modalhead b{font-size:17px}
.modalhead button{background:#555;color:#fff;border:0;border-radius:8px;padding:8px 14px;cursor:pointer}
.rollpick{cursor:pointer}.rollpick:active{filter:brightness(1.3)}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(120px,1fr));gap:12px}
.field{display:flex;flex-direction:column;gap:4px;min-width:0}
.field label{color:var(--muted);font-size:12px}
.field input,.field select{padding:10px;border-radius:8px;border:1px solid #333b44;background:var(--panel2);color:#fff;font-size:15px;width:100%}
.chip{width:46px;height:46px;border-radius:10px;border:2px solid #3a434d;flex:0 0 auto;cursor:pointer;padding:2px;background:none}
.swatches{display:flex;gap:7px;flex-wrap:wrap;align-items:center}
.swatch{width:30px;height:30px;border-radius:7px;border:1px solid #4a5560;cursor:pointer;padding:0}
.rollcard{display:flex;align-items:center;gap:12px;background:var(--panel2);border-radius:10px;padding:10px 12px;margin-bottom:8px}
.rollcard .name{font-weight:600}
.badge{display:inline-block;background:#2a323b;color:#cfd6dd;border-radius:20px;padding:2px 10px;font-size:12px;margin-left:8px}
.grams{margin-left:auto;font-weight:600;white-space:nowrap;font-variant-numeric:tabular-nums}
.iconbtn{background:#2a323b;color:#cfd6dd;border:0;border-radius:8px;width:40px;height:36px;font-size:16px;cursor:pointer}
.iconbtn:active{filter:brightness(1.4)}
.iconbtn.del{background:#4a2323;color:#ff9a9a}
.chiprow{display:flex;align-items:center;gap:10px;background:var(--panel2);border-radius:8px;padding:9px 12px;margin-bottom:6px}
.formbtn{border:0;border-radius:9px;padding:11px 20px;font-size:15px;cursor:pointer}
.formbtn.pri{background:#27ae60;color:#fff}.formbtn.sec{background:#3a434d;color:#eceff2}
.sprow{display:flex;gap:16px;flex-wrap:wrap;align-items:flex-start;margin-bottom:16px}
.sprow .card{margin-bottom:0}
section#spools{max-width:1040px}
</style></head><body>
<header><div class="t">PandaTouch</div><div id="conn" class="muted">verbinden…</div></header>
<nav>
 <button data-tab="dash" class="on">Dashboard</button>
 <button data-tab="fil">Filament</button>
 <button data-tab="files">Files</button>
 <button data-tab="move">Move</button>
 <button data-tab="scale">Scale</button>
 <button data-tab="spools">Spools</button>
 <button data-tab="set">Settings</button>
</nav>

<section id="dash" class="on">
 <div id="warn" style="display:none;background:#5a2020;color:#ffd0d0;border-radius:10px;padding:12px 14px;margin-bottom:14px;font-weight:600"></div>
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

<section id="spools">
 <div class="sprow">
 <div class="card" style="flex:1 1 360px"><h3 id="spTitle">Nieuwe rol</h3>
  <div class="field" style="margin-bottom:14px"><label>Naam / merk</label><input type="text" id="spName" placeholder="bv. Bambu PLA Zwart"></div>
  <div class="field" style="margin-bottom:14px"><label>Kleur</label>
   <div class="swatches"><input type="color" id="spColor" value="#22aa55" class="chip"><span id="spSw" class="swatches"></span></div>
  </div>
  <div class="field" style="margin-bottom:14px"><label>Leeg spoel</label>
   <div style="display:flex;gap:8px;align-items:center">
    <select id="spEmptySel" style="flex:1"><option value="">— kies uit bibliotheek —</option></select>
    <input type="number" id="spEmpty" value="250" style="width:100px" title="gewicht van de lege spoel in gram">
    <span class="muted">g</span>
   </div>
  </div>
  <div class="field" style="margin-bottom:14px"><label>Resterend filament (g)</label>
   <div style="display:flex;gap:8px;align-items:center;flex-wrap:wrap">
    <input type="number" id="spRem" value="1000" style="width:130px">
    <button class="formbtn sec" onclick="spRemWeigh()">⚖ Weeg rol</button>
    <span class="muted" style="font-size:12px">= gewogen − leeg-spoel</span>
   </div>
  </div>
  <div class="grid">
   <div class="field"><label>Materiaal</label><select id="spMat"><option>PLA</option><option>PETG</option><option>ABS</option><option>TPU</option><option>ASA</option><option>PC</option><option>PA</option><option>PVA</option></select></div>
   <div class="field"><label>Nozzle min</label><input type="number" id="spNmin" placeholder="auto"></div>
   <div class="field"><label>Nozzle max</label><input type="number" id="spNmax" placeholder="auto"></div>
   <div class="field"><label>Bambu-code</label><input type="text" id="spCode" placeholder="auto"></div>
   <div class="field"><label>Prijs (€/kg)</label><input type="number" id="spPrice" placeholder="0" step="0.01"></div>
  </div>
  <div class="field" style="margin-top:12px"><label>Notitie</label><input type="text" id="spNote" placeholder="bv. gedroogd 3/7"></div>
  <input type="hidden" id="spIdx" value="-1">
  <div style="display:flex;gap:10px;margin-top:16px"><button id="spSave" class="formbtn pri">Opslaan</button><button id="spNew" class="formbtn sec">Nieuw</button></div>
  <div id="spMsg" class="muted" style="margin-top:8px"></div>
 </div>
 <div class="card" style="flex:1 1 300px"><h3>Lege spoelen</h3>
  <div style="display:flex;gap:10px;align-items:flex-end;flex-wrap:wrap;margin-bottom:14px">
   <div class="field" style="flex:1;min-width:160px"><label>Naam</label><input type="text" id="emName" placeholder="bv. Bambu herbruikbaar"></div>
   <div class="field" style="width:120px"><label>Gewicht (g)</label><input type="number" id="emWeight" value="250"></div>
   <button class="formbtn sec" style="height:42px" onclick="emWeigh()">Weeg</button>
   <button id="emSave" class="formbtn pri" style="height:42px">Toevoegen</button>
  </div>
  <div id="emList"></div>
 </div>
 </div>
 <div class="card"><h3>Rollen</h3>
  <div style="display:flex;gap:8px;align-items:center;margin-bottom:10px;flex-wrap:wrap">
   <input type="text" id="spSearch" placeholder="Zoek op naam of materiaal…" oninput="renderSpList()" style="flex:1;min-width:150px;padding:9px;border-radius:8px;border:1px solid #333b44;background:var(--panel2);color:#fff;font-size:15px">
   <label class="muted" style="display:flex;align-items:center;gap:6px;cursor:pointer"><input type="checkbox" id="spAll" onchange="toggleAll(this.checked)" style="width:18px;height:18px"> alles</label>
  </div>
  <div id="spBulk" style="display:none;gap:8px;align-items:center;flex-wrap:wrap;margin-bottom:12px;background:var(--panel2);border-radius:8px;padding:9px 12px">
   <b id="spSelCount"></b>
   <button class="formbtn" style="background:#4a2323;color:#ff9a9a;padding:8px 14px" onclick="bulkDel()">Verwijder</button>
   <span style="display:flex;align-items:center;gap:5px"><input type="color" id="bulkColor" value="#22aa55" class="chip" style="width:34px;height:32px"><button class="formbtn sec" style="padding:8px 12px" onclick="bulkColorGo()">Kleur</button></span>
   <span style="display:flex;align-items:center;gap:5px"><input type="number" id="bulkWeight" placeholder="g" style="width:74px;padding:8px;border-radius:8px;border:1px solid #333b44;background:var(--bg);color:#fff"><button class="formbtn sec" style="padding:8px 12px" onclick="bulkWeightGo()">Gewicht</button></span>
   <span style="display:flex;align-items:center;gap:5px"><select id="bulkMat" style="padding:8px;border-radius:8px;border:1px solid #333b44;background:var(--bg);color:#fff"><option value="">materiaal…</option><option>PLA</option><option>PETG</option><option>ABS</option><option>TPU</option><option>ASA</option><option>PC</option><option>PA</option><option>PVA</option></select><button class="formbtn sec" style="padding:8px 12px" onclick="bulkMatGo()">Zet</button></span>
   <button class="formbtn sec" style="padding:8px 12px" onclick="clearSel()">Deselecteer</button>
  </div>
  <div id="spList" class="muted">…</div>
 </div>
 <div class="card"><h3>Back-up</h3>
  <div style="display:flex;gap:10px;flex-wrap:wrap;align-items:center">
   <button class="formbtn sec" onclick="downloadBackup()">⬇ Download back-up</button>
   <label class="formbtn sec" style="cursor:pointer">⬆ Importeer<input type="file" accept=".json,application/json" style="display:none" onchange="importBackup(this.files)"></label>
   <span class="muted" style="font-size:12px">rollen + lege spoelen als bestand (import voegt toe)</span>
  </div>
  <div id="bkMsg" class="muted" style="margin-top:8px"></div>
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
 <div class="card"><h3>Meldingen (ntfy)</h3>
  <div class="frow"><label class="muted">ntfy topic (leeg = uit)</label><input type="text" id="cNtfy" placeholder="bv. pandatouch-geheim-x9k2"></div>
  <div style="display:flex;gap:8px;flex-wrap:wrap;margin-top:8px"><button id="cNtfySave" class="formbtn pri">Opslaan</button><button id="cNtfyTest" class="formbtn sec">Test</button></div>
  <div class="muted" style="font-size:12px;margin-top:8px">Installeer de gratis <b>ntfy</b>-app (of ntfy.sh in de browser) en abonneer op dit topic. Je krijgt een melding bij print klaar/mislukt en filament tekort.</div>
  <div id="cNtfyMsg" class="muted" style="margin-top:6px"></div>
 </div>
 <div class="card"><h3>Statistieken</h3>
  <div class="muted" style="font-size:16px">Voltooide prints: <b id="stPrints">–</b></div>
  <div class="muted" style="font-size:16px;margin-top:4px">Totaal filament gebruikt: <b id="stUsed">–</b></div>
  <div class="muted" style="font-size:16px;margin-top:4px">Totale filament-uitgave: <b id="stCost">–</b></div>
 </div>
 <div class="card"><h3>Print-geschiedenis</h3><div id="histList" class="muted">…</div></div>
</section>

<div id="rollModal" class="modal"><div class="modalbox">
 <div class="modalhead"><b id="rollTitle">Kies rol</b><button onclick="closeRoll()">Sluit</button></div>
 <div id="rollList"></div>
</div></div>
<script>
var step="1",curState="",curLight=false,curFan=0,cfgFilled=false,curPath="/",scaleHost="",lowG=100,spCache=[],emCache=[],pickSlot=-1,spSel={},lastS=null;
function $(id){return document.getElementById(id);}
function tab(n){
 document.querySelectorAll('nav button').forEach(function(b){b.classList.toggle('on',b.dataset.tab===n);});
 document.querySelectorAll('section').forEach(function(s){s.classList.toggle('on',s.id===n);});
 if(n==='files')loadFiles(curPath);
 if(n==='spools'){loadSpools();loadEmpties();}
 if(n==='set')loadHistory();
}
function loadHistory(){fetch('/history').then(function(r){return r.json();}).then(function(list){
 var h='';if(!list.length)h='<div class="muted">nog geen prints</div>';
 list.forEach(function(r){h+='<div class="chiprow"><span style="color:'+(r.ok?'#2ecc71':'#e74c3c')+'">'+(r.ok?'✓':'✗')+'</span> <span><b>'+r.name+'</b> <span class="muted">'+r.when+'</span></span><span class="muted" style="margin-left:auto;white-space:nowrap">'+r.grams+' g'+(r.cost>0?' · €'+r.cost.toFixed(2):'')+'</span></div>';});
 $('histList').innerHTML=h;}).catch(function(){});}
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
$('cNtfySave').onclick=function(){fetch('/setcfg?ntfy='+encodeURIComponent($('cNtfy').value)).then(function(r){return r.text();}).then(function(t){$('cNtfyMsg').textContent=t;});};
$('cNtfyTest').onclick=function(){$('cNtfyMsg').textContent='versturen…';fetch('/setcfg?ntfy='+encodeURIComponent($('cNtfy').value)).then(function(){setTimeout(function(){fetch('/notify_test').then(function(r){return r.text();}).then(function(t){$('cNtfyMsg').textContent=t;});},500);});};
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
function humHtml(h){if(!h||h<1)return'';var l,c;if(h<=2){l='droog';c='#2ecc71';}else if(h===3){l='redelijk';c='#f39c12';}else{l='vochtig';c='#e74c3c';}return ' · <span style="color:'+c+'">vocht: '+l+'</span>';}
function slotName(slot){return slot===254?'externe spoel':('AMS'+(Math.floor(slot/4)+1)+' T'+(slot%4+1));}
function trayCell(t,slot,assign){
 var inner;
 if(t&&t.present)inner='<div class="sw" style="background:'+t.rgb+'"></div><div>'+t.type+'</div>'+amt(t);
 else inner='<div class="sw empty"></div><div class="muted">leeg</div><div></div>';
 if(assign)inner+='<button class="rolbtn" onclick="pickRoll('+slot+')">Rol</button>';
 return '<div class="tray">'+inner+'</div>';
}
function amsHtml(ams,ext,assign){
 var h='';
 (ams||[]).forEach(function(u){
  h+='<div class="amsbox"><div class="muted">AMS '+u.id+humHtml(u.humidity)+'</div><div class="trays">';
  u.trays.forEach(function(t,ti){h+=trayCell(t,(u.id-1)*4+ti,assign);});
  h+='</div></div>';
 });
 if((ext&&ext.present)||assign)h+='<div class="amsbox"><div class="muted">Externe spoel</div><div class="trays">'+trayCell(ext,254,assign)+'</div></div>';
 return h||'<div class="muted">geen AMS</div>';
}
function slotMat(slot){if(!lastS)return'';if(slot===254)return(lastS.ext&&lastS.ext.type)||'';var u=Math.floor(slot/4),t=slot%4;if(lastS.ams)for(var i=0;i<lastS.ams.length;i++){if(lastS.ams[i].id===u+1){var tr=lastS.ams[i].trays[t];return(tr&&tr.type)||'';}}return'';}
function pickRoll(slot){pickSlot=slot;$('rollTitle').textContent='Kies rol voor '+slotName(slot);
 var mat=slotMat(slot);
 fetch('/spools').then(function(r){return r.json();}).then(function(list){
  if(mat)list.sort(function(a,b){return (b.material===mat?1:0)-(a.material===mat?1:0);});
  var h='';
  if(!list.length)h='<div class="muted">Nog geen rollen — maak ze aan op de Spools-tab.</div>';
  list.forEach(function(s){var pas=(mat&&s.material===mat)?' <span class="badge" style="margin-left:6px;background:#1e5f3a;color:#b9f5cf">past</span>':'';
   h+='<div class="fitem rollpick" onclick="chooseRoll('+s.i+')"><div style="display:flex;align-items:center;gap:8px"><div class="sw" style="width:26px;height:20px;flex:0 0 auto;background:'+s.rgb+'"></div><span><b>'+s.name+'</b> <span class="muted">'+s.material+' · '+s.rem+' g</span>'+pas+'</span></div></div>';});
  $('rollList').innerHTML=h;$('rollModal').style.display='flex';
 }).catch(function(){});
}
function chooseRoll(i){fetch('/spool_load?idx='+i+'&slot='+pickSlot).then(function(){closeRoll();});}
function closeRoll(){$('rollModal').style.display='none';}
function poll(){
 fetch('/status').then(function(r){return r.json();}).then(function(s){
  $('conn').textContent=s.conn?'● verbonden':'○ offline';$('conn').style.color=s.conn?'#2ecc71':'#e74c3c';
  curState=s.state;curLight=s.light;curFan=s.fan;lastS=s;
  $('state').textContent=s.state||'–';$('task').textContent=s.name||'';
  $('fill').style.width=(s.pct||0)+'%';
  $('prog').textContent=(s.pct||0)+'%'+(s.total?('  laag '+s.layer+'/'+s.total):'')+(s.remain?('  ~'+s.remain+' min'):'');
  var w=$('warn');if(w){if(s.short>0){w.style.display='block';w.textContent='⚠ Filament tekort — komt ~'+s.short+' g te kort voor deze print op het actieve slot. Overweeg een vollere spoel.';}else{w.style.display='none';}}
  $('noz').textContent=s.nozzle+'/'+s.nozzle_t+'°';$('bed').textContent=s.bed+'/'+s.bed_t+'°';$('cham').textContent=s.chamber+'°';
  $('bPause').textContent=(s.state==='PAUSE')?'Resume':'Pause';
  $('bLight').textContent='Licht: '+(s.light?'aan':'uit');
  $('bFan').textContent='Fan '+s.fan+'%';
  if(document.activeElement!==$('speed'))$('speed').value=s.speed;
  if(!$('rollModal')||$('rollModal').style.display!=='flex'){$('amsStrip').innerHTML=amsHtml(s.ams,s.ext,false);$('amsDetail').innerHTML=amsHtml(s.ams,s.ext,true);}
  $('movenoz').textContent='Nozzle '+s.nozzle+'/'+s.nozzle_t+'°C';
  if(s.prints!==undefined&&$('stPrints')){$('stPrints').textContent=s.prints;$('stUsed').textContent=(s.used>=1000?(s.used/1000).toFixed(2)+' kg':s.used+' g');if($('stCost'))$('stCost').textContent='€ '+(s.cost||0).toFixed(2);}
  if(s.cfg&&s.cfg.scale_ip){scaleHost=s.cfg.scale_ip;var si=$('sIp');if(si&&!si.value)si.value=s.cfg.scale_ip;}
  if(s.cfg&&s.cfg.low)lowG=s.cfg.low;
  if(!cfgFilled&&s.cfg){cfgFilled=true;
   $('cIp').value=s.cfg.ip;$('cSerial').value=s.cfg.serial;$('cBri').value=s.cfg.bri;
   var b=$('cView');b.dataset.v=s.cfg.view3d?'1':'0';b.textContent='Screensaver: '+(s.cfg.view3d?'3D':'2D');
   if(s.cfg.code_set)$('cCode').placeholder='•••••••• (ingesteld, laat leeg = ongewijzigd)';
   if(s.cfg.ntfy!==undefined)$('cNtfy').value=s.cfg.ntfy;
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
function spMsg(t){$('spMsg').textContent=t;}
function spSlotOpts(){var o='';for(var u=0;u<2;u++)for(var t=0;t<4;t++){o+='<option value="'+(u*4+t)+'">AMS'+(u+1)+' T'+(t+1)+'</option>';}return o+'<option value="254">Externe spoel</option>';}
function loadSpools(){
 fetch('/spools').then(function(r){return r.json();}).then(function(list){spCache=list;renderSpList();})
 .catch(function(){$('spList').innerHTML='<div class="muted">geen tablet</div>';});
}
function renderSpList(){
 var q=(($('spSearch')&&$('spSearch').value)||'').toLowerCase();
 var h='';
 spCache.forEach(function(s){
  if(q&&(s.name+' '+s.material).toLowerCase().indexOf(q)<0)return;
  h+='<div class="rollcard"><input type="checkbox" '+(spSel[s.i]?'checked':'')+' onchange="selToggle('+s.i+',this.checked)" style="width:20px;height:20px;flex:0 0 auto">'
   +'<div style="width:38px;height:38px;border-radius:8px;flex:0 0 auto;border:1px solid #3a434d;background:'+s.rgb+'"></div>'
   +'<div style="min-width:0"><div class="name">'+s.name+'<span class="badge">'+s.material+'</span></div>'
   +(s.note?'<div class="muted" style="font-size:12px">'+s.note+'</div>':'')+'</div>'
   +'<div class="grams">'+s.rem+' g</div>'
   +'<button class="iconbtn" title="Weeg de rol" onclick="spWeigh('+s.i+')">⚖</button>'
   +'<button class="iconbtn" title="Kopieer" onclick="spCopy('+s.i+')">⧉</button>'
   +'<button class="iconbtn" title="Bewerk" onclick="spEdit('+s.i+')">✎</button>'
   +'<button class="iconbtn del" title="Verwijder" onclick="spDel('+s.i+')">✕</button></div>';
 });
 $('spList').innerHTML=h||'<div class="muted">Geen rollen gevonden.</div>';
 updateBulk();
}
function selToggle(i,c){if(c)spSel[i]=true;else delete spSel[i];updateBulk();}
function selIds(){return Object.keys(spSel);}
function updateBulk(){var bar=$('spBulk');if(!bar)return;var ids=selIds();bar.style.display=ids.length?'flex':'none';if($('spSelCount'))$('spSelCount').textContent=ids.length+' geselecteerd';}
function toggleAll(c){spSel={};if(c){var q=(($('spSearch')&&$('spSearch').value)||'').toLowerCase();spCache.forEach(function(s){if(!q||(s.name+' '+s.material).toLowerCase().indexOf(q)>=0)spSel[s.i]=true;});}renderSpList();}
function clearSel(){spSel={};if($('spAll'))$('spAll').checked=false;renderSpList();}
function bulkGo(qs){fetch('/spool_bulk?idx='+selIds().join(',')+'&'+qs).then(function(){spSel={};if($('spAll'))$('spAll').checked=false;setTimeout(loadSpools,300);});}
function bulkDel(){if(confirm(selIds().length+' rollen verwijderen?'))bulkGo('act=del');}
function bulkColorGo(){bulkGo('act=color&v='+encodeURIComponent($('bulkColor').value));}
function bulkWeightGo(){if($('bulkWeight').value!=='')bulkGo('act=weight&v='+$('bulkWeight').value);}
function bulkMatGo(){if($('bulkMat').value)bulkGo('act=material&v='+encodeURIComponent($('bulkMat').value));}
function spRemWeigh(){if(!scaleHost){alert('Geen schaal-IP bekend — stel het in op de Scale-tab.');return;}
 fetch('http://'+scaleHost+'/weight').then(function(r){return r.json();}).then(function(d){var e=parseFloat($('spEmpty').value)||0;$('spRem').value=Math.max(0,Math.round(d.g-e));}).catch(function(){alert('Geen verbinding met de schaal.');});}
function spWeigh(i){var s=spCache[i];if(!scaleHost){alert('Geen schaal-IP bekend.');return;}
 fetch('http://'+scaleHost+'/weight').then(function(r){return r.json();}).then(function(d){
  var rem=Math.max(0,Math.round(d.g-s.empty));
  if(!confirm('Rol \''+s.name+'\' wegen?\nGewogen '+Math.round(d.g)+' g − leeg '+s.empty+' g = '+rem+' g resterend.'))return;
  fetch('/spool_save?idx='+i+'&name='+encodeURIComponent(s.name)+'&material='+encodeURIComponent(s.material)+'&color='+encodeURIComponent(s.rgb)+'&rem='+rem+'&empty='+s.empty+'&nmin='+(s.nmin||0)+'&nmax='+(s.nmax||0)+'&code='+encodeURIComponent(s.code||'')+'&note='+encodeURIComponent(s.note||'')).then(function(){setTimeout(loadSpools,300);});
 }).catch(function(){alert('Geen verbinding met de schaal.');});}
function spCopy(i){var s=spCache[i];fetch('/spool_save?idx=&name='+encodeURIComponent(s.name+' (kopie)')+'&material='+encodeURIComponent(s.material)+'&color='+encodeURIComponent(s.rgb)+'&rem='+s.rem+'&empty='+s.empty+'&nmin='+(s.nmin||0)+'&nmax='+(s.nmax||0)+'&code='+encodeURIComponent(s.code||'')+'&note='+encodeURIComponent(s.note||'')+'&price='+(s.price||0)).then(function(){setTimeout(loadSpools,300);});}
function spLoad(i){var sl=$('ss'+i).value;fetch('/spool_load?idx='+i+'&slot='+sl).then(function(r){return r.text();}).then(function(t){spMsg(t+' in slot');});}
function spDel(i){if(confirm('Rol verwijderen?'))fetch('/spool_del?idx='+i).then(function(){loadSpools();});}
function spEdit(i){var s=spCache[i];$('spTitle').textContent='Rol bewerken';$('spIdx').value=i;
 $('spName').value=s.name;$('spMat').value=s.material;$('spColor').value=s.rgb;$('spRem').value=s.rem;$('spEmpty').value=s.empty;
 $('spNmin').value=s.nmin||'';$('spNmax').value=s.nmax||'';$('spCode').value=s.code||'';$('spNote').value=s.note||'';$('spPrice').value=s.price||'';}
function spReset(){$('spTitle').textContent='Nieuwe rol';$('spIdx').value=-1;$('spName').value='';$('spNote').value='';$('spCode').value='';$('spNmin').value='';$('spNmax').value='';$('spPrice').value='';}
$('spNew').onclick=spReset;
$('spSave').onclick=function(){
 var q='idx='+$('spIdx').value+'&name='+encodeURIComponent($('spName').value)+'&material='+encodeURIComponent($('spMat').value)
  +'&color='+encodeURIComponent($('spColor').value)+'&rem='+($('spRem').value||0)+'&empty='+($('spEmpty').value||0)
  +'&nmin='+($('spNmin').value||0)+'&nmax='+($('spNmax').value||0)+'&code='+encodeURIComponent($('spCode').value)+'&note='+encodeURIComponent($('spNote').value)+'&price='+($('spPrice').value||0);
 fetch('/spool_save?'+q).then(function(r){return r.text();}).then(function(t){spMsg(t);spReset();setTimeout(loadSpools,300);});
};
function loadEmpties(){
 fetch('/empties').then(function(r){return r.json();}).then(function(list){
  emCache=list;
  var sel=$('spEmptySel'),cur=sel.value,o='<option value="">— kies —</option>';
  list.forEach(function(e){o+='<option value="'+e.weight+'">'+e.name+' ('+e.weight+' g)</option>';});
  sel.innerHTML=o;sel.value=cur;
  var h='';
  list.forEach(function(e){h+='<div class="chiprow"><span>'+e.name+'</span><span class="badge" style="margin-left:0">'+e.weight+' g</span><button class="iconbtn del" style="margin-left:auto" onclick="emDel('+e.i+')">✕</button></div>';});
  $('emList').innerHTML=h||'<div class="muted">Nog geen lege spoelen.</div>';
 }).catch(function(){});
}
$('spEmptySel').onchange=function(){if(this.value)$('spEmpty').value=this.value;};
$('emSave').onclick=function(){
 var q='idx=&name='+encodeURIComponent($('emName').value)+'&weight='+($('emWeight').value||0);
 fetch('/empty_save?'+q).then(function(){$('emName').value='';setTimeout(loadEmpties,300);});
};
function emDel(i){if(confirm('Verwijderen?'))fetch('/empty_del?idx='+i).then(function(){loadEmpties();});}
function emWeigh(){if(!scaleHost){alert('Geen schaal-IP bekend — stel het in op de Scale-tab.');return;}
 fetch('http://'+scaleHost+'/weight').then(function(r){return r.json();}).then(function(d){$('emWeight').value=Math.round(d.g);}).catch(function(){alert('Geen verbinding met de schaal.');});}
function downloadBackup(){
 Promise.all([fetch('/spools').then(function(r){return r.json();}),fetch('/empties').then(function(r){return r.json();})]).then(function(a){
  var blob=new Blob([JSON.stringify({spools:a[0],empties:a[1]},null,1)],{type:'application/json'});
  var url=URL.createObjectURL(blob),link=document.createElement('a');link.href=url;link.download='pandatouch_spools_backup.json';link.click();URL.revokeObjectURL(url);
 }).catch(function(){$('bkMsg').textContent='geen verbinding';});
}
function importBackup(files){
 if(!files||!files.length)return;var rd=new FileReader();
 rd.onload=function(){try{var d=JSON.parse(rd.result),chain=Promise.resolve();
  (d.empties||[]).forEach(function(e){chain=chain.then(function(){return fetch('/empty_save?idx=&name='+encodeURIComponent(e.name)+'&weight='+e.weight);});});
  (d.spools||[]).forEach(function(s){chain=chain.then(function(){return fetch('/spool_save?idx=&name='+encodeURIComponent(s.name)+'&material='+encodeURIComponent(s.material)+'&color='+encodeURIComponent(s.rgb)+'&rem='+s.rem+'&empty='+s.empty+'&nmin='+(s.nmin||0)+'&nmax='+(s.nmax||0)+'&code='+encodeURIComponent(s.code||'')+'&note='+encodeURIComponent(s.note||''));});});
  chain.then(function(){$('bkMsg').textContent='Geïmporteerd.';loadSpools();loadEmpties();});
 }catch(e){$('bkMsg').textContent='Ongeldig back-up-bestand.';}};
 rd.readAsText(files[0]);
}
function setSpColor(c){$('spColor').value=c;}
function buildSwatches(){var p=['#111111','#eeeeee','#9aa0a6','#c0392b','#e67e22','#f1c40f','#27ae60','#2980b9','#8e44ad','#ec407a','#6d4c41','#16a085'];var h='';p.forEach(function(c){h+='<button class="swatch" style="background:'+c+'" onclick="setSpColor(\''+c+'\')"></button>';});if($('spSw'))$('spSw').innerHTML=h;}
buildSwatches();
</script></body></html>)PAGE"
