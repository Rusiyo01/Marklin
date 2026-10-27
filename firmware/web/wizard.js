// ---- Asistente de primera configuración ----
// Aparece la primera vez (setupDone = false): comprueba el bus CAN, recoge
// las locos (detectadas desde la MS2 o a mano), elige la maqueta y la WiFi.
const WZ = { open: false, step: 0, locos: [], removed: new Set(), layouts: [], layout: "", wifi: { mode: "ap", ssid: "", password: "" }, test: true, done: false };
const WZ_STEPS = ["Conexión", "Locomotoras", "Maqueta", "WiFi", "Listo"];

function openWizard() {
  WZ.open = true;
  WZ.step = 0;
  WZ.done = false;
  WZ.locos = (state.locos || []).map((l) => ({ ref: l.ref, name: l.label || l.name || "" }));
  WZ.layout = state.layout || "";
  WZ.wifi.mode = state.wifi && state.wifi.mode === "sta" ? "sta" : "ap";
  $("#wizard").hidden = false;
  api("/api/layouts").then((l) => {
    WZ.layouts = l;
    if (WZ.step === 2) wzRender();
  }).catch(() => {});
  wzRender();
}
function closeWizard(later) {
  WZ.open = false;
  $("#wizard").hidden = true;
  if (later) {
    try { sessionStorage.setItem("wizLater", "1"); } catch (e) {}
  }
}
function wzLayoutsChanged(name) {
  if (!WZ.open) return;
  api("/api/layouts").then((l) => {
    WZ.layouts = l;
    WZ.layout = name;
    wzRender();
  }).catch(() => {});
}

// Se llama en cada refresco del estado: actualiza la comprobación y añade
// las locos que se vayan moviendo con la MS2.
function wzTick() {
  if (!WZ.open || WZ.done) return;
  for (const l of state.locos || [])
    if (!WZ.removed.has(l.ref) && !WZ.locos.some((x) => x.ref === l.ref)) WZ.locos.push({ ref: l.ref, name: l.label || l.name || "" });
  if (WZ.step === 0) wzRender();
  if (WZ.step === 1) wzRenderLocoList();
}

function wzCheck(ok, text, hint) {
  return `<div class="chk ${ok ? "ok" : "no"}"><b>${ok ? "✓" : "…"}</b><div>${text}${!ok && hint ? `<div class="dir">${hint}</div>` : ""}</div></div>`;
}

