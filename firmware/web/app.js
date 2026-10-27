const $=s=>document.querySelector(s);
applyI18n();initLangSelector();
const esc=s=>String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
const kcls=k=>({'can<':'rx','can>':'tx'}[k]||k);
let state={},logSeq=0,logLines=[],tab='desk',layoutsLoaded=false;

function toast(msg,bad){const e=$('#toast');e.textContent=tr(msg);e.style.display='block';e.style.color=bad?'#ff7b72':'#fff';clearTimeout(e._t);e._t=setTimeout(()=>e.style.display='none',2500)}
async function api(path,opt){
  const r=await fetch(path,opt);const ct=r.headers.get('content-type')||'';
  const d=ct.includes('json')?await r.json():await r.text();
  if(!r.ok||(d&&d.ok===false))throw new Error(tr((d&&d.error)||r.statusText));
  return d;
}
async function post(path,params,body){
  const q=new URLSearchParams(params||{}).toString();
  try{const d=await api(path+(q?'?'+q:''),{method:'POST',body:body});if(d&&d.msg&&d.msg!='ok')toast(d.msg);refresh();return d}
  catch(e){toast(e.message,true);throw e}
}

function showTab(t){
  tab=t;
  document.querySelectorAll('nav button').forEach(x=>x.classList.toggle('sel',x.dataset.tab==tab));
  document.querySelectorAll('section').forEach(s=>s.classList.toggle('sel',s.id==tab));
  if(tab=='auto')loadScripts();
  if(tab=='maq')loadLayouts();
  if(tab=='inv')loadInventory();
  if(tab=='cfg'&&!$('#cfgText').value)loadCfg();
}
document.querySelectorAll('nav button').forEach(b=>b.onclick=()=>showTab(b.dataset.tab));

// ---- Automation ----
async function loadScripts(){
  try{
    const list=await api('/api/scripts');
    $('#scripts').innerHTML=list.map(s=>`<tr><td>${esc(s.name)}${s.running?' <span class="badge on">'+t('running')+'</span>':''}</td>
    <td style="text-align:right"><button class="small" onclick="runScript('${s.name}')">▶ ${t('Run')}</button>
    <button class="small" onclick="editScript('${s.name}')">${t('Edit')}</button></td></tr>`).join('')||'<tr><td class="dir">'+t('No scripts')+'</td></tr>';
  }catch(e){toast(e.message,true)}
}
function runScript(n){post('/api/run',{name:n}).then(loadScripts)}
async function editScript(n){
  try{$('#src').value=await api('/api/script?name='+encodeURIComponent(n));$('#sname').value=n;smsg('');$('#src').scrollIntoView({behavior:'smooth'})}
  catch(e){toast(e.message,true)}
}
function newScript(){$('#sname').value='';$('#src').value='# '+t('New script')+'\nloco train mm2 24\n\npower on\n';smsg('')}
function smsg(m,ok){const e=$('#smsg');e.textContent=m;e.className='msg '+(m?(ok?'ok':'err'):'')}
function showCheck(d){if(d.valid)smsg(t('No errors'),true);else smsg(t('Line {0}: {1}',d.line,tr(d.error)))}
async function saveScript(run){
  const n=$('#sname').value.trim();if(!n){smsg(t('Write a name'));return}
  try{const d=await post('/api/script',{name:n},$('#src').value);showCheck(d);loadScripts();
    if(run&&d.valid)await post('/api/run',{name:n});}catch(e){smsg(e.message)}
}
async function checkScript(){try{showCheck(await post('/api/check',{},$('#src').value))}catch(e){smsg(e.message)}}
async function delScript(){const n=$('#sname').value.trim();if(!n||!confirm(t('Delete the script {0}?',n)))return;
  try{await post('/api/script/delete',{name:n});$('#src').value='';$('#sname').value='';loadScripts()}catch(e){}}
function renderRunners(){
  $('#runners').innerHTML=state.runners.map(r=>`<tr><td>${esc(r.name)}</td><td>${esc(tr(r.state))}</td><td>${r.line||''}</td>
  <td>${esc(tr(r.activity))}</td><td>${r.active?`<button class="small" onclick="post('/api/scriptstop',{slot:${r.slot}})">${t('Stop')}</button>`:''}</td></tr>`).join('')
  ||'<tr><td colspan="5" class="dir">'+t('No script running')+'</td></tr>';
}

// ---- Sensors and turnouts by address ----
function renderSensors(){
  $('#sensors').innerHTML=state.sensors.map(s=>`<div class="sensor s${s.state}" onclick="post('/api/sensor',{id:${s.id},state:${s.state==1?0:1}})">
  <b>${s.id}</b>${esc(s.name||'')}<div class="dir">${s.state==1?'ON':s.state==0?t('free'):'?'}</div></div>`).join('')
  ||'<span class="dir">'+t('No sensors. Define them in Settings (GPIO) or they will arrive over CAN (Link S88).')+'</span>';
}
function simSensor(v){const id=$('#simId').value;if(id)post('/api/sensor',{id:id,state:v})}
function acc(pos,addr){addr=addr||$('#accAddr').value.trim();if(addr)post('/api/accessory',{addr:addr,pos:pos})}
function renderAccs(){
  const names=[t('diverging/red'),t('straight/green'),t('yellow'),t('white')];
  $('#accs').innerHTML=state.accessories.map(a=>`<tr><td>${esc(a.ref)}</td><td>${names[a.pos]||a.pos}</td>
  <td style="text-align:right"><button class="small" onclick="acc(1,'${a.ref}')">${t('Straight')}</button> <button class="small" onclick="acc(0,'${a.ref}')">${t('Diverging')}</button></td></tr>`).join('');
}

