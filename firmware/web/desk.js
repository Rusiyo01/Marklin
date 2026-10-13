// ---- Puesto de mando (pensado para el portátil) ----
// Lista de locomotoras registradas a la izquierda, plano de la maqueta con
// los desvíos y de 1 a 4 mandos a la vez en pantalla dividida.
// La disposición (mandos abiertos, vista) se recuerda en cada navegador.
const DESK = { n: 2, view: "both", cabs: [null, null, null, null], active: 0 };
try {
  Object.assign(DESK, JSON.parse(localStorage.getItem("desk") || "{}"));
} catch (e) {}
const saveDesk = () => {
  try {
    localStorage.setItem("desk", JSON.stringify(DESK));
  } catch (e) {}
};
const locoByRef = (ref) => (state.locos || []).find((l) => l.ref === ref);
const locoTitle = (l) => (l ? l.label || l.name || "Loco " + l.ref : "");
const pct = (v) => Math.round(v / 10) + "%";
const speedDrag = {};  // ref -> instante en que se tocó el regulador
const speedTimers = {};

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
function toggleFn(ref, fn) {
  const l = locoByRef(ref), on = l && (l.fn >>> fn) & 1;
  post("/api/loco", { loco: ref, fn, on: on ? 0 : 1 }).catch(() => {});
}

// ---- Vista ----
function setDeskView(view) {
  DESK.view = view;
  saveDesk();
  applyDeskLayout();
}
function setCabCount(n) {
  DESK.n = n;
  if (DESK.active >= n) DESK.active = 0;
  saveDesk();
  applyDeskLayout();
}
function applyDeskLayout() {
  const d = $("#desk");
  d.dataset.view = DESK.view;
  $("#cabs").className = "cabs n" + DESK.n;
  document.querySelectorAll("#deskBar [data-view]").forEach((b) => b.classList.toggle("act", b.dataset.view === DESK.view));
  document.querySelectorAll("#deskBar [data-n]").forEach((b) => b.classList.toggle("act", +b.dataset.n === DESK.n));
  document.querySelectorAll(".cab").forEach((c, i) => (c.hidden = i >= DESK.n));
  renderCabs();
}

// ---- Mandos ----
function buildCabs() {
  const box = $("#cabs");
  let fns = "", more = "";
  for (let i = 0; i < 16; i++) fns += `<button data-fn="${i}">F${i}</button>`;
  for (let i = 16; i < 32; i++) more += `<button data-fn="${i}">F${i}</button>`;
  for (let i = 0; i < 4; i++) {
    const c = document.createElement("div");
    c.className = "cab panel";
    c.dataset.i = i;
    c.innerHTML = `<div class="cabhead"><span class="cabn">${i + 1}</span>
        <div class="cabt"><b class="lbl"></b><small class="ref"></small></div>
        <button class="small rel" title="Dejar libre este mando">✕</button></div>
      <div class="cabbody">
        <div class="speedrow"><span class="spd">0%</span><span class="dirind"></span></div>
        <input type="range" min="0" max="1000" step="10" value="0" aria-label="Velocidad">
        <div class="ctl"><button data-d="rev" title="Atrás (←)">◀</button><button data-st="-100" title="−10 %">−</button>
          <button data-s class="stopb" title="Parar (Espacio)">■</button><button data-st="100" title="+10 %">+</button>
          <button data-d="fwd" title="Adelante (→)">▶</button></div>
        <div class="fns">${fns}</div>
        <details class="more"><summary>F16–F31</summary><div class="fns">${more}</div></details>
      </div>
      <div class="cabempty"><select class="pick" title="Elegir locomotora"></select>
        <p>o haz clic en una locomotora de la lista<br>(también puedes arrastrarla aquí)</p></div>`;
    const ref = () => DESK.cabs[i];
    const rng = c.querySelector("input[type=range]");
    rng.oninput = () => ref() && setLocoSpeed(ref(), +rng.value);
    rng.onchange = () => ref() && setLocoSpeed(ref(), +rng.value, true);
    c.querySelectorAll("[data-d]").forEach((b) => (b.onclick = () => ref() && setDir(ref(), b.dataset.d)));
    c.querySelectorAll("[data-st]").forEach((b) => (b.onclick = () => {
      const l = ref() && locoByRef(ref());
      if (ref()) setLocoSpeed(ref(), (l ? l.speed : 0) + +b.dataset.st, true);
    }));
    c.querySelector("[data-s]").onclick = () => ref() && setLocoSpeed(ref(), 0, true);
    c.querySelectorAll("[data-fn]").forEach((b) => (b.onclick = () => ref() && toggleFn(ref(), +b.dataset.fn)));
    c.querySelector(".rel").onclick = () => assignCab(i, null);
    c.querySelector(".pick").onchange = (e) => assignCab(i, e.target.value || null);
    c.addEventListener("pointerdown", () => setActiveCab(i));
    c.addEventListener("wheel", (e) => {
      if (!ref() || !e.target.closest("input[type=range]")) return;
      e.preventDefault();
      const l = locoByRef(ref());
      setLocoSpeed(ref(), (l ? l.speed : 0) + (e.deltaY < 0 ? 20 : -20));
    }, { passive: false });
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
    box.appendChild(c);
  }
}