function wzRender() {
  const s = WZ.step, b = $("#wzBody");
  $("#wzSteps").innerHTML = WZ_STEPS.map((t, i) => `<span class="${i === s ? "cur" : i < s ? "past" : ""}">${i + 1}. ${t}</span>`).join("");
  $("#wzBack").style.visibility = s > 0 && !WZ.done ? "visible" : "hidden";
  $("#wzNext").textContent = s === WZ_STEPS.length - 1 ? "Terminar" : "Siguiente";
  $("#wzNext").style.display = WZ.done ? "none" : "";
  $("#wzLater").style.display = WZ.done ? "none" : "";
  if (WZ.done) return;
  if (s === 0) {
    const c = state.can || {}, dev = state.devices || [];
    const gb = dev.some((d) => d.type === "Gleisbox");
    const canOk = c.state === "ok" && !(c.tec >= 128);
    b.innerHTML = `<h2>¡Bienvenido!</h2>
      <p>Este asistente deja el sistema listo en un minuto (desde el portátil o el móvil). Primero comprobamos la conexión con la Gleisbox
      (enciéndela y, si la tienes, conecta también la MS2).</p>
      ${wzCheck(canOk, "Bus CAN funcionando", "Revisa el cableado CAN-H / CAN-L / GND (ver docs/hardware.md). Puedes seguir y revisarlo luego.")}
      ${wzCheck(gb, "Gleisbox encontrada", 'Pulsa "Buscar equipos". Si no aparece, revisa el cable y que la Gleisbox esté enchufada.')}
      ${wzCheck(state.ms2, "MS2 conectada", "Opcional: sin MS2 el ESP32 despierta la Gleisbox por sí mismo (locos MM2 y DCC).")}
      <div class="row" style="margin-top:10px"><button onclick="post('/api/ping')">Buscar equipos</button></div>`;
  } else if (s === 1) {
    b.innerHTML = `<h2>Tus locomotoras</h2>
      <p>Mueve cada loco un momento con la MS2 y aparecerá aquí sola. También puedes añadirlas con su dirección.
      Ponles el nombre que quieras (p. ej. "Cocodrilo SBB").</p>
      <div id="wzLocoList"></div>
      <div class="row" style="margin-top:10px">
        <select id="wzProto"><option value="mm2">MM2</option><option value="dcc">DCC</option><option value="mfx">mfx (SID)</option></select>
        <input id="wzAddr" type="number" min="1" placeholder="dirección" style="width:110px">
        <button onclick="wzAddLoco()">Añadir</button></div>`;
    wzRenderLocoList();
  } else if (s === 2) {
    const opt = (v, title, sub) => `<label class="opt"><input type="radio" name="wzLay" value="${esc(v)}"${WZ.layout === v ? " checked" : ""}> <span><b>${esc(title)}</b>${sub ? `<span class="dir"> ${esc(sub)}</span>` : ""}</span></label>`;
    b.innerHTML = `<h2>Tu maqueta</h2>
      <p>Elige una maqueta de ejemplo o sube la tuya desde <b>C-Track Planner</b> (botón Guardar → Exportar archivo .json,
      o la app sin conexión .html). Podrás asignar desvíos y sensores en la pestaña Maqueta.</p>
      ${WZ.layouts.map((l) => opt(l.name, l.title, l.name)).join("")}
      ${opt("", "Ninguna por ahora", "")}
      <div class="row" style="margin-top:10px"><button onclick="$('#layoutFile').click()">Subir maqueta de C-Track Planner…</button></div>`;
    b.querySelectorAll('input[name="wzLay"]').forEach((r) => (r.onchange = () => (WZ.layout = r.value)));
  } else if (s === 3) {
    const w = WZ.wifi;
    b.innerHTML = `<h2>Red WiFi</h2>
      <label class="opt"><input type="radio" name="wzW" value="ap"${w.mode === "ap" ? " checked" : ""}> <span><b>Red propia del ESP32</b>
        <span class="dir"> Conecta el portátil o el móvil a «${esc((state.wifi && state.wifi.apSsid) || "Marklin-ESP32")}». Ese dispositivo no tendrá Internet mientras tanto.</span></span></label>
      <label class="opt"><input type="radio" name="wzW" value="sta"${w.mode === "sta" ? " checked" : ""}> <span><b>Mi WiFi de casa</b>
        <span class="dir"> Recomendado para el portátil: el ESP32 se une a tu red (debe ser de 2,4 GHz), sigues con Internet y lo abres con http://marklin.local o su IP. La red propia sigue activa por si acaso.</span></span></label>
      <div id="wzSta" class="row" style="margin-top:8px"${w.mode === "sta" ? "" : " hidden"}>
        <input id="wzSsid" placeholder="Nombre de la red" value="${esc(w.ssid)}">
        <input id="wzPass" type="password" placeholder="Contraseña" value="${esc(w.password)}"></div>`;
    b.querySelectorAll('input[name="wzW"]').forEach((r) => (r.onchange = () => {
      w.mode = r.value;
      $("#wzSta").hidden = w.mode !== "sta";
    }));
  } else {
    const first = WZ.locos[0];
    const lay = WZ.layouts.find((l) => l.name === WZ.layout);
    b.innerHTML = `<h2>Todo listo</h2>
      <ul class="sum"><li>${WZ.locos.length} locomotora(s): ${WZ.locos.map((l) => `<b>${esc(wzName(l))}</b> (${esc(l.ref)})`).join(", ") || "ninguna"}</li>
      <li>Maqueta: <b>${esc(lay ? lay.title : "ninguna")}</b></li>
      <li>WiFi: <b>${WZ.wifi.mode === "sta" ? "red de casa «" + esc(WZ.wifi.ssid) + "»" : "red propia del ESP32"}</b></li></ul>
      ${first ? `<label class="opt"><input type="checkbox" id="wzTest"${WZ.test ? " checked" : ""}> <span>Crear el script <b>prueba_${esc(wzName(first))}</b>:
        una ida y vuelta corta y lenta para la primera prueba.</span></label>` : ""}`;
    const t = $("#wzTest");
    if (t) t.onchange = () => (WZ.test = t.checked);
  }
}

function wzName(l) { return slug(l.name || "loco_" + l.ref.replace(":", "")); }

