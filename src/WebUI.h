#pragma once

#include <Arduino.h>

// Einseitige Weboberflaeche, komplett offline lauffaehig (keine externen Dateien).
static const char INDEX_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Lamp control</title>
<style>
:root{
  --bg:#1f2329; --panel:#292e36; --line:#3a414b; --text:#e8eaed; --muted:#9aa3ae;
  --field:#1a1d22; --focus:#8ab4f8;
  font-family:system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;
}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);padding:16px;max-width:680px;margin-inline:auto}
h1{font-size:1.15rem;font-weight:600;margin:4px 0 14px}
.panel{background:var(--panel);border:1px solid var(--line);border-radius:10px;padding:14px;margin-bottom:12px}
.panel.off{opacity:.38;pointer-events:none}
.row{display:flex;gap:12px;align-items:center;flex-wrap:wrap}
.row>*{flex:0 0 auto}
label{color:var(--muted);font-size:.9rem}
select,input[type=number]{
  background:var(--field);color:var(--text);border:1px solid var(--line);border-radius:6px;
  padding:7px 8px;font:inherit;font-variant-numeric:tabular-nums
}
input[type=number]{width:5.2em;text-align:right}
select:focus,input:focus,button:focus-visible{outline:2px solid var(--focus);outline-offset:1px}
.seg{display:flex;border:1px solid var(--line);border-radius:8px;overflow:hidden;width:100%}
.seg label{flex:1;text-align:center;padding:9px 4px;cursor:pointer;color:var(--text);font-size:.95rem}
.seg input{position:absolute;opacity:0;pointer-events:none}
.seg label+label{border-left:1px solid var(--line)}
.seg input:checked+span{font-weight:600}
.seg label:has(input:checked){background:#38404b}
.seg label:has(input:focus-visible){outline:2px solid var(--focus);outline-offset:-2px}
.wheelbox{display:flex;justify-content:center}
canvas{width:100%;max-width:300px;aspect-ratio:1;touch-action:none;cursor:crosshair;border-radius:50%}
.rgb{display:grid;grid-template-columns:repeat(3,1fr);gap:10px;margin-top:12px}
.rgb div{display:flex;flex-direction:column;gap:4px}
.rgb input{width:100%}
.slide{display:grid;grid-template-columns:1fr auto;gap:10px;align-items:center;margin-top:6px}
.slide+.slide{margin-top:14px}
.slide label{grid-column:1/-1;margin-bottom:-4px}
input[type=range]{width:100%;accent-color:#c9ced6}
.swatch{height:34px;border-radius:8px;border:1px solid var(--line);margin-bottom:12px}
code{display:block;user-select:all;background:var(--field);border:1px solid var(--line);
  border-radius:6px;padding:8px 10px;font-size:.88rem;margin-top:6px;word-break:break-all}
.hint{color:var(--muted);font-size:.82rem;margin:8px 0 0;line-height:1.35}
#status{color:var(--muted);font-size:.85rem;min-height:1.2em;margin:2px 2px 0}
.chk{display:flex;align-items:center;gap:8px;color:var(--text)}

button{font:inherit;color:var(--text);background:#38404b;border:1px solid var(--line);border-radius:6px;padding:7px 12px;cursor:pointer}
button:hover{background:#424b58}
.scene h2{font-size:1rem;font-weight:600;margin:0 0 10px}
.scene h3{font-size:.9rem;font-weight:600;margin:16px 0 6px}
.keys{display:grid;gap:5px 4px;align-items:center}
.keys.krgb{grid-template-columns:minmax(0,1.3fr) repeat(4,minmax(0,1fr)) auto}
.keys.kwh{grid-template-columns:minmax(0,7em) minmax(0,7em) auto;justify-content:start}
.krow,.khead{display:contents}
.khead span{color:var(--muted);font-size:.78rem;text-align:center}
.scene input[type=number]{width:100%;min-width:0;padding:8px 1px;font-size:.92rem;text-align:center;
  -moz-appearance:textfield;appearance:textfield}
.scene input[type=number]::-webkit-inner-spin-button,
.scene input[type=number]::-webkit-outer-spin-button{-webkit-appearance:none;margin:0}
.scene .row input[type=number]{width:4.6em;font-size:1rem}
.acts{display:flex;gap:3px;margin-left:2px}
.acts button{width:30px;height:34px;padding:0;display:inline-flex;align-items:center;justify-content:center}
.acts button svg{width:16px;height:16px;fill:none;stroke:currentColor;stroke-width:2;stroke-linecap:round;stroke-linejoin:round;pointer-events:none}
.acts button.del:hover{color:#f28b82}
.btns{display:flex;gap:8px;flex-wrap:wrap;margin-top:14px}
#sceneMsg{min-height:1.2em}

[hidden]{display:none!important}
.tabs{display:flex;border:1px solid var(--line);border-radius:8px;overflow:hidden;margin:0 0 14px}
.tabs button{flex:1;border:0;border-radius:0;background:transparent;padding:10px 4px;font-size:.95rem}
.tabs button+button{border-left:1px solid var(--line)}
.tabs button[aria-selected=true]{background:#38404b;font-weight:600}
.dot{width:12px;height:12px;border-radius:50%;background:#6b7380;flex:0 0 auto}
.dot.ok{background:#81c995;box-shadow:0 0 6px #81c995}
.dot.warn{background:#fdd663}
.dot.bad{background:#f28b82}
.kv{display:grid;grid-template-columns:auto 1fr;gap:7px 16px;margin:0;font-variant-numeric:tabular-nums}
.kv dt{color:var(--muted);font-size:.9rem}
.kv dd{margin:0;text-align:right}
.kv.stale dd{opacity:.45}
.panel h2{font-size:1rem;font-weight:600;margin:0 0 10px}
.gauge{position:relative;height:8px;border-radius:4px;margin:16px 4px 6px;background:linear-gradient(90deg,#0b1d4a,#a8481c 45%,#e8c86a 80%,#fff)}
.gauge i{position:absolute;top:-4px;width:4px;height:16px;border-radius:2px;background:#fff;border:1px solid #000;transform:translateX(-50%)}
.gauge b{position:absolute;top:-4px;width:1px;height:16px;background:var(--muted)}
.gscale{display:flex;justify-content:space-between;color:var(--muted);font-size:.75rem;margin:0 2px}
.out{display:grid;grid-template-columns:34px minmax(0,1.6fr) repeat(6,minmax(0,.6fr));gap:6px 4px;align-items:center;font-size:.88rem;font-variant-numeric:tabular-nums}
.out .h{color:var(--muted);font-size:.75rem;text-align:center}
.out .n{text-align:center}
.out .sw{height:26px;border-radius:6px;border:1px solid var(--line)}
</style>
</head>
<body>
<h1>Lamp control</h1>

<div class="tabs" role="tablist">
  <button type="button" role="tab" id="tabBtnSetup" aria-selected="true" data-tab="setup">Setup</button>
  <button type="button" role="tab" id="tabBtnLive" aria-selected="false" data-tab="live">Live</button>
</div>

<div id="tabSetup" role="tabpanel">

<div class="panel">
  <div class="row">
    <label for="lamp">Lamp</label>
    <select id="lamp"><option value="0">All</option></select>
    <label class="chk"><input type="checkbox" id="auto"> Automatic mode by sun position</label>
  </div>
  <div class="seg" role="radiogroup" aria-label="Mode" style="margin-top:12px">
    <label><input type="radio" name="mode" value="rgb" checked><span>Colour</span></label>
    <label><input type="radio" name="mode" value="white"><span>White</span></label>
    <label><input type="radio" name="mode" value="both"><span>Both</span></label>
  </div>
  <p class="hint">Many lamps cannot show colour and white at the same time. “Colour” sends the white values as 0, “White” sends the colour values as 0, “Both” sends everything unchanged.</p>
</div>

<div class="swatch" id="swatch" aria-hidden="true"></div>

<div class="panel" id="rgbPanel">
  <div class="wheelbox"><canvas id="wheel" width="300" height="300" aria-label="Colour wheel"></canvas></div>
  <div class="rgb">
    <div><label for="n_r">Red</label><input type="number" id="n_r" min="0" max="255" step="1" inputmode="numeric"></div>
    <div><label for="n_g">Green</label><input type="number" id="n_g" min="0" max="255" step="1" inputmode="numeric"></div>
    <div><label for="n_b">Blue</label><input type="number" id="n_b" min="0" max="255" step="1" inputmode="numeric"></div>
  </div>
</div>

<div class="panel" id="whitePanel">
  <div class="slide">
    <label for="s_w6">Warm white (byte 6)</label>
    <input type="range" id="s_w6" min="0" max="255" step="1">
    <input type="number" id="n_w6" min="0" max="255" step="1" inputmode="numeric">
  </div>
  <div class="slide">
    <label for="s_w7">Cold white (byte 7)</label>
    <input type="range" id="s_w7" min="0" max="255" step="1">
    <input type="number" id="n_w7" min="0" max="255" step="1" inputmode="numeric">
  </div>
  <p class="hint">On this lamp, white only reacts when both values are 255 and red, green and blue are 0. Dimming is done with the brightness.</p>
</div>

<div class="panel">
  <div class="slide">
    <label for="s_br">Brightness</label>
    <input type="range" id="s_br" min="0" max="255" step="1">
    <input type="number" id="n_br" min="0" max="255" step="1" inputmode="numeric">
  </div>
</div>

<div class="panel">
  <label>Values for the scene table</label>
  <code id="snip"></code>
  <p class="hint">Tap to select the line. Colour: R, G, B, brightness. White: brightness only, warm and cold are fixed at 255.</p>
</div>

<div class="panel scene">
  <h2>Light scenes by sun elevation</h2>
  <div class="row">
    <label for="sc_sw">Switch colour → white at</label>
    <input type="number" id="sc_sw" step="0.1" min="-20" max="60" inputmode="decimal"><label>°</label>
    <label for="sc_hy">Hysteresis</label>
    <input type="number" id="sc_hy" step="0.1" min="0" max="5" inputmode="decimal"><label>°</label>
  </div>

  <h3>Colour (sun elevation in degrees, R, G, B, brightness)</h3>
  <div class="keys krgb" id="rgbKeys"></div>
  <div class="btns"><button type="button" id="addRgb">Add row</button></div>

  <h3>White (sun elevation in degrees, brightness)</h3>
  <div class="keys kwh" id="whKeys"></div>
  <div class="btns"><button type="button" id="addWh">Add row</button></div>

  <div class="btns">
    <button type="button" id="saveScene">Save</button>
    <button type="button" id="resetScene">Defaults</button>
  </div>
  <p class="hint" id="sceneMsg"></p>
  <p class="hint">Icons next to each row: flask = show the row on the lamp (switches automatic mode off), check mark = copy the current values from the wheel and sliders into the row, trash can = remove the row. Values are blended linearly between keyframes. On saving, the rows are sorted by elevation and stored in the ESP32 flash.</p>
</div>

<div class="panel">
  <h2>Default mission date</h2>
  <div class="row">
    <label for="dt_y">Year</label><input type="number" id="dt_y" min="1900" max="2200" step="1" inputmode="numeric">
    <label for="dt_m">Month</label><input type="number" id="dt_m" min="1" max="12" step="1" inputmode="numeric">
    <label for="dt_d">Day</label><input type="number" id="dt_d" min="1" max="31" step="1" inputmode="numeric">
  </div>
  <div class="btns"><button type="button" id="saveDate">Save</button></div>
  <p class="hint" id="dateMsg"></p>
  <p class="hint">DCS-BIOS only delivers the time of day. This date is used for the sun position until DCS provides the mission date. In use now: <span id="dateNow">–</span></p>
</div>

<div class="panel">
  <h2>Map time zones (UTC)</h2>
  <p class="hint">Offsets are local mission time relative to UTC. The fallback is used when no DCS map ID is available.</p>
  <div id="tzRows"></div>
  <div class="btns"><button type="button" id="saveTimeZones">Save time zones</button></div>
  <p class="hint" id="tzMsg"></p>
</div>

<div id="status"></div>
</div>

<div id="tabLive" role="tabpanel" hidden>
  <div class="panel">
    <div class="row"><span class="dot" id="dcsDot"></span><strong id="dcsText">Loading …</strong></div>
    <p class="hint" id="dcsSub"></p>
  </div>

  <div class="panel">
    <h2>Mission (DCS-BIOS)</h2>
    <dl class="kv" id="kvMission">
      <dt>Mission start date</dt><dd id="l_mdate">–</dd>
      <dt>DCS date received</dt><dd id="l_draw">–</dd>
      <dt>Date</dt><dd id="l_date">–</dd>
      <dt>Time</dt><dd id="l_time">–</dd>
      <dt>Altitude (MSL)</dt><dd id="l_alt">–</dd>
      <dt>Latitude</dt><dd id="l_lat">–</dd>
      <dt>Longitude</dt><dd id="l_lon">–</dd>
    </dl>
  </div>

  <div class="panel">
    <h2>Active map / time zone</h2>
    <dl class="kv" id="kvTimeZone">
      <dt>Map</dt><dd id="l_map">–</dd>
      <dt>Time zone source</dt><dd id="l_tzsrc">–</dd>
      <dt>UTC offset in use</dt><dd id="l_tzoffset">–</dd>
      <dt>Daylight saving time</dt><dd id="l_dst">–</dd>
    </dl>
    <p class="hint">Shows the time zone used by the solar-position calculation. The default setting is used until a valid DCS map ID is detected.</p>
  </div>

  <div class="panel">
    <h2>Calculated sun position</h2>
    <dl class="kv" id="kvSun">
      <dt>Sun elevation</dt><dd id="l_elev">–</dd>
      <dt>Horizon dip (altitude)</dt><dd id="l_dip">–</dd>
      <dt>Effective elevation for the scene</dt><dd id="l_eff">–</dd>
      <dt>Azimuth</dt><dd id="l_az">–</dd>
    </dl>
    <div class="gauge" aria-hidden="true"><b id="g_zero"></b><i id="g_mark" style="left:0"></i></div>
    <div class="gscale"><span>-12°</span><span>0°</span><span>40°</span></div>
  </div>

  <div class="panel">
    <h2>Output</h2>
    <div class="out" id="outGrid"></div>
    <p class="hint" id="outHint"></p>
  </div>
</div>

<script>
(function(){
  var $ = function(id){ return document.getElementById(id); };
  var st = {r:255, g:255, b:255, br:255, w6:0, w7:0};
  var els = {
    r:[$('n_r')], g:[$('n_g')], b:[$('n_b')],
    br:[$('n_br'), $('s_br')], w6:[$('n_w6'), $('s_w6')], w7:[$('n_w7'), $('s_w7')]
  };
  var mode = 'rgb', pending = false, timer = null, lastSend = 0;
  var canvas = $('wheel'), ctx = canvas.getContext('2d');
  var W = canvas.width, CX = W / 2, RAD = W / 2 - 2, wheelImg = null;

  function clamp(v){ v = Math.round(Number(v)); if (isNaN(v)) return 0; return Math.max(0, Math.min(255, v)); }
  function setStatus(t){ $('status').textContent = t || ''; }

  // ---- Farbrechnung ----
  function hsv2rgb(h, s, v){
    var c = v * s, x = c * (1 - Math.abs((h / 60) % 2 - 1)), m = v - c, r = 0, g = 0, b = 0;
    if (h < 60){ r = c; g = x; } else if (h < 120){ r = x; g = c; }
    else if (h < 180){ g = c; b = x; } else if (h < 240){ g = x; b = c; }
    else if (h < 300){ r = x; b = c; } else { r = c; b = x; }
    return [Math.round((r + m) * 255), Math.round((g + m) * 255), Math.round((b + m) * 255)];
  }
  function rgb2hs(r, g, b){
    var mx = Math.max(r, g, b), mn = Math.min(r, g, b), d = mx - mn, h = 0;
    if (d > 0){
      if (mx === r) h = 60 * (((g - b) / d) % 6);
      else if (mx === g) h = 60 * ((b - r) / d + 2);
      else h = 60 * ((r - g) / d + 4);
      if (h < 0) h += 360;
    }
    return {h: h, s: mx === 0 ? 0 : d / mx};
  }

  // ---- Farbrad ----
  function buildWheel(){
    var img = ctx.createImageData(W, W), d = img.data;
    for (var y = 0; y < W; y++){
      for (var x = 0; x < W; x++){
        var dx = x - CX, dy = y - CX, dist = Math.sqrt(dx * dx + dy * dy), i = (y * W + x) * 4;
        if (dist <= RAD){
          var h = Math.atan2(dy, dx) * 180 / Math.PI; if (h < 0) h += 360;
          var c = hsv2rgb(h, dist / RAD, 1);
          d[i] = c[0]; d[i+1] = c[1]; d[i+2] = c[2];
          d[i+3] = dist > RAD - 1.2 ? Math.round(255 * (RAD - dist) / 1.2) : 255;
        }
      }
    }
    wheelImg = img;
  }
  function drawWheel(){
    ctx.clearRect(0, 0, W, W);
    ctx.putImageData(wheelImg, 0, 0);
    var hs = rgb2hs(st.r, st.g, st.b), a = hs.h * Math.PI / 180;
    var mx = CX + Math.cos(a) * hs.s * RAD, my = CX + Math.sin(a) * hs.s * RAD;
    ctx.lineWidth = 3; ctx.strokeStyle = '#fff';
    ctx.beginPath(); ctx.arc(mx, my, 9, 0, 6.2832); ctx.stroke();
    ctx.lineWidth = 1.5; ctx.strokeStyle = '#000';
    ctx.beginPath(); ctx.arc(mx, my, 11, 0, 6.2832); ctx.stroke();
  }
  function pickWheel(e){
    var rect = canvas.getBoundingClientRect();
    var x = (e.clientX - rect.left) * W / rect.width - CX;
    var y = (e.clientY - rect.top) * W / rect.height - CX;
    var dist = Math.min(Math.sqrt(x * x + y * y), RAD);
    var h = Math.atan2(y, x) * 180 / Math.PI; if (h < 0) h += 360;
    var v = Math.max(st.r, st.g, st.b) / 255; if (v === 0) v = 1;   // Intensitaet bleibt erhalten
    var c = hsv2rgb(h, dist / RAD, v);
    st.r = c[0]; st.g = c[1]; st.b = c[2];
    refresh(null); queueSend();
  }
  var dragging = false;
  canvas.addEventListener('pointerdown', function(e){ dragging = true; canvas.setPointerCapture(e.pointerId); pickWheel(e); });
  canvas.addEventListener('pointermove', function(e){ if (dragging) pickWheel(e); });
  canvas.addEventListener('pointerup', function(){ dragging = false; });
  canvas.addEventListener('pointercancel', function(){ dragging = false; });

  // ---- Anzeige ----
  function refresh(src){
    for (var k in els){ els[k].forEach(function(el){ if (el !== src) el.value = st[k]; }); }
    drawWheel();
    var f = st.br / 255, sw = $('swatch');
    var rgb = [st.r, st.g, st.b];
    if (mode === 'white'){   // grobe Naeherung: warm = gelblich, kalt = bläulich
      rgb = [Math.min(255, st.w6 + 0.6 * st.w7), Math.min(255, 0.75 * st.w6 + 0.9 * st.w7), Math.min(255, 0.4 * st.w6 + st.w7)];
    }
    sw.style.background = 'rgb(' + Math.round(rgb[0] * f) + ',' + Math.round(rgb[1] * f) + ',' + Math.round(rgb[2] * f) + ')';
    $('rgbPanel').classList.toggle('off', mode === 'white');
    $('whitePanel').classList.toggle('off', mode === 'rgb');
        var txt = st.r + ', ' + st.g + ', ' + st.b + ', ' + st.br;
    var wtxt = 'White: brightness ' + st.br;
    $('snip').textContent = mode === 'rgb' ? txt : (mode === 'white' ? wtxt : txt + '  |  ' + wtxt);
  }

  // ---- Eingaben ----
  Object.keys(els).forEach(function(k){
    els[k].forEach(function(el){
      el.addEventListener('input', function(){
        if (el.value === '') return;
        st[k] = clamp(el.value);
        refresh(el); queueSend();
      });
      el.addEventListener('change', function(){ el.value = st[k]; });
    });
  });
  Array.prototype.forEach.call(document.getElementsByName('mode'), function(r){
    r.addEventListener('change', function(){ if (r.checked){ mode = r.value; if (mode === 'white'){ st.w6 = 255; st.w7 = 255; } refresh(null); queueSend(); } });
  });
  $('lamp').addEventListener('change', loadState);
  $('auto').addEventListener('change', function(){
    fetch('/api/auto?on=' + ($('auto').checked ? 1 : 0)).then(function(r){ return r.json(); }).then(apply).catch(function(){ setStatus('No connection'); });
  });

  // ---- Senden (gedrosselt, mit Wiederholung bei voller Warteschlange) ----
  function payload(){
    var p = {r: st.r, g: st.g, b: st.b, br: st.br, w6: st.w6, w7: st.w7};
    if (mode === 'rgb'){ p.w6 = 0; p.w7 = 0; }
    if (mode === 'white'){ p.r = 0; p.g = 0; p.b = 0; }
    return p;
  }
  function queueSend(){
    pending = true;
    if (timer) return;
    timer = setTimeout(doSend, Math.max(0, 150 - (Date.now() - lastSend)));
  }
  function doSend(){
    timer = null;
    if (!pending) return;
    pending = false; lastSend = Date.now();
    var p = payload(); p.lamp = $('lamp').value;
    var q = Object.keys(p).map(function(k){ return k + '=' + encodeURIComponent(p[k]); }).join('&');
    fetch('/api/set?' + q).then(function(res){ return res.json(); }).then(function(j){
      if (j.busy){ pending = true; timer = setTimeout(doSend, 300); setStatus('Lamp is busy, resending …'); return; }
      apply(j); setStatus('');
    }).catch(function(){ pending = true; timer = setTimeout(doSend, 1000); setStatus('No connection, retrying …'); });
  }
  function apply(j){ if (j && typeof j.auto !== 'undefined') $('auto').checked = !!j.auto; }

  // ---- Zustand laden ----
  function loadState(){
    fetch('/api/state?lamp=' + $('lamp').value).then(function(r){ return r.json(); }).then(function(j){
      var sel = $('lamp');
      if (sel.options.length === 1){
        for (var i = 1; i <= j.lamps; i++){ var o = document.createElement('option'); o.value = i; o.textContent = 'Lamp ' + i; sel.appendChild(o); }
      }
      st.r = j.r; st.g = j.g; st.b = j.b; st.br = j.br; st.w6 = j.w6; st.w7 = j.w7;
      var hasRgb = j.r + j.g + j.b > 0, hasW = j.w6 + j.w7 > 0;
      mode = hasRgb && hasW ? 'both' : (hasW ? 'white' : 'rgb');
      Array.prototype.forEach.call(document.getElementsByName('mode'), function(r){ r.checked = (r.value === mode); });
      apply(j); refresh(null); setStatus('');
    }).catch(function(){ setStatus('No connection to the lamp controller'); });
  }

  // ---- Lichtszenen ----
  var SCENE_MAX = 10;

  function syncModeRadios(){
    Array.prototype.forEach.call(document.getElementsByName('mode'), function(r){ r.checked = (r.value === mode); });
  }
  function numInput(v, min, max, step){
    var i = document.createElement('input');
    i.type = 'number'; i.min = min; i.max = max; i.step = step; i.value = v;
    i.setAttribute('inputmode', step === '1' ? 'numeric' : 'decimal');
    return i;
  }
  var ICONS = {
    test:  '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M10 2v7.5a2 2 0 0 1-.2.9L4.7 20.5a1 1 0 0 0 .9 1.5h12.8a1 1 0 0 0 .9-1.5l-5.1-10.1a2 2 0 0 1-.2-.9V2"/><path d="M8.5 2h7"/><path d="M7 16h10"/></svg>',
    apply: '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M20 6 9 17l-5-5"/></svg>',
    del:   '<svg viewBox="0 0 24 24" aria-hidden="true"><path d="M3 6h18"/><path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6"/><path d="M8 6V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"/><path d="M10 11v6"/><path d="M14 11v6"/></svg>'
  };
  function iconBtn(icon, title, fn){
    var b = document.createElement('button');
    b.type = 'button'; b.title = title; b.setAttribute('aria-label', title);
    if (icon === 'del') b.className = 'del';
    b.innerHTML = ICONS[icon];
    b.addEventListener('click', fn);
    return b;
  }
  function headRow(names){
    var d = document.createElement('div'); d.className = 'khead';
    names.concat(['']).forEach(function(t){ var sp = document.createElement('span'); sp.textContent = t; d.appendChild(sp); });
    return d;
  }
  function rgbRow(k){
    var d = document.createElement('div'); d.className = 'krow';
    var f = [numInput(k.e, -30, 90, '0.1'), numInput(k.r, 0, 255, '1'), numInput(k.g, 0, 255, '1'),
             numInput(k.b, 0, 255, '1'), numInput(k.h, 0, 255, '1')];
    f.forEach(function(x){ d.appendChild(x); });
    var acts = document.createElement('div'); acts.className = 'acts';
    acts.appendChild(iconBtn('test', 'Test: show this row on the lamp', function(){
      mode = 'rgb'; st.r = clamp(f[1].value); st.g = clamp(f[2].value); st.b = clamp(f[3].value); st.br = clamp(f[4].value);
      syncModeRadios(); refresh(null); queueSend();
    }));
    acts.appendChild(iconBtn('apply', 'Apply: write the current colour and brightness into this row', function(){
      f[1].value = st.r; f[2].value = st.g; f[3].value = st.b; f[4].value = st.br;
    }));
    acts.appendChild(iconBtn('del', 'Remove row', function(){ d.parentNode.removeChild(d); }));
    d.appendChild(acts);
    d._f = f;
    return d;
  }
  function whRow(k){
    var d = document.createElement('div'); d.className = 'krow';
    var f = [numInput(k.e, -30, 90, '0.1'), numInput(k.h, 0, 255, '1')];
    f.forEach(function(x){ d.appendChild(x); });
    var acts = document.createElement('div'); acts.className = 'acts';
    acts.appendChild(iconBtn('test', 'Test: show this row on the lamp', function(){
      mode = 'white'; st.w6 = 255; st.w7 = 255; st.br = clamp(f[1].value);
      syncModeRadios(); refresh(null); queueSend();
    }));
    acts.appendChild(iconBtn('apply', 'Apply: write the current brightness into this row', function(){ f[1].value = st.br; }));
    acts.appendChild(iconBtn('del', 'Remove row', function(){ d.parentNode.removeChild(d); }));
    d.appendChild(acts);
    d._f = f;
    return d;
  }
  function rows(container){ return Array.prototype.filter.call(container.children, function(c){ return c._f; }); }

  function renderScene(j){
    SCENE_MAX = j.max || 10;
    $('sc_sw').value = j.switch; $('sc_hy').value = j.hyst;
    var rc = $('rgbKeys'), wc = $('whKeys');
    rc.innerHTML = ''; wc.innerHTML = '';
    rc.appendChild(headRow(['Elev °', 'R', 'G', 'B', 'Bright']));
    wc.appendChild(headRow(['Elev °', 'Bright']));
    j.rgb.forEach(function(k){ rc.appendChild(rgbRow(k)); });
    j.white.forEach(function(k){ wc.appendChild(whRow(k)); });
  }
  function loadScene(){
    fetch('/api/scene').then(function(r){ return r.json(); }).then(renderScene)
      .catch(function(){ $('sceneMsg').textContent = 'Could not load scenes.'; });
  }
  function readTable(container){
    return rows(container).map(function(r){ return r._f.map(function(i){ return i.value; }).join(','); }).join(';');
  }
  $('addRgb').addEventListener('click', function(){
    var rc = $('rgbKeys'), list = rows(rc);
    if (list.length >= SCENE_MAX){ $('sceneMsg').textContent = 'Maximum ' + SCENE_MAX + ' rows.'; return; }
    var last = list.length ? list[list.length - 1]._f : null;
    rc.appendChild(rgbRow(last ? {e: (parseFloat(last[0].value) || 0) + 1, r: last[1].value, g: last[2].value, b: last[3].value, h: last[4].value}
                               : {e: 0, r: 255, g: 255, b: 255, h: 128}));
  });
  $('addWh').addEventListener('click', function(){
    var wc = $('whKeys'), list = rows(wc);
    if (list.length >= SCENE_MAX){ $('sceneMsg').textContent = 'Maximum ' + SCENE_MAX + ' rows.'; return; }
    var last = list.length ? list[list.length - 1]._f : null;
    wc.appendChild(whRow(last ? {e: (parseFloat(last[0].value) || 0) + 1, h: last[1].value} : {e: 0, h: 128}));
  });
  $('saveScene').addEventListener('click', function(){
    var body = new URLSearchParams();
    body.set('switch', $('sc_sw').value); body.set('hyst', $('sc_hy').value);
    body.set('rgb', readTable($('rgbKeys'))); body.set('white', readTable($('whKeys')));
    $('sceneMsg').textContent = 'Saving …';
    fetch('/api/scene', {method: 'POST', body: body}).then(function(r){ return r.json(); }).then(function(j){
      if (j.ok){ $('sceneMsg').textContent = 'Saved.'; loadScene(); }
      else { $('sceneMsg').textContent = 'Not saved: ' + (j.error || 'unknown error'); }
    }).catch(function(){ $('sceneMsg').textContent = 'No connection.'; });
  });
  $('resetScene').addEventListener('click', function(){
    if (!window.confirm('Reset all scenes to the default values?')) return;
    fetch('/api/scene/reset', {method: 'POST'}).then(function(r){ return r.json(); }).then(function(j){
      $('sceneMsg').textContent = j.ok ? 'Defaults saved.' : ('Error: ' + (j.error || ''));
      loadScene();
    }).catch(function(){ $('sceneMsg').textContent = 'No connection.'; });
  });

  buildWheel(); refresh(null); loadState(); loadScene();
})();
</script>
<script>
(function(){
  var $ = function(id){ return document.getElementById(id); };
  var GMIN = -12, GMAX = 40, timer = null, active = false;

  function pad(n){ return (n < 10 ? '0' : '') + n; }
  function num(v, d){ return v === null || v === undefined ? '–' : Number(v).toFixed(d); }
  // Dezimalgrad -> Grad ° Minuten ' Sekunden " (Sekunden mit einer Nachkommastelle)
  function coord(v, pos, neg){
    if (v === null || v === undefined) return '–';
    var t = Math.round(Math.abs(v) * 36000) / 10;   // Bogensekunden, auf 0,1" gerundet
    var deg = Math.floor(t / 3600);
    t -= deg * 3600;
    var min = Math.floor(t / 60);
    var sec = t - min * 60;
    return deg + '\u00B0 ' + pad(min) + "' " + (sec < 10 ? '0' : '') + sec.toFixed(0) + '" ' + (v >= 0 ? pos : neg);
  }
  function ft(v){ return String(Math.round(v)).replace(/\B(?=(\d{3})+(?!\d))/g, '\u2009'); }
  function swatch(r, g, b, br, w6, w7){
    // Bei niedriger Helligkeit (Nacht: 6 bis 28 von 255) waere das Feld sonst fast schwarz.
    // Darum bleibt die Farbe erkennbar, die Helligkeit dunkelt sie nur gemaessigt ab.
    var f = br === 0 ? 0 : 0.3 + 0.7 * Math.sqrt(br / 255);
    if (r + g + b === 0 && w6 + w7 > 0){
      r = Math.min(255, w6 + 0.6 * w7); g = Math.min(255, 0.75 * w6 + 0.9 * w7); b = Math.min(255, 0.4 * w6 + w7);
    }
    return 'rgb(' + Math.round(r * f) + ',' + Math.round(g * f) + ',' + Math.round(b * f) + ')';
  }

  function cell(cls, text){
    var d = document.createElement('div'); d.className = cls; d.textContent = text; return d;
  }
  function outRow(grid, label, v){
    var sw = document.createElement('div'); sw.className = 'sw';
    sw.style.background = swatch(v.r, v.g, v.b, v.br, v.w6, v.w7);
    grid.appendChild(sw);
    grid.appendChild(cell('', label));
    [v.r, v.g, v.b, v.br, v.w6, v.w7].forEach(function(x){ grid.appendChild(cell('n', x)); });
  }

  var lastLiveJson = null;

  function nthSunday(year, month, nth){
    var firstDow = new Date(Date.UTC(year, month - 1, 1)).getUTCDay();
    return 1 + ((7 - firstDow) % 7) + (nth - 1) * 7;
  }
  function lastSunday(year, month){
    var lastDay = new Date(Date.UTC(year, month, 0)).getUTCDate();
    var dow = new Date(Date.UTC(year, month - 1, lastDay)).getUTCDay();
    return lastDay - dow;
  }
  // Keep these transitions identical to isDaylightSavingTime() in main.cpp.
  function isDst(rule, year, month, day, hour){
    if (!rule) return false;
    if (rule === 1){
      var usStart = nthSunday(year, 3, 2), usEnd = nthSunday(year, 11, 1);
      if (month < 3 || month > 11) return false;
      if (month > 3 && month < 11) return true;
      if (month === 3) return day > usStart || (day === usStart && hour >= 2);
      return day < usEnd || (day === usEnd && hour < 2);
    }
    if (rule === 2){
      var euStart = lastSunday(year, 3), euEnd = lastSunday(year, 10);
      if (month < 3 || month > 10) return false;
      if (month > 3 && month < 10) return true;
      if (month === 3) return day > euStart || (day === euStart && hour >= 2);
      return day < euEnd || (day === euEnd && hour < 3);
    }
    return false;
  }
  function renderTimeZone(d, sun){
    $('l_map').textContent = d.map || 'Unknown / not detected';
    var fromMap = d.tzsrc === 'map' && !!d.map;
    $('l_tzsrc').textContent = fromMap ? 'Map configuration' : 'Default fallback';
    var timeZoneConfig = window.brmeshTimeZoneConfig || null;
    if (!timeZoneConfig){
      $('l_tzoffset').textContent = 'Loading configuration …';
      $('l_dst').textContent = '–';
      return;
    }
    var cfg = fromMap ? (timeZoneConfig.maps || []).filter(function(m){ return m.name === d.map; })[0] : null;
    if (!cfg) cfg = timeZoneConfig.default;
    if (!cfg){
      $('l_tzoffset').textContent = '–'; $('l_dst').textContent = '–'; return;
    }
    var base = Number(cfg.offset);
    var dst = null;
    if (sun && sun.date && sun.time){
      var dateParts = sun.date.split('-'), timeParts = sun.time.split(':');
      if (dateParts.length === 3 && timeParts.length >= 1){
        dst = isDst(Number(cfg.rule), Number(dateParts[0]), Number(dateParts[1]),
                    Number(dateParts[2]), Number(timeParts[0]));
      }
    }
    var activeOffset = base + (dst ? 1 : 0);
    var sign = activeOffset >= 0 ? '+' : '−';
    var abs = Math.abs(activeOffset), hours = Math.floor(abs), minutes = Math.round((abs - hours) * 60);
    if (minutes === 60){ hours++; minutes = 0; }
    $('l_tzoffset').textContent = 'UTC' + sign + hours + ':' + (minutes < 10 ? '0' : '') + minutes +
      (dst ? ' (standard ' + (base >= 0 ? '+' : '−') + Math.abs(base).toFixed(2) + ')' : '');
    $('l_dst').textContent = dst === null ? 'Not available (no calculated mission time)' : (dst ? 'Active' : 'Inactive');
  }

  function render(j){
    lastLiveJson = j;
    var d = j.data || {}, sun = d.sun, dcs = d.dcs;
    renderTimeZone(d, sun);
    var dot = $('dcsDot'), txt = $('dcsText'), sub = $('dcsSub');
    var mode = j.manual ? 'Manual mode, automatic mode is off.' : (d.fallback ? 'Default scene active because no DCS data is arriving.' : 'Automatic mode active.');
    if (dcs === 1){ dot.className = 'dot ok'; txt.textContent = 'DCS-BIOS connected'; sub.textContent = 'Mission time is updating. ' + mode; }
    else if (dcs === 2){ dot.className = 'dot bad'; txt.textContent = 'No updates from DCS-BIOS';
      sub.textContent = 'Mission time unchanged for ' + d.age + ' s: DCS is paused or the connection is lost. ' + mode; }
    else { dot.className = 'dot warn'; txt.textContent = 'Waiting for DCS-BIOS'; sub.textContent = 'No data received yet. ' + mode; }

    var stale = dcs !== 1;
    $('kvMission').classList.toggle('stale', stale);
    $('kvSun').classList.toggle('stale', stale);

    $('l_mdate').textContent = d.mdate ? d.mdate + (d.msrc === 'dcs' ? ' (from DCS)' : ' (default, set in Setup)') : '–';
    var raw;
    if (d.dcfg === false) raw = 'not set up in the firmware';
    else if (d.dy == null && d.dm == null && d.dd == null) raw = 'nothing received yet';
    else raw = (d.dy == null ? '–' : d.dy) + '-' + (d.dm == null ? '–' : d.dm) + '-' + (d.dd == null ? '–' : d.dd);
    $('l_draw').textContent = raw;
    if (sun){
      var p = sun.date.split('-');
      $('l_date').textContent = p[0] + '-' + p[1] + '-' + p[2];
      $('l_time').textContent = sun.time;
    } else { $('l_date').textContent = '–'; $('l_time').textContent = '–'; }
    $('l_alt').textContent = d.alt_ft === null || d.alt_ft === undefined ? '–' : ft(d.alt_ft) + ' ft (' + ft(d.alt_m) + ' m)';
    $('l_lat').textContent = coord(d.lat, 'N', 'S');
    $('l_lon').textContent = coord(d.lon, 'E', 'W');

    $('l_elev').textContent = sun ? num(sun.elev, 1) + '°' : '–';
    $('l_dip').textContent = sun ? '+' + num(sun.dip, 1) + '°' : '–';
    $('l_eff').textContent = sun ? num(sun.eff, 1) + '°' : '–';
    $('l_az').textContent = sun ? num(sun.az, 1) + '°' : '–';
    var mark = $('g_mark');
    if (sun){
      var pc = Math.max(0, Math.min(1, (sun.eff - GMIN) / (GMAX - GMIN)));
      mark.style.left = (pc * 100) + '%'; mark.hidden = false;
    } else mark.hidden = true;
    $('g_zero').style.left = ((0 - GMIN) / (GMAX - GMIN) * 100) + '%';

    var grid = $('outGrid');
    grid.innerHTML = '';
    ['', '', 'R', 'G', 'B', 'Bright', 'W6', 'W7'].forEach(function(t){ grid.appendChild(cell('h', t)); });
    if (sun) outRow(grid, 'Scene: ' + (sun.white ? 'White' : 'Colour'), sun);
    (j.lamps || []).forEach(function(l, i){ outRow(grid, 'Lamp ' + (i + 1), l); });
    $('outHint').textContent = 'Scene = calculated from the sun position. Lamp = most recently sent values' +
      (j.queue ? ' (' + j.queue + ' commands in the queue)' : '') + '.';
  }

  window.addEventListener('brmesh-timezone-config', function(){
    if (lastLiveJson) renderTimeZone(lastLiveJson.data || {}, (lastLiveJson.data || {}).sun);
  });

  function poll(){
    timer = null;
    if (!active || document.hidden) return;
    fetch('/api/live').then(function(r){ return r.json(); }).then(render).catch(function(){
      $('dcsDot').className = 'dot bad'; $('dcsText').textContent = 'No connection to the lamp controller'; $('dcsSub').textContent = '';
    }).then(function(){ if (active && !timer) timer = setTimeout(poll, 1000); });
  }
  function start(){ if (!timer) poll(); }

  function show(name){
    active = name === 'live';
    $('tabSetup').hidden = active; $('tabLive').hidden = !active;
    $('tabBtnSetup').setAttribute('aria-selected', String(!active));
    $('tabBtnLive').setAttribute('aria-selected', String(active));
    try { history.replaceState(null, '', '#' + name); } catch (e) {}
    if (active) start(); else if (timer){ clearTimeout(timer); timer = null; }
  }
  Array.prototype.forEach.call(document.querySelectorAll('.tabs button'), function(b){
    b.addEventListener('click', function(){ show(b.getAttribute('data-tab')); });
  });
  document.addEventListener('visibilitychange', function(){ if (active && !document.hidden) start(); });
  show(location.hash === '#live' ? 'live' : 'setup');
})();
</script>
<script>
(function(){
  var $ = function(id){ return document.getElementById(id); };
  function pad(n){ return (n < 10 ? '0' : '') + n; }
  function load(){
    fetch('/api/date').then(function(r){ return r.json(); }).then(function(j){
      $('dt_y').value = j.y; $('dt_m').value = j.m; $('dt_d').value = j.d;
      $('dateNow').textContent = j.ey + '-' + pad(j.em) + '-' + pad(j.ed) + (j.src === 'dcs' ? ' (from DCS)' : ' (default date)');
    }).catch(function(){ $('dateMsg').textContent = 'Could not load the date.'; });
  }
  $('saveDate').addEventListener('click', function(){
    var body = new URLSearchParams();
    body.set('y', $('dt_y').value); body.set('m', $('dt_m').value); body.set('d', $('dt_d').value);
    $('dateMsg').textContent = 'Saving …';
    fetch('/api/date', {method: 'POST', body: body}).then(function(r){ return r.json(); }).then(function(j){
      if (j.ok){ $('dateMsg').textContent = 'Saved.'; setTimeout(load, 1200); }
      else { $('dateMsg').textContent = 'Not saved: ' + (j.error || 'unknown error'); }
    }).catch(function(){ $('dateMsg').textContent = 'No connection.'; });
  });
  $('tabBtnSetup').addEventListener('click', load);
  load();
})();
</script>
<script>
(function(){
  var $ = function(id){ return document.getElementById(id); };
  var rules = ['No DST','US DST','EU DST'];
  var rows = [];
  function makeRow(item, isDefault){
    var wrap = document.createElement('div');
    wrap.className = 'row'; wrap.style.margin = '10px 0';
    var label = document.createElement('label');
    label.textContent = isDefault ? 'Default (no DCS map)' : item.name;
    label.style.minWidth = '145px'; wrap.appendChild(label);
    var offset = document.createElement('input'); offset.type = 'number'; offset.step = '0.25';
    offset.min = '-12'; offset.max = '14'; offset.value = item.offset;
    offset.setAttribute('aria-label','UTC offset hours'); wrap.appendChild(offset);
    var rule = document.createElement('select');
    rules.forEach(function(t,i){ var o=document.createElement('option'); o.value=i; o.textContent=t; rule.appendChild(o); });
    rule.value = item.rule; wrap.appendChild(rule);
    rows.push({id:isDefault ? 0 : item.id, offset:offset, rule:rule});
    $('tzRows').appendChild(wrap);
  }
  function load(){
    rows=[]; $('tzRows').innerHTML='';
    fetch('/api/timezone').then(function(r){return r.json();}).then(function(j){
      window.brmeshTimeZoneConfig = j;
      makeRow(j.default,true);
      (j.maps || []).forEach(function(m){makeRow(m,false);});
      window.dispatchEvent(new Event('brmesh-timezone-config'));
    }).catch(function(){ $('tzMsg').textContent='Could not load time zones.'; });
  }
  $('saveTimeZones').addEventListener('click',function(){
    var chain=Promise.resolve();
    $('tzMsg').textContent='Saving …';
    rows.forEach(function(row){
      chain=chain.then(function(){
        var body=new URLSearchParams();
        body.set('id',row.id); body.set('offset',row.offset.value); body.set('rule',row.rule.value);
        return fetch('/api/timezone',{method:'POST',body:body}).then(function(r){return r.json();}).then(function(j){
          if(!j.ok) throw new Error(j.error || 'Save failed');
        });
      });
    });
    chain.then(function(){ $('tzMsg').textContent='Saved to ESP32 flash.'; load(); })
      .catch(function(e){ $('tzMsg').textContent='Not saved: '+e.message; });
  });
  $('tabBtnSetup').addEventListener('click',load);
  load();
})();
</script>
</body>
</html>
)rawliteral";