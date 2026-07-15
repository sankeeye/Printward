// The web control single-page app, as a C++ raw string literal. Included by
// webctl.cpp as the initializer of PAGE_HTML. Kept in its own file so the HTML/
// CSS/JS is editable on its own. Self-contained: inline CSS + vanilla JS, dark,
// mobile-first. Mirrors the tablet: Dashboard, Filament, Files, Move, Settings.
R"PAGE(<!doctype html>
<html lang="nl"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>FilaTrack</title>
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
<header><div class="t">FilaTrack</div><div style="display:flex;gap:14px;align-items:center"><span id="clock" class="muted"></span><span id="conn" class="muted">verbinden…</span></div></header>
<nav>
 <button data-tab="dash" class="on">Dashboard</button>
 <button data-tab="files">Files</button>
 <button data-tab="move">Move</button>
 <button data-tab="spools">Spools</button>
 <button data-tab="hist">Historie</button>
 <button data-tab="set">Settings</button>
</nav>

<section id="dash" class="on">
 <div id="warn" style="display:none;background:#5a2020;color:#ffd0d0;border-radius:10px;padding:12px 14px;margin-bottom:14px;font-weight:600"></div>
 <div class="card"><div id="state">–</div><div id="task" class="muted"></div>
  <div class="bar"><div id="fill"></div></div><div id="prog" class="muted"></div>
  <div id="pcost" class="muted" style="margin-top:4px"></div>
  <img id="pthumb" alt="" style="display:none;max-width:100%;max-height:240px;border-radius:8px;margin-top:10px"></div>
 <div class="card temps"><div>Nozzle <b id="noz">–</b></div><div>Bed <b id="bed">–</b></div><div>Kamer <b id="cham">–</b></div></div>
 <div class="card"><h3>Bediening</h3><div class="ctrls">
   <button id="bPause" class="b-blue">Pause</button>
   <button id="bStop" class="b-red">Stop</button>
   <button id="bLight">Licht</button>
   <button id="bFan">Fan</button>
   <select id="speed"><option value="1">Silent</option><option value="2">Standard</option><option value="3">Sport</option><option value="4">Ludicrous</option></select>
 </div></div>
 <div class="card"><h3>Filament / AMS</h3><div id="amsStrip" class="ams"></div></div>
</section>

<section id="files">
 <div class="fbar"><button id="fUp">⬆ Up</button><span id="fpath">/</span><button id="fRef">↻</button><button id="fDel" style="display:none;background:#a40000;margin-left:auto">Verwijder</button></div>
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
  <div class="card"><h3>Extruder</h3><div class="ecol">
   <div style="display:flex;gap:6px;align-items:center;margin-bottom:4px" class="muted">Lengte <input type="number" id="mExt" value="10" style="width:64px;padding:8px;border-radius:8px;border:1px solid #333b44;background:var(--panel2);color:#fff"> mm</div>
   <button class="ext big" onclick="mcmd('ext',$('mExt').value)">Extrude</button>
   <button class="ret big" onclick="mcmd('ret',$('mExt').value)">Retract</button>
  </div></div>
 </div>
 <div class="card"><h3>Ventilator</h3>
  <div style="display:flex;gap:6px;flex-wrap:wrap"><button onclick="mcmd('fan',0)">Uit</button><button onclick="mcmd('fan',25)">25%</button><button onclick="mcmd('fan',50)">50%</button><button onclick="mcmd('fan',100)">100%</button></div>
 </div>
 <div class="card"><h3>Motoren &amp; posities</h3>
  <div style="display:flex;gap:6px;flex-wrap:wrap"><button onclick="mcmd('motoff')">Motoren uit</button><button onclick="mcmd('center')">Naar midden</button><button onclick="mcmd('front')">Bed naar voren</button><button onclick="mcmd('zup')">Z omhoog</button><button onclick="mcmd('homex')">Home X</button><button onclick="mcmd('homey')">Home Y</button><button onclick="mcmd('homez')">Home Z</button></div>
 </div>
 <div id="hint"></div>
</section>