function setActiveCab(i) {
  if (DESK.active === i) return;
  DESK.active = i;
  saveDesk();
  document.querySelectorAll(".cab").forEach((c, k) => c.classList.toggle("activecab", k === i));
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

// Clic en una loco de la lista: si ya está en un mando lo activa; si no, la
// pone en el primer mando libre (o en el activo si están todos ocupados).
function takeLoco(ref) {
  const at = DESK.cabs.slice(0, DESK.n).indexOf(ref);
  if (at >= 0) return setActiveCab(at);
  let free = DESK.cabs.slice(0, DESK.n).indexOf(null);
  if (free < 0) free = DESK.active;
  assignCab(free, ref);
}

function pickOptions(sel) {
  const reg = (state.locos || []).filter((l) => l.reg), other = (state.locos || []).filter((l) => !l.reg);
  return '<option value="">— elegir loco —</option>' +
    reg.map((l) => `<option value="${esc(l.ref)}"${l.ref === sel ? " selected" : ""}>${esc(locoTitle(l))} (${esc(l.ref)})</option>`).join("") +
    (other.length ? '<optgroup label="Sin registrar">' + other.map((l) => `<option value="${esc(l.ref)}"${l.ref === sel ? " selected" : ""}>${esc(l.ref)}</option>`).join("") + "</optgroup>" : "");
}

function renderCabs() {
  if (!state.locos) return;
  document.querySelectorAll(".cab").forEach((c, i) => {
    if (i >= DESK.n) return;
    const ref = DESK.cabs[i], l = ref ? locoByRef(ref) : null;
    c.classList.toggle("empty", !ref);
    c.classList.toggle("activecab", i === DESK.active);
    const pick = c.querySelector(".pick"), opts = pickOptions(ref);
    if (pick._opts !== opts && document.activeElement !== pick) {
      pick.innerHTML = opts;
      pick._opts = opts;
    }
    if (!ref) return;
    c.querySelector(".lbl").textContent = l ? locoTitle(l) : "Loco " + ref;
    c.querySelector(".ref").textContent = ref + (l && l.name && l.label ? " · " + l.name : "");
    const speed = l ? l.speed : 0;
    if (Date.now() - (speedDrag[ref] || 0) > 1200 || !l) c.querySelector("input[type=range]").value = speed;
    c.querySelector(".spd").textContent = pct(speed);
    c.querySelector(".dirind").textContent = !l || !l.dir ? "sentido ?" : l.dir === 1 ? "adelante ▶" : "◀ atrás";
    c.querySelectorAll("[data-d]").forEach((b) => b.classList.toggle("act", !!l && ((b.dataset.d === "fwd" && l.dir === 1) || (b.dataset.d === "rev" && l.dir === 2))));
    c.querySelectorAll("[data-fn]").forEach((b) => b.classList.toggle("act", !!l && ((l.fn >>> b.dataset.fn) & 1) === 1));
  });
}

// ---- Lista de locomotoras ----
function renderRoster() {
  const box = $("#roster");
  if (!box || !state.locos || box._dragging) return;  // no repintar mientras se arrastra
  const reg = state.locos.filter((l) => l.reg).sort((a, b) => locoTitle(a).localeCompare(locoTitle(b)));
  const other = state.locos.filter((l) => !l.reg);
  const row = (l) => {
    const at = DESK.cabs.slice(0, DESK.n).indexOf(l.ref);
    return `<div class="rl${at >= 0 ? " incab" : ""}" draggable="true" data-ref="${esc(l.ref)}">
      <div class="rlt"><b>${esc(l.reg ? locoTitle(l) : l.ref)}</b><small>${esc(l.reg ? l.ref : "detectada")}${l.speed ? " · " + pct(l.speed) : ""}</small>
        <i class="bar" style="width:${l.speed / 10}%"></i></div>
      ${at >= 0 ? `<span class="badge">${at + 1}</span>` : ""}
      <button class="small" data-edit title="${l.reg ? "Editar" : "Registrar"}">${l.reg ? "✎" : "Registrar"}</button></div>`;
  };
  const html = (reg.length ? reg.map(row).join("") : '<p class="dir">Aún no hay locomotoras registradas.</p>') +
    (other.length ? '<h3>Detectadas sin registrar</h3>' + other.map(row).join("") : "");
  if (box._html === html) return;
  box._html = html;
  box.innerHTML = html;
  box.querySelectorAll(".rl").forEach((r) => {
    r.onclick = (e) => {
      if (e.target.closest("[data-edit]")) return openLocoDialog(r.dataset.ref);
      takeLoco(r.dataset.ref);
    };
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

// ---- Registro de locomotoras ----
let dlgOld = null;
function openLocoDialog(ref) {
  const l = ref ? locoByRef(ref) : null;
  dlgOld = l && l.reg ? ref : null;
  const [pr, ad] = ref ? ref.split(":") : ["mm2", ""];
  $("#dlgTitle").textContent = dlgOld ? "Editar locomotora" : "Registrar locomotora";
  $("#dlgLabel").value = l ? l.label || (l.reg ? l.name : "") : "";
  $("#dlgProto").value = ["mm2", "dcc", "mfx"].includes(pr) ? pr : "mm2";
  $("#dlgAddr").value = ad || "";
  $("#dlgName").value = l && l.reg ? l.name : "";
  $("#dlgName").placeholder = slug($("#dlgLabel").value || "loco");
  $("#dlgDel").hidden = !dlgOld;
  $("#locoDlg").hidden = false;
  $("#dlgLabel").focus();
}
function closeLocoDialog() { $("#locoDlg").hidden = true; }
async function saveLocoDialog() {
  const label = $("#dlgLabel").value.trim(), addr = parseInt($("#dlgAddr").value, 10);
  if (!label) return toast("Escribe un nombre para la locomotora", true);
  if (!(addr > 0)) return toast("Escribe la dirección del decodificador", true);
  const ref = $("#dlgProto").value + ":" + addr;
  const name = slug($("#dlgName").value.trim() || label);
  try {
    await post("/api/locoreg", { loco: ref, name, label, old: dlgOld || "" });
    if (dlgOld && dlgOld !== ref) DESK.cabs = DESK.cabs.map((r) => (r === dlgOld ? ref : r));
    saveDesk();
    closeLocoDialog();
  } catch (e) {}
}
async function deleteLocoDialog() {
  if (!dlgOld || !confirm("¿Quitar esta locomotora del registro?")) return;
  try {
    await post("/api/locoreg", { loco: dlgOld, delete: 1 });
    closeLocoDialog();
  } catch (e) {}
}
$("#dlgLabel").addEventListener("input", () => ($("#dlgName").placeholder = slug($("#dlgLabel").value || "loco")));

// ---- Teclado (portátil) ----
document.addEventListener("keydown", (e) => {
  if (tab !== "desk" || e.ctrlKey || e.metaKey || e.altKey) return;
  if (!$("#locoDlg").hidden || (typeof WZ !== "undefined" && WZ.open)) return;
  if (e.target.closest && e.target.closest("input:not([type=range]),textarea,select")) return;
  const ref = DESK.cabs[DESK.active], l = ref && locoByRef(ref), sp = l ? l.speed : 0;
  let handled = true;
  switch (e.key) {
    case "Tab": setActiveCab((DESK.active + (e.shiftKey ? DESK.n - 1 : 1)) % DESK.n); break;
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
