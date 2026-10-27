// ---- Mandos de locomotoras ----
// Pestaña "Locomotoras": el inventario a la izquierda y de 1 a 8 mandos a la
// vez. Pestaña "Desvíos": el plano y un mando para la loco que se elija.
// Las funciones se muestran con su símbolo (luces, sonido, bocina...) según
// el tipo guardado en el inventario (leído de la MS2 o puesto a mano).
// La disposición (mandos abiertos) se recuerda en cada navegador.
const MAXCABS = 8;
const DESK = { n: 2, cabs: [], active: 0, sw: null };
try {
  Object.assign(DESK, JSON.parse(localStorage.getItem("desk") || "{}"));
} catch (e) {}
DESK.n = Math.min(MAXCABS, Math.max(1, +DESK.n || 2));
DESK.cabs = Array.from({ length: MAXCABS }, (_, i) => DESK.cabs[i] || null);
const saveDesk = () => {
  try {
    localStorage.setItem("desk", JSON.stringify(DESK));
  } catch (e) {}
};
const locoByRef = (ref) => (state.locos || []).find((l) => l.ref === ref);
const invByRef = (ref) => (INV.locos || []).find((l) => l.ref === ref);
const locoTitle = (l) => {
  const r = l && invByRef(l.ref);
  return r ? r.label || r.name : l ? l.label || l.name || "Loco " + l.ref : "";
};
const titleOf = (ref) => locoTitle(locoByRef(ref) || { ref });
const pct = (v) => Math.round(v / 10) + "%";
const photoUrl = (ref) => `/api/photo?loco=${encodeURIComponent(ref)}&v=${INV.seq}`;
const speedDrag = {};  // ref -> instante en que se tocó el regulador
const speedTimers = {};

// ---- Tipos de función (símbolos de la CS2/MS2) ----
const FN_TYPES = {
  1: ["💡", "Luces"], 2: ["🔆", "Luz interior"], 3: ["🔴", "Luz trasera"], 4: ["🔦", "Luces largas"],
  5: ["🔊", "Sonido"], 6: ["⚡", "Pantógrafo"], 7: ["💨", "Humo"], 8: ["🐢", "Maniobras"],
  9: ["🔗", "Enganche Telex"], 10: ["📯", "Bocina"], 11: ["👮", "Silbato revisor"], 12: ["🎶", "Silbato"],
  13: ["🔔", "Campana"], 14: ["↔", "Izquierda/derecha"], 15: ["↕", "Subir/bajar"], 16: ["↺", "Girar a la izquierda"],
  17: ["🏗", "Brazo de grúa"], 18: ["⏱", "Inercia"], 19: ["⛽", "Bomba"], 20: ["🛑", "Chirrido de frenos"],
  21: ["⚙", "Cambio de marcha"], 22: ["🔋", "Generador"], 23: ["🚂", "Ruido de marcha"], 24: ["⚙", "Motor"],
  25: ["📢", "Anuncio en estación"], 26: ["⛏", "Echar carbón"], 27: ["🚪", "Cerrar puertas"], 28: ["🚪", "Abrir puertas"],
  29: ["🌀", "Ventilador"], 30: ["🌀", "Ventilador"], 31: ["🔥", "Hogar"], 32: ["🔆", "Luz interior"],
  33: ["💡", "Lámpara de mesa"], 34: ["💡", "Lámpara de mesa"], 35: ["💡", "Lámpara de mesa"], 36: ["🔥", "Sacudir parrilla"],
  37: ["🛤", "Juntas de vía"], 38: ["🔢", "Número de loco"], 39: ["🎵", "Música"], 40: ["🪧", "Indicador de destino"],
  41: ["💡", "Luz de cabina 2"], 42: ["💡", "Luz de cabina 1"], 43: ["🔗", "Enganche"], 44: ["💥", "Choque de topes"],
  45: ["📢", "Anuncio en el tren"], 46: ["🪝", "Gancho de grúa"], 47: ["🚨", "Luz intermitente"], 48: ["💡", "Luz de cabina"],
  49: ["💨", "Aire comprimido"],
  82: ["🔗", "Telex trasero"], 83: ["🔗", "Telex delantero"], 84: ["⚡", "Pantógrafo trasero"], 85: ["⚡", "Pantógrafo delantero"],
  86: ["💡", "Luz delantera (atrás)"], 87: ["💡", "Luz delantera"], 88: ["⬆", "Subir"], 89: ["🌀", "Ventilador"],
  90: ["💡", "Luz del motor"], 91: ["♨", "Purga de vapor"], 92: ["♨", "Vapor"], 93: ["🏗", "Grúa"],
  94: ["⬆", "Subir"], 95: ["⬇", "Bajar"], 96: ["⬅", "Izquierda"], 97: ["➡", "Derecha"],
  98: ["↻", "Girar a la derecha"], 99: ["🧲", "Imán"], 101: ["⚡", "Pantógrafo"], 103: ["📻", "Radio"],
  104: ["📢", "Anuncio"], 105: ["🔴", "Luz trasera (atrás)"], 106: ["💨", "Bomba de aire"], 107: ["🗣", "Voz"],
  108: ["⚙", "Motor"], 109: ["🔇", "Sin sonido"], 110: ["💡", "Luz de escalera"], 111: ["💧", "Cargar agua"],
  112: ["🛑", "Freno"],
};
// [símbolo, nombre, momentánea] de un tipo de función.
function fnInfo(t, n) {
  const mom = (t & 0x80) !== 0, base = t & 0x7f;
  if (base >= 50 && base <= 81) return ["", "F" + (base - 50), mom];
  const d = FN_TYPES[base];
  return d ? [d[0], d[1], mom] : ["", "F" + n, mom];
}
// Funciones de una loco: [{n, t}] (las del inventario o F0..F15 genéricas).
function locoFns(ref) {
  const r = invByRef(ref), list = [];
  if (r && r.fn && r.fn.some((t) => t)) r.fn.forEach((t, n) => t && list.push({ n, t }));
  else for (let n = 0; n < 16; n++) list.push({ n, t: n === 0 ? 1 : 0 });
  return list;
}