<section id="scale">
 <div style="margin-bottom:10px"><button onclick="tab('set')">&#8592; Instellingen</button></div>
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
 <div class="card" id="spInv" style="display:none"></div>
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
   <select id="spFMat" onchange="renderSpList()" style="padding:9px;border-radius:8px;border:1px solid #333b44;background:var(--panel2);color:#fff"><option value="">alle materialen</option></select>
   <select id="spSort" onchange="renderSpList()" style="padding:9px;border-radius:8px;border:1px solid #333b44;background:var(--panel2);color:#fff"><option value="name">naam A-Z</option><option value="low">weinig &rarr; veel</option><option value="high">veel &rarr; weinig</option><option value="price">prijs/kg</option><option value="mat">materiaal</option></select>
   <label class="muted" style="display:flex;align-items:center;gap:6px;cursor:pointer"><input type="checkbox" id="spLow" onchange="renderSpList()" style="width:18px;height:18px">bijna leeg</label>
   <label class="muted" style="display:flex;align-items:center;gap:6px;cursor:pointer"><input type="checkbox" id="spAll" onchange="toggleAll(this.checked)" style="width:18px;height:18px"> alles</label>
  </div>
  <div id="spBulk" style="display:none;gap:8px;align-items:center;flex-wrap:wrap;margin-bottom:12px;background:var(--panel2);border-radius:8px;padding:9px 12px">
   <b id="spSelCount"></b>
   <button class="formbtn" style="background:#4a2323;color:#ff9a9a;padding:8px 14px" onclick="bulkDel()">Verwijder</button>
   <span style="display:flex;align-items:center;gap:5px"><input type="color" id="bulkColor" value="#22aa55" class="chip" style="width:34px;height:32px"><button class="formbtn sec" style="padding:8px 12px" onclick="bulkColorGo()">Kleur</button></span>
   <span style="display:flex;align-items:center;gap:5px"><input type="number" id="bulkWeight" placeholder="g" style="width:74px;padding:8px;border-radius:8px;border:1px solid #333b44;background:var(--bg);color:#fff"><button class="formbtn sec" style="padding:8px 12px" onclick="bulkWeightGo()">Gewicht</button></span>
   <span style="display:flex;align-items:center;gap:5px"><select id="bulkMat" style="padding:8px;border-radius:8px;border:1px solid #333b44;background:var(--bg);color:#fff"><option value="">materiaal…</option><option>PLA</option><option>PETG</option><option>ABS</option><option>TPU</option><option>ASA</option><option>PC</option><option>PA</option><option>PVA</option></select><button class="formbtn sec" style="padding:8px 12px" onclick="bulkMatGo()">Zet</button></span>
   <span style="display:flex;align-items:center;gap:5px"><input type="number" id="bulkPrice" placeholder="€/kg" step="0.01" style="width:74px;padding:8px;border-radius:8px;border:1px solid #333b44;background:var(--bg);color:#fff"><button class="formbtn sec" style="padding:8px 12px" onclick="bulkPriceGo()">Prijs</button></span>
   <button class="formbtn sec" style="padding:8px 12px" onclick="clearSel()">Deselecteer</button>
  </div>
  <div id="spList" class="muted">…</div>
 </div>
 <div class="card"><h3>Rollen exporteren / importeren</h3>
  <div class="muted" style="font-size:12px;margin-bottom:8px">Alleen je <b>rollen + lege spoelen</b>, als los bestand. Handig om te delen of van een andere tablet over te nemen. Importeren <b>voegt toe</b> — het overschrijft niets.</div>
  <div style="display:flex;gap:10px;flex-wrap:wrap;align-items:center">
   <button class="formbtn sec" onclick="exportRolls()">⬇ Exporteer rollen</button>
   <label class="formbtn sec" style="cursor:pointer">⬆ Importeer rollen<input type="file" accept=".json,application/json" style="display:none" onchange="importRolls(this.files)"></label>
  </div>
  <div id="rollExpMsg" class="muted" style="margin-top:8px"></div>
  <div class="muted" style="font-size:12px;margin-top:8px">Een volledige back-up (incl. historie en statistieken) staat bij <b>Settings</b>.</div>
 </div>
</section>

<section id="set">
 <div class="card"><h3>FilaTrack Scale (weegschaal)</h3>
  <div class="muted" style="margin-bottom:8px">Gewicht, tarra, kalibreren en WiFi/IP van de schaal.</div>
  <button onclick="tab('scale')" class="formbtn pri">Schaal beheren</button>
 </div>
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
  <div class="frow"><label class="muted">ntfy topic (leeg = uit)</label><input type="text" id="cNtfy" placeholder="bv. filatrack-geheim-x9k2"></div>
  <div style="display:flex;gap:8px;flex-wrap:wrap;margin-top:8px"><button id="cNtfySave" class="formbtn pri">Opslaan</button><button id="cNtfyTest" class="formbtn sec">Test</button></div>
  <div class="frow" style="margin-top:14px"><label class="muted">Waarschuwen als een rol onder dit aantal gram komt</label>
   <div style="display:flex;gap:8px;align-items:center"><input type="number" id="cLow" min="0" step="10" placeholder="100" style="width:120px"><span class="muted">g</span><button id="cLowSave" class="formbtn sec">Opslaan</button></div></div>
  <div class="muted" style="font-size:12px;margin-top:8px">Installeer de gratis <b>ntfy</b>-app (of ntfy.sh in de browser) en abonneer op dit topic. Je krijgt een melding bij print klaar/mislukt, filament tekort en als een gewogen rol onder de drempel komt.</div>
  <div id="cNtfyMsg" class="muted" style="margin-top:6px"></div>
 </div>
 <div class="card"><h3>Back-up &amp; herstel (alles)</h3>
  <div id="bkStatus" style="border-radius:8px;padding:10px 12px;margin-bottom:10px;display:none"></div>
  <div class="muted" style="font-size:12px;margin-bottom:8px">Alles op de tablet in één bestand: rollen, lege spoelen, gewichten, historie en statistieken. (Printer-IP/serial/toegangscode zitten er <b>niet</b> in.)</div>
  <div style="display:flex;gap:10px;flex-wrap:wrap;align-items:center">
   <button id="bkDl" class="formbtn sec" onclick="downloadBackup()">⬇ Download back-up</button>
   <label class="formbtn sec" style="cursor:pointer">⬆ Herstel alles<input type="file" accept=".ptb,.conf,.txt" style="display:none" onchange="importBackup(this.files)"></label>
   <span class="muted" style="font-size:12px">herstel <b>overschrijft</b> je huidige rollen, historie en statistieken</span>
  </div>
  <div id="bkMsg" class="muted" style="margin-top:8px"></div>
  <div class="muted" style="font-size:12px;margin-top:8px">Alleen je rollen delen/overnemen? Dat staat bij <b>Spools</b>.</div>
 </div>
 <div class="card"><h3>Diagnose</h3>
  <div id="diagBox" class="muted">…</div>
  <button class="formbtn sec" style="margin-top:10px" onclick="loadDiag()">Ververs</button>
 </div>
</section>

