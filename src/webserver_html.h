#pragma once

const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>FilaTrack - Bambu P1S Panel</title>
<style>
  body { font-family: -apple-system, Segoe UI, Roboto, sans-serif; background: #14161a; color: #eee; margin: 0; padding: 20px; }
  .wrap { max-width: 640px; margin: 0 auto; }
  h1 { font-size: 20px; margin-bottom: 4px; }
  .ver { color: #888; font-size: 12px; margin-bottom: 20px; }
  .card { background: #1e2126; border-radius: 10px; padding: 16px 20px; margin-bottom: 16px; }
  .card h2 { font-size: 15px; margin: 0 0 12px; color: #9fd; }
  .row { display: flex; justify-content: space-between; padding: 4px 0; font-size: 14px; }
  .row span:first-child { color: #999; }
  .bar { background: #333; border-radius: 6px; height: 14px; overflow: hidden; margin: 8px 0; }
  .bar-fill { background: #2ecc71; height: 100%; width: 0%; transition: width .3s; }
  label { display: block; font-size: 13px; color: #aaa; margin: 10px 0 4px; }
  input { width: 100%; box-sizing: border-box; padding: 8px; border-radius: 6px; border: 1px solid #333; background: #14161a; color: #eee; font-size: 14px; }
  button { margin-top: 14px; padding: 10px 16px; border: none; border-radius: 6px; background: #3465a4; color: #fff; font-size: 14px; cursor: pointer; }
  button:hover { background: #4a7fc4; }
  .msg { font-size: 13px; margin-top: 8px; }
  .dot { display: inline-block; width: 9px; height: 9px; border-radius: 50%; margin-right: 6px; }
  button.secondary { background: #444; }
  button.secondary:hover { background: #5a5a5a; }
  .wifi-list { margin-top: 10px; max-height: 220px; overflow-y: auto; }
  .wifi-item { padding: 9px 12px; background: #14161a; border-radius: 6px; margin-bottom: 6px; cursor: pointer; font-size: 14px; display: flex; justify-content: space-between; }
  .wifi-item:hover { background: #2a2d33; }
  .wifi-item .sig { color: #888; font-size: 12px; }
</style>
</head>
<body>
<div class="wrap">
  <h1>Bambu P1S Control Panel</h1>
  <div class="ver">Firmware __VERSION__</div>

  <div class="card">
    <h2>Status</h2>
    <div class="row"><span>Connection</span><span id="conn"><span class="dot" id="connDot" style="background:#888"></span><span id="connTxt">-</span></span></div>
    <div class="row"><span>State</span><span id="state">-</span></div>
    <div class="row"><span>Job</span><span id="task">-</span></div>
    <div class="bar"><div class="bar-fill" id="bar"></div></div>
    <div class="row"><span id="pct">0%</span><span id="remain"></span></div>
    <div class="row"><span>Nozzle</span><span id="nozzle">-</span></div>
    <div class="row"><span>Bed</span><span id="bed">-</span></div>
    <div class="row"><span>Chamber</span><span id="chamber">-</span></div>
  </div>

  <div class="card">
    <h2>WiFi</h2>
    <label>SSID</label>
    <input id="wifi_ssid">
    <button type="button" class="secondary" onclick="scanWifi()">Scan for networks</button>
    <div class="wifi-list" id="wifiList"></div>
    <label>Password (leave blank to keep current)</label>
    <input id="wifi_pass" type="password">
  </div>

  <div class="card">
    <h2>Printer connection (LAN-only mode)</h2>
    <label>Printer IP address</label>
    <input id="printer_ip">
    <label>Serial number</label>
    <input id="printer_serial">
    <label>LAN-only access code</label>
    <input id="printer_access_code">
    <button onclick="saveConfig()">Save &amp; restart</button>
    <div class="msg" id="cfgMsg"></div>
  </div>

  <div class="card">
    <h2>Bambu Cloud (only used for filament weight tracking)</h2>
    <div class="row"><span>Status</span><span id="cloudStatus">-</span></div>
    <div id="cloudCodeBox" style="display:none;border:1px solid #e0a800;background:#fff8e1;padding:10px;border-radius:6px;margin:8px 0">
      <label id="cloudCodeLabel">Bambu sent a 6-digit verification code by email. Enter it here to finish logging in:</label>
      <input id="cloud_code" inputmode="numeric" maxlength="6" placeholder="123456">
      <button type="button" onclick="submitCloudCode()">Submit code</button>
      <div class="msg" id="cloudCodeMsg"></div>
    </div>
    <label>Bambu account email</label>
    <input id="cloud_email">
    <label>Password</label>
    <input id="cloud_password" type="password">
    <button onclick="cloudLogin()">Log in</button>
    <button type="button" class="secondary" onclick="cloudLogout()">Log out</button>
    <div class="msg" id="cloudMsg"></div>
    <hr>
    <label>Experimental: paste an access token (e.g. from the PC relay script's relay_state.json) to test whether the device can poll Bambu Cloud directly with it</label>
    <input id="cloud_token">
    <button type="button" class="secondary" onclick="cloudSetToken()">Save token &amp; test now</button>
    <div class="msg" id="cloudTokenMsg"></div>
  </div>

  <div class="card">
    <h2>Firmware update</h2>
    <input type="file" id="fwFile" accept=".bin">
    <button onclick="uploadFirmware()">Upload &amp; flash</button>
    <div class="msg" id="fwMsg"></div>
  </div>
</div>

<script>
async function refreshStatus() {
  try {
    const r = await fetch('/api/status');
    const s = await r.json();
    document.getElementById('connDot').style.background = s.mqtt_connected ? '#2ecc71' : '#e74c3c';
    document.getElementById('connTxt').textContent = s.mqtt_connected ? 'Connected' : 'Offline';
    document.getElementById('state').textContent = s.have_data ? s.gcode_state : '-';
    document.getElementById('task').textContent = s.task_name || '-';
    document.getElementById('bar').style.width = s.progress_pct + '%';
    document.getElementById('pct').textContent = s.progress_pct + '%';
    document.getElementById('remain').textContent = s.remaining_min ? (Math.floor(s.remaining_min/60) + 'h' + (s.remaining_min%60) + 'm left') : '';
    document.getElementById('nozzle').textContent = s.nozzle_temp.toFixed(0) + ' / ' + s.nozzle_target.toFixed(0) + ' °C';
    document.getElementById('bed').textContent = s.bed_temp.toFixed(0) + ' / ' + s.bed_target.toFixed(0) + ' °C';
    document.getElementById('chamber').textContent = s.chamber_temp.toFixed(0) + ' °C';
  } catch (e) { /* device may be mid-restart, ignore */ }
}

async function loadConfig() {
  try {
    const r = await fetch('/api/config');
    const c = await r.json();
    document.getElementById('wifi_ssid').value = c.wifi_ssid || '';
    document.getElementById('printer_ip').value = c.printer_ip || '';
    document.getElementById('printer_serial').value = c.printer_serial || '';
    document.getElementById('printer_access_code').value = c.printer_access_code || '';
  } catch (e) {}
}

async function saveConfig() {
  const body = new URLSearchParams({
    wifi_ssid: document.getElementById('wifi_ssid').value,
    wifi_pass: document.getElementById('wifi_pass').value,
    printer_ip: document.getElementById('printer_ip').value,
    printer_serial: document.getElementById('printer_serial').value,
    printer_access_code: document.getElementById('printer_access_code').value,
  });
  document.getElementById('cfgMsg').textContent = 'Saving...';
  try {
    await fetch('/api/config', { method: 'POST', body });
    document.getElementById('cfgMsg').textContent = 'Saved. Device is restarting...';
  } catch (e) {
    document.getElementById('cfgMsg').textContent = 'Saved (device restarting, connection dropped as expected).';
  }
}

let wifiScanTimer = null;
let wifiScanTimeout = null;

function stopWifiPoll() {
  if (wifiScanTimer) { clearInterval(wifiScanTimer); wifiScanTimer = null; }
  if (wifiScanTimeout) { clearTimeout(wifiScanTimeout); wifiScanTimeout = null; }
}

async function scanWifi() {
  const list = document.getElementById('wifiList');
  list.innerHTML = '<div class="msg">Scanning...</div>';
  stopWifiPoll();

  // `done` is local to this scan - guards against the very first poll
  // landing on a 202 ("still scanning") response, in which case we must
  // keep polling via setInterval below rather than giving up.
  let done = false;

  const poll = async () => {
    if (done) return;
    try {
      const r = await fetch('/api/wifi_scan');
      if (r.status === 202) return; // still scanning - keep polling
      done = true;
      stopWifiPoll();
      const nets = await r.json();
      renderWifiList(nets);
    } catch (e) {
      done = true;
      stopWifiPoll();
      list.innerHTML = '<div class="msg">Scan failed.</div>';
    }
  };

  wifiScanTimeout = setTimeout(() => {
    if (!done) {
      done = true;
      stopWifiPoll();
      list.innerHTML = '<div class="msg">Scan timed out, try again.</div>';
    }
  }, 15000);

  await poll();
  if (!done) {
    wifiScanTimer = setInterval(poll, 1500);
  }
}

function renderWifiList(nets) {
  const list = document.getElementById('wifiList');
  list.innerHTML = '';
  if (!nets || !nets.length) { list.innerHTML = '<div class="msg">No networks found.</div>'; return; }

  const seen = new Set();
  const unique = nets
    .sort((a, b) => b.rssi - a.rssi)
    .filter(n => { if (seen.has(n.ssid)) return false; seen.add(n.ssid); return true; });

  // Built with DOM APIs (not innerHTML string interpolation) so SSIDs
  // containing quotes, ampersands, etc. can't break the markup or get
  // mangled when passed into the click handler.
  unique.forEach(n => {
    const bars = n.rssi > -60 ? '███' : (n.rssi > -75 ? '██░' : '█░░');
    const item = document.createElement('div');
    item.className = 'wifi-item';

    const label = document.createElement('span');
    label.textContent = (n.secure ? '🔒 ' : '') + n.ssid;

    const sig = document.createElement('span');
    sig.className = 'sig';
    sig.textContent = bars;

    item.appendChild(label);
    item.appendChild(sig);
    item.addEventListener('click', () => pickWifi(n.ssid));
    list.appendChild(item);
  });
}

function pickWifi(ssid) {
  document.getElementById('wifi_ssid').value = ssid;
  document.getElementById('wifi_pass').focus();
}

async function refreshCloudStatus() {
  try {
    const r = await fetch('/api/cloud_status');
    const c = await r.json();
    document.getElementById('cloudStatus').textContent = c.logged_in ? ('Logged in as ' + c.email) : 'Not logged in';
    if (c.logged_in) document.getElementById('cloud_email').value = c.email;

    // Show the verification-code field only while the PC relay is waiting for
    // one. code_submitted flips true once we've sent a code and the relay
    // hasn't cleared the flag yet - keep the box up but show "waiting".
    const box = document.getElementById('cloudCodeBox');
    if (c.code_needed) {
      box.style.display = 'block';
      const label = document.getElementById('cloudCodeLabel');
      if (c.code_submitted) {
        label.textContent = 'Code sent - waiting for the relay to finish logging in...';
      } else {
        label.textContent = 'Bambu sent a 6-digit verification code by email'
          + (c.code_email ? ' (' + c.code_email + ')' : '')
          + '. Enter it here to finish logging in:';
      }
    } else {
      box.style.display = 'none';
      document.getElementById('cloudCodeMsg').textContent = '';
    }
  } catch (e) {}
}

async function submitCloudCode() {
  const code = document.getElementById('cloud_code').value.trim();
  if (!/^[0-9]{6}$/.test(code)) { document.getElementById('cloudCodeMsg').textContent = 'Enter the 6-digit code.'; return; }
  document.getElementById('cloudCodeMsg').textContent = 'Sending code...';
  try {
    await fetch('/api/cloud_submit_code', { method: 'POST', body: new URLSearchParams({ code }) });
    document.getElementById('cloud_code').value = '';
    document.getElementById('cloudCodeMsg').textContent = 'Code sent. The relay will pick it up shortly.';
    refreshCloudStatus();
  } catch (e) {
    document.getElementById('cloudCodeMsg').textContent = 'Sending the code failed - try again.';
  }
}

async function cloudLogin() {
  const email = document.getElementById('cloud_email').value;
  const password = document.getElementById('cloud_password').value;
  if (!email || !password) { document.getElementById('cloudMsg').textContent = 'Enter both email and password.'; return; }
  const body = new URLSearchParams({ email, password });
  document.getElementById('cloudMsg').textContent = 'Logging in... (this can take a couple of seconds, and the printer screen may pause briefly)';
  try {
    const r = await fetch('/api/cloud_login', { method: 'POST', body });
    const res = await r.json();
    if (res.result === 'ok') {
      document.getElementById('cloudMsg').textContent = 'Logged in.';
      document.getElementById('cloud_password').value = '';
      refreshCloudStatus();
    } else if (res.result === 'needs_code') {
      document.getElementById('cloudMsg').textContent = 'Bambu wants a verification code for this login - that flow isn\'t supported here yet. Try again later, or from a network Bambu already recognizes.';
    } else {
      document.getElementById('cloudMsg').textContent = 'Login failed: ' + (res.message || 'check your email and password.');
    }
  } catch (e) {
    document.getElementById('cloudMsg').textContent = 'Login request failed.';
  }
}

async function cloudLogout() {
  await fetch('/api/cloud_logout', { method: 'POST' });
  document.getElementById('cloudMsg').textContent = 'Logged out.';
  refreshCloudStatus();
}

async function cloudSetToken() {
  const token = document.getElementById('cloud_token').value.trim();
  if (!token) { document.getElementById('cloudTokenMsg').textContent = 'Paste a token first.'; return; }
  const msgEl = document.getElementById('cloudTokenMsg');
  msgEl.textContent = 'Saving token and testing a direct poll... (a few seconds)';
  try {
    await fetch('/api/cloud_set_token', { method: 'POST', body: new URLSearchParams({ token }) });
    const r = await fetch('/api/cloud_test_poll', { method: 'POST' });
    const res = await r.json();
    msgEl.textContent = (res.result === 'ok' ? 'It worked! ' : 'Failed: ') + (res.message || '');
    refreshCloudStatus();
  } catch (e) {
    msgEl.textContent = 'Request failed.';
  }
}

function uploadFirmware() {
  const f = document.getElementById('fwFile').files[0];
  if (!f) { document.getElementById('fwMsg').textContent = 'Select a .bin file first.'; return; }
  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/api/update');
  xhr.upload.onprogress = (e) => {
    if (e.lengthComputable) {
      document.getElementById('fwMsg').textContent = 'Uploading ' + Math.round(e.loaded/e.total*100) + '%';
    }
  };
  xhr.onload = () => { document.getElementById('fwMsg').textContent = xhr.responseText; };
  xhr.onerror = () => { document.getElementById('fwMsg').textContent = 'Upload failed.'; };
  xhr.send(f);
}

loadConfig();
refreshStatus();
refreshCloudStatus();
setInterval(refreshStatus, 2000);
// Poll cloud status too so the verification-code prompt appears on its own
// when the relay asks for one, without the user reloading the page.
setInterval(refreshCloudStatus, 3000);
</script>
</body>
</html>
)HTML";