// ---- Monitor ----
function sendCmd(){const c=$('#cmd').value.trim();if(c)post('/api/exec',{},c).then(()=>$('#cmd').value='').catch(()=>{})}
$('#cmd').addEventListener('keydown',e=>{if(e.key=='Enter')sendCmd()});
function logVisible(k){return k=='can<'?$('#fRx').checked:k=='can>'?$('#fTx').checked:$('#fInfo').checked}
function renderLog(){
  if($('#fPause').checked)return;
  const el=$('#log');const atEnd=el.scrollTop+el.clientHeight>=el.scrollHeight-20;
  el.innerHTML=logLines.filter(e=>logVisible(e.k)).map(e=>`<div class="k-${kcls(e.k)}">${(e.t/1000).toFixed(1).padStart(8)}  ${esc(e.k=='can<'||e.k=='can>'?e.m:tr(e.m))}</div>`).join('');
  if(atEnd)el.scrollTop=el.scrollHeight;
  $('#slog').innerHTML=logLines.filter(e=>e.k=='script'||e.k=='error').slice(-10).map(e=>`<div class="k-${e.k}">${esc(tr(e.m))}</div>`).join('');
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

// ---- Settings ----
async function loadCfg(){try{$('#cfgText').value=await api('/api/config').then(j=>JSON.stringify(j,null,2))}catch(e){toast(e.message,true)}}
async function saveCfg(){
  const txt=$('#cfgText').value;try{JSON.parse(txt)}catch(e){$('#cmsg').textContent=t('Invalid JSON: {0}',e.message);$('#cmsg').className='msg err';return}
  try{await post('/api/config',{},txt);$('#cmsg').textContent=t('Saved. Restarting... reload the page in a few seconds.');$('#cmsg').className='msg ok'}
  catch(e){$('#cmsg').textContent=e.message;$('#cmsg').className='msg err'}
}
function renderInfo(){
  const c=state.can,w=state.wifi;
  const kv=[[t('Home WiFi'),w.mode!='sta'?t('not configured (own network only)'):w.connected?t('connected to «{0}» (signal {1} dBm)',w.ssid,w.rssi):t('NOT connected to «{0}»: check the password and that it is a 2.4 GHz network',w.ssid)],
    [t('Open from home'),w.connected?'http://'+w.ip+'  ·  http://'+w.hostname+'.local':'—'],
    [t('Own network'),'«'+w.apSsid+'» → http://'+w.apIp],
    ['CAN',c.state+' · hash '+c.hash],[t('Frames'),t('received {0} · sent {1} · send failures {2}',c.rx,c.tx,c.txFail)],
    [t('CAN errors'),`TEC ${c.tec} · REC ${c.rec} · `+t('bus errors {0}',c.busErrors)+(c.tec>=128?' — '+t('nobody acknowledges the frames! check CAN-H/CAN-L/GND'):'')],
    ['MS2',state.ms2?t('detected'):t('not detected')],[t('Active layout'),state.layout||t('none')],[t('Free memory'),state.heap+' bytes'],
    [t('Uptime'),Math.floor(state.uptime/60)+' min'],[t('Version'),state.version]];
  $('#info').innerHTML=kv.map(([k,v])=>`<span>${k}</span><span>${esc(v)}</span>`).join('');
  const n=state.lan||{},apps=(n.tcp||0)+(n.udp||0);
  $('#lan').innerHTML=[[t('Connected apps'),apps?apps+' app'+(apps==1?'':'s'):t('none')],
    [t('IP for the app'),t('{0} (network «{1}»)',w.apIp,w.apSsid)+(w.connected?' · '+t('{0} (home WiFi)',w.ip):'')],
    [t('Ports'),'TCP 15731 · UDP 15731/15730'],
    [t('App traffic'),t('{0} commands · {1} lists sent',n.frames||0,n.files||0)]]
    .map(([k,v])=>`<span>${k}</span><span>${esc(v)}</span>`).join('');
  const help=t('The ESP32 presents itself on the network as a Central Station. On the iPad: join the WiFi <b>{0}</b> (or your home WiFi, if the ESP32 is on it), open RailControl Pro, choose <b>Central Station</b> and type the IP <b>{1}</b>. The app receives the inventory locos and the turnouts of the active layout.',esc(w.apSsid),esc(w.apIp));
  if($('#lanHelp').innerHTML!==help)$('#lanHelp').innerHTML=help;
  $('#devices').innerHTML=state.devices.map(d=>`<tr><td>${esc(tr(d.type))}</td><td>${d.uid}</td><td>v${d.version}</td></tr>`).join('')
    ||'<tr><td class="dir">'+t('None (press "Find devices")')+'</td></tr>';
}

async function refresh(){
  if(document.hidden)return;
  try{
    state=await api('/api/state');
    const p=$('#power');p.textContent=state.overload?t('OVERLOAD!'):t('track: {0}',state.power=='on'?'ON':state.power=='off'?'OFF':'?');
    p.className='badge '+(state.power=='on'?'on':state.power=='off'?'off':'');
    $('#ms2').textContent='MS2: '+(state.ms2?t('yes'):t('no'));
    if(state.inv!==INV.seq)loadInventory();
    renderDesk();renderDetected();renderImport();renderRunners();renderSensors();renderAccs();renderInfo();
    if(!layoutsLoaded){layoutsLoaded=true;loadLayouts()}
    updatePlanState();
    if(state.logSeq!=logSeq)pollLog();
    let later=false;try{later=sessionStorage.getItem('wizLater')=='1'}catch(e){}
    if(!state.setupDone&&!WZ.open&&!WZ.done&&!later)openWizard();
    wzTick();
  }catch(e){$('#power').textContent=t('no connection');$('#power').className='badge off'}
}
setInterval(refresh,800);refresh();pollLog();
