#pragma once

// Setup page served by ConfigPortal. Self-contained (no external assets:
// the phone has no internet while joined to the setup AP). All dynamic text
// is inserted with textContent / .value, never innerHTML.

static const char kPortalPage[] = R"HTML(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Starlink Monitor setup</title>
<style>
:root{--bg:#0b0f14;--card:#151b23;--text:#e6edf3;--muted:#8b949e;--line:#30363d;--accent:#58a6ff;--ok:#3fb950;--bad:#f85149}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);font:16px/1.4 system-ui,-apple-system,sans-serif}
main{max-width:420px;margin:0 auto;padding:20px 16px 40px}
h1{font-size:20px;letter-spacing:.08em;margin:0 0 2px}
.sub{color:var(--muted);font-size:14px;margin:0 0 20px}
section{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:14px;margin-bottom:14px}
h2{font-size:13px;letter-spacing:.06em;color:var(--muted);margin:0 0 10px;font-weight:600}
label{display:block;font-size:14px;color:var(--muted);margin:10px 0 4px}
input{width:100%;padding:11px;border-radius:8px;border:1px solid var(--line);background:var(--bg);color:var(--text);font-size:16px}
input:focus{outline:2px solid var(--accent);border-color:transparent}
.hint{font-size:13px;color:var(--muted);margin:4px 0 0}
button{font:inherit;cursor:pointer}
.primary{width:100%;margin-top:4px;padding:12px;border:0;border-radius:8px;background:var(--accent);color:#04111f;font-weight:600}
.primary:disabled{opacity:.5}
.link{background:none;border:0;color:var(--accent);padding:0;font-size:14px}
#nets{display:flex;flex-direction:column;gap:6px}
.net{display:flex;justify-content:space-between;align-items:center;width:100%;padding:10px;border:1px solid var(--line);border-radius:8px;background:var(--bg);color:var(--text);text-align:left}
.net span{color:var(--muted);font-size:13px;white-space:nowrap;margin-left:8px}
.row{display:flex;justify-content:space-between;align-items:center;margin-bottom:10px}
.row h2{margin:0}
#msg{min-height:1.4em;margin:12px 0 0;font-size:15px}
.ok{color:var(--ok)}.bad{color:var(--bad)}
</style></head><body><main>
<h1>STARLINK MONITOR</h1>
<p class="sub">WiFi setup</p>
<section>
  <div class="row"><h2>NEARBY NETWORKS</h2><button class="link" id="rescan" type="button">Rescan</button></div>
  <div id="nets" class="hint">Scanning&hellip;</div>
</section>
<form id="f">
<section>
  <h2>WIFI</h2>
  <label for="ssid">Network name</label>
  <input id="ssid" name="ssid" maxlength="32" autocomplete="off" autocapitalize="off" spellcheck="false" required>
  <label for="pass">Password</label>
  <input id="pass" name="pass" type="password" maxlength="64" autocomplete="off">
  <p class="hint" id="passhint">Leave empty for an open network.</p>
</section>
<section>
  <h2>STARLINK</h2>
  <label for="host">Dish IP address</label>
  <input id="host" name="host" inputmode="decimal" maxlength="15" required>
  <p class="hint">Default 192.168.100.1</p>
</section>
<button class="primary" id="save" type="submit">Save &amp; connect</button>
<p id="msg"></p>
</form>
</main>
<script>
const $=id=>document.getElementById(id);
const sleep=ms=>new Promise(r=>setTimeout(r,ms));
async function getJson(u,o){const r=await fetch(u,o);return r.json()}
let saved={ssid:'',hasPassword:false};
function passHint(){
  const keep=saved.hasPassword&&$('ssid').value===saved.ssid;
  $('pass').placeholder=keep?'(unchanged)':'';
  $('passhint').textContent=keep?'Leave empty to keep the saved password.':'Leave empty for an open network.';
}
function bars(rssi){return rssi>=-55?'▮▮▮▮':rssi>=-67?'▮▮▮▯':rssi>=-75?'▮▮▯▯':'▮▯▯▯'}
function render(s){
  const l=$('nets');l.textContent='';
  if(s.state!=='done'){l.textContent='Scan unavailable — type the network name below.';return}
  if(!s.networks.length){l.textContent='No networks found.';return}
  for(const n of s.networks){
    const b=document.createElement('button');b.type='button';b.className='net';
    b.textContent=n.ssid;
    const m=document.createElement('span');m.textContent=(n.open?'open ':'🔒 ')+bars(n.rssi);
    b.appendChild(m);
    b.onclick=()=>{$('ssid').value=n.ssid;$('pass').value='';passHint();$('pass').focus()};
    l.appendChild(b);
  }
}
async function scan(refresh){
  $('nets').textContent='Scanning…';
  try{
    for(let i=0;i<20;i++){
      const s=await getJson('/scan'+(refresh?'?refresh=1':''));refresh=false;
      if(s.state==='scanning'){await sleep(1000);continue}
      render(s);return;
    }
  }catch(e){}
  render({state:'failed'});
}
async function load(){
  try{
    saved=await getJson('/config');
    $('ssid').value=saved.ssid;$('host').value=saved.host;passHint();
  }catch(e){}
  scan(false);
}
function setMsg(t,c){const m=$('msg');m.textContent=t;m.className=c||''}
async function follow(ssid){
  for(;;){
    await sleep(1000);
    let s;
    try{s=await getJson('/status')}catch(e){setMsg('Lost contact with the setup network — check the monitor\'s screen. If it shows an IP address, setup is done.');return}
    if(s.sta==='connected'){setMsg('Connected to '+s.ssid+' — IP '+s.ip+'. This setup network will close shortly.','ok');continue}
    if(s.sta==='retry'&&s.failures>0){setMsg('Could not connect to '+ssid+' ('+s.error+'). Check the password and try again.','bad');$('save').disabled=false;return}
    setMsg('Connecting to '+ssid+'…');
  }
}
$('ssid').addEventListener('input',passHint);
$('rescan').onclick=()=>scan(true);
$('f').addEventListener('submit',async e=>{
  e.preventDefault();
  $('save').disabled=true;setMsg('Saving…');
  try{
    const r=await getJson('/save',{method:'POST',body:new URLSearchParams(new FormData($('f')))});
    if(!r.ok){setMsg(r.error,'bad');$('save').disabled=false;return}
    const ssid=$('ssid').value;
    saved={ssid,hasPassword:$('pass').value!==''||(saved.hasPassword&&ssid===saved.ssid)};
    follow($('ssid').value);
  }catch(err){setMsg('Could not reach the monitor.','bad');$('save').disabled=false}
});
load();
</script>
</body></html>
)HTML";
