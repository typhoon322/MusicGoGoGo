#include "net/webui.h"

#if defined(BOARD_S3_DEV)

#include <WiFi.h>
#include <ESPAsyncWebServer.h>

#include <Arduino.h>
#include <stdio.h>

namespace {

const char kPageHtml[] PROGMEM = R"html(
<!DOCTYPE html>
<html lang="zh">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>MusicGoGoGo</title>
<style>
:root{--bg:#10141c;--panel:#1a2230;--line:#2a3446;--tx:#d7e0f0;--dim:#7a88a0;--acc:#38bdf8;--ok:#22c55e;--warn:#f59e0b}
*{box-sizing:border-box;margin:0;padding:0}
body{background:var(--bg);color:var(--tx);font:14px/1.5 -apple-system,Segoe UI,Roboto,sans-serif;padding:16px;max-width:560px;margin:0 auto}
h1{font-size:20px;margin-bottom:4px}
h1 small{color:var(--dim);font-weight:400;font-size:13px}
.status{background:var(--panel);border:1px solid var(--line);border-radius:10px;padding:10px 14px;margin:12px 0;display:grid;grid-template-columns:repeat(4,1fr);gap:6px}
.status b{display:block;font-size:18px}
.status span{color:var(--dim);font-size:11px}
.card{background:var(--panel);border:1px solid var(--line);border-radius:10px;padding:14px;margin-bottom:14px}
.card h2{font-size:13px;color:var(--dim);text-transform:uppercase;letter-spacing:.08em;margin-bottom:10px}
.modes{display:grid;grid-template-columns:repeat(auto-fill,minmax(96px,1fr));gap:8px}
.modes button{background:#0f1726;border:1px solid var(--line);color:var(--tx);border-radius:8px;padding:10px 6px;cursor:pointer;font-size:12px}
.modes button.active{background:var(--acc);color:#04121f;border-color:var(--acc);font-weight:700}
.row{margin-bottom:12px}
.row label{display:flex;justify-content:space-between;font-size:13px;margin-bottom:4px}
.row label b{color:var(--acc)}
input[type=range]{width:100%;accent-color:var(--acc)}
input[type=number]{background:#0f1726;border:1px solid var(--line);color:var(--tx);border-radius:6px;padding:4px 8px;width:90px;font-size:13px}
.switch{display:flex;align-items:center;justify-content:space-between;padding:8px 0}
.switch input{transform:scale(1.3);accent-color:var(--acc);margin-right:6px}
.grid2{display:grid;grid-template-columns:1fr 1fr;gap:12px}
a{color:var(--acc)}
.foot{color:var(--dim);font-size:11px;text-align:center;margin-top:10px}
.ok{color:var(--ok)}.warn{color:var(--warn)}
</style>
</head>
<body>
<h1>MusicGoGoGo <small>ESP32-S3 频谱</small></h1>
<div class="status">
<div><span>模式</span><b id="st-mode">-</b></div>
<div><span>FPS</span><b id="st-fps">-</b></div>
<div><span>VU</span><b id="st-vu">-</b></div>
<div><span>RMS</span><b id="st-rms">-</b></div>
<div><span>Peak</span><b id="st-peak">-</b></div>
<div><span>自动增益</span><b id="st-ag">-</b></div>
<div><span>增益</span><b id="st-gain">-</b></div>
<div><span>运行时间</span><b id="st-up">-</b></div>
</div>

<div class="card">
<h2>效果切换</h2>
<div class="modes" id="modes"></div>
</div>

<div class="card">
<h2>麦克风</h2>
<div class="row">
<label>增益 <b id="v-gain"></b></label>
<input type="range" id="gain" min="0.1" max="32" step="0.1">
</div>
<div class="switch"><label>自动电平</label><input type="checkbox" id="alevel"></div>
</div>

<div class="card">
<h2>响应</h2>
<div class="grid2">
<div class="row">
<label>Attack <b id="v-attack"></b></label>
<input type="range" id="attack" min="0.05" max="0.99" step="0.01">
</div>
<div class="row">
<label>Decay <b id="v-decay"></b></label>
<input type="range" id="decay" min="0.1" max="0.99" step="0.01">
</div>
<div class="row">
<label>Peak 衰减 <b id="v-peak"></b></label>
<input type="range" id="peak" min="0.5" max="0.999" step="0.001">
</div>
<div class="row">
<label>帧间隔 ms <b id="v-frame"></b></label>
<input type="range" id="frame" min="10" max="200" step="1">
</div>
</div>
</div>

<div class="card">
<h2>轮播 & 屏幕</h2>
<div class="switch"><label>自动轮播</label><input type="checkbox" id="auto"></div>
<div class="row">
<label>轮播间隔 ms <b id="v-cycle"></b></label>
<input type="range" id="cycle" min="0" max="60000" step="500">
</div>
<div class="row">
<label>背光亮度 <b id="v-bright"></b></label>
<input type="range" id="bright" min="0" max="255" step="1">
</div>
</div>

<div class="foot">连接至 AP <span id="ssid"></span> · 控制页自动刷新</div>

<script>
const $=id=>document.getElementById(id);
const MODES=[];
async function state(){
  try{
    const r=await fetch('/api/state');const s=await r.json();
    $('st-mode').textContent=s.modeName;
    $('st-fps').textContent=s.fps.toFixed(1);
    $('st-vu').textContent=s.vu.toFixed(3);
    $('st-rms').textContent=s.rms.toFixed(3);
    $('st-peak').textContent=s.peak.toFixed(3);
    $('st-ag').textContent=s.autoGain.toFixed(2);
    $('st-gain').textContent=s.gain.toFixed(2);
    const up=s.uptime; const h=Math.floor(up/3600),m=Math.floor(up%3600/60),sec=up%60;
    $('st-up').textContent=h+'h'+String(m).padStart(2,'0')+'m'+String(sec).padStart(2,'0')+'s';
    if(!MODES.length){for(const m of s.modes){MODES.push(m);}}
    paintModes(s.mode);
    if(document.activeElement!==$('gain'))$('gain').value=s.gain;
    $('v-gain').textContent=s.gain.toFixed(2);
    $('attack').value=s.attack;$('v-attack').textContent=s.attack.toFixed(2);
    $('decay').value=s.decay;$('v-decay').textContent=s.decay.toFixed(2);
    $('peak').value=s.peakDecay;$('v-peak').textContent=s.peakDecay.toFixed(3);
    $('frame').value=s.frameMs;$('v-frame').textContent=s.frameMs;
    $('cycle').value=s.cycleMs;$('v-cycle').textContent=s.cycleMs;
    $('bright').value=s.brightness;$('v-bright').textContent=s.brightness;
    $('auto').checked=s.autoCycle;
    $('alevel').checked=s.autoLevel;
  }catch(e){}
}
function paintModes(cur){
  const wrap=$('modes');
  if(!wrap.dataset.built){
    for(let i=0;i<MODES.length;i++){
      const b=document.createElement('button');b.textContent=MODES[i];
      b.onclick=()=>ctrl('mode='+i);
      wrap.appendChild(b);
    }
    wrap.dataset.built='1';
  }
  [...wrap.children].forEach((b,i)=>b.classList.toggle('active',i===cur));
}
async function ctrl(q){
  try{await fetch('/api/control?'+q);}catch(e){}
}
function bind(id,evt,fmt){
  const el=$(id);el.addEventListener(evt,()=>{ctrl(id+'='+el.value);$('v-'+id).textContent=fmt(el.value);});
}
$('ssid').textContent=location.hostname;
bind('gain','input',v=>Number(v).toFixed(2));
bind('attack','input',v=>Number(v).toFixed(2));
bind('decay','input',v=>Number(v).toFixed(2));
bind('peak','input',v=>Number(v).toFixed(3));
bind('frame','input',v=>v);
bind('cycle','input',v=>v);
bind('bright','input',v=>v);
$('auto').addEventListener('change',e=>ctrl('auto='+(e.target.checked?1:0)));
$('alevel').addEventListener('change',e=>ctrl('alevel='+(e.target.checked?1:0)));
state();setInterval(state,500);
</script>
</body>
</html>
)html";

String joinModes(WebCallbacks &cb) {
  String out = "[";
  for (uint8_t i = 0; i < cb.getModeCount(); ++i) {
    if (i > 0) {
      out += ",";
    }
    out += '"';
    out += cb.getModeName(i);
    out += '"';
  }
  out += "]";
  return out;
}

}  // namespace

void WebUi::attach(const WebCallbacks &cb) {
  cb_ = cb;
}

bool WebUi::begin(const char *ssid, const char *pass) {
  WiFi.mode(WIFI_AP);
  wifiOk_ = WiFi.softAP(ssid, pass);
  if (!wifiOk_) {
    Serial.println(F("[web] softAP failed"));
    return false;
  }
  Serial.printf("[web] AP \"%s\" ready, IP %s\n", ssid, WiFi.softAPIP().toString().c_str());
  startServer_();
  return true;
}

void WebUi::printStatus() {
  if (!wifiOk_) {
    return;
  }
  Serial.printf("[web] clients=%u ip=%s heap=%u maxalloc=%u\n", WiFi.softAPgetStationNum(),
                WiFi.softAPIP().toString().c_str(), ESP.getFreeHeap(),
                ESP.getMaxAllocHeap());
}

void WebUi::startServer_() {
  static AsyncWebServer server(80);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", kPageHtml);
  });

  server.on("/api/state", HTTP_GET, [this](AsyncWebServerRequest *request) {
    String json = "{";
    json += "\"mode\":" + String(cb_.getMode());
    json += ",\"modeName\":\"" + String(cb_.getModeName(cb_.getMode())) + "\"";
    json += ",\"modes\":" + joinModes(cb_);
    json += ",\"fps\":" + String(cb_.getFps(), 1);
    json += ",\"vu\":" + String(cb_.getVu(), 4);
    json += ",\"rms\":" + String(cb_.getRms(), 4);
    json += ",\"peak\":" + String(cb_.getPeak(), 4);
    json += ",\"autoGain\":" + String(cb_.getAutoGain(), 3);
    json += ",\"gain\":" + String(cb_.getGain(), 3);
    json += ",\"brightness\":" + String(cb_.getBrightness());
    json += ",\"autoCycle\":" + String(cb_.getAutoCycle() ? 1 : 0);
    json += ",\"cycleMs\":" + String(cb_.getCycleMs());
    json += ",\"frameMs\":" + String(cb_.getFrameMs());
    json += ",\"decay\":" + String(cb_.getDecay(), 3);
    json += ",\"attack\":" + String(cb_.getAttack(), 3);
    json += ",\"peakDecay\":" + String(cb_.getPeakDecay(), 3);
    json += ",\"autoLevel\":" + String(cb_.getAutoLevel() ? 1 : 0);
    json += ",\"uptime\":" + String(cb_.getUptimeMs() / 1000);
    json += "}";
    request->send(200, "application/json", json);
  });

  server.on("/api/control", HTTP_GET, [this](AsyncWebServerRequest *request) {
    if (request->hasParam("mode")) {
      cb_.setMode(static_cast<uint8_t>(request->arg("mode").toInt()));
    }
    if (request->hasParam("gain")) {
      cb_.setGain(request->arg("gain").toFloat());
    }
    if (request->hasParam("brightness")) {
      cb_.setBrightness(static_cast<uint8_t>(request->arg("brightness").toInt()));
    }
    if (request->hasParam("auto")) {
      cb_.setAutoCycle(request->arg("auto").toInt() != 0);
    }
    if (request->hasParam("cycle")) {
      cb_.setCycleMs(static_cast<uint32_t>(request->arg("cycle").toInt()));
    }
    if (request->hasParam("frame")) {
      cb_.setFrameMs(static_cast<uint32_t>(request->arg("frame").toInt()));
    }
    if (request->hasParam("decay")) {
      cb_.setDecay(request->arg("decay").toFloat());
    }
    if (request->hasParam("attack")) {
      cb_.setAttack(request->arg("attack").toFloat());
    }
    if (request->hasParam("peak")) {
      cb_.setPeakDecay(request->arg("peak").toFloat());
    }
    if (request->hasParam("alevel")) {
      cb_.setAutoLevel(request->arg("alevel").toInt() != 0);
    }
    request->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(204);
  });

  server.begin();
}

#endif  // BOARD_S3_DEV
