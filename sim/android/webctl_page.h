// The web control single-page app, as a C++ raw string literal. Included by
// webctl.cpp as the initializer of PAGE_HTML. Kept in its own file so the HTML/
// CSS/JS is editable on its own. Self-contained: inline CSS + vanilla JS, dark,
// mobile-first. Mirrors the tablet: Dashboard, Filament, Files, Move, Settings.
R"PAGE(<!doctype html>
<html lang="nl"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Printward</title>
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
.rnum{display:inline-block;background:#0e639c;color:#fff;border-radius:6px;padding:1px 8px;font-size:13px;font-weight:700;margin-right:4px;font-variant-numeric:tabular-nums}
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
<header><div class="t">Printward</div><div style="display:flex;gap:14px;align-items:center"><span id="clock" class="muted"></span><span id="conn" class="muted" data-i18n="dash.connecting">verbinden…</span></div></header>
<nav>
 <button data-tab="dash" class="on" data-i18n="nav.dashboard">Dashboard</button>
 <button data-tab="files" data-i18n="nav.files">Files</button>
 <button data-tab="move" data-i18n="nav.move">Move</button>
 <button data-tab="spools" data-i18n="nav.spools">Spools</button>
 <button data-tab="hist" data-i18n="nav.history">Historie</button>
 <button data-tab="set" data-i18n="nav.settings">Settings</button>
</nav>

<section id="dash" class="on">
 <div id="warn" style="display:none;background:#5a2020;color:#ffd0d0;border-radius:10px;padding:12px 14px;margin-bottom:14px;font-weight:600"></div>
 <div class="card"><div id="state">–</div><div id="task" class="muted"></div>
  <div class="bar"><div id="fill"></div></div><div id="prog" class="muted"></div>
  <div id="pcost" class="muted" style="margin-top:4px"></div>
  <div id="platepick" class="muted" style="display:none;margin-top:8px;align-items:center;gap:8px;font-size:14px"><span data-i18n="dash.plate">Plaat</span><select id="plateSel" onchange="setPlate(this.value)" style="padding:6px;border-radius:6px;border:1px solid #333b44;background:var(--panel2);color:#fff"></select></div>
  <img id="pthumb" alt="" style="display:none;max-width:100%;max-height:240px;border-radius:8px;margin-top:10px"></div>
 <div class="card temps"><div><span data-i18n="dash.nozzle">Nozzle</span> <b id="noz">–</b></div><div><span data-i18n="dash.bed">Bed</span> <b id="bed">–</b></div><div><span data-i18n="dash.chamber">Kamer</span> <b id="cham">–</b></div></div>
 <div class="card"><h3 data-i18n="dash.controls">Bediening</h3><div class="ctrls">
   <button id="bPause" class="b-blue" data-i18n="dash.pause">Pause</button>
   <button id="bStop" class="b-red" data-i18n="dash.stop">Stop</button>
   <button id="bLight" data-i18n="dash.light">Licht</button>
   <button id="bFan" data-i18n="dash.fan">Fan</button>
   <select id="speed"><option value="1">Silent</option><option value="2">Standard</option><option value="3">Sport</option><option value="4">Ludicrous</option></select>
 </div></div>
 <div class="card"><h3 data-i18n="dash.filament_ams">Filament / AMS</h3><div id="amsStrip" class="ams"></div></div>
</section>

<section id="files">
 <div class="fbar"><button id="fUp" data-i18n="files.up">⬆ Up</button><span id="fpath">/</span><button id="fRef">↻</button><button id="fDel" style="display:none;background:#a40000;margin-left:auto" data-i18n="delete">Verwijder</button></div>
 <div id="flist" class="muted">…</div>
</section>

<section id="move">
 <div class="step"><span data-i18n="move.step">Stap:</span>
  <button data-s="0.1">0.1</button><button data-s="1" class="sel">1</button><button data-s="10">10</button> mm</div>
 <div class="movewrap">
  <div class="card"><h3>X / Y</h3><div class="pad">
   <span></span><button data-a="yp">Y+</button><span></span>
   <button data-a="xm">X-</button><button class="home" data-a="home">&#8962;</button><button data-a="xp">X+</button>
   <span></span><button data-a="ym">Y-</button><span></span>
  </div></div>
  <div class="card"><h3>Z</h3><div class="zcol"><button data-a="zp">Z+</button><button data-a="zm">Z-</button></div></div>
  <div class="card"><h3 data-i18n="move.extruder">Extruder</h3><div class="ecol">
   <div style="display:flex;gap:6px;align-items:center;margin-bottom:4px" class="muted"><span data-i18n="move.length">Lengte</span> <input type="number" id="mExt" value="10" style="width:64px;padding:8px;border-radius:8px;border:1px solid #333b44;background:var(--panel2);color:#fff"> mm</div>
   <button class="ext big" onclick="mcmd('ext',$('mExt').value)" data-i18n="move.extrude">Extrude</button>
   <button class="ret big" onclick="mcmd('ret',$('mExt').value)" data-i18n="move.retract">Retract</button>
  </div></div>
 </div>
 <div class="card"><h3 data-i18n="dash.fan">Ventilator</h3>
  <div style="display:flex;gap:6px;flex-wrap:wrap"><button onclick="mcmd('fan',0)" data-i18n="move.off">Uit</button><button onclick="mcmd('fan',25)">25%</button><button onclick="mcmd('fan',50)">50%</button><button onclick="mcmd('fan',100)">100%</button></div>
 </div>
 <div class="card"><h3 data-i18n="move.motors_pos">Motoren &amp; posities</h3>
  <div style="display:flex;gap:6px;flex-wrap:wrap"><button onclick="mcmd('motoff')" data-i18n="move.motors_off">Motoren uit</button><button onclick="mcmd('center')" data-i18n="move.center">Naar midden</button><button onclick="mcmd('front')" data-i18n="move.bed_front">Bed naar voren</button><button onclick="mcmd('zup')" data-i18n="move.z_up">Z omhoog</button><button onclick="mcmd('homex')">Home X</button><button onclick="mcmd('homey')">Home Y</button><button onclick="mcmd('homez')">Home Z</button></div>
 </div>
 <div id="hint"></div>
</section>

<section id="scale">
 <div style="margin-bottom:10px"><button onclick="tab('set')" data-i18n="scale.back_set">&#8592; Instellingen</button></div>
 <div class="card" style="text-align:center"><div id="swt" style="font-size:52px;font-weight:700">– g</div><div id="sst" class="muted">…</div></div>
 <div class="card"><h3 data-i18n="scale.calibration">Kalibratie</h3>
  <button id="sTare" data-i18n="scale.tare">Tarra (nulstellen)</button>
  <input type="number" id="sKnown" value="500" style="width:110px">
  <button id="sCal" class="ext" data-i18n="scale.calibrate">Kalibreer</button>
  <div id="sMsg" class="muted" style="margin-top:8px"></div>
 </div>
 <div class="card"><h3 data-i18n="scale.network">Netwerk</h3><div id="sInfo" class="muted" style="margin-bottom:10px">…</div>
  <div class="frow"><label class="muted" data-i18n="scale.ip_label">Scale IP (waar de tablet de schaal zoekt)</label><input type="text" id="sIp"></div>
  <div style="display:flex;gap:8px;flex-wrap:wrap"><button id="sIpSave" data-i18n="scale.save_tablet">Opslaan op tablet</button><button id="sIpFix" data-i18n="scale.fix_ip">Vast IP op schaal</button></div>
  <div class="frow" style="margin-top:12px"><label class="muted" data-i18n="scale.wifi_label">WiFi van de schaal wijzigen</label><input type="text" id="sSsid" data-i18n-ph="scale.wifi_name" placeholder="WiFi naam"><input type="password" id="sPass" data-i18n-ph="scale.password" placeholder="wachtwoord"></div>
  <button id="sWifi" class="ph" data-i18n="scale.wifi_change">WiFi wijzigen</button>
 </div>
</section>

<section id="spools">
 <div class="card" id="spInv" style="display:none"></div>
 <div class="sprow">
 <div class="card" style="flex:1 1 360px"><h3 id="spTitle" data-i18n="spools.new_roll">Nieuwe rol</h3>
  <div class="field" style="margin-bottom:14px"><label data-i18n="spools.name">Naam / merk</label><input type="text" id="spName" data-i18n-ph="spools.name_ph" placeholder="bv. Bambu PLA Zwart"></div>
  <div class="field" style="margin-bottom:14px"><label data-i18n="spools.colour">Kleur</label>
   <div class="swatches"><input type="color" id="spColor" value="#22aa55" class="chip"><span id="spSw" class="swatches"></span></div>
  </div>
  <div class="field" style="margin-bottom:14px"><label data-i18n="spools.empty_weight">Leeg spoel</label>
   <div style="display:flex;gap:8px;align-items:center">
    <select id="spEmptySel" style="flex:1"><option value="" data-i18n="spools.pick_library">— kies uit bibliotheek —</option></select>
    <input type="number" id="spEmpty" value="250" style="width:100px" data-i18n-title="spools.empty_weight_hint" title="gewicht van de lege spoel in gram">
    <span class="muted">g</span>
   </div>
  </div>
  <div class="field" style="margin-bottom:14px"><label data-i18n="spools.remaining">Resterend filament (g)</label>
   <div style="display:flex;gap:8px;align-items:center;flex-wrap:wrap">
    <input type="number" id="spRem" value="1000" style="width:130px">
    <button class="formbtn sec" onclick="spRemWeigh()">⚖ <span data-i18n="spools.weigh_roll">Weeg rol</span></button>
    <span class="muted" style="font-size:12px" data-i18n="spools.weigh_hint">= gewogen − leeg-spoel</span>
   </div>
  </div>
  <div class="grid">
   <div class="field"><label data-i18n="spools.material">Materiaal</label><select id="spMat"><option>PLA</option><option>PETG</option><option>ABS</option><option>TPU</option><option>ASA</option><option>PC</option><option>PA</option><option>PVA</option></select></div>
   <div class="field"><label data-i18n="spools.nozzle_min">Nozzle min</label><input type="number" id="spNmin" data-i18n-ph="auto" placeholder="auto"></div>
   <div class="field"><label data-i18n="spools.nozzle_max">Nozzle max</label><input type="number" id="spNmax" data-i18n-ph="auto" placeholder="auto"></div>
   <div class="field"><label data-i18n="spools.bambu_code">Bambu-code</label><input type="text" id="spCode" data-i18n-ph="auto" placeholder="auto"></div>
   <div class="field"><label data-i18n="spools.price">Prijs (EUR/kg)</label><input type="number" id="spPrice" placeholder="0" step="0.01"></div>
   <div class="field"><label data-i18n="spools.number">Rolnummer #</label><input type="number" id="spNum" min="1" step="1" data-i18n-title="spools.number_hint" title="Uniek nummer voor deze rol - schrijf het op de spoel. Wordt automatisch ingevuld."></div>
  </div>
  <div class="field" style="margin-top:12px"><label data-i18n="spools.note">Notitie</label><input type="text" id="spNote" data-i18n-ph="spools.note_ph" placeholder="bv. gedroogd 3/7"></div>
  <input type="hidden" id="spIdx" value="-1">
  <div style="display:flex;gap:10px;margin-top:16px"><button id="spSave" class="formbtn pri" data-i18n="save">Opslaan</button><button id="spNew" class="formbtn sec" data-i18n="spools.new">Nieuw</button></div>
  <div id="spMsg" class="muted" style="margin-top:8px"></div>
 </div>
 <div class="card" style="flex:1 1 300px"><h3 data-i18n="spools.empty_spools">Lege spoelen</h3>
  <div style="display:flex;gap:10px;align-items:flex-end;flex-wrap:wrap;margin-bottom:14px">
   <div class="field" style="flex:1;min-width:160px"><label data-i18n="spools.name_short">Naam</label><input type="text" id="emName" data-i18n-ph="spools.empty_ph" placeholder="bv. Bambu herbruikbaar"></div>
   <div class="field" style="width:120px"><label data-i18n="spools.weight_g">Gewicht (g)</label><input type="number" id="emWeight" value="250"></div>
   <button class="formbtn sec" style="height:42px" onclick="emWeigh()" data-i18n="spools.weigh">Weeg</button>
   <button id="emSave" class="formbtn pri" style="height:42px" data-i18n="spools.add">Toevoegen</button>
  </div>
  <div id="emList"></div>
 </div>
 </div>
 <div class="card"><h3 data-i18n="spools.title">Rollen</h3>
  <div style="display:flex;gap:8px;align-items:center;margin-bottom:10px;flex-wrap:wrap">
   <input type="text" id="spSearch" data-i18n-ph="spools.search" placeholder="Zoek op naam of materiaal…" oninput="renderSpList()" style="flex:1;min-width:150px;padding:9px;border-radius:8px;border:1px solid #333b44;background:var(--panel2);color:#fff;font-size:15px">
   <select id="spFMat" onchange="renderSpList()" style="padding:9px;border-radius:8px;border:1px solid #333b44;background:var(--panel2);color:#fff"><option value="" data-i18n="hist.all_materials">alle materialen</option></select>
   <select id="spSort" onchange="renderSpList()" style="padding:9px;border-radius:8px;border:1px solid #333b44;background:var(--panel2);color:#fff"><option value="num" data-i18n="spools.sort_num">op nummer (oudste eerst)</option><option value="name" data-i18n="spools.sort_name">naam A-Z</option><option value="low" data-i18n="spools.sort_low">weinig &rarr; veel</option><option value="high" data-i18n="spools.sort_high">veel &rarr; weinig</option><option value="price" data-i18n="spools.sort_price">prijs/kg</option><option value="mat" data-i18n="spools.material">materiaal</option></select>
   <label class="muted" style="display:flex;align-items:center;gap:6px;cursor:pointer"><input type="checkbox" id="spLow" onchange="renderSpList()" style="width:18px;height:18px"><span data-i18n="spools.almost_empty">bijna leeg</span></label>
   <label class="muted" style="display:flex;align-items:center;gap:6px;cursor:pointer"><input type="checkbox" id="spAll" onchange="toggleAll(this.checked)" style="width:18px;height:18px"> <span data-i18n="all">alles</span></label>
  </div>
  <div id="spBulk" style="display:none;gap:8px;align-items:center;flex-wrap:wrap;margin-bottom:12px;background:var(--panel2);border-radius:8px;padding:9px 12px">
   <b id="spSelCount"></b>
   <button class="formbtn" style="background:#4a2323;color:#ff9a9a;padding:8px 14px" onclick="bulkDel()" data-i18n="delete">Verwijder</button>
   <span style="display:flex;align-items:center;gap:5px"><input type="color" id="bulkColor" value="#22aa55" class="chip" style="width:34px;height:32px"><button class="formbtn sec" style="padding:8px 12px" onclick="bulkColorGo()" data-i18n="spools.colour">Kleur</button></span>
   <span style="display:flex;align-items:center;gap:5px"><input type="number" id="bulkWeight" placeholder="g" style="width:74px;padding:8px;border-radius:8px;border:1px solid #333b44;background:var(--bg);color:#fff"><button class="formbtn sec" style="padding:8px 12px" onclick="bulkWeightGo()" data-i18n="spools.weight">Gewicht</button></span>
   <span style="display:flex;align-items:center;gap:5px"><select id="bulkMat" style="padding:8px;border-radius:8px;border:1px solid #333b44;background:var(--bg);color:#fff"><option value="" data-i18n="spools.material_dots">materiaal…</option><option>PLA</option><option>PETG</option><option>ABS</option><option>TPU</option><option>ASA</option><option>PC</option><option>PA</option><option>PVA</option></select><button class="formbtn sec" style="padding:8px 12px" onclick="bulkMatGo()" data-i18n="spools.set">Zet</button></span>
   <span style="display:flex;align-items:center;gap:5px"><input type="number" id="bulkPrice" placeholder="€/kg" step="0.01" style="width:74px;padding:8px;border-radius:8px;border:1px solid #333b44;background:var(--bg);color:#fff"><button class="formbtn sec" style="padding:8px 12px" onclick="bulkPriceGo()" data-i18n="spools.price_short">Prijs</button></span>
   <button class="formbtn sec" style="padding:8px 12px" onclick="clearSel()" data-i18n="spools.deselect">Deselecteer</button>
  </div>
  <div id="spList" class="muted">…</div>
 </div>
 <div class="card"><h3 data-i18n="spools.export_title">Rollen exporteren / importeren</h3>
  <div class="muted" style="font-size:12px;margin-bottom:8px" data-i18n="spools.export_hint">Alleen je <b>rollen + lege spoelen</b>, als los bestand. Handig om te delen of van een andere tablet over te nemen. Importeren <b>voegt toe</b> — het overschrijft niets.</div>
  <div style="display:flex;gap:10px;flex-wrap:wrap;align-items:center">
   <button class="formbtn sec" onclick="exportRolls()">⬇ <span data-i18n="spools.export_btn">Exporteer rollen</span></button>
   <label class="formbtn sec" style="cursor:pointer">⬆ <span data-i18n="spools.import_btn">Importeer rollen</span><input type="file" accept=".json,application/json" style="display:none" onchange="importRolls(this.files)"></label>
  </div>
  <div id="rollExpMsg" class="muted" style="margin-top:8px"></div>
  <div class="muted" style="font-size:12px;margin-top:8px" data-i18n="spools.full_bk_hint">Een volledige back-up (incl. historie en statistieken) staat bij <b>Settings</b>.</div>
 </div>
</section>

<section id="set">
 <div class="card"><h3 data-i18n="set.scale">Printward Scale (weegschaal)</h3>
  <div class="muted" style="margin-bottom:8px" data-i18n="set.scale_hint">Gewicht, tarra, kalibreren en WiFi/IP van de schaal.</div>
  <button onclick="tab('scale')" class="formbtn pri" data-i18n="set.manage_scale">Schaal beheren</button>
 </div>
 <div class="card"><h3 data-i18n="set.printer">Printer</h3>
  <div class="frow"><label class="muted" data-i18n="set.printer_ip">Printer IP</label><input type="text" id="cIp"></div>
  <div class="frow"><label class="muted" data-i18n="set.serial">Serial</label><input type="text" id="cSerial"></div>
  <div class="frow"><label class="muted" data-i18n="set.access_code">Access code</label><input type="password" id="cCode" data-i18n-ph="set.code_empty" placeholder="laat leeg = ongewijzigd"></div>
  <button id="cView">Screensaver: 2D</button>
  <div class="frow" style="margin-top:12px"><label class="muted" data-i18n="dash.brightness">Helderheid</label><input type="range" id="cBri" min="5" max="100"></div>
  <button id="cSave" data-i18n="set.save_connect">Opslaan &amp; verbinden</button>
  <div id="cMsg"></div>
 </div>
 <div class="card"><h3 data-i18n="set.notifications">Meldingen (ntfy)</h3>
  <div class="frow"><label class="muted" data-i18n="set.ntfy_topic">ntfy topic (leeg = uit)</label><input type="text" id="cNtfy" data-i18n-ph="set.ntfy_ph" placeholder="bv. printward-geheim-x9k2"></div>
  <div style="display:flex;gap:8px;flex-wrap:wrap;margin-top:8px"><button id="cNtfySave" class="formbtn pri" data-i18n="save">Opslaan</button><button id="cNtfyTest" class="formbtn sec" data-i18n="set.test">Test</button></div>
  <div class="frow" style="margin-top:14px"><label class="muted" data-i18n="set.low_threshold">Waarschuwen als een rol onder dit aantal gram komt</label>
   <div style="display:flex;gap:8px;align-items:center"><input type="number" id="cLow" min="0" step="10" placeholder="100" style="width:120px"><span class="muted">g</span><button id="cLowSave" class="formbtn sec" data-i18n="save">Opslaan</button></div></div>
  <div class="muted" style="font-size:12px;margin-top:8px" data-i18n="set.ntfy_hint">Installeer de gratis <b>ntfy</b>-app (of ntfy.sh in de browser) en abonneer op dit topic. Je krijgt een melding bij print klaar/mislukt, filament tekort en als een gewogen rol onder de drempel komt.</div>
  <div id="cNtfyMsg" class="muted" style="margin-top:6px"></div>
 </div>
 <div class="card"><h3 data-i18n="set.backup">Back-up &amp; herstel (alles)</h3>
  <div id="bkStatus" style="border-radius:8px;padding:10px 12px;margin-bottom:10px;display:none"></div>
  <div class="muted" style="font-size:12px;margin-bottom:8px" data-i18n="set.backup_hint">Alles op de tablet in één bestand: rollen, lege spoelen, gewichten, historie en statistieken. (Printer-IP/serial/toegangscode zitten er <b>niet</b> in.)</div>
  <div style="display:flex;gap:10px;flex-wrap:wrap;align-items:center">
   <button id="bkDl" class="formbtn sec" onclick="downloadBackup()">⬇ <span data-i18n="set.bk_download">Download back-up</span></button>
   <label class="formbtn sec" style="cursor:pointer">⬆ <span data-i18n="set.bk_restore">Herstel alles</span><input type="file" accept=".ptb,.conf,.txt" style="display:none" onchange="importBackup(this.files)"></label>
   <span class="muted" style="font-size:12px" data-i18n="set.restore_warn">herstel <b>overschrijft</b> je huidige rollen, historie en statistieken</span>
  </div>
  <div id="bkMsg" class="muted" style="margin-top:8px"></div>
  <div class="muted" style="font-size:12px;margin-top:8px" data-i18n="set.rolls_only_hint">Alleen je rollen delen/overnemen? Dat staat bij <b>Spools</b>.</div>
 </div>
 <div class="card"><h3 data-i18n="set.language">Taal</h3>
  <div style="display:flex;gap:10px;align-items:center;flex-wrap:wrap">
   <select id="cLang" style="padding:10px;border-radius:8px;border:1px solid #333b44;background:var(--panel2);color:#fff;font-size:16px;min-width:150px"></select>
   <span class="muted" style="font-size:12px" data-i18n="set.lang_hint">Geldt voor de tablet én deze pagina. Een taal toevoegen? Zet een bestand printward_lang_&lt;code&gt;.conf op de tablet — zie lang/README.md.</span>
  </div>
  <div id="cLangMsg" class="muted" style="margin-top:8px"></div>
 </div>
 <div class="card"><h3 data-i18n="set.diagnostics">Diagnose</h3>
  <div id="diagBox" class="muted">…</div>
  <button class="formbtn sec" style="margin-top:10px" onclick="loadDiag()" data-i18n="refresh">Ververs</button>
 </div>
</section>

<section id="hist"><div class="card"><h3 data-i18n="hist.stats">Statistieken</h3>
  <div class="muted"><span data-i18n="hist.all_time">Aller tijden</span>: <b id="stPrints">–</b> <span data-i18n="hist.prints_done">prints voltooid</span> &middot; <b id="stUsed">–</b> <span data-i18n="spools.filament">filament</span> &middot; <span data-i18n="hist.spend">uitgave</span> <b id="stCost">–</b></div>
  <div id="statRich" style="margin-top:6px"></div>
 </div>
 <div class="card"><h3 data-i18n="hist.title">Historie / kosten</h3>
 <div style="display:flex;gap:8px;flex-wrap:wrap;align-items:center;margin-bottom:10px">
  <input id="hSearch" oninput="renderHist()" data-i18n-ph="hist.search" placeholder="Zoek op naam…" style="flex:1;min-width:130px;padding:9px;border-radius:8px;border:1px solid #333b44;background:var(--panel2);color:#fff;font-size:15px">
  <select id="hMat" onchange="renderHist()" style="padding:9px;border-radius:8px;border:1px solid #333b44;background:var(--panel2);color:#fff"><option value="" data-i18n="hist.all_materials">alle materialen</option></select>
  <select id="hOk" onchange="renderHist()" style="padding:9px;border-radius:8px;border:1px solid #333b44;background:var(--panel2);color:#fff"><option value="" data-i18n="hist.ok_and_fail">gelukt + mislukt</option><option value="1" data-i18n="hist.only_ok">alleen gelukt</option><option value="0" data-i18n="hist.only_fail">alleen mislukt</option></select>
  <select id="hSort" onchange="renderHist()" style="padding:9px;border-radius:8px;border:1px solid #333b44;background:var(--panel2);color:#fff"><option value="date" data-i18n="hist.newest">nieuwste eerst</option><option value="cost" data-i18n="hist.costliest">duurste eerst</option><option value="grams" data-i18n="hist.most_filament">meeste filament</option></select>
  <button onclick="exportCsv()" class="formbtn sec" style="padding:9px 14px">CSV</button>
 </div>
 <div id="histTotal" class="muted" style="font-size:16px;margin-bottom:8px"></div>
 <div style="display:flex;align-items:center;gap:10px;flex-wrap:wrap;margin-bottom:10px">
  <label style="display:flex;align-items:center;gap:6px"><input type="checkbox" id="hArch" onchange="renderHist()" style="width:18px;height:18px"><span data-i18n="hist.show_arch">Gearchiveerd tonen</span></label>
  <button id="hArchBtn" onclick="hBulk(($('hArch')&&$('hArch').checked)?'unarch':'arch')" style="display:none;background:#8e44ad;color:#fff;border:0;border-radius:6px;padding:8px 12px;cursor:pointer;margin-left:auto" data-i18n="hist.archive">Archiveer</button>
  <button id="hDelBtn" onclick="hBulk('del')" style="display:none;background:#a40000;color:#fff;border:0;border-radius:6px;padding:8px 12px;cursor:pointer" data-i18n="delete">Verwijder</button>
 </div>
 <div id="histList"></div></div></section>

<div id="rollModal" class="modal"><div class="modalbox">
 <div class="modalhead"><b id="rollTitle" data-i18n="spools.pick_roll">Kies rol</b><button onclick="closeRoll()" data-i18n="close">Sluit</button></div>
 <div id="rollList"></div>
</div></div>
<div id="prevModal" class="modal" onclick="this.style.display='none'"><div class="modalbox" style="text-align:center">
 <div class="modalhead"><b data-i18n="preview">Voorbeeld</b><button onclick="event.stopPropagation();document.getElementById('prevModal').style.display='none'" data-i18n="close">Sluit</button></div>
 <div id="prevBody" class="muted">…</div>
</div></div>
<script>
var step="1",curState="",curLight=false,curFan=0,cfgFilled=false,curPath="/",scaleHost="",lowG=100,spCache=[],emCache=[],pickSlot=-1,spSel={},lastS=null,pthumbFile="";
function $(id){return document.getElementById(id);}
function tab(n){
 document.querySelectorAll('nav button').forEach(function(b){b.classList.toggle('on',b.dataset.tab===n);});
 document.querySelectorAll('section').forEach(function(s){s.classList.toggle('on',s.id===n);});
 if(n==='files')loadFiles(curPath);
 if(n==='spools'){loadSpools();loadEmpties();}
 if(n==='hist')loadHistory();
 if(n==='set')loadDiag();
}
// --- translations -------------------------------------------------------
// The tablet owns the strings (built-in EN/NL + any printward_lang_*.conf), so
// the page just asks for the active table. t() falls back to the key, which
// makes a missing translation visible instead of blank.
var I18N={},I18NT={},LANGS=[];
// Bambu's gcode_state -> our keys (the state names differ from the button labels)
var STATE_KEY={RUNNING:'dash.printing',PAUSE:'dash.paused',FINISH:'dash.finished',FAILED:'dash.failed',PREPARE:'dash.preparing',IDLE:'dash.idle'};
// Translations are written for innerHTML, so they may carry entities (&hellip;,
// &amp;) and even <b>. A placeholder, textContent or alert() takes plain text and
// would show "&hellip;" literally, so keep a decoded copy alongside the raw one:
// data-i18n uses the raw value, everything else goes through t().
function deent(s){if(s.indexOf('&')<0)return s;var d=document.createElement('textarea');d.innerHTML=s;return d.value;}
// Anything below can hold characters that mean something in HTML. File names come
// from the printer - you download models from the internet, and nothing stops one
// being called a<img src=x onerror=...>.3mf - and roll names are typed by hand.
// Pasted into innerHTML raw, that breaks the page at best and runs as script at
// worst. Quotes are escaped too: several of these land inside an attribute.
function esc(v){return String(v==null?'':v).replace(/[&<>"']/g,function(c){
 return {'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c];});}
function t(k,dflt){var v=I18NT[k];if(v!==undefined)return v;return dflt!==undefined?deent(dflt):k;}
// The screensaver button carries a 2D/3D suffix, so it cannot be a plain
// data-i18n element - it is rebuilt whenever the text or the language changes.
function viewBtn(){var b=$('cView');if(b)b.textContent=t('set.screensaver','Screensaver')+': '+(b.dataset.v==='1'?'3D':'2D');}
function applyI18n(){
 document.querySelectorAll('[data-i18n]').forEach(function(el){
  var v=I18N[el.dataset.i18n]; if(v!==undefined)el.innerHTML=v;});
 document.querySelectorAll('[data-i18n-ph]').forEach(function(el){
  var v=I18NT[el.dataset.i18nPh]; if(v!==undefined)el.placeholder=v;});
 document.querySelectorAll('[data-i18n-title]').forEach(function(el){
  var v=I18NT[el.dataset.i18nTitle]; if(v!==undefined)el.title=v;});
 viewBtn();
}
function loadLang(){
 return fetch('/lang').then(function(r){return r.json();}).then(function(d){
  I18N=d.s||{}; I18NT={}; for(var k in I18N)I18NT[k]=deent(I18N[k]);
  document.documentElement.lang=d.lang||'en';
  var sel=$('cLang'); if(sel)sel.value=d.lang;
  applyI18n();
 }).catch(function(){});
}
function loadLangList(){
 fetch('/langs').then(function(r){return r.json();}).then(function(list){
  LANGS=list||[];
  var sel=$('cLang'); if(!sel)return;
  var names={en:'English',nl:'Nederlands',de:'Deutsch',fr:'Français',es:'Español',it:'Italiano',pl:'Polski',pt:'Português',sv:'Svenska',da:'Dansk',no:'Norsk',fi:'Suomi'};
  sel.innerHTML=LANGS.map(function(c){return '<option value="'+c+'">'+(names[c]||c)+'</option>';}).join('');
  if(lastS&&lastS.cfg&&lastS.cfg.lang)sel.value=lastS.cfg.lang;
 }).catch(function(){});
}
function setLang(c){
 fetch('/setcfg?lang='+encodeURIComponent(c)).then(function(){
  return loadLang();
 }).then(function(){
  if($('cLangMsg'))$('cLangMsg').textContent=t('set.lang_saved','Taal opgeslagen.');
  // repaint the bits that are drawn from JS rather than from the HTML
  if($('spList'))loadSpools(); if($('histList'))loadHistory(); if($('diagBox'))loadDiag();
 });
}
function fmtAge(s){if(s<0)return t('unknown','onbekend');if(s<60)return s+' s';if(s<3600)return Math.round(s/60)+' '+t('min','min');if(s<86400)return Math.round(s/3600)+' '+t('hour','uur');return Math.round(s/86400)+' '+t('days','dagen');}
function dRow(label,val,good){return '<div style="display:flex;justify-content:space-between;gap:10px;padding:3px 0"><span>'+label+'</span><b style="color:'+(good===true?'#2ecc71':(good===false?'#e74c3c':'#eceff2'))+'">'+val+'</b></div>';}
function loadDiag(){
 var b=$('diagBox');if(!b)return;b.innerHTML=t('loading','laden…');
 fetch('/diag').then(function(r){return r.json();}).then(function(d){
  var h='';
  h+=dRow('Printer (MQTT)',d.mqtt?t('diag.connected','verbonden'):t('diag.offline','offline'),d.mqtt);
  h+=dRow(t('diag.last_status','Laatste status'),d.age<0?t('diag.nothing_yet','nog niets ontvangen'):fmtAge(d.age)+' '+t('ago','geleden'),d.age>=0&&d.age<60);
  h+=dRow(t('set.printer_ip','Printer-IP'),d.ip||t('diag.not_set','niet ingesteld'),!!d.ip);
  h+=dRow(t('set.serial','Serial'),d.serial_set?t('diag.set','ingesteld'):t('diag.missing','ontbreekt'),d.serial_set);
  h+=dRow(t('set.access_code','Toegangscode'),d.code_set?t('diag.set','ingesteld'):t('diag.missing','ontbreekt'),d.code_set);
  h+=dRow(t('diag.scale_ip','Weegschaal-IP'),d.scale_ip||t('diag.not_set','niet ingesteld'),null);
  h+=dRow(t('diag.web_url','Webadres'),d.url||'-',null);
  h+=dRow(t('diag.uptime','App draait al'),fmtAge(d.uptime),null);
  h+='<div class="muted" style="font-size:13px;margin:10px 0 4px">'+t('diag.data_files','Databestanden (op de tablet)')+'</div>';
  (d.files||[]).forEach(function(f){h+=dRow(f.n,f.ok?(f.bytes+' B · '+t('diag.changed','gewijzigd')+' '+fmtAge(f.age)+' '+t('ago','geleden')):t('diag.missing','ontbreekt'),f.ok);});
  b.innerHTML=h;
 }).catch(function(){b.innerHTML='<span style="color:#e74c3c">'+t('no_conn_tablet','geen verbinding met de tablet')+'</span>';});
}
var histCache=[];
function loadHistory(){fetch('/history').then(function(r){return r.json();}).then(function(list){histCache=list;
 var mats={};list.forEach(function(r){if(r.mat)mats[r.mat]=1;});
 var sel=$('hMat');if(sel){var cur=sel.value,o='<option value="">'+t('hist.all_materials','alle materialen')+'</option>';Object.keys(mats).sort().forEach(function(m){o+='<option>'+m+'</option>';});sel.innerHTML=o;sel.value=cur;}
 renderStats();renderHist();
}).catch(function(){});}
function fmtDur(m){if(!m||m<=0)return'';if(m<60)return m+' '+t('min','min');var h=Math.floor(m/60),mm=m%60;return h+'u'+(mm<10?'0':'')+mm;}
function statCell(t,big,sub){return '<div style="background:var(--panel2);border-radius:10px;padding:10px 12px"><div class="muted" style="font-size:12px">'+t+'</div><div style="font-size:20px;font-weight:600">'+big+'</div><div class="muted" style="font-size:12px">'+sub+'</div></div>';}
function bar(label,val,max,extra){var pct=max>0?Math.round(val/max*100):0;return '<div style="margin:4px 0"><div style="display:flex;justify-content:space-between;font-size:13px;gap:8px"><span>'+label+'</span><span class="muted" style="white-space:nowrap">'+extra+'</span></div><div style="height:9px;background:#232a31;border-radius:5px;overflow:hidden;margin-top:2px"><div style="height:100%;width:'+pct+'%;background:#3465a4"></div></div></div>';}
function grid(){return 'display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:8px';}
function renderStats(){
 var box=$('statRich');if(!box)return;
 var list=histCache.filter(function(r){return !r.arch;});
 if(!list.length){box.innerHTML='<div class="muted" style="margin-top:8px">'+t('hist.log_empty','Nog geen prints in het logboek.')+'</div>';return;}
 var ok=list.filter(function(r){return r.ok;}),fail=list.filter(function(r){return !r.ok;});
 var okG=0,okC=0,okT=0,failG=0,failC=0,totT=0;
 ok.forEach(function(r){okG+=r.grams||0;okC+=r.cost||0;okT+=r.mins||0;});
 fail.forEach(function(r){failG+=r.grams||0;failC+=r.cost||0;});
 list.forEach(function(r){totT+=r.mins||0;});
 var rate=Math.round(ok.length/list.length*100);
 var h='<div class="muted" style="font-size:13px;margin:6px 0 4px">'+t('hist.based_on','Op basis van je logboek')+' ('+list.length+')</div>';
 h+='<div style="'+grid()+'">';
 h+=statCell(t('hist.succeeded','Geslaagd'),rate+'%',ok.length+' '+t('of','van')+' '+list.length);
 h+=statCell(t('hist.failed','Mislukt'),fail.length+'x',failG>0?(Math.round(failG)+' g · € '+failC.toFixed(2)+' '+t('hist.wasted','verspild')):t('hist.no_waste','geen verspilling'));
 h+=statCell(t('hist.per_print','Gem./print'),(ok.length?Math.round(okG/ok.length):0)+' g',(ok.length&&okC>0?'€ '+(okC/ok.length).toFixed(2):'')+(okT>0?' · '+fmtDur(Math.round(okT/ok.length)):''));
 h+=statCell(t('hist.print_time','Printtijd'),fmtDur(totT)||'–',totT>0?t('total','totaal'):t('unknown','onbekend'));
 h+='</div>';
 var now=Date.now()/1000,DAY=86400;
 function since(d){var s={n:0,g:0,c:0};list.forEach(function(r){if(r.ts&&now-r.ts<=d*DAY){s.n++;s.g+=r.grams||0;s.c+=r.cost||0;}});return s;}
 var wk=since(7),mo=since(30),yr=since(365);
 h+='<div class="muted" style="font-size:13px;margin:12px 0 4px">'+t('hist.timeline','Tijdlijn')+'</div><div style="'+grid()+'">';
 h+=statCell(t('hist.this_week','Deze week'),wk.n+' '+t('prints','prints'),Math.round(wk.g)+' g · € '+wk.c.toFixed(2));
 h+=statCell(t('hist.this_month','Deze maand'),mo.n+' '+t('prints','prints'),Math.round(mo.g)+' g · € '+mo.c.toFixed(2));
 h+=statCell(t('hist.this_year','Dit jaar'),yr.n+' '+t('prints','prints'),Math.round(yr.g)+' g · € '+yr.c.toFixed(2));
 h+='</div>';
 var mm={};list.forEach(function(r){var k=r.mat||'?';if(!mm[k])mm[k]={g:0,c:0,n:0};mm[k].g+=r.grams||0;mm[k].c+=r.cost||0;mm[k].n++;});
 var mk=Object.keys(mm).sort(function(a,b){return mm[b].g-mm[a].g;}),maxG=0,totG=0;
 mk.forEach(function(k){if(mm[k].g>maxG)maxG=mm[k].g;totG+=mm[k].g;});
 h+='<div class="muted" style="font-size:13px;margin:12px 0 4px">'+t('hist.per_material','Per materiaal')+'</div>';
 mk.forEach(function(k){var pc=totG>0?Math.round(mm[k].g/totG*100):0;h+=bar(k+' <span class="muted">('+mm[k].n+'x)</span>',mm[k].g,maxG,Math.round(mm[k].g)+' g · € '+mm[k].c.toFixed(2)+' · '+pc+'%');});
 var mon={};list.forEach(function(r){if(!r.ts)return;var d=new Date(r.ts*1000),key=d.getFullYear()+'-'+('0'+(d.getMonth()+1)).slice(-2);if(!mon[key])mon[key]={n:0,g:0};mon[key].n++;mon[key].g+=r.grams||0;});
 var mkeys=Object.keys(mon).sort();
 if(mkeys.length){mkeys=mkeys.slice(-6);var maxN=0;mkeys.forEach(function(k){if(mon[k].n>maxN)maxN=mon[k].n;});
  h+='<div class="muted" style="font-size:13px;margin:12px 0 4px">'+t('hist.per_month','Prints per maand')+'</div>';
  mkeys.forEach(function(k){h+=bar(k,mon[k].n,maxN,mon[k].n+' prints · '+Math.round(mon[k].g)+' g');});}
 box.innerHTML=h;
}
function renderHist(){
 var showArch=$('hArch')&&$('hArch').checked;
 var q=(($('hSearch')&&$('hSearch').value)||'').toLowerCase();
 var fmat=($('hMat')&&$('hMat').value)||'',fok=($('hOk')&&$('hOk').value)||'',sort=($('hSort')&&$('hSort').value)||'date';
 var list=histCache.filter(function(r){return showArch?r.arch:!r.arch;});
 if(q)list=list.filter(function(r){return (r.name||'').toLowerCase().indexOf(q)>=0;});
 if(fmat)list=list.filter(function(r){return r.mat===fmat;});
 if(fok!=='')list=list.filter(function(r){return String(r.ok)===fok;});
 if(sort==='cost')list.sort(function(a,b){return (b.cost||0)-(a.cost||0);});
 else if(sort==='grams')list.sort(function(a,b){return (b.grams||0)-(a.grams||0);});
 else list.sort(function(a,b){return (b.ts||0)-(a.ts||0);});
 var tot=0,totg=0;list.forEach(function(r){tot+=r.cost||0;totg+=r.grams||0;});
 if($('histTotal'))$('histTotal').innerHTML=list.length+' '+(showArch?t('hist.archived','gearchiveerd'):t('prints','prints'))+' &middot; '+Math.round(totg)+' g &middot; '+t('total','totaal')+' <b>&euro; '+tot.toFixed(2)+'</b>';
 if(!list.length){$('histList').style.display='block';$('histList').innerHTML='<div class="muted">'+t('no_results','geen resultaten')+'</div>';hSelUpd();return;}
 var h='';
 list.forEach(function(r){
  var pv=r.file?'<button class="hprev" data-p="/cache/'+r.file+'" style="background:#2c3e50;color:#fff;border:0;border-radius:6px;padding:6px 10px;margin-top:8px;cursor:pointer;width:100%">'+t('preview','Preview')+'</button>':'';
  var dur=fmtDur(r.mins);
  h+='<div class="rollcard" style="flex-direction:column;align-items:stretch">'
    +'<div style="display:flex;align-items:center;gap:8px"><input type="checkbox" class="hsel" data-i="'+r.i+'" style="width:18px;height:18px;flex:0 0 auto"><span style="color:'+(r.ok?'#2ecc71':'#e74c3c')+'">'+(r.ok?'✓':'✗')+'</span><b style="flex:1;overflow:hidden;text-overflow:ellipsis;white-space:nowrap">'+r.name+'</b></div>'
    +'<div class="muted" style="font-size:12px">'+r.when+(r.mat?' · '+r.mat:'')+(dur?' · '+dur:'')+'</div>'
    +'<div style="margin-top:4px">'+r.grams+' g'+(r.cost>0?' &middot; <b>&euro; '+r.cost.toFixed(2)+'</b>':'')+'</div>'+pv+'</div>';
 });
 $('histList').style.display='grid';$('histList').style.gridTemplateColumns='repeat(auto-fill,minmax(220px,1fr))';$('histList').style.gap='10px';
 $('histList').innerHTML=h;
 document.querySelectorAll('.hprev').forEach(function(el){el.onclick=function(){showPreview(el.dataset.p);};});
 document.querySelectorAll('.hsel').forEach(function(el){el.onchange=hSelUpd;});
 hSelUpd();
}
function exportCsv(){
 var rows=[['datum','naam','materiaal','grammen','kosten_eur','duur_min','resultaat']];
 histCache.forEach(function(r){rows.push([r.when,'"'+(r.name||'').replace(/"/g,'""')+'"',r.mat||'',r.grams||0,(r.cost||0).toFixed(2),r.mins||0,r.ok?'gelukt':'mislukt']);});
 var csv=rows.map(function(x){return x.join(',');}).join('\r\n');
 var a=document.createElement('a');a.href=URL.createObjectURL(new Blob([csv],{type:'text/csv'}));a.download='printward-historie.csv';a.click();URL.revokeObjectURL(a.href);
}
function hSelIds(){var a=[];document.querySelectorAll('.hsel:checked').forEach(function(el){a.push(el.dataset.i);});return a;}
function hSelUpd(){var n=hSelIds().length;var showArch=$('hArch')&&$('hArch').checked;
 if($('hArchBtn')){$('hArchBtn').style.display=n?'inline-block':'none';$('hArchBtn').textContent=(showArch?t('hist.unarchive','Herstel'):t('hist.archive','Archiveer'))+' ('+n+')';}
 if($('hDelBtn')){$('hDelBtn').style.display=n?'inline-block':'none';$('hDelBtn').textContent=t('delete','Verwijder')+' ('+n+')';}}
function hBulk(act){var ids=hSelIds();if(!ids.length)return;
 if(act==='del'&&!confirm(ids.length+' '+t('confirm.del_items','item(s) definitief verwijderen?')))return;
 fetch('/hist_bulk?act='+act+'&idx='+ids.join(',')).then(function(){setTimeout(loadHistory,300);});}
document.querySelectorAll('nav button').forEach(function(b){b.onclick=function(){tab(b.dataset.tab);};});
document.querySelectorAll('.step button').forEach(function(b){b.onclick=function(){step=b.dataset.s;
 document.querySelectorAll('.step button').forEach(function(x){x.classList.remove('sel');});b.classList.add('sel');};});
document.querySelectorAll('.pad button,.zcol button').forEach(function(b){b.onclick=function(){
 fetch('/cmd?a='+b.dataset.a+'&s='+step).then(function(r){return r.text();}).then(function(txt){$('hint').textContent=txt;}).catch(function(){});};});
function mcmd(a,s){var u='/cmd?a='+a;if(s!==undefined&&s!=='')u+='&s='+s;fetch(u).then(function(r){return r.text();}).then(function(txt){if($('hint'))$('hint').textContent=txt;}).catch(function(){});}
$('bPause').onclick=function(){fetch('/ctl?a='+(curState==='PAUSE'?'resume':'pause'));};
$('bStop').onclick=function(){if(confirm(t('dash.stop_confirm_t','Print stoppen?')))fetch('/ctl?a=stop');};
$('bLight').onclick=function(){fetch('/ctl?a=light&v='+(curLight?0:1));};
$('bFan').onclick=function(){fetch('/ctl?a=fan&v='+(curFan>0?0:1));};
$('speed').onchange=function(){fetch('/ctl?a=speed&v='+$('speed').value);};
$('cView').onclick=function(){var b=$('cView');b.dataset.v=(b.dataset.v==='1'?'0':'1');viewBtn();};
$('cSave').onclick=function(){
 var q='ip='+encodeURIComponent($('cIp').value)+'&serial='+encodeURIComponent($('cSerial').value)+'&view3d='+($('cView').dataset.v||'0')+'&bri='+$('cBri').value;
 if($('cCode').value)q+='&code='+encodeURIComponent($('cCode').value);
 fetch('/setcfg?'+q).then(function(r){return r.text();}).then(function(txt){$('cMsg').textContent=txt+' – '+t('set.connecting','verbinden…');$('cCode').value='';});
};
$('cNtfySave').onclick=function(){fetch('/setcfg?ntfy='+encodeURIComponent($('cNtfy').value)).then(function(r){return r.text();}).then(function(txt){$('cNtfyMsg').textContent=txt;});};
$('cNtfyTest').onclick=function(){$('cNtfyMsg').textContent=t('set.sending','versturen…');fetch('/setcfg?ntfy='+encodeURIComponent($('cNtfy').value)).then(function(){setTimeout(function(){fetch('/notify_test').then(function(r){return r.text();}).then(function(txt){$('cNtfyMsg').textContent=txt;});},500);});};
$('cLowSave').onclick=function(){var g=parseInt($('cLow').value,10);if(isNaN(g)||g<0){$('cNtfyMsg').textContent=t('invalid_number','ongeldig getal');return;}fetch('/setcfg?low='+g).then(function(r){return r.text();}).then(function(txt){lowG=g;$('cNtfyMsg').textContent=t('set.threshold_saved','drempel opgeslagen')+': '+g+' g';});};
function joinPath(b,n){return (b==='/'?'':b)+'/'+n;}
function fmtSize(b){return b>1048576?(b/1048576).toFixed(1)+' MB':b>1024?(b/1024).toFixed(0)+' KB':b+' B';}
function loadFiles(path){curPath=path;$('fpath').textContent=path;$('flist').innerHTML='<div class=muted>'+t('loading','laden…')+'</div>';
 fetch('/files?path='+encodeURIComponent(path)).then(function(r){return r.json();}).then(function(d){
  if(!d.ok){$('flist').innerHTML='<div class=muted>'+t('error','fout')+': '+(d.err||'')+'</div>';return;}
  var h='';
  d.items.forEach(function(it){
   if(it.dir)h+='<div class="fitem" data-dir="'+esc(it.name)+'"><b>📁 '+esc(it.name)+'</b><span></span></div>';
   else {var fp=joinPath(path,it.name);
    var pv=/\.3mf$/i.test(it.name)?'<button class="fprev" data-p="'+esc(fp)+'" '+'title="'+t('files.show_preview','Toon voorbeeld')+'" style="background:#2c3e50;color:#fff;border:0;border-radius:6px;padding:6px 10px;cursor:pointer">Preview</button>':'';
    h+='<div class="fitem"><input type="checkbox" class="fsel" data-p="'+esc(fp)+'" style="width:20px;height:20px;flex:0 0 auto"><span style="flex:1;min-width:0;overflow:hidden;text-overflow:ellipsis">📄 '+esc(it.name)+'</span><span class=muted>'+fmtSize(it.size)+'</span>'+pv+'</div>';}
  });
  $('flist').innerHTML=h||'<div class=muted>'+t('empty','leeg')+'</div>';
  document.querySelectorAll('.fitem[data-dir]').forEach(function(el){el.onclick=function(){loadFiles(joinPath(path,el.dataset.dir));};});
  document.querySelectorAll('.fsel').forEach(function(el){el.onchange=fSelUpd;});fSelUpd();
  document.querySelectorAll('.fprev').forEach(function(el){el.onclick=function(e){e.stopPropagation();showPreview(el.dataset.p);};});
 }).catch(function(){$('flist').innerHTML='<div class=muted>'+t('no_conn','geen verbinding')+'</div>';});
}
$('fUp').onclick=function(){var p=curPath.replace(/\/[^\/]*\/?$/,'');loadFiles(p||'/');};
$('fRef').onclick=function(){loadFiles(curPath);};
function fSelPaths(){var a=[];document.querySelectorAll('.fsel:checked').forEach(function(el){a.push(el.dataset.p);});return a;}
function fSelUpd(){var n=fSelPaths().length;var b=$('fDel');if(b){b.style.display=n?'inline-block':'none';b.textContent=t('delete','Verwijder')+' ('+n+')';}}
$('fDel').onclick=function(){var ps=fSelPaths();if(!ps.length)return;
 if(!confirm(ps.length+' '+t('confirm.del_files','bestand(en) definitief van de printer verwijderen?')))return;
 var i=0;(function nxt(){if(i>=ps.length){loadFiles(curPath);return;}
  fetch('/delete?path='+encodeURIComponent(ps[i])).then(function(){i++;nxt();}).catch(function(){i++;nxt();});})();};
function showPreview(p){var m=$('prevModal');if(!m)return;$('prevBody').innerHTML='<div class="muted">'+t('preview_loading','voorbeeld laden…')+'</div>';m.style.display='flex';
 var img=new Image();img.style.maxWidth='80vw';img.style.maxHeight='70vh';img.style.borderRadius='8px';
 img.onload=function(){$('prevBody').innerHTML='';$('prevBody').appendChild(img);};
 img.onerror=function(){$('prevBody').innerHTML='<div class="muted">'+t('preview_none','geen voorbeeld beschikbaar')+'</div>';};
 img.src='/thumb?path='+encodeURIComponent(p);}
function setPlate(v){pthumbFile='';fetch('/setplate?p='+encodeURIComponent(v));}
function amt(t){
 if(t.gram>=0){return '<div class="muted"'+(t.gram<lowG?' style="color:#e74c3c;font-weight:600"':'')+'>'+t.gram+' g</div>';}
 return '<div class="muted">'+(t.remain>=0?t.remain+'%':'–')+'</div>';
}
function humHtml(h){if(!h||h<1)return'';var l,c;if(h<=2){l=t('dash.hum_dry','droog');c='#2ecc71';}else if(h===3){l=t('dash.hum_ok','redelijk');c='#f39c12';}else{l=t('dash.hum_wet','vochtig');c='#e74c3c';}return ' · <span style="color:'+c+'">'+t('dash.humidity','vocht')+': '+l+'</span>';}
function slotName(slot){return slot===254?'externe spoel':('AMS'+(Math.floor(slot/4)+1)+' T'+(slot%4+1));}
function trayCell(t,slot,assign){
 var inner;
 if(t&&t.present)inner='<div class="sw" style="background:'+t.rgb+'"></div><div>'+t.type+'</div>'+amt(t);
 else inner='<div class="sw empty"></div><div class="muted">'+t('spools.empty_slot','leeg')+'</div><div></div>';
 if(assign)inner+='<button class="rolbtn" onclick="pickRoll('+slot+')">Rol</button>';
 var act=(lastS&&lastS.active_tray===slot)?' style="outline:2px solid #2ecc71;outline-offset:3px;border-radius:6px" title="Wordt nu gebruikt"':'';
 return '<div class="tray"'+act+'>'+inner+'</div>';
}
function amsHtml(ams,ext,assign){
 var h='';
 (ams||[]).forEach(function(u){
  h+='<div class="amsbox"><div class="muted">AMS '+u.id+humHtml(u.humidity)+'</div><div class="trays">';
  u.trays.forEach(function(t,ti){h+=trayCell(t,(u.id-1)*4+ti,assign);});
  h+='</div></div>';
 });
 if((ext&&ext.present)||assign)h+='<div class="amsbox"><div class="muted">'+t('spools.external','Externe spoel')+'</div><div class="trays">'+trayCell(ext,254,assign)+'</div></div>';
 return h||'<div class="muted">'+t('dash.no_ams','geen AMS')+'</div>';
}
function slotMat(slot){if(!lastS)return'';if(slot===254)return(lastS.ext&&lastS.ext.type)||'';var u=Math.floor(slot/4),t=slot%4;if(lastS.ams)for(var i=0;i<lastS.ams.length;i++){if(lastS.ams[i].id===u+1){var tr=lastS.ams[i].trays[t];return(tr&&tr.type)||'';}}return'';}
function pickRoll(slot){pickSlot=slot;$('rollTitle').textContent=t('spools.pick_for','Kies rol voor')+' '+slotName(slot);
 var mat=slotMat(slot);
 fetch('/spools').then(function(r){return r.json();}).then(function(list){
  list.sort(function(a,b){if(mat){var am=(a.material===mat)?0:1,bm=(b.material===mat)?0:1;if(am!==bm)return am-bm;}return (a.num||0)-(b.num||0);});
  var h='<div class="fitem rollpick" onclick="clearRoll()"><div style="display:flex;align-items:center;gap:8px"><div class="sw" style="width:26px;height:20px;flex:0 0 auto;background:#555b63;border:1px solid #888"></div><span><b>'+t('spools.empty_slot','Leeg')+'</b> <span class="muted">'+t('spools.no_roll','geen rol in dit slot')+'</span></span></div></div>';
  if(!list.length)h+='<div class="muted">'+t('spools.none_yet','Nog geen rollen — maak ze aan op de Spools-tab.')+'</div>';
  list.forEach(function(s){var pas=(mat&&s.material===mat)?' <span class="badge" title="'+t('spools.same_mat','Zelfde materiaal als er nu in dit slot zit')+'" style="margin-left:6px;background:#1e5f3a;color:#b9f5cf">'+s.material+' &#10003;</span>':'';
   h+='<div class="fitem rollpick" onclick="chooseRoll('+s.i+')"><div style="display:flex;align-items:center;gap:8px"><span class="rnum">#'+(s.num||0)+'</span><div class="sw" style="width:26px;height:20px;flex:0 0 auto;background:'+esc(s.rgb)+'"></div><span><b>'+esc(s.name)+'</b> <span class="muted">'+esc(s.material)+' · '+s.rem+' g</span>'+pas+'</span></div></div>';});
  $('rollList').innerHTML=h;$('rollModal').style.display='flex';
 }).catch(function(){});
}
function chooseRoll(i){fetch('/spool_load?idx='+i+'&slot='+pickSlot).then(function(){closeRoll();});}
function clearRoll(){fetch('/spool_clear?slot='+pickSlot).then(function(){closeRoll();});}
function closeRoll(){$('rollModal').style.display='none';}
function poll(){
 fetch('/status').then(function(r){return r.json();}).then(function(s){
  $('conn').textContent=s.conn?'● '+t('diag.connected','verbonden'):'○ '+t('diag.offline','offline');$('conn').style.color=s.conn?'#2ecc71':'#e74c3c';
  curState=s.state;curLight=s.light;curFan=s.fan;lastS=s;
  $('state').textContent=s.state?t(STATE_KEY[s.state]||'',s.state):'–';$('task').textContent=s.name||'';
  $('fill').style.width=(s.pct||0)+'%';
  var eta='';if(s.remain>0){var f=new Date(Date.now()+s.remain*60000);eta='  '+t('dash.eta','klaar')+' '+('0'+f.getHours()).slice(-2)+':'+('0'+f.getMinutes()).slice(-2);}
  $('prog').textContent=(s.pct||0)+'%'+(s.total?('  '+t('dash.layer','laag')+' '+s.layer+'/'+s.total):'')+(s.remain?('  ~'+s.remain+' '+t('min','min')+eta):'');
  if($('pcost'))$('pcost').innerHTML=(s.printg>=0)?(t('dash.this_print','deze print')+': <b>'+s.printg+' g</b>'+(s.printcost>0?(' &middot; <b>&euro; '+s.printcost.toFixed(2)+'</b>'):'')):'';
  if($('pthumb')){var pf=((s.state==='RUNNING'||s.state==='PAUSE')&&s.file)?s.file:'';var pkey=pf+'#'+(s.plate||1);if(pkey!==pthumbFile){pthumbFile=pkey;if(pf){var pth=pf.charAt(0)==='/'?pf:'/cache/'+pf;var im=$('pthumb');im.onload=function(){im.style.display='block';};im.onerror=function(){im.style.display='none';};im.src='/thumb?path='+encodeURIComponent(pth)+'&plate='+(s.plate||1);}else{$('pthumb').style.display='none';}}}
  var pp=$('platepick');if(pp){if(s.plates>1){var sel=$('plateSel');if(sel.dataset.n!==String(s.plates)){sel.dataset.n=String(s.plates);var h='<option value="0">'+t('dash.plate_auto','Automatisch')+'</option>';for(var pi=1;pi<=s.plates;pi++)h+='<option value="'+pi+'">'+t('dash.plate','Plaat')+' '+pi+'</option>';sel.innerHTML=h;}if(document.activeElement!==sel)sel.value=String(s.mplate||0);pp.style.display='flex';}else pp.style.display='none';}
  var w=$('warn');if(w){if(s.short>0){w.style.display='block';w.textContent='⚠ '+t('dash.warn_short','Filament tekort')+' — ~'+s.short+' g '+t('dash.short_body','te kort voor deze print op het actieve slot.');}else{w.style.display='none';}}
  $('noz').textContent=s.nozzle+'/'+s.nozzle_t+'°';$('bed').textContent=s.bed+'/'+s.bed_t+'°';$('cham').textContent=s.chamber+'°';
  $('bPause').textContent=(s.state==='PAUSE')?t('dash.resume','Resume'):t('dash.pause','Pause');
  $('bLight').textContent=t('dash.light','Licht')+': '+(s.light?t('on','aan'):t('off','uit'));
  $('bFan').textContent=t('dash.fan','Fan')+' '+s.fan+'%';
  if(document.activeElement!==$('speed'))$('speed').value=s.speed;
  if(!$('rollModal')||$('rollModal').style.display!=='flex'){$('amsStrip').innerHTML=amsHtml(s.ams,s.ext,true);}
  if($('movetemps'))$('movetemps').textContent=t('dash.nozzle','nozzle')+' '+s.nozzle+'/'+s.nozzle_t+'° · '+t('dash.bed','bed')+' '+s.bed+'/'+s.bed_t+'° · '+t('dash.chamber','kamer')+' '+s.chamber+'°';
  if(s.prints!==undefined&&$('stPrints')){$('stPrints').textContent=s.prints;$('stUsed').textContent=(s.used>=1000?(s.used/1000).toFixed(2)+' kg':s.used+' g');if($('stCost'))$('stCost').textContent='€ '+(s.cost||0).toFixed(2);}
  if(s.bkage!==undefined)renderBkStatus(s.bkage);
  if(s.cfg&&s.cfg.scale_ip){scaleHost=s.cfg.scale_ip;var si=$('sIp');if(si&&!si.value)si.value=s.cfg.scale_ip;}
  if(s.cfg&&s.cfg.low)lowG=s.cfg.low;
  if(!cfgFilled&&s.cfg){cfgFilled=true;
   $('cIp').value=s.cfg.ip;$('cSerial').value=s.cfg.serial;$('cBri').value=s.cfg.bri;
   var b=$('cView');b.dataset.v=s.cfg.view3d?'1':'0';viewBtn();
   if(s.cfg.code_set)$('cCode').placeholder='•••••••• '+t('set.code_set','(ingesteld, laat leeg = ongewijzigd)');
   if(s.cfg.ntfy!==undefined)$('cNtfy').value=s.cfg.ntfy;
   if(s.cfg.low!==undefined&&$('cLow'))$('cLow').value=s.cfg.low;
  }
 }).catch(function(){$('conn').textContent='○ '+t('no_tablet','geen tablet');$('conn').style.color='#e74c3c';});
}
setInterval(poll,1500);poll();
function tickClock(){var d=new Date();if($('clock'))$('clock').textContent=('0'+d.getHours()).slice(-2)+':'+('0'+d.getMinutes()).slice(-2);}
setInterval(tickClock,10000);tickClock();
if($('cLang'))$('cLang').onchange=function(){setLang(this.value);};
loadLang();loadLangList();
function sMsg(t){$('sMsg').textContent=t;}
function scalePoll(){
 if(!$('scale').classList.contains('on')||!scaleHost)return;
 fetch('http://'+scaleHost+'/weight?t='+Date.now()).then(function(r){return r.json();}).then(function(d){
  $('swt').textContent=d.g.toFixed(0)+' g';$('sst').textContent=d.stable?t('scale.stable','stabiel'):t('scale.measuring','…meten');
 }).catch(function(){$('sst').textContent=t('scale.no_conn','geen verbinding met schaal');});
 fetch('http://'+scaleHost+'/info?t='+Date.now()).then(function(r){return r.json();}).then(function(d){
  $('sInfo').textContent=(d.connected?t('diag.connected','verbonden'):t('scale.ap_mode','AP-modus'))+' · '+d.ip+' · '+d.ssid+' · '+d.rssi+'dBm'+(d.static?' · '+t('scale.static_ip','vast IP'):'');
 }).catch(function(){});
}
function sGet(p){fetch('http://'+scaleHost+p).then(function(r){return r.text();}).then(sMsg).catch(function(){sMsg(t('scale.no_conn','geen verbinding met schaal'));});}
$('sTare').onclick=function(){sGet('/tare');};
$('sCal').onclick=function(){sGet('/cal?known='+$('sKnown').value);};
$('sIpFix').onclick=function(){sGet('/setip?ip='+encodeURIComponent($('sIp').value));};
$('sWifi').onclick=function(){sGet('/setwifi?ssid='+encodeURIComponent($('sSsid').value)+'&pass='+encodeURIComponent($('sPass').value));};
$('sIpSave').onclick=function(){var v=$('sIp').value;fetch('/setcfg?scale_ip='+encodeURIComponent(v)).then(function(){scaleHost=v;sMsg(t('scale.saved_tablet','opgeslagen op tablet'));});};
setInterval(scalePoll,1200);
setInterval(function(){if($('spList')&&$('spList').offsetParent!==null)loadSpools();},3000);
setInterval(function(){if($('histList')&&$('histList').offsetParent!==null)loadHistory();},8000);
function spMsg(t){$('spMsg').textContent=t;}
function spSlotOpts(){var o='';for(var u=0;u<2;u++)for(var tr=0;tr<4;tr++){o+='<option value="'+(u*4+tr)+'">AMS'+(u+1)+' T'+(tr+1)+'</option>';}return o+'<option value="254">'+t('spools.external','Externe spoel')+'</option>';}
function loadSpools(){
 fetch('/spools').then(function(r){return r.json();}).then(function(list){spCache=list;
  var mats={};list.forEach(function(s){if(s.material)mats[s.material]=1;});
  var sel=$('spFMat');if(sel){var cur=sel.value,o='<option value="">'+t('hist.all_materials','alle materialen')+'</option>';Object.keys(mats).sort().forEach(function(m){o+='<option>'+m+'</option>';});sel.innerHTML=o;sel.value=cur;}
  if($('spNum')&&$('spIdx')&&$('spIdx').value==-1&&!$('spNum').value)$('spNum').value=nextSpNum();
  renderInv();renderSpList();})
 .catch(function(){$('spList').innerHTML='<div class="muted">'+t('no_tablet','geen tablet')+'</div>';});
}
function mLen(g,mat){var d={PLA:1.24,PETG:1.27,ABS:1.04,ASA:1.07,TPU:1.21,PC:1.20,PA:1.14,PVA:1.23}[mat]||1.24;return g/(d*2.4053);}
function spGrams(s){return (s.live!=null?s.live:s.rem)||0;}
function renderInv(){
 var box=$('spInv');if(!box)return;
 if(!spCache.length){box.style.display='none';return;}
 box.style.display='block';
 var totG=0,totV=0,mm={};
 spCache.forEach(function(s){var g=spGrams(s);totG+=g;if(s.price>0)totV+=g*s.price/1000;var k=s.material||'?';if(!mm[k])mm[k]={g:0,n:0,v:0};mm[k].g+=g;mm[k].n++;if(s.price>0)mm[k].v+=g*s.price/1000;});
 var h='<h3>'+t('spools.stock','Voorraad')+'</h3><div style="font-size:16px"><b>'+spCache.length+'</b> '+t('spools.rolls','rollen')+' &middot; <b>'+(totG/1000).toFixed(2)+' kg</b> '+t('spools.filament','filament')+(totV>0?' &middot; '+t('spools.value','waarde')+' <b>&euro; '+totV.toFixed(2)+'</b>':'')+'</div>';
 var mk=Object.keys(mm).sort(function(a,b){return mm[b].g-mm[a].g;}),maxG=0;
 mk.forEach(function(k){if(mm[k].g>maxG)maxG=mm[k].g;});
 h+='<div style="margin-top:8px">';
 mk.forEach(function(k){h+=bar(k+' <span class="muted">('+mm[k].n+')</span>',mm[k].g,maxG,(mm[k].g/1000).toFixed(2)+' kg'+(mm[k].v>0?' · € '+mm[k].v.toFixed(2):''));});
 box.innerHTML=h+'</div>';
}
function spMatch(s){
 var q=(($('spSearch')&&$('spSearch').value)||'').toLowerCase();
 var fmat=($('spFMat')&&$('spFMat').value)||'';
 if(q&&(s.name+' '+s.material).toLowerCase().indexOf(q)<0)return false;
 if(fmat&&s.material!==fmat)return false;
 if($('spLow')&&$('spLow').checked&&!(spGrams(s)<lowG))return false;
 return true;
}
function renderSpList(){
 var sort=($('spSort')&&$('spSort').value)||'name';
 var arr=spCache.filter(spMatch);
 arr.sort(function(a,b){
  if(sort==='num')return (a.num||0)-(b.num||0);
  if(sort==='low')return spGrams(a)-spGrams(b);
  if(sort==='high')return spGrams(b)-spGrams(a);
  if(sort==='price')return (b.price||0)-(a.price||0);
  if(sort==='mat')return (a.material||'').localeCompare(b.material||'')||(a.name||'').localeCompare(b.name||'');
  return (a.name||'').localeCompare(b.name||'');});
 var h='';
 arr.forEach(function(s){
  var g=spGrams(s),m=mLen(g,s.material);
  var sl=(s.slot!=null&&s.slot>=0)?' <span class="badge" style="background:#1e4e6e;color:#bfe3ff">'+slotName(s.slot)+'</span>':'';
  var low=g<lowG?' <span class="badge" style="background:#5a2020;color:#ffd0d0">'+t('spools.almost_empty','bijna leeg')+'</span>':'';
  h+='<div class="rollcard"><input type="checkbox" '+(spSel[s.i]?'checked':'')+' onchange="selToggle('+s.i+',this.checked)" style="width:20px;height:20px;flex:0 0 auto">'
   +'<div style="width:38px;height:38px;border-radius:8px;flex:0 0 auto;border:1px solid #3a434d;background:'+s.rgb+'"></div>'
   +'<div style="min-width:0"><div class="name"><span class="rnum">#'+(s.num||0)+'</span> '+esc(s.name)+'<span class="badge">'+esc(s.material)+'</span>'+sl+low+'</div>'
   +(s.note?'<div class="muted" style="font-size:12px">'+esc(s.note)+'</div>':'')
   +(s.price>0?'<div class="muted" style="font-size:12px">€ '+s.price.toFixed(2)+'/kg</div>':'')+'</div>'
   +'<div class="grams">'+g+' g<div class="muted" style="font-size:12px;font-weight:400">&asymp; '+Math.round(m)+' m'+(s.price>0?' · € '+(g*s.price/1000).toFixed(2):'')+'</div></div>'
   +'<button class="iconbtn" title="'+t('spools.weigh_roll','Weeg de rol')+'" onclick="spWeigh('+s.i+')">⚖</button>'
   +'<button class="iconbtn" title="'+t('copy','Kopieer')+'" onclick="spCopy('+s.i+')">⧉</button>'
   +'<button class="iconbtn" title="'+t('edit','Bewerk')+'" onclick="spEdit('+s.i+')">✎</button>'
   +'<button class="iconbtn del" title="'+t('delete','Verwijder')+'" onclick="spDel('+s.i+')">✕</button></div>';
 });
 $('spList').innerHTML=h||'<div class="muted">'+t('spools.none_found','Geen rollen gevonden.')+'</div>';
 updateBulk();
}
function selToggle(i,c){if(c)spSel[i]=true;else delete spSel[i];updateBulk();}
function selIds(){return Object.keys(spSel);}
function updateBulk(){var bar=$('spBulk');if(!bar)return;var ids=selIds();bar.style.display=ids.length?'flex':'none';if($('spSelCount'))$('spSelCount').textContent=ids.length+' '+t('spools.selected','geselecteerd');}
function toggleAll(c){spSel={};if(c){spCache.forEach(function(s){if(spMatch(s))spSel[s.i]=true;});}renderSpList();}
function clearSel(){spSel={};if($('spAll'))$('spAll').checked=false;renderSpList();}
function bulkGo(qs){fetch('/spool_bulk?idx='+selIds().join(',')+'&'+qs).then(function(){spSel={};if($('spAll'))$('spAll').checked=false;setTimeout(loadSpools,300);});}
function bulkDel(){if(confirm(selIds().length+' '+t('confirm.del_rolls','rollen verwijderen?')))bulkGo('act=del');}
function bulkColorGo(){bulkGo('act=color&v='+encodeURIComponent($('bulkColor').value));}
function bulkWeightGo(){if($('bulkWeight').value!=='')bulkGo('act=weight&v='+$('bulkWeight').value);}
function bulkMatGo(){if($('bulkMat').value)bulkGo('act=material&v='+encodeURIComponent($('bulkMat').value));}
function bulkPriceGo(){if($('bulkPrice').value!=='')bulkGo('act=price&v='+$('bulkPrice').value);}
function spRemWeigh(){if(!scaleHost){alert(t('scale.no_ip_tab','Geen schaal-IP bekend — stel het in op de Scale-tab.'));return;}
 fetch('http://'+scaleHost+'/weight?t='+Date.now()).then(function(r){return r.json();}).then(function(d){var e=parseFloat($('spEmpty').value)||0;$('spRem').value=Math.max(0,Math.round(d.g-e));}).catch(function(){alert(t('scale.no_conn_alert','Geen verbinding met de schaal.'));});}
function spWeigh(i){var s=spCache[i];if(!scaleHost){alert(t('scale.no_ip','Geen schaal-IP bekend.'));return;}
 fetch('http://'+scaleHost+'/weight?t='+Date.now()).then(function(r){return r.json();}).then(function(d){
  var rem=Math.max(0,Math.round(d.g-s.empty));
  if(!confirm(t('spools.weigh_roll','Weeg rol')+' \''+s.name+'\'?\n'+t('spools.weighed','Gewogen')+' '+Math.round(d.g)+' g − '+t('spools.empty_weight','lege spoel')+' '+s.empty+' g = '+rem+' g '+t('dash.left','resterend')+'.'))return;
  fetch('/spool_save?idx='+i+'&name='+encodeURIComponent(s.name)+'&material='+encodeURIComponent(s.material)+'&color='+encodeURIComponent(s.rgb)+'&rem='+rem+'&empty='+s.empty+'&nmin='+(s.nmin||0)+'&nmax='+(s.nmax||0)+'&code='+encodeURIComponent(s.code||'')+'&note='+encodeURIComponent(s.note||'')+'&price='+(s.price||0)).then(function(){setTimeout(loadSpools,300);});
 }).catch(function(){alert(t('scale.no_conn_alert','Geen verbinding met de schaal.'));});}
function spCopy(i){var s=spCache[i];fetch('/spool_save?idx=&name='+encodeURIComponent(s.name+' (kopie)')+'&material='+encodeURIComponent(s.material)+'&color='+encodeURIComponent(s.rgb)+'&rem='+s.rem+'&empty='+s.empty+'&nmin='+(s.nmin||0)+'&nmax='+(s.nmax||0)+'&code='+encodeURIComponent(s.code||'')+'&note='+encodeURIComponent(s.note||'')+'&price='+(s.price||0)).then(function(){setTimeout(loadSpools,300);});}
function spLoad(i){var sl=$('ss'+i).value;fetch('/spool_load?idx='+i+'&slot='+sl).then(function(r){return r.text();}).then(function(txt){spMsg(txt+' '+t('spools.in_slot','in slot'));});}
function spDel(i){if(confirm(t('confirm.del_roll','Rol verwijderen?')))fetch('/spool_del?idx='+i).then(function(){loadSpools();});}
function spEdit(i){var s=spCache[i];$('spTitle').textContent=t('spools.edit_roll','Rol bewerken');$('spIdx').value=i;
 $('spName').value=s.name;$('spMat').value=s.material;$('spColor').value=s.rgb;$('spRem').value=s.rem;$('spEmpty').value=s.empty;
 $('spNmin').value=s.nmin||'';$('spNmax').value=s.nmax||'';$('spCode').value=s.code||'';$('spNote').value=s.note||'';$('spPrice').value=s.price||'';if($('spNum'))$('spNum').value=s.num||'';}
function nextSpNum(){var m=0;spCache.forEach(function(s){if((s.num||0)>m)m=s.num;});return m+1;}
function spReset(){$('spTitle').textContent=t('spools.new_roll','Nieuwe rol');$('spIdx').value=-1;$('spName').value='';$('spNote').value='';$('spCode').value='';$('spNmin').value='';$('spNmax').value='';$('spPrice').value='';if($('spNum'))$('spNum').value=nextSpNum();}
$('spNew').onclick=spReset;
$('spSave').onclick=function(){
 var q='idx='+$('spIdx').value+'&name='+encodeURIComponent($('spName').value)+'&material='+encodeURIComponent($('spMat').value)
  +'&color='+encodeURIComponent($('spColor').value)+'&rem='+($('spRem').value||0)+'&empty='+($('spEmpty').value||0)
  +'&nmin='+($('spNmin').value||0)+'&nmax='+($('spNmax').value||0)+'&code='+encodeURIComponent($('spCode').value)+'&note='+encodeURIComponent($('spNote').value)+'&price='+($('spPrice').value||0)+'&num='+($('spNum').value||0);
 fetch('/spool_save?'+q).then(function(r){return r.text();}).then(function(txt){spMsg(txt);spReset();setTimeout(loadSpools,300);});
};
function loadEmpties(){
 fetch('/empties').then(function(r){return r.json();}).then(function(list){
  emCache=list;
  var sel=$('spEmptySel'),cur=sel.value,o='<option value="">'+t('spools.pick_library','— kies uit bibliotheek —')+'</option>';
  list.forEach(function(e){o+='<option value="'+e.weight+'">'+esc(e.name)+' ('+e.weight+' g)</option>';});
  sel.innerHTML=o;sel.value=cur;
  var h='';
  list.forEach(function(e){h+='<div class="chiprow"><span>'+esc(e.name)+'</span><span class="badge" style="margin-left:0">'+e.weight+' g</span><button class="iconbtn del" style="margin-left:auto" onclick="emDel('+e.i+')">✕</button></div>';});
  $('emList').innerHTML=h||'<div class="muted">'+t('empties.none_yet','Nog geen lege spoelen.')+'</div>';
 }).catch(function(){});
}
$('spEmptySel').onchange=function(){if(this.value)$('spEmpty').value=this.value;};
$('emSave').onclick=function(){
 var q='idx=&name='+encodeURIComponent($('emName').value)+'&weight='+($('emWeight').value||0);
 fetch('/empty_save?'+q).then(function(){$('emName').value='';setTimeout(loadEmpties,300);});
};
function emDel(i){if(confirm(t('confirm.delete','Verwijderen?')))fetch('/empty_del?idx='+i).then(function(){loadEmpties();});}
function emWeigh(){if(!scaleHost){alert(t('scale.no_ip_tab','Geen schaal-IP bekend — stel het in op de Scale-tab.'));return;}
 fetch('http://'+scaleHost+'/weight?t='+Date.now()).then(function(r){return r.json();}).then(function(d){$('emWeight').value=Math.round(d.g);}).catch(function(){alert(t('scale.no_conn_alert','Geen verbinding met de schaal.'));});}
// A snapshot on the tablet only survives an app reinstall - not a wipe or a dead
// tablet. So we track when a backup last actually LEFT the device and say so.
function renderBkStatus(age){
 var b=$('bkStatus'),btn=$('bkDl');if(!b)return;
 b.style.display='block';
 var d=(age>=0)?Math.floor(age/86400):-1;
 var overdue=(d<0||d>=14);
 if(d<0){b.style.background='#5a2020';b.style.color='#ffd0d0';
  b.innerHTML='&#9888; <b>'+t('bk.never','Nog nooit een back-up gedownload.')+'</b> '+t('bk.only_tablet','Je data staat alleen op de tablet.');}
 else if(overdue){b.style.background='#5a2020';b.style.color='#ffd0d0';
  b.innerHTML='&#9888; '+t('bk.last','Laatste back-up')+' <b>'+d+' '+t('days','dagen')+'</b> '+t('ago','geleden')+' &mdash; '+t('bk.time_for_new','tijd voor een nieuwe.');}
 else{b.style.background='#1e3a28';b.style.color='#b9f5cf';
  b.innerHTML='&#10003; '+t('bk.last','Laatste back-up')+' '+(d===0?t('today','vandaag'):(d+' '+t('days','dagen')+' '+t('ago','geleden')))+'.';}
 if(btn)btn.className='formbtn '+(overdue?'pri':'sec');
}
function downloadBackup(){
 var a=document.createElement('a');a.href='/backup';a.download='printward-backup.ptb';
 document.body.appendChild(a);a.click();a.remove();
 if($('bkMsg'))$('bkMsg').textContent=t('bk.downloaded','back-up gedownload');
}
// Rolls-only export/import: just the spool library, as a shareable JSON file.
// Import ADDS rolls (no wipe) - that is the whole point of keeping it separate
// from the full backup, which does overwrite everything.
function exportRolls(){
 var m=$('rollExpMsg');
 Promise.all([fetch('/spools').then(function(r){return r.json();}),fetch('/empties').then(function(r){return r.json();})]).then(function(a){
  var blob=new Blob([JSON.stringify({spools:a[0],empties:a[1]},null,1)],{type:'application/json'});
  var url=URL.createObjectURL(blob),link=document.createElement('a');
  link.href=url;link.download='printward_rollen.json';link.click();URL.revokeObjectURL(url);
  if(m)m.textContent=(a[0]||[]).length+' '+t('roll.exported','rollen geexporteerd');
 }).catch(function(){if(m)m.textContent=t('no_conn','geen verbinding');});
}
function importRolls(files){
 if(!files||!files.length)return;
 var m=$('rollExpMsg');
 var rd=new FileReader();
 rd.onload=function(){
  var d;
  try{ d=JSON.parse(rd.result); }catch(e){ if(m)m.textContent=t('invalid_file','Ongeldig bestand.'); return; }
  if(!d.spools&&!d.empties){ if(m)m.textContent=t('roll.none_in_file','Geen rollen in dit bestand.'); return; }
  var ns=(d.spools||[]).length;
  if(!confirm(ns+' '+t('roll.import_confirm','rol(len) toevoegen aan je bibliotheek? Bestaande rollen blijven staan.')))return;
  var chain=Promise.resolve();
  (d.empties||[]).forEach(function(e){chain=chain.then(function(){return fetch('/empty_save?idx=&name='+encodeURIComponent(e.name)+'&weight='+e.weight);});});
  (d.spools||[]).forEach(function(s){chain=chain.then(function(){return fetch('/spool_save?idx=&name='+encodeURIComponent(s.name)+'&material='+encodeURIComponent(s.material)+'&color='+encodeURIComponent(s.rgb)+'&rem='+(s.rem||0)+'&empty='+(s.empty||0)+'&nmin='+(s.nmin||0)+'&nmax='+(s.nmax||0)+'&code='+encodeURIComponent(s.code||'')+'&note='+encodeURIComponent(s.note||'')+'&price='+(s.price||0));});});
  chain.then(function(){if(m)m.textContent=ns+' '+t('roll.imported','rollen toegevoegd.');loadSpools();loadEmpties();})
       .catch(function(){if(m)m.textContent=t('import_failed','importeren mislukt');});
 };
 rd.readAsText(files[0]);
}
function importBackup(files){
 if(!files||!files.length)return;
 if(!confirm(t('bk.restore_confirm','Volledige back-up terugzetten? Dit OVERSCHRIJFT je huidige rollen, gewichten, historie en statistieken op de tablet.')))return;
 var rd=new FileReader();
 rd.onload=function(){
  fetch('/restore',{method:'POST',body:rd.result}).then(function(r){return r.text();}).then(function(txt){
   $('bkMsg').textContent=txt;setTimeout(function(){loadSpools();loadEmpties();loadHistory();},900);
  }).catch(function(){$('bkMsg').textContent=t('bk.restore_failed','herstel mislukt');});
 };
 rd.readAsText(files[0]);
}
function setSpColor(c){$('spColor').value=c;}
function buildSwatches(){var p=['#111111','#eeeeee','#9aa0a6','#c0392b','#e67e22','#f1c40f','#27ae60','#2980b9','#8e44ad','#ec407a','#6d4c41','#16a085'];var h='';p.forEach(function(c){h+='<button class="swatch" style="background:'+c+'" onclick="setSpColor(\''+c+'\')"></button>';});if($('spSw'))$('spSw').innerHTML=h;}
buildSwatches();
</script></body></html>)PAGE"