function wzRenderLocoList() {
  const box = $("#wzLocoList");
  if (!box) return;
  const focused = document.activeElement && document.activeElement.dataset && document.activeElement.dataset.ref;
  if (focused) return;  // no repintar mientras se escribe
  box.innerHTML = WZ.locos.map((l, i) => `<div class="row lrow"><input data-ref="${esc(l.ref)}" value="${esc(l.name)}" placeholder="${esc(wzName(l))}" oninput="WZ.locos[${i}].name=this.value">
    <span class="dir">${esc(l.ref)}</span><span class="sp"></span><button class="small" onclick="wzDelLoco(${i})">✕</button></div>`).join("")
    || '<p class="dir">Todavía no hay ninguna. Mueve una loco con la MS2 o añádela abajo.</p>';
}
function wzAddLoco() {
  const a = parseInt($("#wzAddr").value, 10);
  if (!(a > 0)) return toast("Escribe la dirección de la loco", true);
  const ref = $("#wzProto").value + ":" + a;
  if (!WZ.locos.some((l) => l.ref === ref)) WZ.locos.push({ ref, name: "" });
  WZ.removed.delete(ref);
  $("#wzAddr").value = "";
  wzRenderLocoList();
}
function wzDelLoco(i) {
  WZ.removed.add(WZ.locos[i].ref);
  WZ.locos.splice(i, 1);
  wzRenderLocoList();
}

function wzBack() {
  if (WZ.step > 0) WZ.step--;
  wzRender();
}
async function wzNext() {
  if (WZ.step === 3) {
    const w = WZ.wifi;
    if (w.mode === "sta") {
      w.ssid = $("#wzSsid").value.trim();
      w.password = $("#wzPass").value;
      if (!w.ssid) return toast("Escribe el nombre de tu red WiFi", true);
    }
  }
  if (WZ.step < WZ_STEPS.length - 1) {
    WZ.step++;
    return wzRender();
  }
  // Terminar: nombres únicos y válidos.
  const used = new Set(), locos = [];
  for (const l of WZ.locos) {
    let n = wzName(l), k = 2;
    while (used.has(n)) n = wzName(l).slice(0, 28) + "_" + k++;
    used.add(n);
    locos.push(Object.assign({ name: n, loco: l.ref }, l.name.trim() ? { label: l.name.trim() } : {}));
  }
  try {
    if (WZ.test && locos.length) {
      const n = locos[0].name;
      const src = `# Script de prueba creado por el asistente.\n# Pon la loco "${n}" en un tramo recto y ten a mano el STOP de la MS2.\n` +
        `power on\nfunc ${n} 0 on\ndir ${n} fwd\nramp ${n} 25% 2s\nwait 3s\nramp ${n} 0 2s\nwait 2s\n` +
        `dir ${n} rev\nramp ${n} 25% 2s\nwait 3s\nramp ${n} 0 2s\nfunc ${n} 0 off\nlog prueba terminada\n`;
      await api("/api/script?name=" + encodeURIComponent(("prueba_" + n).slice(0, 31)), { method: "POST", body: src });
    }
    const body = { locos, layout: WZ.layout, wifi: WZ.wifi };
    const d = await api("/api/setup", { method: "POST", body: JSON.stringify(body) });
    WZ.done = true;
    wzRender();
    if (d.restart) {
      $("#wzBody").innerHTML = `<h2>Conectando a «${esc(WZ.wifi.ssid)}»…</h2>
        <p>El ESP32 se reinicia y se une a tu red. Conecta este dispositivo a esa misma red y abre
        <b>http://marklin.local</b> (o busca la IP del ESP32 en tu router).</p>
        <p class="dir">Si <b>marklin.local</b> no abre (pasa en algunos Windows), conéctate a la red «Marklin-ESP32»,
        que sigue siempre activa, abre <b>http://192.168.4.1</b> y en <b>Ajustes</b> verás la IP de casa del ESP32.
        Recuerda: el ESP32 solo funciona con redes de <b>2,4 GHz</b>.</p>`;
    } else {
      $("#wzBody").innerHTML = `<h2>¡Configurado!</h2>
        <p>Pulsa <b>GO</b> arriba para dar corriente a la vía. En la pestaña <b>Locomotoras</b> tienes tus locos a la izquierda:
        haz clic en una para llevarla a un mando (hasta 8 a la vez). Los desvíos se cambian en la pestaña <b>Desvíos</b>, y en <b>Inventario</b> añades fotos y funciones.
        En <b>Maqueta</b> asignas desvíos y sensores; los scripts de ejemplo están en <b>Automático</b>.</p>
        <div class="row"><span class="sp"></span><button class="act" onclick="closeWizard();refresh()">Empezar</button></div>`;
      if (typeof loadLayouts === "function") loadLayouts();
    }
  } catch (e) {
    toast(e.message, true);
  }
}
