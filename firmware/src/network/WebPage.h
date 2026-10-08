#pragma once

// The monitor's single web page, served by WebUi on the LAN and as the
// setup portal. Self-contained (no external assets: in setup mode the phone
// has no internet). Dynamic text is only ever set via textContent / .value,
// never innerHTML. Every API call sends the X-SM-Request header (CSRF guard).

static const char kWebPage[] = R"HTML(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Starlink Monitor</title>
<style>
:root{--bg:#0b0f14;--card:#151b23;--text:#e6edf3;--muted:#8b949e;--line:#30363d;--accent:#58a6ff;--up:#bc8cff;--ok:#3fb950;--warn:#d29922;--bad:#f85149}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);font:16px/1.4 system-ui,-apple-system,sans-serif}
main{max-width:460px;margin:0 auto;padding:20px 16px 40px}
h1{font-size:20px;letter-spacing:.08em;margin:0 0 2px}
.sub{color:var(--muted);font-size:14px;margin:0 0 18px}
section{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:14px;margin-bottom:14px}
h2{font-size:13px;letter-spacing:.06em;color:var(--muted);margin:0 0 10px;font-weight:600}
.row{display:flex;justify-content:space-between;align-items:center;gap:8px}
.row h2{margin:0}
label{display:block;font-size:14px;color:var(--muted);margin:10px 0 4px}
input,select{width:100%;padding:10px;border-radius:8px;border:1px solid var(--line);background-color:var(--bg);color:var(--text);font-size:16px}
select{padding-right:40px;appearance:none;-webkit-appearance:none;background-image:url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='12' height='8' viewBox='0 0 12 8'%3E%3Cpath d='M1 1.5l5 5 5-5' fill='none' stroke='%238b949e' stroke-width='1.6' stroke-linecap='round' stroke-linejoin='round'/%3E%3C/svg%3E");background-repeat:no-repeat;background-position:right 14px center;background-size:12px 8px}
input[type=range]{padding:0;accent-color:var(--accent)}
input:focus,select:focus{outline:2px solid var(--accent);border-color:transparent}
.hint{font-size:13px;color:var(--muted);margin:4px 0 0}
button{font:inherit;cursor:pointer}
.primary{width:100%;margin-top:12px;padding:11px;border:0;border-radius:8px;background:var(--accent);color:#04111f;font-weight:600}
.primary:disabled{opacity:.5}
.ghost{padding:9px 12px;border-radius:8px;border:1px solid var(--line);background:none;color:var(--text)}
.danger{border-color:var(--bad);color:var(--bad)}
.link{background:none;border:0;color:var(--accent);padding:0;font-size:14px}
.badge{font-size:13px;font-weight:600;padding:3px 10px;border-radius:999px;background:#ffffff14}
.grid{display:grid;grid-template-columns:1fr 1fr 1fr;gap:10px;margin-top:12px}
.grid div{background:var(--bg);border-radius:8px;padding:8px}
.grid b{display:block;font-size:18px;font-weight:600}
.grid span{font-size:12px;color:var(--muted)}
.kv{display:grid;grid-template-columns:auto 1fr;gap:4px 12px;font-size:14px}
.kv dt{color:var(--muted)}.kv dd{margin:0;text-align:right;word-break:break-all}
#nets{display:flex;flex-direction:column;gap:6px;margin-top:8px}
.net{display:flex;justify-content:space-between;align-items:center;width:100%;padding:9px 10px;border:1px solid var(--line);border-radius:8px;background:var(--bg);color:var(--text);text-align:left}
.net span{color:var(--muted);font-size:13px;white-space:nowrap;margin-left:8px}
.msg{min-height:1.4em;margin:10px 0 0;font-size:14px}
.ok{color:var(--ok)}.warn{color:var(--warn)}.bad{color:var(--bad)}.muted{color:var(--muted)}
.note{border-color:var(--accent)}
.actions{display:flex;gap:10px;margin-top:12px}
</style></head><body><main>
<h1>STARLINK MONITOR</h1>
<p class="sub" id="sub">&nbsp;</p>

<section class="note" id="setupNote" hidden>
  <h2>SETUP</h2>
  <p class="hint" style="margin:0">Choose the WiFi network that can reach your Starlink dish (usually the Starlink router's own network), then save.</p>
</section>

<section id="statusCard">
  <div class="row"><h2>STATUS</h2><span class="badge" id="health">--</span></div>
  <p class="hint" id="reason"></p>
  <div class="grid">
    <div><b id="down">--</b><span>Download now</span></div>
    <div><b id="up">--</b><span>Upload now</span></div>
    <div><b id="lat">--</b><span>Latency</span></div>
    <div><b id="obs">--</b><span>Obstruction</span></div>
    <div><b id="sig">--</b><span>Signal</span></div>
    <div><b id="dup">--</b><span>Dish uptime</span></div>
  </div>
</section>

<section id="wifiCard">
  <div class="row"><h2>WIFI</h2><button class="link" id="rescan" type="button">Scan networks</button></div>
  <p class="hint" id="wifiNow"></p>
  <div id="nets"></div>
  <form id="wifiForm">
    <label for="ssid">Network name</label>
    <input id="ssid" name="ssid" maxlength="32" autocomplete="off" autocapitalize="off" spellcheck="false" required>
    <label for="pass">Password</label>
    <input id="pass" name="pass" type="password" maxlength="64" autocomplete="off">
    <p class="hint" id="passhint">Leave empty for an open network.</p>
    <p class="hint warn" id="wifiWarn" hidden>Saving reconnects the monitor; this page will lose contact if the network changes.</p>
    <button class="primary" id="wifiSave" type="submit">Save &amp; connect</button>
    <p class="msg" id="wifiMsg"></p>
  </form>
</section>

<section id="dishCard">
  <h2>STARLINK</h2>
  <form id="dishForm">
    <label for="host">Dish IP address</label>
    <input id="host" name="host" inputmode="decimal" maxlength="15" required>
    <p class="hint">Default 192.168.100.1</p>
    <label for="poll">Refresh every</label>
    <select id="poll" name="poll_ms"><option value="1000">1 second</option><option value="2000">2 seconds</option></select>
    <button class="primary" type="submit">Save</button>
    <p class="msg" id="dishMsg"></p>
  </form>
</section>

<section id="displayCard">
  <h2>DISPLAY</h2>
  <label for="bright">Brightness</label>
  <input id="bright" type="range" min="10" max="255" step="1">
  <label for="rot">Orientation</label>
  <select id="rot">
    <option value="0">Portrait</option><option value="1">Landscape</option>
    <option value="2">Portrait, upside down</option><option value="3">Landscape, upside down</option>
  </select>
  <p class="msg" id="dispMsg"></p>
</section>

<section id="deviceCard">
  <h2>DEVICE</h2>
  <dl class="kv" id="device"></dl>
  <div class="actions">
    <button class="ghost" id="reboot" type="button">Restart</button>
    <button class="ghost danger" id="reset" type="button">Factory reset</button>
  </div>
  <p class="msg" id="devMsg"></p>
</section>
</main>
<script>
const $=id=>document.getElementById(id);
const sleep=ms=>new Promise(r=>setTimeout(r,ms));
async function api(path,opt={}){
  opt.headers=Object.assign({'X-SM-Request':'1'},opt.headers||{});
  const r=await fetch(path,opt);
  const j=await r.json().catch(()=>({}));
  if(!r.ok&&!j.error)j.error='HTTP '+r.status;
  return j;
}
const post=(path,data)=>api(path,{method:'POST',body:new URLSearchParams(data||{})});
function msg(id,t,c){const m=$(id);m.textContent=t||'';m.className='msg '+(c||'')}
function bps(b){if(b==null)return'--';if(b<1e6)return Math.round(b/1e3)+' kbps';return(b<9.95e6?(b/1e6).toFixed(1):Math.round(b/1e6))+' Mbps'}
function pct(f){if(f==null)return'--';const p=f*100;if(p>0&&p<0.05)return'<0.1%';return(p<9.95?p.toFixed(1):Math.round(p))+'%'}
function dur(s){if(s==null)return'--';const d=Math.floor(s/86400),h=Math.floor(s/3600)%24,m=Math.floor(s/60)%60;return d?d+'d '+h+'h':h?h+'h '+String(m).padStart(2,'0')+'m':m+'m'}
function bars(r){return r>=-55?'▮▮▮▮':r>=-67?'▮▮▮▯':r>=-75?'▮▮▯▯':'▮▯▯▯'}
const healthClass={ONLINE:'ok',DEGRADED:'warn',OFFLINE:'bad',ERROR:'bad',CONNECTING:'muted'};
let settings={},portal=false;

function passHint(){
  const keep=settings.has_password&&$('ssid').value===settings.ssid;
  $('pass').placeholder=keep?'(unchanged)':'';
  $('passhint').textContent=keep?'Leave empty to keep the saved password.':'Leave empty for an open network.';
}
function render(s){
  portal=s.wifi.portal;
  $('sub').textContent='v'+s.monitor.version+' · '+s.monitor.board;
  $('setupNote').hidden=!portal;
  for(const id of ['statusCard','displayCard'])$(id).hidden=portal;
  $('wifiWarn').hidden=portal;
  const h=$('health');h.textContent=s.dish.health;h.className='badge '+(healthClass[s.dish.health]||'');
  $('reason').textContent=s.dish.reason+(s.dish.last_seen_s!=null&&s.dish.link!=='online'?' · last seen '+dur(s.dish.last_seen_s)+' ago':'');
  const fresh=s.dish.link==='online';
  $('down').textContent=fresh?bps(s.dish.down_bps):'--';
  $('up').textContent=fresh?bps(s.dish.up_bps):'--';
  $('lat').textContent=fresh&&s.dish.latency_ms!=null?Math.round(s.dish.latency_ms)+' ms':'--';
  $('obs').textContent=fresh?pct(s.dish.obstruction):'--';
  $('sig').textContent=fresh?pct(s.dish.signal):'--';
  $('dup').textContent=fresh?dur(s.dish.uptime_s):'--';
  const w=s.wifi;
  $('wifiNow').textContent=w.state==='connected'?'Connected to '+w.ssid+' · '+w.ip+' · '+w.rssi+' dBm':
    w.ssid?'Not connected ('+(w.error||w.state)+')':'No network saved';
  const dev=$('device');dev.textContent='';
  for(const [k,v] of [['Firmware','v'+s.monitor.version],['Built',s.monitor.build],['Board',s.monitor.board],
      ['Dish',s.dish.hardware||'--'],['Dish firmware',s.dish.software||'--'],['Free memory',Math.round(s.monitor.heap/1024)+' KB'],
      ['Monitor uptime',dur(s.monitor.uptime_s)],['Commit',s.monitor.commit]]){
    const dt=document.createElement('dt');dt.textContent=k;const dd=document.createElement('dd');dd.textContent=v;dev.append(dt,dd);
  }
}
async function refresh(){try{render(await api('/api/status'))}catch(e){}}
async function loadSettings(){
  settings=await api('/api/settings');
  $('ssid').value=settings.ssid;$('host').value=settings.host;$('poll').value=String(settings.poll_ms);
  $('bright').value=settings.brightness;$('rot').value=String(settings.rotation);passHint();
}
async function scan(refresh){
  const l=$('nets');l.textContent='Scanning…';
  for(let i=0;i<20;i++){
    let s;try{s=await api('/api/scan'+(refresh?'?refresh=1':''))}catch(e){break}
    refresh=false;
    if(s.state==='scanning'){await sleep(1000);continue}
    l.textContent='';
    if(s.state!=='done'){l.textContent='Scan unavailable — type the network name.';return}
    if(!s.networks.length){l.textContent='No networks found.';return}
    for(const n of s.networks){
      const b=document.createElement('button');b.type='button';b.className='net';b.textContent=n.ssid;
      const m=document.createElement('span');m.textContent=(n.open?'open ':'🔒 ')+bars(n.rssi);b.appendChild(m);
      b.onclick=()=>{$('ssid').value=n.ssid;$('pass').value='';passHint();$('pass').focus()};
      l.appendChild(b);
    }
    return;
  }
  l.textContent='Scan unavailable — type the network name.';
}
async function followWifi(ssid){
  for(;;){
    await sleep(1500);
    let s;try{s=await api('/api/status')}catch(e){msg('wifiMsg','Lost contact. Check the monitor\'s screen: when it shows its new IP address, open that (or http://starlink-monitor.local/) from the same network.');return}
    const w=s.wifi;
    if(w.state==='connected'&&w.ssid===ssid){msg('wifiMsg','Connected to '+ssid+' — IP '+w.ip+'. Open http://starlink-monitor.local/ or that address from the same network.','ok');return}
    if(w.state==='retry'&&w.failures>0){msg('wifiMsg','Could not connect to '+ssid+' ('+w.error+'). Check the password and try again.','bad');$('wifiSave').disabled=false;return}
    msg('wifiMsg','Connecting to '+ssid+'…');
  }
}
$('ssid').addEventListener('input',passHint);
$('rescan').onclick=()=>scan(true);
$('wifiForm').addEventListener('submit',async e=>{
  e.preventDefault();$('wifiSave').disabled=true;msg('wifiMsg','Saving…');
  try{
    const r=await post('/api/settings',{ssid:$('ssid').value,pass:$('pass').value});
    if(!r.ok){msg('wifiMsg',r.error,'bad');$('wifiSave').disabled=false;return}
    if(!r.wifi_changed){msg('wifiMsg','No change.','muted');$('wifiSave').disabled=false;return}
    await loadSettings().catch(()=>{});
    followWifi($('ssid').value);
  }catch(err){msg('wifiMsg','Could not reach the monitor.','bad');$('wifiSave').disabled=false}
});
$('dishForm').addEventListener('submit',async e=>{
  e.preventDefault();msg('dishMsg','Saving…');
  try{const r=await post('/api/settings',{host:$('host').value,poll_ms:$('poll').value});
    msg('dishMsg',r.ok?'Saved.':r.error,r.ok?'ok':'bad')}catch(err){msg('dishMsg','Could not reach the monitor.','bad')}
});
async function saveDisplay(data){
  try{const r=await post('/api/settings',data);msg('dispMsg',r.ok?'Saved.':r.error,r.ok?'ok':'bad')}
  catch(err){msg('dispMsg','Could not reach the monitor.','bad')}
}
$('bright').addEventListener('change',()=>saveDisplay({brightness:$('bright').value}));
$('rot').addEventListener('change',()=>saveDisplay({rotation:$('rot').value}));
$('reboot').onclick=async()=>{
  if(!confirm('Restart the monitor?'))return;
  await post('/api/reboot').catch(()=>{});msg('devMsg','Restarting… this page will reconnect.');
  await sleep(8000);location.reload();
};
$('reset').onclick=async()=>{
  if(!confirm('Erase all settings, including WiFi? The monitor will restart in setup mode.'))return;
  await post('/api/factory-reset').catch(()=>{});
  msg('devMsg','Settings erased. Join the STARLINK-MONITOR setup network shown on the screen.','warn');
};
(async()=>{
  await refresh();
  try{await loadSettings()}catch(e){}
  if(portal)scan(false);
  setInterval(()=>{if(!document.hidden)refresh()},2000);
})();
</script>
</body></html>
)HTML";