// ---- Órdenes ----
function setLocoSpeed(ref, v, now) {
  v = Math.max(0, Math.min(1000, Math.round(v)));
  speedDrag[ref] = Date.now();
  const l = locoByRef(ref);
  if (l) l.speed = v;  // respuesta inmediata en pantalla
  renderCabs();
  const send = () => {
    speedTimers[ref] = null;
    post("/api/loco", { loco: ref, speed: (locoByRef(ref) || { speed: v }).speed }).catch(() => {});
  };
  if (now) {
    clearTimeout(speedTimers[ref]);
    send();
  } else if (!speedTimers[ref]) {
    speedTimers[ref] = setTimeout(send, 120);
  }
}
const setDir = (ref, d) => post("/api/loco", { loco: ref, dir: d }).catch(() => {});
function setFn(ref, fn, on) {
  const l = locoByRef(ref);
  if (l) l.fn = on ? l.fn | (1 << fn) : l.fn & ~(1 << fn);
  renderCabs();
  post("/api/loco", { loco: ref, fn, on: on ? 1 : 0 }).catch(() => {});
}
function toggleFn(ref, fn) {
  const l = locoByRef(ref);
  setFn(ref, fn, !(l && (l.fn >>> fn) & 1));
}

// ---- Número de mandos ----
function setCabCount(n) {
  DESK.n = n;
  if (DESK.active >= n) DESK.active = 0;
  saveDesk();
  applyDeskLayout();
}
function applyDeskLayout() {
  $("#cabs").className = "cabs n" + DESK.n;
  document.querySelectorAll("#deskBar [data-n]").forEach((b) => b.classList.toggle("act", +b.dataset.n === DESK.n));
  document.querySelectorAll("#cabs .cab").forEach((c, i) => (c.hidden = i >= DESK.n));
  renderCabs();
  renderRoster();
}

