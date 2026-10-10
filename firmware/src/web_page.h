// Interfaz web (una sola página, sin dependencias externas: funciona sin
// Internet en el punto de acceso del ESP32).
#pragma once

#include <pgmspace.h>

static const char kIndexHtml[] PROGMEM = R"HTML(<!doctype html>
<html lang="es">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Märklin ESP32</title>
<style>
:root{--bg:#14161a;--panel:#1e2228;--line:#2e343d;--text:#e6e8eb;--dim:#9aa3ad;--acc:#f0b429;--ok:#3fb950;--bad:#f85149;--blue:#58a6ff}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);font:15px/1.4 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
header{position:sticky;top:0;z-index:5;background:#0e1013;border-bottom:1px solid var(--line);padding:8px 12px;display:flex;flex-wrap:wrap;gap:8px;align-items:center}
header h1{font-size:17px;margin:0 8px 0 0;color:var(--acc)}
.badge{padding:3px 9px;border-radius:12px;font-size:13px;background:var(--line)}
.badge.on{background:var(--ok);color:#000}.badge.off{background:var(--bad);color:#fff}
.sp{flex:1}
button{background:var(--line);color:var(--text);border:1px solid #3a414b;border-radius:7px;padding:7px 12px;font-size:14px;cursor:pointer}
button:hover{border-color:var(--dim)}
button.go{background:#1f6f37;border-color:#2ea043}button.stop{background:#8b1d1d;border-color:var(--bad)}
button.emerg{background:var(--bad);color:#fff;font-weight:700;border-color:#ff7b72}
button.act{background:var(--acc);color:#000;border-color:var(--acc)}
button.small{padding:4px 8px;font-size:13px}
nav{display:flex;gap:4px;padding:8px 12px 0;flex-wrap:wrap}
nav button{border-radius:7px 7px 0 0;border-bottom:none}
nav button.sel{background:var(--panel);color:var(--acc)}
main{padding:12px}
section{display:none}section.sel{display:block}
.panel{background:var(--panel);border:1px solid var(--line);border-radius:10px;padding:12px;margin-bottom:12px}
.panel h2{font-size:15px;margin:0 0 10px;color:var(--dim);font-weight:600}
.row{display:flex;flex-wrap:wrap;gap:8px;align-items:center}
input,textarea,select{background:#0e1013;color:var(--text);border:1px solid var(--line);border-radius:7px;padding:7px 9px;font-size:14px}
textarea{width:100%;font:13px/1.45 ui-monospace,Consolas,monospace;resize:vertical}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(270px,1fr));gap:12px}
.loco .name{font-weight:600}.loco .ref{color:var(--dim);font-size:13px}
.loco .spd{font-size:26px;font-weight:700;color:var(--acc);min-width:70px;text-align:right}
.loco input[type=range]{width:100%;margin:10px 0;accent-color:var(--acc)}
.fns{display:grid;grid-template-columns:repeat(8,1fr);gap:4px;margin-top:8px}
.fns button{padding:5px 0;font-size:12px}
.dir{color:var(--dim);font-size:13px}
table{width:100%;border-collapse:collapse;font-size:14px}
td,th{padding:6px;border-bottom:1px solid var(--line);text-align:left;vertical-align:top}
th{color:var(--dim);font-weight:500}
.sensors{display:grid;grid-template-columns:repeat(auto-fill,minmax(110px,1fr));gap:8px}
.sensor{border:1px solid var(--line);border-radius:8px;padding:8px;text-align:center;cursor:pointer;user-select:none}
.sensor.s1{background:var(--ok);color:#000;border-color:var(--ok)}.sensor.s1 .dir{color:#000}
.sensor.s-1{border-style:dashed;color:var(--dim)}
.sensor b{display:block;font-size:20px}
#log{height:55vh;overflow:auto;background:#0b0c0e;border:1px solid var(--line);border-radius:8px;padding:8px;font:12px/1.5 ui-monospace,Consolas,monospace;white-space:pre-wrap}
.k-rx{color:#7d8590}.k-tx{color:var(--blue)}.k-script{color:var(--acc)}.k-error{color:var(--bad)}
.msg{min-height:20px;font-size:14px;margin-top:6px}.msg.err{color:var(--bad)}.msg.ok{color:var(--ok)}
#toast{position:fixed;bottom:16px;left:50%;transform:translateX(-50%);background:#000c;padding:8px 16px;border-radius:8px;display:none;z-index:9}
.help{font:12.5px/1.5 ui-monospace,Consolas,monospace;color:var(--dim);white-space:pre-wrap;margin:0}
details summary{cursor:pointer;color:var(--dim)}
.kv{display:grid;grid-template-columns:max-content 1fr;gap:4px 14px;font-size:14px}.kv span:nth-child(odd){color:var(--dim)}
</style>
</head>
<body>
<header>
  <h1>Märklin ESP32</h1>
  <span id="power" class="badge">vía: ?</span>
  <span id="ms2" class="badge">MS2: ?</span>
  <span class="sp"></span>
  <button class="go" onclick="post('/api/power',{on:1})">GO</button>
  <button class="stop" onclick="post('/api/power',{on:0})">STOP</button>
  <button class="emerg" onclick="post('/api/emergency')" title="Corta la vía y detiene todos los scripts">EMERGENCIA</button>
</header>
<nav>
  <button data-tab="mando" class="sel">Mando</button>
  <button data-tab="auto">Automático</button>
  <button data-tab="sens">Sensores y desvíos</button>
  <button data-tab="mon">Monitor</button>
  <button data-tab="cfg">Ajustes</button>
</nav>
<main>

<section id="mando" class="sel">
  <div class="panel row">
    <input id="newLoco" placeholder="mm2:24, dcc:3, mfx:5" size="16">
    <button onclick="addLoco()">Añadir loco</button>
    <span class="sp"></span>
    <button onclick="post('/api/halt')">Parar todas las locos</button>
  </div>
  <div id="locos" class="grid"></div>
  <p id="noLocos" class="dir">No hay locos todavía. Añade una con su dirección, defínelas en Ajustes,
  o mueve una loco con la MS2: aparecerá aquí automáticamente.</p>
</section>

<section id="auto">
  <div class="panel">
    <h2>Scripts en marcha</h2>
    <table><thead><tr><th>Script</th><th>Estado</th><th>Línea</th><th>Actividad</th><th></th></tr></thead>
    <tbody id="runners"></tbody></table>
    <div class="row" style="margin-top:8px"><button onclick="post('/api/scriptstop',{all:1})">Detener todos</button></div>
  </div>
  <div class="panel">
    <h2>Scripts guardados</h2>
    <table><tbody id="scripts"></tbody></table>
  </div>
  <div class="panel">
    <h2>Editor</h2>
    <div class="row" style="margin-bottom:8px">
      <input id="sname" placeholder="nombre_del_script" size="22">
      <button onclick="saveScript()">Guardar</button>
      <button onclick="checkScript()">Comprobar</button>
      <button onclick="saveScript(true)">Guardar y ejecutar</button>
      <button onclick="newScript()">Nuevo</button>
      <span class="sp"></span>
      <button onclick="delScript()">Borrar</button>
    </div>
    <textarea id="src" rows="20" spellcheck="false" placeholder="# Escribe aquí tu script"></textarea>
    <div id="smsg" class="msg"></div>
    <details><summary>Chuleta del lenguaje</summary><pre class="help">
loco tren mm2 24            alias de loco (mm2 / dcc / mfx + dirección)
sensor anden 3              alias de sensor
power on | off              corriente de vía
speed tren 400 | 40%        velocidad 0-1000 o porcentaje
ramp tren 60% 3s            acelera/frena gradualmente
dir tren fwd | rev | toggle sentido (deja la loco parada)
func tren 0 on | off        funciones F0-F31
estop tren                  parada de emergencia de una loco
halt                        para todas las locos
switch 5 straight | round   desvío/señal (también dcc:5)
wait 2s | 500ms | 1m        espera
wait random 10s 30s         espera aleatoria
wait sensor 3 on [timeout 60s [else etiqueta]]
label nombre / goto nombre  saltos
if sensor 3 on goto x       salto condicional
if chance 30 goto x         salto al azar (30 %)
if locked puente goto x     si otro script tiene el bloqueo
repeat 5 ... end            bucle (repeat sin número = siempre)
lock puente / unlock puente reservar un tramo entre scripts
log texto                   mensaje al registro
exit                        terminar
</pre></details>
  </div>
  <div class="panel"><h2>Mensajes de los scripts</h2><div id="slog" class="help"></div></div>
</section>

<section id="sens">
  <div class="panel">
    <h2>Sensores <span class="dir">(clic = simular cambio)</span></h2>
    <div id="sensors" class="sensors"></div>
    <div class="row" style="margin-top:10px">
      <input id="simId" type="number" min="1" placeholder="nº sensor" style="width:110px">
      <button onclick="simSensor(1)">Simular ON</button>
      <button onclick="simSensor(0)">Simular OFF</button>
    </div>
  </div>
  <div class="panel">
    <h2>Desvíos y señales</h2>
    <div class="row">
      <input id="accAddr" placeholder="5 o dcc:12" size="10">
      <button onclick="acc(1)">Recto / verde</button>
      <button onclick="acc(0)">Desviado / rojo</button>
      <button onclick="acc(2)">Amarillo</button>
      <button onclick="acc(3)">Blanco</button>
    </div>
    <table style="margin-top:10px"><tbody id="accs"></tbody></table>
  </div>
</section>

<section id="mon">
  <div class="panel row">
    <input id="cmd" placeholder="speed mm2:24 30%   |   func dcc:3 0 on   |   switch 5 round" style="flex:1;min-width:200px">
    <button onclick="sendCmd()">Enviar</button>
  </div>
  <div class="panel">
    <div class="row" style="margin-bottom:8px">
      <label><input type="checkbox" id="fRx" checked> CAN recibido</label>
      <label><input type="checkbox" id="fTx" checked> CAN enviado</label>
      <label><input type="checkbox" id="fInfo" checked> Info/scripts</label>
      <label><input type="checkbox" id="fPause"> Pausa</label>
      <span class="sp"></span>
      <button class="small" onclick="logLines=[];renderLog()">Limpiar</button>
    </div>
    <div id="log"></div>
  </div>
</section>

<section id="cfg">
  <div class="panel">
    <h2>Estado</h2>
    <div id="info" class="kv"></div>
  </div>
  <div class="panel">
    <h2>Equipos en el bus CAN</h2>
    <table><tbody id="devices"></tbody></table>
    <div class="row" style="margin-top:8px"><button onclick="post('/api/ping')">Buscar equipos (ping)</button></div>
  </div>
  <div class="panel">
    <h2>Configuración (JSON)</h2>
    <textarea id="cfgText" rows="22" spellcheck="false"></textarea>
    <div class="row" style="margin-top:8px">
      <button onclick="loadCfg()">Recargar</button>
      <button class="act" onclick="saveCfg()">Guardar y reiniciar</button>
      <span class="sp"></span>
      <button onclick="post('/api/restart')">Reiniciar ESP32</button>
    </div>
    <div id="cmsg" class="msg"></div>
  </div>
</section>
</main>
<div id="toast"></div>

<script>
const $=s=>document.querySelector(s);
const esc=s=>String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
const kcls=k=>({'can<':'rx','can>':'tx'}[k]||k);
let state={},logSeq=0,logLines=[],tab='mando',dragging={};

function toast(t,bad){const e=$('#toast');e.textContent=t;e.style.display='block';e.style.color=bad?'#ff7b72':'#fff';clearTimeout(e._t);e._t=setTimeout(()=>e.style.display='none',2500)}
async function api(path,opt){
  const r=await fetch(path,opt);const ct=r.headers.get('content-type')||'';
  const d=ct.includes('json')?await r.json():await r.text();
  if(!r.ok||(d&&d.ok===false))throw new Error((d&&d.error)||r.statusText);
  return d;
}
async function post(path,params,body){
  const q=new URLSearchParams(params||{}).toString();
  try{const d=await api(path+(q?'?'+q:''),{method:'POST',body:body});if(d&&d.msg&&d.msg!='ok')toast(d.msg);refresh();return d}
  catch(e){toast(e.message,true);throw e}
}

document.querySelectorAll('nav button').forEach(b=>b.onclick=()=>{
  tab=b.dataset.tab;
  document.querySelectorAll('nav button').forEach(x=>x.classList.toggle('sel',x==b));
  document.querySelectorAll('section').forEach(s=>s.classList.toggle('sel',s.id==tab));
  if(tab=='auto')loadScripts();
  if(tab=='cfg'&&!$('#cfgText').value)loadCfg();
});

// ---- Mando ----
function addLoco(){const v=$('#newLoco').value.trim();if(v)post('/api/loco',{loco:v,add:1}).then(()=>$('#newLoco').value='')}
function locoCard(l){
  const d=document.createElement('div');d.className='panel loco';d.dataset.ref=l.ref;
  let fns='';for(let i=0;i<16;i++)fns+=`<button data-fn="${i}">F${i}</button>`;
  d.innerHTML=`<div class="row"><div><div class="name"></div><div class="ref">${esc(l.ref)}</div></div><span class="sp"></span><div class="spd">0</div></div>
  <input type="range" min="0" max="1000" step="10" value="0">
  <div class="row"><button data-d="rev">◀ Atrás</button><button data-s="0">■ Parar</button><button data-d="fwd">Adelante ▶</button><span class="sp"></span><span class="dir"></span></div>
  <div class="fns">${fns}</div>`;
  const rng=d.querySelector('input');let timer=null;
  const send=()=>{timer=null;post('/api/loco',{loco:l.ref,speed:rng.value}).catch(()=>{})};
  rng.oninput=()=>{dragging[l.ref]=Date.now();d.querySelector('.spd').textContent=Math.round(rng.value/10)+'%';if(!timer)timer=setTimeout(send,150)};
  rng.onchange=()=>{dragging[l.ref]=Date.now();if(timer){clearTimeout(timer)}send()};
  d.querySelectorAll('[data-d]').forEach(b=>b.onclick=()=>post('/api/loco',{loco:l.ref,dir:b.dataset.d}));
  d.querySelector('[data-s]').onclick=()=>post('/api/loco',{loco:l.ref,speed:0});
  d.querySelectorAll('[data-fn]').forEach(b=>b.onclick=()=>{
    const cur=state.locos.find(x=>x.ref==l.ref);const on=cur&&(cur.fn>>>b.dataset.fn)&1;
    post('/api/loco',{loco:l.ref,fn:b.dataset.fn,on:on?0:1});
  });
  return d;
}
function renderLocos(){
  const box=$('#locos');const seen=new Set();
  for(const l of state.locos){
    seen.add(l.ref);
    let d=box.querySelector(`[data-ref="${CSS.escape(l.ref)}"]`);
    if(!d){d=locoCard(l);box.appendChild(d)}
    d.querySelector('.name').textContent=l.name||'Loco '+l.ref;
    if(!(Date.now()-(dragging[l.ref]||0)<1500)){
      d.querySelector('input').value=l.speed;
      d.querySelector('.spd').textContent=Math.round(l.speed/10)+'%';
    }
    d.querySelector('.dir').textContent=l.dir==1?'sentido: adelante':l.dir==2?'sentido: atrás':'sentido: ?';
    d.querySelectorAll('[data-fn]').forEach(b=>b.classList.toggle('act',((l.fn>>>b.dataset.fn)&1)==1));
  }
  box.querySelectorAll('.loco').forEach(d=>{if(!seen.has(d.dataset.ref))d.remove()});
  $('#noLocos').style.display=state.locos.length?'none':'block';
}

// ---- Automático ----
async function loadScripts(){
  try{
    const list=await api('/api/scripts');
    $('#scripts').innerHTML=list.map(s=>`<tr><td>${esc(s.name)}${s.running?' <span class="badge on">en marcha</span>':''}</td>
    <td style="text-align:right"><button class="small" onclick="runScript('${s.name}')">▶ Ejecutar</button>
    <button class="small" onclick="editScript('${s.name}')">Editar</button></td></tr>`).join('')||'<tr><td class="dir">No hay scripts</td></tr>';
  }catch(e){toast(e.message,true)}
}
function runScript(n){post('/api/run',{name:n}).then(loadScripts)}
async function editScript(n){
  try{$('#src').value=await api('/api/script?name='+encodeURIComponent(n));$('#sname').value=n;smsg('');$('#src').scrollIntoView({behavior:'smooth'})}
  catch(e){toast(e.message,true)}
}
function newScript(){$('#sname').value='';$('#src').value='# Nuevo script\nloco tren mm2 24\n\npower on\n';smsg('')}
function smsg(t,ok){const e=$('#smsg');e.textContent=t;e.className='msg '+(t?(ok?'ok':'err'):'')}
function showCheck(d){if(d.valid)smsg('Sin errores',true);else smsg('Línea '+d.line+': '+d.error)}
async function saveScript(run){
  const n=$('#sname').value.trim();if(!n){smsg('Escribe un nombre');return}
  try{const d=await post('/api/script',{name:n},$('#src').value);showCheck(d);loadScripts();
    if(run&&d.valid)await post('/api/run',{name:n});}catch(e){smsg(e.message)}
}
async function checkScript(){try{showCheck(await post('/api/check',{},$('#src').value))}catch(e){smsg(e.message)}}
async function delScript(){const n=$('#sname').value.trim();if(!n||!confirm('¿Borrar el script '+n+'?'))return;
  try{await post('/api/script/delete',{name:n});$('#src').value='';$('#sname').value='';loadScripts()}catch(e){}}
function renderRunners(){
  $('#runners').innerHTML=state.runners.map(r=>`<tr><td>${esc(r.name)}</td><td>${esc(r.state)}</td><td>${r.line||''}</td>
  <td>${esc(r.activity)}</td><td>${r.active?`<button class="small" onclick="post('/api/scriptstop',{slot:${r.slot}})">Detener</button>`:''}</td></tr>`).join('')
  ||'<tr><td colspan="5" class="dir">Ningún script en marcha</td></tr>';
}

// ---- Sensores y desvíos ----
function renderSensors(){
  $('#sensors').innerHTML=state.sensors.map(s=>`<div class="sensor s${s.state}" onclick="post('/api/sensor',{id:${s.id},state:${s.state==1?0:1}})">
  <b>${s.id}</b>${esc(s.name||'')}<div class="dir">${s.state==1?'ON':s.state==0?'libre':'?'}</div></div>`).join('')
  ||'<span class="dir">Sin sensores. Defínelos en Ajustes (GPIO) o llegarán por CAN (Link S88).</span>';
}
function simSensor(v){const id=$('#simId').value;if(id)post('/api/sensor',{id:id,state:v})}
function acc(pos,addr){addr=addr||$('#accAddr').value.trim();if(addr)post('/api/accessory',{addr:addr,pos:pos})}
function renderAccs(){
  const names=['desviado/rojo','recto/verde','amarillo','blanco'];
  $('#accs').innerHTML=state.accessories.map(a=>`<tr><td>${esc(a.ref)}</td><td>${names[a.pos]||a.pos}</td>
  <td style="text-align:right"><button class="small" onclick="acc(1,'${a.ref}')">Recto</button> <button class="small" onclick="acc(0,'${a.ref}')">Desviado</button></td></tr>`).join('');
}

// ---- Monitor ----
function sendCmd(){const c=$('#cmd').value.trim();if(c)post('/api/exec',{},c).then(()=>$('#cmd').value='').catch(()=>{})}
$('#cmd').addEventListener('keydown',e=>{if(e.key=='Enter')sendCmd()});
function logVisible(k){return k=='can<'?$('#fRx').checked:k=='can>'?$('#fTx').checked:$('#fInfo').checked}
function renderLog(){
  if($('#fPause').checked)return;
  const el=$('#log');const atEnd=el.scrollTop+el.clientHeight>=el.scrollHeight-20;
  el.innerHTML=logLines.filter(e=>logVisible(e.k)).map(e=>`<div class="k-${kcls(e.k)}">${(e.t/1000).toFixed(1).padStart(8)}  ${esc(e.m)}</div>`).join('');
  if(atEnd)el.scrollTop=el.scrollHeight;
  $('#slog').innerHTML=logLines.filter(e=>e.k=='script'||e.k=='error').slice(-10).map(e=>`<div class="k-${e.k}">${esc(e.m)}</div>`).join('');
}
['#fRx','#fTx','#fInfo','#fPause'].forEach(s=>$(s).onchange=renderLog);
let logBusy=false;
async function pollLog(){
  if(logBusy)return;logBusy=true;
  try{let d=await api('/api/log?since='+logSeq);
    if(d.seq<logSeq){logSeq=0;logLines=[];d=await api('/api/log?since=0')}
    const fresh=d.entries.filter(e=>e.s>logSeq);
    if(fresh.length){logLines=logLines.concat(fresh).slice(-600);renderLog()}
    logSeq=Math.max(logSeq,d.seq);
  }catch(e){}finally{logBusy=false}
}

// ---- Ajustes ----
async function loadCfg(){try{$('#cfgText').value=await api('/api/config').then(j=>JSON.stringify(j,null,2))}catch(e){toast(e.message,true)}}
async function saveCfg(){
  const t=$('#cfgText').value;try{JSON.parse(t)}catch(e){$('#cmsg').textContent='JSON no válido: '+e.message;$('#cmsg').className='msg err';return}
  try{await post('/api/config',{},t);$('#cmsg').textContent='Guardado. Reiniciando... recarga la página en unos segundos.';$('#cmsg').className='msg ok'}
  catch(e){$('#cmsg').textContent=e.message;$('#cmsg').className='msg err'}
}
function renderInfo(){
  const c=state.can,w=state.wifi;
  const kv=[['WiFi',w.mode=='sta'?'conectado a la red (RSSI '+w.rssi+' dBm)':'punto de acceso propio'],['IP',w.ip],
    ['CAN',c.state+' · hash '+c.hash],['Tramas',`recibidas ${c.rx} · enviadas ${c.tx} · fallos de envío ${c.txFail}`],
    ['Errores CAN',`TEC ${c.tec} · REC ${c.rec} · errores de bus ${c.busErrors}`+(c.tec>=128?' — ¡nadie confirma las tramas! revisa CAN-H/CAN-L/GND':'')],
    ['MS2',state.ms2?'detectada':'no detectada'],['Memoria libre',state.heap+' bytes'],['En marcha',Math.floor(state.uptime/60)+' min']];
  $('#info').innerHTML=kv.map(([k,v])=>`<span>${k}</span><span>${esc(v)}</span>`).join('');
  $('#devices').innerHTML=state.devices.map(d=>`<tr><td>${esc(d.type)}</td><td>${d.uid}</td><td>v${d.version}</td></tr>`).join('')
    ||'<tr><td class="dir">Ninguno (pulsa "Buscar equipos")</td></tr>';
}

async function refresh(){
  if(document.hidden)return;
  try{
    state=await api('/api/state');
    const p=$('#power');p.textContent=state.overload?'¡SOBRECARGA!':'vía: '+(state.power=='on'?'ON':state.power=='off'?'OFF':'?');
    p.className='badge '+(state.power=='on'?'on':state.power=='off'?'off':'');
    $('#ms2').textContent='MS2: '+(state.ms2?'sí':'no');
    renderLocos();renderRunners();renderSensors();renderAccs();renderInfo();
    if(state.logSeq!=logSeq)pollLog();
  }catch(e){$('#power').textContent='sin conexión';$('#power').className='badge off'}
}
setInterval(refresh,800);refresh();pollLog();
</script>
</body>
</html>
)HTML";
