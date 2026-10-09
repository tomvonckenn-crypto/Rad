#pragma once
const char CONTROL_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="nl"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Rad van fortuin</title><style>
:root{color-scheme:light}*{box-sizing:border-box}body{margin:0;background:#eef2f6;color:#172a3a;font:17px system-ui,sans-serif}main{max-width:700px;margin:28px auto;padding:0 18px 30px}header{display:flex;align-items:center;justify-content:space-between;gap:12px}h1{font-size:27px;margin:0 0 8px}h2{font-size:19px;margin:0 0 12px}.card{background:white;border:1px solid #dbe2ea;border-radius:16px;padding:20px;margin-top:16px}.badge{border-radius:30px;background:#e3eaf2;padding:6px 12px;font-size:14px;white-space:nowrap}button{font:inherit;font-weight:650;padding:13px 16px;border:1px solid #a9bacb;border-radius:10px;background:#f4f7fa;color:#172a3a;cursor:pointer;touch-action:manipulation}button:disabled{opacity:.4;cursor:default}button:focus-visible,input:focus-visible{outline:3px solid #2185d0;outline-offset:3px}.emergency{display:block;width:100%;padding:23px;background:#be1825;color:white;border:2px solid #930c17;font-size:27px;letter-spacing:.06em;box-shadow:0 4px 0 #930c17}.primary{background:#17699e;color:white;border-color:#17699e;width:100%}.row{display:flex;gap:10px}.row>*{flex:1}.metrics{display:grid;grid-template-columns:1fr 1fr;gap:16px}.metric{background:#f2f6fa;border-radius:10px;padding:14px}.metric strong{display:block;font-size:37px;font-variant-numeric:tabular-nums}.small{font-size:14px;color:#526579;line-height:1.5;margin:10px 0 0}.message{font-weight:650;min-height:26px;line-height:1.5}.error{color:#aa1621}input[type=range]{width:100%;height:35px;accent-color:#17699e}label{display:block;margin-top:14px}.stopping{width:100%;margin-top:12px;background:#fff3d4;border-color:#d9b358}.details{display:flex;flex-wrap:wrap;gap:12px}@media(max-width:470px){main{margin-top:18px}header{display:block}.badge{display:inline-block;margin:5px 0}.card{padding:16px}h1{font-size:25px}.row{gap:8px}button{padding:12px 8px}}
</style></head><body><main><header><div><h1>Rad van fortuin</h1><div class="small">Handbediening</div></div><span id="connection" class="badge">Verbinding controleren</span></header>
<section class="card" aria-label="Stop"><button id="emergency" class="emergency" type="button">NOODSTOP</button>
<p class="small">Softwarestop via wifi: zet beide PWM-uitgangen direct op nul en vergrendelt de bediening. De 12 V-motorvoeding blijft aangesloten. Gebruik een fysieke onderbreker van de motorvoeding voor een echte noodstop.</p>
<p id="message" class="message" role="status" aria-live="polite">Bediening wordt geladen...</p></section>
<section class="card"><h2>Bediening</h2><button id="arm" class="primary" type="button" disabled>Vrijgeven: rad staat fysiek stil</button>
<p class="small">Vrijgeven start de motor niet. Kies daarna een richting; PWM begint altijd op 0. Verhoog de instelling pas wanneer je klaar bent voor een motortest.</p>
<div class="row" style="margin-top:16px"><button id="left" type="button" disabled>Start links op PWM 0</button><button id="right" type="button" disabled>Start rechts op PWM 0</button></div>
<p id="direction" class="small">Richting: geen</p>
<div class="metrics" style="margin-top:14px"><div class="metric">Ingestelde PWM<strong id="target">0</strong></div><div class="metric">Actuele PWM<strong id="current">0</strong></div></div>
<label for="pwm">PWM instellen <span class="small">(0–300 van 1023)</span></label><input id="pwm" type="range" min="0" max="300" step="1" value="0" disabled>
<div class="row"><button id="minus" type="button" disabled>−1</button><button id="plus" type="button" disabled>+1</button></div>
<button id="slow" class="stopping" type="button" disabled>Rustig stoppen: PWM afbouwen</button>
<p class="small">Actuele PWM volgt de instelling met maximaal 1 stap per 30 ms. PWM 0 bewijst geen fysieke stilstand. Voor een nieuwe richting moet je opnieuw vrijgeven. Bij verlies van de bedieningsverbinding vergrendelt de firmware en zet zij PWM op 0.</p></section>
<section class="card"><h2>Verbinding en hardware</h2><div class="details"><span id="wifi">Wifi: —</span><span id="ip">Adres: —</span></div><p id="version" class="small"></p>
<p class="small">Reserve-wifi: Rad-Wemos · 192.168.4.1. Houd deze pagina zichtbaar tijdens bediening. De 3,3 V-ingangscompatibiliteit van jouw drivermodule is nog niet bevestigd. PWM begrenst het motorkoppel niet. Deze stopfunctie is afhankelijk van wifi en werkende firmware.</p></section>
</main><script>
'use strict';
const $=id=>document.getElementById(id);
let token='', epoch=0, state=null, online=false, busy=false, generation=0, polling=false, dragging=false;
const messages={boot:'Vergrendeld na opstarten. Bevestig eerst fysieke stilstand.',emergency:'NOODSTOP actief: PWM is 0. Vrijgeven vereist een nieuwe bevestiging.',connection_lost:'Vergrendeld: bedieningsverbinding verloren. Controleer fysieke stilstand.',stopped:'PWM is 0. Wacht op fysieke stilstand voordat je opnieuw vrijgeeft.',ota:'Vergrendeld tijdens upload.',ota_error:'Uploadfout: bediening blijft geblokkeerd tot herstart.'};
function render(s){state=s;epoch=s.safety_epoch;online=true;
 if(!s.control_armed)token='';
 $('connection').textContent='Wemos bereikbaar';$('target').textContent=s.target_pwm;$('current').textContent=s.actual_pwm;
 if(!dragging)$('pwm').value=s.target_pwm;
 const own=!!token && s.control_armed;
 $('arm').disabled=busy || (s.state!=='locked' && s.state!=='await_standstill');
 $('left').disabled=$('right').disabled=busy || !own || s.state!=='ready';
 const running=own && s.state==='running';
 $('pwm').disabled=busy || !running;$('minus').disabled=busy || !running || s.target_pwm===0;$('plus').disabled=busy || !running || s.target_pwm>=s.max_pwm;
 $('slow').disabled=busy || !own || (s.state!=='running' && s.state!=='ready');
 $('direction').textContent='Richting: '+({left:'links',right:'rechts',none:'geen'}[s.direction]||s.direction);
 $('wifi').textContent='Wifi: '+(s.station_connected?'verbonden':'reserveverbinding');$('ip').textContent='Adres: '+(s.station_ip||s.ap_ip);
 $('version').textContent='Firmware '+s.version+' · '+s.build;$('message').classList.remove('error');
 $('message').textContent=messages[s.stop_reason]||(own?(s.state==='ready'?'Vrijgegeven. Kies een richting; PWM blijft eerst 0.':s.state==='stopping'?'PWM wordt geleidelijk afgebouwd.':'Bediening actief.'):'Bediening actief in een andere sessie. De noodstop blijft beschikbaar.');
}
function offline(message){online=false;token='';busy=false;dragging=false;generation++;$('connection').textContent='Verbinding niet bevestigd';$('message').textContent=message;$('message').classList.add('error');for(const id of ['arm','left','right','pwm','plus','minus','slow'])$(id).disabled=true;}
async function request(path,data){const ac=new AbortController();const timer=setTimeout(()=>ac.abort(),1500);try{const options={cache:'no-store',signal:ac.signal};if(data){options.method='POST';options.headers={'Content-Type':'application/x-www-form-urlencoded'};options.body=new URLSearchParams(data);}const response=await fetch(path,options);const body=await response.json();if(!response.ok)throw new Error(body.error||'Commando geweigerd');return body;}finally{clearTimeout(timer);}}
async function command(action,extra={}){if(busy||!online)return;const g=generation;busy=true;if(state)render(state);try{const s=await request('/api/control',{action,token,...extra});if(g!==generation)return;if(s.session_token)token=String(s.session_token);render(s);}catch(e){if(g===generation)offline(e.name==='AbortError'?'Commando niet bevestigd; bediening wordt vergrendeld.':e.message);}finally{if(g===generation){busy=false;if(online&&state)render(state);}}}
async function emergency(){generation++;token='';busy=false;offline('Stop aangevraagd; wacht op bevestiging van de Wemos.');const g=generation;try{const s=await request('/api/stop',{});if(g===generation)render(s);}catch(e){if(g===generation)offline('STOP NIET BEVESTIGD. Onderbreek de motorvoeding met de fysieke schakelaar.');}}
async function poll(){if(polling)return;polling=true;const g=generation;try{const s=token?await request('/api/heartbeat',{token}):await request('/api/status');if(g===generation)render(s);}catch(e){if(g===generation)offline('Verbinding verloren. Softwarestop niet bevestigd; gebruik zo nodig de fysieke onderbreker.');}finally{polling=false;}}
$('emergency').addEventListener('click',emergency);
$('arm').addEventListener('click',()=>{if(confirm('Het rad staat fysiek stil en ik ben klaar voor handbediening. Vrijgeven start de motor niet.'))command('arm',{confirm:'stil',epoch:String(epoch)});});
$('left').addEventListener('click',()=>command('start',{direction:'left'}));$('right').addEventListener('click',()=>command('start',{direction:'right'}));
$('minus').addEventListener('click',()=>command('step',{delta:'-1'}));$('plus').addEventListener('click',()=>command('step',{delta:'1'}));$('slow').addEventListener('click',()=>command('slow_stop'));
$('pwm').addEventListener('input',()=>{dragging=true;});$('pwm').addEventListener('change',()=>{dragging=false;command('set_pwm',{value:$('pwm').value});});
poll();setInterval(poll,650);
</script></body></html>)HTML";