// ---- Mandos ----
// Cada mando es un elemento .cab; `getRef` dice qué loco lleva.
function makeCab(i, getRef, opts) {
  const c = document.createElement("div");
  c.className = "cab panel";
  c.dataset.i = i;
  c.innerHTML = `<div class="cabhead">${opts.number ? `<span class="cabn">${i + 1}</span>` : ""}
      <img class="cabimg" alt="" hidden>
      <div class="cabt"><b class="lbl"></b><small class="ref"></small></div>
      ${opts.number ? '<button class="small rel" title="Dejar libre este mando">✕</button>' : ""}</div>
    <div class="cabbody">
      <div class="speedrow"><span class="spd">0%</span><span class="dirind"></span></div>
      <input type="range" min="0" max="1000" step="10" value="0" aria-label="Velocidad">
      <div class="ctl"><button data-d="rev" title="Atrás (←)">◀</button><button data-st="-100" title="−10 %">−</button>
        <button data-s class="stopb" title="Parar (Espacio)">■</button><button data-st="100" title="+10 %">+</button>
        <button data-d="fwd" title="Adelante (→)">▶</button></div>
      <div class="fns main"></div>
      <details class="more"><summary>Otras funciones</summary><div class="fns rest"></div></details>
    </div>
    <div class="cabempty">${opts.number ? `<select class="pick" title="Elegir locomotora"></select>
      <p>o haz clic en una locomotora del inventario<br>(también puedes arrastrarla aquí)</p>` : "<p>Elige arriba la locomotora que quieres llevar.</p>"}</div>`;
  c._ref = getRef;
  const rng = c.querySelector("input[type=range]");
  rng.oninput = () => getRef() && setLocoSpeed(getRef(), +rng.value);
  rng.onchange = () => getRef() && setLocoSpeed(getRef(), +rng.value, true);
  c.querySelectorAll("[data-d]").forEach((b) => (b.onclick = () => getRef() && setDir(getRef(), b.dataset.d)));
  c.querySelectorAll("[data-st]").forEach((b) => (b.onclick = () => {
    const l = getRef() && locoByRef(getRef());
    if (getRef()) setLocoSpeed(getRef(), (l ? l.speed : 0) + +b.dataset.st, true);
  }));
  c.querySelector("[data-s]").onclick = () => getRef() && setLocoSpeed(getRef(), 0, true);
  // Funciones: las normales cambian con cada clic; las momentáneas (bocina,
  // silbato...) suenan mientras se mantiene pulsado el botón.
  c.addEventListener("pointerdown", (e) => {
    const b = e.target.closest("[data-fn]");
    if (!b || !getRef()) return;
    if (b.dataset.mom) {
      e.preventDefault();
      b.setPointerCapture(e.pointerId);
      setFn(getRef(), +b.dataset.fn, true);
      const off = () => {
        b.onpointerup = b.onpointercancel = null;
        setFn(getRef(), +b.dataset.fn, false);
      };
      b.onpointerup = b.onpointercancel = off;
    }
  });
  c.addEventListener("click", (e) => {
    const b = e.target.closest("[data-fn]");
    if (b && !b.dataset.mom && getRef()) toggleFn(getRef(), +b.dataset.fn);
  });
  c.addEventListener("wheel", (e) => {
    if (!getRef() || !e.target.closest("input[type=range]")) return;
    e.preventDefault();
    const l = locoByRef(getRef());
    setLocoSpeed(getRef(), (l ? l.speed : 0) + (e.deltaY < 0 ? 20 : -20));
  }, { passive: false });
  if (opts.number) {
    c.querySelector(".rel").onclick = () => assignCab(i, null);
    c.querySelector(".pick").onchange = (e) => assignCab(i, e.target.value || null);
    c.addEventListener("pointerdown", () => setActiveCab(i));
    c.addEventListener("dragover", (e) => {
      e.preventDefault();
      c.classList.add("drop");
    });
    c.addEventListener("dragleave", () => c.classList.remove("drop"));
    c.addEventListener("drop", (e) => {
      e.preventDefault();
      c.classList.remove("drop");
      const r = e.dataTransfer.getData("text/plain");
      if (r) assignCab(i, r);
    });
  }
  return c;
}

function buildCabs() {
  const box = $("#cabs");
  for (let i = 0; i < MAXCABS; i++) box.appendChild(makeCab(i, () => DESK.cabs[i], { number: true }));
  $("#swCab").appendChild(makeCab(MAXCABS, () => DESK.sw, { number: false }));
  $("#swPick").onchange = (e) => {
    DESK.sw = e.target.value || null;
    saveDesk();
    renderCabs();
  };
}

function setActiveCab(i) {
  if (DESK.active === i) return;
  DESK.active = i;
  saveDesk();
  document.querySelectorAll("#cabs .cab").forEach((c, k) => c.classList.toggle("activecab", k === i));
}

function assignCab(i, ref) {
  // Una loco solo puede estar en un mando a la vez.
  if (ref) DESK.cabs = DESK.cabs.map((r, k) => (r === ref && k !== i ? null : r));
  DESK.cabs[i] = ref;
  if (ref && !locoByRef(ref)) post("/api/loco", { loco: ref, add: 1 }).catch(() => {});
  setActiveCab(i);
  saveDesk();
  renderCabs();
  renderRoster();
}