<section id="hist"><div class="card"><h3>Statistieken</h3>
  <div class="muted">Aller tijden: <b id="stPrints">–</b> prints voltooid &middot; <b id="stUsed">–</b> filament &middot; uitgave <b id="stCost">–</b></div>
  <div id="statRich" style="margin-top:6px"></div>
 </div>
 <div class="card"><h3>Historie / kosten</h3>
 <div style="display:flex;gap:8px;flex-wrap:wrap;align-items:center;margin-bottom:10px">
  <input id="hSearch" oninput="renderHist()" placeholder="Zoek op naam…" style="flex:1;min-width:130px;padding:9px;border-radius:8px;border:1px solid #333b44;background:var(--panel2);color:#fff;font-size:15px">
  <select id="hMat" onchange="renderHist()" style="padding:9px;border-radius:8px;border:1px solid #333b44;background:var(--panel2);color:#fff"><option value="">alle materialen</option></select>
  <select id="hOk" onchange="renderHist()" style="padding:9px;border-radius:8px;border:1px solid #333b44;background:var(--panel2);color:#fff"><option value="">gelukt + mislukt</option><option value="1">alleen gelukt</option><option value="0">alleen mislukt</option></select>
  <select id="hSort" onchange="renderHist()" style="padding:9px;border-radius:8px;border:1px solid #333b44;background:var(--panel2);color:#fff"><option value="date">nieuwste eerst</option><option value="cost">duurste eerst</option><option value="grams">meeste filament</option></select>
  <button onclick="exportCsv()" class="formbtn sec" style="padding:9px 14px">CSV</button>
 </div>
 <div id="histTotal" class="muted" style="font-size:16px;margin-bottom:8px"></div>
 <div style="display:flex;align-items:center;gap:10px;flex-wrap:wrap;margin-bottom:10px">
  <label style="display:flex;align-items:center;gap:6px"><input type="checkbox" id="hArch" onchange="renderHist()" style="width:18px;height:18px">Gearchiveerd tonen</label>
  <button id="hArchBtn" onclick="hBulk(($('hArch')&&$('hArch').checked)?'unarch':'arch')" style="display:none;background:#8e44ad;color:#fff;border:0;border-radius:6px;padding:8px 12px;cursor:pointer;margin-left:auto">Archiveer</button>
  <button id="hDelBtn" onclick="hBulk('del')" style="display:none;background:#a40000;color:#fff;border:0;border-radius:6px;padding:8px 12px;cursor:pointer">Verwijder</button>
 </div>
 <div id="histList"></div></div></section>

<div id="rollModal" class="modal"><div class="modalbox">
 <div class="modalhead"><b id="rollTitle">Kies rol</b><button onclick="closeRoll()">Sluit</button></div>
 <div id="rollList"></div>
