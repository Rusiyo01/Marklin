const $=s=>document.querySelector(s);
const esc=s=>String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
const kcls=k=>({'can<':'rx','can>':'tx'}[k]||k);
let state={},logSeq=0,logLines=[],tab='desk',layoutsLoaded=false;

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
  if(tab=='maq')loadLayouts();
  if(tab=='cfg'&&!$('#cfgText').value)loadCfg();
});

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
  const kv=[['WiFi de casa',w.mode!='sta'?'no configurada (solo red propia)':w.connected?'conectado a «'+w.ssid+'» (señal '+w.rssi+' dBm)':'NO conectado a «'+w.ssid+'»: revisa la contraseña y que sea de 2,4 GHz'],
    ['Abrir desde casa',w.connected?'http://'+w.ip+'  ó  http://'+w.hostname+'.local':'—'],
    ['Red propia','«'+w.apSsid+'» → http://'+w.apIp],
    ['CAN',c.state+' · hash '+c.hash],['Tramas',`recibidas ${c.rx} · enviadas ${c.tx} · fallos de envío ${c.txFail}`],
    ['Errores CAN',`TEC ${c.tec} · REC ${c.rec} · errores de bus ${c.busErrors}`+(c.tec>=128?' — ¡nadie confirma las tramas! revisa CAN-H/CAN-L/GND':'')],
    ['MS2',state.ms2?'detectada':'no detectada'],['Maqueta activa',state.layout||'ninguna'],['Memoria libre',state.heap+' bytes'],
    ['En marcha',Math.floor(state.uptime/60)+' min'],['Versión',state.version]];
  $('#info').innerHTML=kv.map(([k,v])=>`<span>${k}</span><span>${esc(v)}</span>`).join('');
  const n=state.lan||{},apps=(n.tcp||0)+(n.udp||0);
  $('#lan').innerHTML=[['Apps conectadas',apps?apps+(apps==1?' app':' apps'):'ninguna'],
    ['IP para la app',w.apIp+' (red «'+w.apSsid+'»)'+(w.connected?' · '+w.ip+' (WiFi de casa)':'')],
    ['Puertos','TCP 15731 · UDP 15731/15730'],
    ['Tráfico de apps',`${n.frames||0} órdenes · ${n.files||0} listas enviadas`]]
    .map(([k,v])=>`<span>${k}</span><span>${esc(v)}</span>`).join('');
  document.querySelectorAll('.apName').forEach(e=>e.textContent=w.apSsid);
  document.querySelectorAll('.lanIp').forEach(e=>e.textContent=w.apIp);
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
    renderDesk();renderRunners();renderSensors();renderAccs();renderInfo();
    if(!layoutsLoaded){layoutsLoaded=true;loadLayouts()}
    updatePlanState();
    if(state.logSeq!=logSeq)pollLog();
    let later=false;try{later=sessionStorage.getItem('wizLater')=='1'}catch(e){}
    if(!state.setupDone&&!WZ.open&&!WZ.done&&!later)openWizard();
    wzTick();
  }catch(e){$('#power').textContent='sin conexión';$('#power').className='badge off'}
}
setInterval(refresh,800);refresh();pollLog();