// Clic en una loco del inventario: si ya está en un mando lo activa; si no,
// la pone en el primer mando libre (o en el activo si están todos ocupados).
function takeLoco(ref) {
  const at = DESK.cabs.slice(0, DESK.n).indexOf(ref);
  if (at >= 0) return setActiveCab(at);
  let free = DESK.cabs.slice(0, DESK.n).indexOf(null);
  if (free < 0) free = DESK.active;
  assignCab(free, ref);
}
// Desde el inventario: llevarla en la pestaña Locomotoras.
function driveLoco(ref) {
  takeLoco(ref);
  showTab("desk");
}

const regLocos = () => (INV.locos || []).slice().sort((a, b) => (a.label || a.name).localeCompare(b.label || b.name));
const opt = (ref, sel, text) => `<option value="${esc(ref)}"${ref === sel ? " selected" : ""}>${esc(text)}</option>`;

function pickOptions(sel) {
  return '<option value="">— elegir loco —</option>' + regLocos().map((l) => opt(l.ref, sel, `${l.label || l.name} (${l.ref})`)).join("");
}
// Selector de la pestaña Desvíos: primero las locos de los mandos y las que
// están en marcha.
function swOptions(sel) {
  const inCabs = DESK.cabs.slice(0, DESK.n).filter((r) => r);
  const moving = (state.locos || []).filter((l) => l.speed > 0 && !inCabs.includes(l.ref)).map((l) => l.ref);
  const active = inCabs.concat(moving);
  const rest = regLocos().filter((l) => !active.includes(l.ref));
  return '<option value="">— elegir loco —</option>' +
    (active.length ? '<optgroup label="Activas (en los mandos o en marcha)">' + active.map((r) => opt(r, sel, `${titleOf(r)} (${r})`)).join("") + "</optgroup>" : "") +
    (rest.length ? '<optgroup label="Resto del inventario">' + rest.map((l) => opt(l.ref, sel, `${l.label || l.name} (${l.ref})`)).join("") + "</optgroup>" : "");
}

function fnButton(f) {
  const [icon, name, mom] = fnInfo(f.t, f.n);
  return `<button data-fn="${f.n}"${mom ? ' data-mom="1"' : ""} title="F${f.n} · ${esc(name)}${mom ? " (mientras se pulsa)" : ""}">` +
    (icon ? `<i>${icon}</i>` : "") + `<small>F${f.n}</small></button>`;
}

function renderCab(c, ref) {
  const l = ref ? locoByRef(ref) : null;
  c.classList.toggle("empty", !ref);
  if (!ref) return;
  c.querySelector(".lbl").textContent = titleOf(ref);
  c.querySelector(".ref").textContent = ref;
  const r = invByRef(ref), img = c.querySelector(".cabimg"), src = r && r.photo ? photoUrl(ref) : "";
  img.hidden = !src;
  if (src && img.getAttribute("src") !== src) img.src = src;
  // Botones de funciones (se rehacen solo si cambia la loco o su inventario).
  const key = ref + "|" + INV.seq;
  if (c._fnKey !== key) {
    c._fnKey = key;
    const main = locoFns(ref), used = new Set(main.map((f) => f.n)), rest = [];
    for (let n = 0; n < 32; n++) if (!used.has(n)) rest.push({ n, t: 0 });
    c.querySelector(".fns.main").innerHTML = main.map(fnButton).join("");
    c.querySelector(".fns.rest").innerHTML = rest.map(fnButton).join("");
  }
  const speed = l ? l.speed : 0;
  if (Date.now() - (speedDrag[ref] || 0) > 1200 || !l) c.querySelector("input[type=range]").value = speed;
  c.querySelector(".spd").textContent = pct(speed);
  c.querySelector(".dirind").textContent = !l || !l.dir ? "sentido ?" : l.dir === 1 ? "adelante ▶" : "◀ atrás";
  c.querySelectorAll("[data-d]").forEach((b) => b.classList.toggle("act", !!l && ((b.dataset.d === "fwd" && l.dir === 1) || (b.dataset.d === "rev" && l.dir === 2))));
  c.querySelectorAll("[data-fn]").forEach((b) => b.classList.toggle("act", !!l && ((l.fn >>> b.dataset.fn) & 1) === 1));
}