</div></div>
<div id="prevModal" class="modal" onclick="this.style.display='none'"><div class="modalbox" style="text-align:center">
 <div class="modalhead"><b>Voorbeeld</b><button onclick="event.stopPropagation();document.getElementById('prevModal').style.display='none'">Sluit</button></div>
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
function fmtAge(s){if(s<0)return'onbekend';if(s<60)return s+' s';if(s<3600)return Math.round(s/60)+' min';if(s<86400)return Math.round(s/3600)+' uur';return Math.round(s/86400)+' dagen';}
function dRow(label,val,good){return '<div style="display:flex;justify-content:space-between;gap:10px;padding:3px 0"><span>'+label+'</span><b style="color:'+(good===true?'#2ecc71':(good===false?'#e74c3c':'#eceff2'))+'">'+val+'</b></div>';}
function loadDiag(){
 var b=$('diagBox');if(!b)return;b.innerHTML='laden…';
 fetch('/diag').then(function(r){return r.json();}).then(function(d){
  var h='';
  h+=dRow('Printer (MQTT)',d.mqtt?'verbonden':'offline',d.mqtt);
  h+=dRow('Laatste status',d.age<0?'nog niets ontvangen':fmtAge(d.age)+' geleden',d.age>=0&&d.age<60);
  h+=dRow('Printer-IP',d.ip||'niet ingesteld',!!d.ip);
  h+=dRow('Serial',d.serial_set?'ingesteld':'ontbreekt',d.serial_set);
  h+=dRow('Toegangscode',d.code_set?'ingesteld':'ontbreekt',d.code_set);
  h+=dRow('Weegschaal-IP',d.scale_ip||'niet ingesteld',null);
  h+=dRow('Webadres',d.url||'-',null);
  h+=dRow('App draait al',fmtAge(d.uptime),null);
  h+='<div class="muted" style="font-size:13px;margin:10px 0 4px">Databestanden (op de tablet)</div>';
  (d.files||[]).forEach(function(f){h+=dRow(f.n,f.ok?(f.bytes+' B · gewijzigd '+fmtAge(f.age)+' geleden'):'ontbreekt',f.ok);});
  b.innerHTML=h;
 }).catch(function(){b.innerHTML='<span style="color:#e74c3c">geen verbinding met de tablet</span>';});
}
var histCache=[];
function loadHistory(){fetch('/history').then(function(r){return r.json();}).then(function(list){histCache=list;
 var mats={};list.forEach(function(r){if(r.mat)mats[r.mat]=1;});
 var sel=$('hMat');if(sel){var cur=sel.value,o='<option value="">alle materialen</option>';Object.keys(mats).sort().forEach(function(m){o+='<option>'+m+'</option>';});sel.innerHTML=o;sel.value=cur;}
 renderStats();renderHist();
}).catch(function(){});}
function fmtDur(m){if(!m||m<=0)return'';if(m<60)return m+' min';var h=Math.floor(m/60),mm=m%60;return h+'u'+(mm<10?'0':'')+mm;}
function statCell(t,big,sub){return '<div style="background:var(--panel2);border-radius:10px;padding:10px 12px"><div class="muted" style="font-size:12px">'+t+'</div><div style="font-size:20px;font-weight:600">'+big+'</div><div class="muted" style="font-size:12px">'+sub+'</div></div>';}
function bar(label,val,max,extra){var pct=max>0?Math.round(val/max*100):0;return '<div style="margin:4px 0"><div style="display:flex;justify-content:space-between;font-size:13px;gap:8px"><span>'+label+'</span><span class="muted" style="white-space:nowrap">'+extra+'</span></div><div style="height:9px;background:#232a31;border-radius:5px;overflow:hidden;margin-top:2px"><div style="height:100%;width:'+pct+'%;background:#3465a4"></div></div></div>';}
function grid(){return 'display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:8px';}
function renderStats(){
 var box=$('statRich');if(!box)return;
 var list=histCache.filter(function(r){return !r.arch;});
 if(!list.length){box.innerHTML='<div class="muted" style="margin-top:8px">Nog geen prints in het logboek.</div>';return;}
 var ok=list.filter(function(r){return r.ok;}),fail=list.filter(function(r){return !r.ok;});
 var okG=0,okC=0,okT=0,failG=0,failC=0,totT=0;
 ok.forEach(function(r){okG+=r.grams||0;okC+=r.cost||0;okT+=r.mins||0;});
 fail.forEach(function(r){failG+=r.grams||0;failC+=r.cost||0;});
 list.forEach(function(r){totT+=r.mins||0;});
 var rate=Math.round(ok.length/list.length*100);
 var h='<div class="muted" style="font-size:13px;margin:6px 0 4px">Op basis van je logboek ('+list.length+' prints)</div>';
 h+='<div style="'+grid()+'">';
 h+=statCell('Geslaagd',rate+'%',ok.length+' van '+list.length);
 h+=statCell('Mislukt',fail.length+'x',failG>0?(Math.round(failG)+' g · € '+failC.toFixed(2)+' verspild'):'geen verspilling');
 h+=statCell('Gem./print',(ok.length?Math.round(okG/ok.length):0)+' g',(ok.length&&okC>0?'€ '+(okC/ok.length).toFixed(2):'')+(okT>0?' · '+fmtDur(Math.round(okT/ok.length)):''));
 h+=statCell('Printtijd',fmtDur(totT)||'–',totT>0?'totaal':'onbekend');
 h+='</div>';
 var now=Date.now()/1000,DAY=86400;
 function since(d){var s={n:0,g:0,c:0};list.forEach(function(r){if(r.ts&&now-r.ts<=d*DAY){s.n++;s.g+=r.grams||0;s.c+=r.cost||0;}});return s;}
 var wk=since(7),mo=since(30),yr=since(365);
 h+='<div class="muted" style="font-size:13px;margin:12px 0 4px">Tijdlijn</div><div style="'+grid()+'">';
 h+=statCell('Deze week',wk.n+' prints',Math.round(wk.g)+' g · € '+wk.c.toFixed(2));
 h+=statCell('Deze maand',mo.n+' prints',Math.round(mo.g)+' g · € '+mo.c.toFixed(2));
 h+=statCell('Dit jaar',yr.n+' prints',Math.round(yr.g)+' g · € '+yr.c.toFixed(2));
 h+='</div>';
 var mm={};list.forEach(function(r){var k=r.mat||'?';if(!mm[k])mm[k]={g:0,c:0,n:0};mm[k].g+=r.grams||0;mm[k].c+=r.cost||0;mm[k].n++;});
 var mk=Object.keys(mm).sort(function(a,b){return mm[b].g-mm[a].g;}),maxG=0,totG=0;
 mk.forEach(function(k){if(mm[k].g>maxG)maxG=mm[k].g;totG+=mm[k].g;});
 h+='<div class="muted" style="font-size:13px;margin:12px 0 4px">Per materiaal</div>';
 mk.forEach(function(k){var pc=totG>0?Math.round(mm[k].g/totG*100):0;h+=bar(k+' <span class="muted">('+mm[k].n+'x)</span>',mm[k].g,maxG,Math.round(mm[k].g)+' g · € '+mm[k].c.toFixed(2)+' · '+pc+'%');});
 var mon={};list.forEach(function(r){if(!r.ts)return;var d=new Date(r.ts*1000),key=d.getFullYear()+'-'+('0'+(d.getMonth()+1)).slice(-2);if(!mon[key])mon[key]={n:0,g:0};mon[key].n++;mon[key].g+=r.grams||0;});
 var mkeys=Object.keys(mon).sort();
 if(mkeys.length){mkeys=mkeys.slice(-6);var maxN=0;mkeys.forEach(function(k){if(mon[k].n>maxN)maxN=mon[k].n;});
  h+='<div class="muted" style="font-size:13px;margin:12px 0 4px">Prints per maand</div>';
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
 if($('histTotal'))$('histTotal').innerHTML=list.length+(showArch?' gearchiveerd':' prints')+' &middot; '+Math.round(totg)+' g &middot; totaal <b>&euro; '+tot.toFixed(2)+'</b>';
 if(!list.length){$('histList').style.display='block';$('histList').innerHTML='<div class="muted">geen resultaten</div>';hSelUpd();return;}
 var h='';
 list.forEach(function(r){
  var pv=r.file?'<button class="hprev" data-p="/cache/'+r.file+'" style="background:#2c3e50;color:#fff;border:0;border-radius:6px;padding:6px 10px;margin-top:8px;cursor:pointer;width:100%">Preview</button>':'';
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
 var a=document.createElement('a');a.href=URL.createObjectURL(new Blob([csv],{type:'text/csv'}));a.download='filatrack-historie.csv';a.click();URL.revokeObjectURL(a.href);
}
function hSelIds(){var a=[];document.querySelectorAll('.hsel:checked').forEach(function(el){a.push(el.dataset.i);});return a;}
function hSelUpd(){var n=hSelIds().length;var showArch=$('hArch')&&$('hArch').checked;
 if($('hArchBtn')){$('hArchBtn').style.display=n?'inline-block':'none';$('hArchBtn').textContent=(showArch?'Herstel':'Archiveer')+' ('+n+')';}
 if($('hDelBtn')){$('hDelBtn').style.display=n?'inline-block':'none';$('hDelBtn').textContent='Verwijder ('+n+')';}}
function hBulk(act){var ids=hSelIds();if(!ids.length)return;
 if(act==='del'&&!confirm(ids.length+' item(s) definitief verwijderen?'))return;
 fetch('/hist_bulk?act='+act+'&idx='+ids.join(',')).then(function(){setTimeout(loadHistory,300);});}
document.querySelectorAll('nav button').forEach(function(b){b.onclick=function(){tab(b.dataset.tab);};});
document.querySelectorAll('.step button').forEach(function(b){b.onclick=function(){step=b.dataset.s;
 document.querySelectorAll('.step button').forEach(function(x){x.classList.remove('sel');});b.classList.add('sel');};});
document.querySelectorAll('.pad button,.zcol button').forEach(function(b){b.onclick=function(){
 fetch('/cmd?a='+b.dataset.a+'&s='+step).then(function(r){return r.text();}).then(function(t){$('hint').textContent=t;}).catch(function(){});};});
function mcmd(a,s){var u='/cmd?a='+a;if(s!==undefined&&s!=='')u+='&s='+s;fetch(u).then(function(r){return r.text();}).then(function(t){if($('hint'))$('hint').textContent=t;}).catch(function(){});}
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
$('cLowSave').onclick=function(){var g=parseInt($('cLow').value,10);if(isNaN(g)||g<0){$('cNtfyMsg').textContent='ongeldig getal';return;}fetch('/setcfg?low='+g).then(function(r){return r.text();}).then(function(t){lowG=g;$('cNtfyMsg').textContent='drempel opgeslagen: '+g+' g';});};
function joinPath(b,n){return (b==='/'?'':b)+'/'+n;}
function fmtSize(b){return b>1048576?(b/1048576).toFixed(1)+' MB':b>1024?(b/1024).toFixed(0)+' KB':b+' B';}
function loadFiles(path){curPath=path;$('fpath').textContent=path;$('flist').innerHTML='<div class=muted>laden…</div>';
 fetch('/files?path='+encodeURIComponent(path)).then(function(r){return r.json();}).then(function(d){
  if(!d.ok){$('flist').innerHTML='<div class=muted>fout: '+(d.err||'')+'</div>';return;}
  var h='';
  d.items.forEach(function(it){
   if(it.dir)h+='<div class="fitem" data-dir="'+it.name+'"><b>📁 '+it.name+'</b><span></span></div>';
   else {var fp=joinPath(path,it.name);
    var pv=/\.3mf$/i.test(it.name)?'<button class="fprev" data-p="'+fp+'" title="Toon voorbeeld" style="background:#2c3e50;color:#fff;border:0;border-radius:6px;padding:6px 10px;cursor:pointer">Preview</button>':'';
    h+='<div class="fitem"><input type="checkbox" class="fsel" data-p="'+fp+'" style="width:20px;height:20px;flex:0 0 auto"><span style="flex:1;min-width:0;overflow:hidden;text-overflow:ellipsis">📄 '+it.name+'</span><span class=muted>'+fmtSize(it.size)+'</span>'+pv+'</div>';}
  });
  $('flist').innerHTML=h||'<div class=muted>leeg</div>';
  document.querySelectorAll('.fitem[data-dir]').forEach(function(el){el.onclick=function(){loadFiles(joinPath(path,el.dataset.dir));};});
  document.querySelectorAll('.fsel').forEach(function(el){el.onchange=fSelUpd;});fSelUpd();
  document.querySelectorAll('.fprev').forEach(function(el){el.onclick=function(e){e.stopPropagation();showPreview(el.dataset.p);};});
 }).catch(function(){$('flist').innerHTML='<div class=muted>geen verbinding</div>';});
}
$('fUp').onclick=function(){var p=curPath.replace(/\/[^\/]*\/?$/,'');loadFiles(p||'/');};
$('fRef').onclick=function(){loadFiles(curPath);};
function fSelPaths(){var a=[];document.querySelectorAll('.fsel:checked').forEach(function(el){a.push(el.dataset.p);});return a;}
function fSelUpd(){var n=fSelPaths().length;var b=$('fDel');if(b){b.style.display=n?'inline-block':'none';b.textContent='Verwijder ('+n+')';}}
$('fDel').onclick=function(){var ps=fSelPaths();if(!ps.length)return;
 if(!confirm(ps.length+' bestand(en) definitief van de printer verwijderen?'))return;
 var i=0;(function nxt(){if(i>=ps.length){loadFiles(curPath);return;}
  fetch('/delete?path='+encodeURIComponent(ps[i])).then(function(){i++;nxt();}).catch(function(){i++;nxt();});})();};
function showPreview(p){var m=$('prevModal');if(!m)return;$('prevBody').innerHTML='<div class="muted">voorbeeld laden…</div>';m.style.display='flex';
 var img=new Image();img.style.maxWidth='80vw';img.style.maxHeight='70vh';img.style.borderRadius='8px';
 img.onload=function(){$('prevBody').innerHTML='';$('prevBody').appendChild(img);};
 img.onerror=function(){$('prevBody').innerHTML='<div class="muted">geen voorbeeld beschikbaar</div>';};
 img.src='/thumb?path='+encodeURIComponent(p);}
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
 if((ext&&ext.present)||assign)h+='<div class="amsbox"><div class="muted">Externe spoel</div><div class="trays">'+trayCell(ext,254,assign)+'</div></div>';
 return h||'<div class="muted">geen AMS</div>';
}
function slotMat(slot){if(!lastS)return'';if(slot===254)return(lastS.ext&&lastS.ext.type)||'';var u=Math.floor(slot/4),t=slot%4;if(lastS.ams)for(var i=0;i<lastS.ams.length;i++){if(lastS.ams[i].id===u+1){var tr=lastS.ams[i].trays[t];return(tr&&tr.type)||'';}}return'';}
function pickRoll(slot){pickSlot=slot;$('rollTitle').textContent='Kies rol voor '+slotName(slot);
 var mat=slotMat(slot);
 fetch('/spools').then(function(r){return r.json();}).then(function(list){
  if(mat)list.sort(function(a,b){return (b.material===mat?1:0)-(a.material===mat?1:0);});
  var h='<div class="fitem rollpick" onclick="clearRoll()"><div style="display:flex;align-items:center;gap:8px"><div class="sw" style="width:26px;height:20px;flex:0 0 auto;background:#555b63;border:1px solid #888"></div><span><b>Leeg</b> <span class="muted">geen rol in dit slot</span></span></div></div>';
  if(!list.length)h+='<div class="muted">Nog geen rollen — maak ze aan op de Spools-tab.</div>';
  list.forEach(function(s){var pas=(mat&&s.material===mat)?' <span class="badge" title="Zelfde materiaal als er nu in dit slot zit" style="margin-left:6px;background:#1e5f3a;color:#b9f5cf">'+s.material+' &#10003;</span>':'';
   h+='<div class="fitem rollpick" onclick="chooseRoll('+s.i+')"><div style="display:flex;align-items:center;gap:8px"><div class="sw" style="width:26px;height:20px;flex:0 0 auto;background:'+s.rgb+'"></div><span><b>'+s.name+'</b> <span class="muted">'+s.material+' · '+s.rem+' g</span>'+pas+'</span></div></div>';});
  $('rollList').innerHTML=h;$('rollModal').style.display='flex';
 }).catch(function(){});
}
function chooseRoll(i){fetch('/spool_load?idx='+i+'&slot='+pickSlot).then(function(){closeRoll();});}
function clearRoll(){fetch('/spool_clear?slot='+pickSlot).then(function(){closeRoll();});}
function closeRoll(){$('rollModal').style.display='none';}
function poll(){
 fetch('/status').then(function(r){return r.json();}).then(function(s){
  $('conn').textContent=s.conn?'● verbonden':'○ offline';$('conn').style.color=s.conn?'#2ecc71':'#e74c3c';
  curState=s.state;curLight=s.light;curFan=s.fan;lastS=s;
  $('state').textContent=s.state||'–';$('task').textContent=s.name||'';
  $('fill').style.width=(s.pct||0)+'%';
  var eta='';if(s.remain>0){var f=new Date(Date.now()+s.remain*60000);eta='  klaar '+('0'+f.getHours()).slice(-2)+':'+('0'+f.getMinutes()).slice(-2);}
  $('prog').textContent=(s.pct||0)+'%'+(s.total?('  laag '+s.layer+'/'+s.total):'')+(s.remain?('  ~'+s.remain+' min'+eta):'');
  if($('pcost'))$('pcost').innerHTML=(s.printg>=0)?('deze print: <b>'+s.printg+' g</b>'+(s.printcost>0?(' &middot; <b>&euro; '+s.printcost.toFixed(2)+'</b>'):'')):'';
  if($('pthumb')){var pf=((s.state==='RUNNING'||s.state==='PAUSE')&&s.file)?s.file:'';if(pf!==pthumbFile){pthumbFile=pf;if(pf){var pth=pf.charAt(0)==='/'?pf:'/cache/'+pf;var im=$('pthumb');im.onload=function(){im.style.display='block';};im.onerror=function(){im.style.display='none';};im.src='/thumb?path='+encodeURIComponent(pth);}else{$('pthumb').style.display='none';}}}
  var w=$('warn');if(w){if(s.short>0){w.style.display='block';w.textContent='⚠ Filament tekort — komt ~'+s.short+' g te kort voor deze print op het actieve slot. Overweeg een vollere spoel.';}else{w.style.display='none';}}
  $('noz').textContent=s.nozzle+'/'+s.nozzle_t+'°';$('bed').textContent=s.bed+'/'+s.bed_t+'°';$('cham').textContent=s.chamber+'°';
  $('bPause').textContent=(s.state==='PAUSE')?'Resume':'Pause';
  $('bLight').textContent='Licht: '+(s.light?'aan':'uit');
  $('bFan').textContent='Fan '+s.fan+'%';
  if(document.activeElement!==$('speed'))$('speed').value=s.speed;
  if(!$('rollModal')||$('rollModal').style.display!=='flex'){$('amsStrip').innerHTML=amsHtml(s.ams,s.ext,true);}
  if($('movetemps'))$('movetemps').textContent='nozzle '+s.nozzle+'/'+s.nozzle_t+'° · bed '+s.bed+'/'+s.bed_t+'° · kamer '+s.chamber+'°';
  if(s.prints!==undefined&&$('stPrints')){$('stPrints').textContent=s.prints;$('stUsed').textContent=(s.used>=1000?(s.used/1000).toFixed(2)+' kg':s.used+' g');if($('stCost'))$('stCost').textContent='€ '+(s.cost||0).toFixed(2);}
  if(s.bkage!==undefined)renderBkStatus(s.bkage);
  if(s.cfg&&s.cfg.scale_ip){scaleHost=s.cfg.scale_ip;var si=$('sIp');if(si&&!si.value)si.value=s.cfg.scale_ip;}
  if(s.cfg&&s.cfg.low)lowG=s.cfg.low;
  if(!cfgFilled&&s.cfg){cfgFilled=true;
   $('cIp').value=s.cfg.ip;$('cSerial').value=s.cfg.serial;$('cBri').value=s.cfg.bri;
   var b=$('cView');b.dataset.v=s.cfg.view3d?'1':'0';b.textContent='Screensaver: '+(s.cfg.view3d?'3D':'2D');
   if(s.cfg.code_set)$('cCode').placeholder='•••••••• (ingesteld, laat leeg = ongewijzigd)';
   if(s.cfg.ntfy!==undefined)$('cNtfy').value=s.cfg.ntfy;
   if(s.cfg.low!==undefined&&$('cLow'))$('cLow').value=s.cfg.low;
  }
 }).catch(function(){$('conn').textContent='○ geen tablet';$('conn').style.color='#e74c3c';});
}
setInterval(poll,1500);poll();
function tickClock(){var d=new Date();if($('clock'))$('clock').textContent=('0'+d.getHours()).slice(-2)+':'+('0'+d.getMinutes()).slice(-2);}
setInterval(tickClock,10000);tickClock();
function sMsg(t){$('sMsg').textContent=t;}
function scalePoll(){
 if(!$('scale').classList.contains('on')||!scaleHost)return;
 fetch('http://'+scaleHost+'/weight?t='+Date.now()).then(function(r){return r.json();}).then(function(d){
  $('swt').textContent=d.g.toFixed(0)+' g';$('sst').textContent=d.stable?'stabiel':'…meten';
 }).catch(function(){$('sst').textContent='geen verbinding met schaal';});
 fetch('http://'+scaleHost+'/info?t='+Date.now()).then(function(r){return r.json();}).then(function(d){
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
setInterval(function(){if($('spList')&&$('spList').offsetParent!==null)loadSpools();},3000);
setInterval(function(){if($('histList')&&$('histList').offsetParent!==null)loadHistory();},8000);
function spMsg(t){$('spMsg').textContent=t;}
function spSlotOpts(){var o='';for(var u=0;u<2;u++)for(var t=0;t<4;t++){o+='<option value="'+(u*4+t)+'">AMS'+(u+1)+' T'+(t+1)+'</option>';}return o+'<option value="254">Externe spoel</option>';}
function loadSpools(){
 fetch('/spools').then(function(r){return r.json();}).then(function(list){spCache=list;
  var mats={};list.forEach(function(s){if(s.material)mats[s.material]=1;});
  var sel=$('spFMat');if(sel){var cur=sel.value,o='<option value="">alle materialen</option>';Object.keys(mats).sort().forEach(function(m){o+='<option>'+m+'</option>';});sel.innerHTML=o;sel.value=cur;}
  renderInv();renderSpList();})
 .catch(function(){$('spList').innerHTML='<div class="muted">geen tablet</div>';});
}
function mLen(g,mat){var d={PLA:1.24,PETG:1.27,ABS:1.04,ASA:1.07,TPU:1.21,PC:1.20,PA:1.14,PVA:1.23}[mat]||1.24;return g/(d*2.4053);}
function spGrams(s){return (s.live!=null?s.live:s.rem)||0;}
function renderInv(){
 var box=$('spInv');if(!box)return;
 if(!spCache.length){box.style.display='none';return;}
 box.style.display='block';
 var totG=0,totV=0,mm={};
 spCache.forEach(function(s){var g=spGrams(s);totG+=g;if(s.price>0)totV+=g*s.price/1000;var k=s.material||'?';if(!mm[k])mm[k]={g:0,n:0,v:0};mm[k].g+=g;mm[k].n++;if(s.price>0)mm[k].v+=g*s.price/1000;});
 var h='<h3>Voorraad</h3><div style="font-size:16px"><b>'+spCache.length+'</b> rollen &middot; <b>'+(totG/1000).toFixed(2)+' kg</b> filament'+(totV>0?' &middot; waarde <b>&euro; '+totV.toFixed(2)+'</b>':'')+'</div>';
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
  if(sort==='low')return spGrams(a)-spGrams(b);
  if(sort==='high')return spGrams(b)-spGrams(a);
  if(sort==='price')return (b.price||0)-(a.price||0);
  if(sort==='mat')return (a.material||'').localeCompare(b.material||'')||(a.name||'').localeCompare(b.name||'');
  return (a.name||'').localeCompare(b.name||'');});
 var h='';
 arr.forEach(function(s){
  var g=spGrams(s),m=mLen(g,s.material);
  var sl=(s.slot!=null&&s.slot>=0)?' <span class="badge" style="background:#1e4e6e;color:#bfe3ff">'+slotName(s.slot)+'</span>':'';
  var low=g<lowG?' <span class="badge" style="background:#5a2020;color:#ffd0d0">bijna leeg</span>':'';
  h+='<div class="rollcard"><input type="checkbox" '+(spSel[s.i]?'checked':'')+' onchange="selToggle('+s.i+',this.checked)" style="width:20px;height:20px;flex:0 0 auto">'
   +'<div style="width:38px;height:38px;border-radius:8px;flex:0 0 auto;border:1px solid #3a434d;background:'+s.rgb+'"></div>'
   +'<div style="min-width:0"><div class="name">'+s.name+'<span class="badge">'+s.material+'</span>'+sl+low+'</div>'
   +(s.note?'<div class="muted" style="font-size:12px">'+s.note+'</div>':'')
   +(s.price>0?'<div class="muted" style="font-size:12px">€ '+s.price.toFixed(2)+'/kg</div>':'')+'</div>'
   +'<div class="grams">'+g+' g<div class="muted" style="font-size:12px;font-weight:400">&asymp; '+Math.round(m)+' m'+(s.price>0?' · € '+(g*s.price/1000).toFixed(2):'')+'</div></div>'
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
function toggleAll(c){spSel={};if(c){spCache.forEach(function(s){if(spMatch(s))spSel[s.i]=true;});}renderSpList();}
function clearSel(){spSel={};if($('spAll'))$('spAll').checked=false;renderSpList();}
function bulkGo(qs){fetch('/spool_bulk?idx='+selIds().join(',')+'&'+qs).then(function(){spSel={};if($('spAll'))$('spAll').checked=false;setTimeout(loadSpools,300);});}
function bulkDel(){if(confirm(selIds().length+' rollen verwijderen?'))bulkGo('act=del');}
function bulkColorGo(){bulkGo('act=color&v='+encodeURIComponent($('bulkColor').value));}
function bulkWeightGo(){if($('bulkWeight').value!=='')bulkGo('act=weight&v='+$('bulkWeight').value);}
function bulkMatGo(){if($('bulkMat').value)bulkGo('act=material&v='+encodeURIComponent($('bulkMat').value));}
function bulkPriceGo(){if($('bulkPrice').value!=='')bulkGo('act=price&v='+$('bulkPrice').value);}
function spRemWeigh(){if(!scaleHost){alert('Geen schaal-IP bekend — stel het in op de Scale-tab.');return;}
 fetch('http://'+scaleHost+'/weight?t='+Date.now()).then(function(r){return r.json();}).then(function(d){var e=parseFloat($('spEmpty').value)||0;$('spRem').value=Math.max(0,Math.round(d.g-e));}).catch(function(){alert('Geen verbinding met de schaal.');});}
function spWeigh(i){var s=spCache[i];if(!scaleHost){alert('Geen schaal-IP bekend.');return;}
 fetch('http://'+scaleHost+'/weight?t='+Date.now()).then(function(r){return r.json();}).then(function(d){
  var rem=Math.max(0,Math.round(d.g-s.empty));
  if(!confirm('Rol \''+s.name+'\' wegen?\nGewogen '+Math.round(d.g)+' g − leeg '+s.empty+' g = '+rem+' g resterend.'))return;
  fetch('/spool_save?idx='+i+'&name='+encodeURIComponent(s.name)+'&material='+encodeURIComponent(s.material)+'&color='+encodeURIComponent(s.rgb)+'&rem='+rem+'&empty='+s.empty+'&nmin='+(s.nmin||0)+'&nmax='+(s.nmax||0)+'&code='+encodeURIComponent(s.code||'')+'&note='+encodeURIComponent(s.note||'')+'&price='+(s.price||0)).then(function(){setTimeout(loadSpools,300);});
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
 fetch('http://'+scaleHost+'/weight?t='+Date.now()).then(function(r){return r.json();}).then(function(d){$('emWeight').value=Math.round(d.g);}).catch(function(){alert('Geen verbinding met de schaal.');});}
// A snapshot on the tablet only survives an app reinstall - not a wipe or a dead
// tablet. So we track when a backup last actually LEFT the device and say so.
function renderBkStatus(age){
 var b=$('bkStatus'),btn=$('bkDl');if(!b)return;
 b.style.display='block';
 var d=(age>=0)?Math.floor(age/86400):-1;
 var overdue=(d<0||d>=14);
 if(d<0){b.style.background='#5a2020';b.style.color='#ffd0d0';
  b.innerHTML='&#9888; <b>Nog nooit een back-up gedownload.</b> Je data staat alleen op de tablet.';}
 else if(overdue){b.style.background='#5a2020';b.style.color='#ffd0d0';
  b.innerHTML='&#9888; Laatste back-up <b>'+d+' dagen</b> geleden &mdash; tijd voor een nieuwe.';}
 else{b.style.background='#1e3a28';b.style.color='#b9f5cf';
  b.innerHTML='&#10003; Laatste back-up '+(d===0?'vandaag':(d+' dag'+(d===1?'':'en')+' geleden'))+'.';}
 if(btn)btn.className='formbtn '+(overdue?'pri':'sec');
}
function downloadBackup(){
 var a=document.createElement('a');a.href='/backup';a.download='filatrack-backup.ptb';
 document.body.appendChild(a);a.click();a.remove();
 if($('bkMsg'))$('bkMsg').textContent='back-up gedownload';
}
// Rolls-only export/import: just the spool library, as a shareable JSON file.
// Import ADDS rolls (no wipe) - that is the whole point of keeping it separate
// from the full backup, which does overwrite everything.
function exportRolls(){
 var m=$('rollExpMsg');
 Promise.all([fetch('/spools').then(function(r){return r.json();}),fetch('/empties').then(function(r){return r.json();})]).then(function(a){
  var blob=new Blob([JSON.stringify({spools:a[0],empties:a[1]},null,1)],{type:'application/json'});
  var url=URL.createObjectURL(blob),link=document.createElement('a');
  link.href=url;link.download='filatrack_rollen.json';link.click();URL.revokeObjectURL(url);
  if(m)m.textContent=(a[0]||[]).length+' rollen geexporteerd';
 }).catch(function(){if(m)m.textContent='geen verbinding';});
}
function importRolls(files){
 if(!files||!files.length)return;
 var m=$('rollExpMsg');
 var rd=new FileReader();
 rd.onload=function(){
  var d;
  try{ d=JSON.parse(rd.result); }catch(e){ if(m)m.textContent='Ongeldig bestand.'; return; }
  if(!d.spools&&!d.empties){ if(m)m.textContent='Geen rollen in dit bestand.'; return; }
  var ns=(d.spools||[]).length;
  if(!confirm(ns+' rol(len) toevoegen aan je bibliotheek? Bestaande rollen blijven staan.'))return;
  var chain=Promise.resolve();
  (d.empties||[]).forEach(function(e){chain=chain.then(function(){return fetch('/empty_save?idx=&name='+encodeURIComponent(e.name)+'&weight='+e.weight);});});
  (d.spools||[]).forEach(function(s){chain=chain.then(function(){return fetch('/spool_save?idx=&name='+encodeURIComponent(s.name)+'&material='+encodeURIComponent(s.material)+'&color='+encodeURIComponent(s.rgb)+'&rem='+(s.rem||0)+'&empty='+(s.empty||0)+'&nmin='+(s.nmin||0)+'&nmax='+(s.nmax||0)+'&code='+encodeURIComponent(s.code||'')+'&note='+encodeURIComponent(s.note||'')+'&price='+(s.price||0));});});
  chain.then(function(){if(m)m.textContent=ns+' rollen toegevoegd.';loadSpools();loadEmpties();})
       .catch(function(){if(m)m.textContent='importeren mislukt';});
 };
 rd.readAsText(files[0]);
}
function importBackup(files){
 if(!files||!files.length)return;
 if(!confirm('Volledige back-up terugzetten? Dit OVERSCHRIJFT je huidige rollen, gewichten, historie en statistieken op de tablet.'))return;
 var rd=new FileReader();
 rd.onload=function(){
  fetch('/restore',{method:'POST',body:rd.result}).then(function(r){return r.text();}).then(function(t){
   $('bkMsg').textContent=t;setTimeout(function(){loadSpools();loadEmpties();loadHistory();},900);
  }).catch(function(){$('bkMsg').textContent='herstel mislukt';});
 };
 rd.readAsText(files[0]);
}
function setSpColor(c){$('spColor').value=c;}
function buildSwatches(){var p=['#111111','#eeeeee','#9aa0a6','#c0392b','#e67e22','#f1c40f','#27ae60','#2980b9','#8e44ad','#ec407a','#6d4c41','#16a085'];var h='';p.forEach(function(c){h+='<button class="swatch" style="background:'+c+'" onclick="setSpColor(\''+c+'\')"></button>';});if($('spSw'))$('spSw').innerHTML=h;}
buildSwatches();
</script></body></html>)PAGE"