function renderCabs() {
  if (!state.locos) return;
  document.querySelectorAll("#cabs .cab").forEach((c, i) => {
    if (i >= DESK.n) return;
    c.classList.toggle("activecab", i === DESK.active);
    const pick = c.querySelector(".pick"), opts = pickOptions(DESK.cabs[i]);
    if (pick._opts !== opts && document.activeElement !== pick) {
      pick.innerHTML = opts;
      pick._opts = opts;
    }
    renderCab(c, DESK.cabs[i]);
  });
  if (!DESK.sw) DESK.sw = DESK.cabs[DESK.active] || DESK.cabs.slice(0, DESK.n).find((r) => r) || null;
  const sp = $("#swPick"), so = swOptions(DESK.sw);
  if (sp._opts !== so && document.activeElement !== sp) {
    sp.innerHTML = so;
    sp._opts = so;
  }
  renderCab($("#swCab .cab"), DESK.sw);
}

// ---- Inventario en la pestaña Locomotoras ----
function renderRoster() {
  const box = $("#roster");
  if (!box || !state.locos || box._dragging) return;  // no repintar mientras se arrastra
  const row = (r) => {
    const l = locoByRef(r.ref) || { speed: 0 }, at = DESK.cabs.slice(0, DESK.n).indexOf(r.ref);
    return `<div class="rl${at >= 0 ? " incab" : ""}" draggable="true" data-ref="${esc(r.ref)}">
      ${r.photo ? `<img src="${photoUrl(r.ref)}" alt="">` : '<span class="noimg">🚂</span>'}
      <div class="rlt"><b>${esc(r.label || r.name)}</b><small>${esc(r.ref)}${l.speed ? " · " + pct(l.speed) : ""}</small>
        <i class="bar" style="width:${l.speed / 10}%"></i></div>
      ${at >= 0 ? `<span class="badge">${at + 1}</span>` : ""}</div>`;
  };
  const list = regLocos();
  const html = list.length ? list.map(row).join("") :
    '<p class="dir">Aún no hay locomotoras en el inventario. Añádelas en la pestaña <b>Inventario</b> (o léelas de la MS2).</p>';
  if (box._html === html) return;
  box._html = html;
  box.innerHTML = html;
  box.querySelectorAll(".rl").forEach((r) => {
    r.onclick = () => takeLoco(r.dataset.ref);
    r.ondragstart = (e) => {
      box._dragging = true;
      e.dataTransfer.setData("text/plain", r.dataset.ref);
    };
    r.ondragend = () => {
      box._dragging = false;
      renderRoster();
    };
  });
}

// ---- Teclado (portátil) ----
document.addEventListener("keydown", (e) => {
  if ((tab !== "desk" && tab !== "sw") || e.ctrlKey || e.metaKey || e.altKey) return;
  if (!$("#locoDlg").hidden || (typeof WZ !== "undefined" && WZ.open)) return;
  if (e.target.closest && e.target.closest("input:not([type=range]),textarea,select")) return;
  const ref = tab === "sw" ? DESK.sw : DESK.cabs[DESK.active], l = ref && locoByRef(ref), sp = l ? l.speed : 0;
  let handled = true;
  switch (e.key) {
    case "Tab":
      if (tab === "desk") setActiveCab((DESK.active + (e.shiftKey ? DESK.n - 1 : 1)) % DESK.n);
      else handled = false;
      break;
    case "ArrowUp": if (ref) setLocoSpeed(ref, sp + 50); break;
    case "ArrowDown": if (ref) setLocoSpeed(ref, sp - 50); break;
    case "PageUp": if (ref) setLocoSpeed(ref, sp + 200, true); break;
    case "PageDown": if (ref) setLocoSpeed(ref, sp - 200, true); break;
    case "ArrowRight": if (ref) setDir(ref, "fwd"); break;
    case "ArrowLeft": if (ref) setDir(ref, "rev"); break;
    case " ": if (ref) setLocoSpeed(ref, 0, true); break;
    case "Escape": post("/api/power", { on: 0 }).catch(() => {}); break;
    default:
      if (/^[0-9]$/.test(e.key) && ref) toggleFn(ref, +e.key);
      else handled = false;
  }
  if (handled) e.preventDefault();
});

function renderDesk() {
  renderRoster();
  renderCabs();
}

buildCabs();
applyDeskLayout();
