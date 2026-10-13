// ---- Maqueta: planos de C-Track Planner ----
// Se dibuja el plano, los desvíos se mueven con un clic y los sensores se
// iluminan en directo. En modo "Asignar" se da a cada desvío su dirección y
// a cada pieza un sensor; los nombres se pueden usar en los scripts.
const PLAN = { list: [], name: "", L: null, edit: false, sel: null, dirty: false };
const slug = (s) => (String(s || "").toLowerCase().normalize("NFD").replace(/[̀-ͯ]/g, "")
  .replace(/[^a-z0-9_-]+/g, "_").replace(/^[_-]+|[_-]+$/g, "").replace(/^([^a-z])/, "m$1").slice(0, 31)) || "maqueta";
const accRef = (a) => {
  a = String(a == null ? "" : a).trim().toLowerCase();
  if (!a) return "";
  if (/^\d+$/.test(a)) return "mm:" + a;
  if (a.startsWith("mm2:")) return "mm:" + a.slice(4);
  return a;
};
const pieceById = (id) => PLAN.L && PLAN.L.pieces.find((p) => p.id === id);
const turnoutOf = (id) => PLAN.L && PLAN.L.marklin.turnouts.find((t) => t.piece === id);
const sensorOf = (id) => PLAN.L && PLAN.L.marklin.sensors.find((s) => s.piece === id);
const pathD = (pts) => "M" + pts.map((q) => q[0].toFixed(1) + " " + q[1].toFixed(1)).join("L");

async function loadLayouts(show) {
  try {
    PLAN.list = await api("/api/layouts");
  } catch (e) {
    toast(e.message, true);
    return;
  }
  const sel = $("#layoutSel");
  sel.innerHTML = '<option value="">(ninguna)</option>' +
    PLAN.list.map((l) => `<option value="${esc(l.name)}">${esc(l.title)}</option>`).join("");
  const active = (PLAN.list.find((l) => l.active) || {}).name || "";
  const name = show !== undefined ? show : active;
  sel.value = name;
  if (name !== PLAN.name || !PLAN.L) await openLayout(name, true);
}

async function openLayout(name, quiet) {
  if (PLAN.dirty && !confirm("Hay cambios sin guardar en las asignaciones. ¿Descartarlos?")) {
    $("#layoutSel").value = PLAN.name;
    return;
  }
  PLAN.name = name;
  PLAN.L = null;
  PLAN.sel = null;
  PLAN.dirty = false;
  if (name) {
    try {
      PLAN.L = await api("/api/layout?name=" + encodeURIComponent(name));
      PLAN.L.marklin = Object.assign({ turnouts: [], sensors: [] }, PLAN.L.marklin || {});
    } catch (e) {
      toast(e.message, true);
    }
  }
  const active = (PLAN.list.find((l) => l.active) || {}).name || "";
  if (!quiet && name !== active) {
    try {
      await post("/api/layout/activate", { name });
      PLAN.list.forEach((l) => (l.active = l.name === name));
    } catch (e) {}
  }
  drawPlan();
  renderMap();
  renderEdit();
}

// ---- Dibujo ----
function drawPlan() {
  const svg = $("#plan"), L = PLAN.L;
  $("#planEmpty").style.display = L ? "none" : "block";
  svg.style.display = L ? "block" : "none";
  $("#planTitle").textContent = L ? L.name || PLAN.name : "";
  if (!L) return (svg.innerHTML = "");
  const b = CT.bounds(L.pieces), m = 90;
  svg.setAttribute("viewBox", `${b.x - m} ${b.y - m} ${b.w + 2 * m} ${b.h + 2 * m}`);
  let html = "", labels = "";
  for (const p of L.pieces) {
    const P = CT.paths(p), sw = CT.isSwitch(p.t), elev = (p.z || []).some((z) => z > 0);
    html += `<g class="pc${sw ? " sw" : ""}${elev ? " elev" : ""}" data-id="${p.id}"><path class="bed" d="${P.map(pathD).join("")}"/>`;
    P.forEach((pts, k) => (html += `<path class="rt" data-k="${k}" d="${pathD(pts)}"/>`));
    html += "</g>";
    const T = turnoutOf(p.id), S = sensorOf(p.id), [cx, cy] = CT.center(p);
    if (T) labels += `<text x="${cx.toFixed(0)}" y="${(cy - 48).toFixed(0)}">${esc(T.name || "?")}${T.addr ? " · " + esc(accRef(T.addr)) : ""}</text>`;
    if (S) labels += `<text class="st" x="${cx.toFixed(0)}" y="${(cy + 70).toFixed(0)}">S${S.id}${S.name ? " " + esc(S.name) : ""}</text>`;
  }
  svg.innerHTML = html + labels;
  svg.classList.toggle("editing", PLAN.edit);
  updatePlanState();
}

// Recorridos activos de un desvío según el estado de sus accesorios.
function activeRoutes(p, T) {
  const kind = CT.LIB[p.t].kind;
  const pos = (ref) => {
    const a = (state.accessories || []).find((x) => x.ref === accRef(ref));
    if (!a) return null;
    return T.invert ? (a.pos === 1 ? 0 : 1) : a.pos;
  };
  const s = pos(T.addr);
  if (kind === "threeway") {
    const s2 = pos(T.addr2);
    if (s == null && s2 == null) return null;
    if (s === 0) return [1];
    if (s2 === 0) return [2];
    return [0];
  }
  if (s == null) return null;
  if (kind === "dslip") return s === 1 ? [0, 1] : [2, 3];
  return s === 1 ? [0] : [1];
}

function updatePlanState() {
  if (!PLAN.L) return;
  const svg = $("#plan");
  for (const g of svg.querySelectorAll("g.pc")) {
    const id = +g.dataset.id, p = pieceById(id), T = turnoutOf(id), S = sensorOf(id);
    g.classList.toggle("selp", PLAN.edit && PLAN.sel === id);
    g.classList.toggle("sens", !!S);
    const st = S ? (state.sensors || []).find((x) => x.id === S.id) : null;
    g.classList.toggle("s1", !!(st && st.state === 1));
    if (g.classList.contains("sw")) {
      const act = T ? activeRoutes(p, T) : null;
      g.querySelectorAll(".rt").forEach((r) => {
        const k = +r.dataset.k;
        r.classList.toggle("on", !!act && act.includes(k));
        r.classList.toggle("off", !!act && !act.includes(k));
      });
    }
  }
}

$("#plan").addEventListener("click", (e) => {
  const g = e.target.closest("g.pc");
  if (!g || !PLAN.L) return;
  const id = +g.dataset.id;
  if (PLAN.edit) {
    PLAN.sel = id;
    renderEdit();
    updatePlanState();
    return;
  }
  const p = pieceById(id);
  if (!CT.isSwitch(p.t)) return;
  const T = turnoutOf(id);
  if (!T || !T.addr) return toast('Este desvío aún no tiene dirección: pulsa "Asignar desvíos y sensores".', true);
  throwSwitch(p, T);
});

function sendPos(ref, pos, T) {
  return post("/api/accessory", { addr: accRef(ref), pos: T.invert ? (pos ? 0 : 1) : pos });
}
async function throwSwitch(p, T) {
  const kind = CT.LIB[p.t].kind, act = activeRoutes(p, T);
  try {
    if (kind === "threeway") {
      const next = !act || act[0] === 2 ? 0 : act[0] + 1;  // recto -> izquierda -> derecha
      await sendPos(T.addr, next === 1 ? 0 : 1, T);
      if (T.addr2) await sendPos(T.addr2, next === 2 ? 0 : 1, T);
    } else {
      const straight = !!act && act.includes(0);
      await sendPos(T.addr, straight ? 0 : 1, T);
    }
  } catch (e) {}
}

// ---- Asignación ----
function toggleEdit() {
  PLAN.edit = !PLAN.edit;
  PLAN.sel = null;
  $("#editBtn").classList.toggle("act", PLAN.edit);
  $("#planHint").textContent = PLAN.edit
    ? "Toca una pieza para asignarle un desvío o un sensor. Después pulsa Guardar."
    : "Toca un desvío para cambiarlo. Los sensores activos se ven en verde.";
  drawPlan();
  renderEdit();
}

function renderEdit() {
  const box = $("#editPanel");
  const p = PLAN.edit && pieceById(PLAN.sel);
  box.hidden = !p;
  if (!p) return;
  const lib = CT.LIB[p.t], T = turnoutOf(p.id) || {}, S = sensorOf(p.id) || {}, sw = CT.isSwitch(p.t);
  const split = (ref) => {
    const r = accRef(ref);
    return r ? r.split(":") : ["mm", ""];
  };
  const [pr, ad] = split(T.addr), [pr2, ad2] = split(T.addr2);
  const protoSel = (id, v) => `<select id="${id}"><option value="mm"${v === "mm" ? " selected" : ""}>MM</option><option value="dcc"${v === "dcc" ? " selected" : ""}>DCC</option></select>`;
  box.innerHTML = `<h2>Pieza ${esc(p.t)} · ${esc(lib.name)} <span class="dir">(id ${p.id})</span></h2>
    ${sw ? `<div class="row"><label>Nombre <input id="eName" size="12" value="${esc(T.name || "")}" placeholder="entrada"></label>
      <label>Dirección ${protoSel("eProto", pr)} <input id="eAddr" type="number" min="1" style="width:80px" value="${esc(ad)}"></label>
      ${lib.kind === "threeway" ? `<label>2ª dirección ${protoSel("eProto2", pr2)} <input id="eAddr2" type="number" min="1" style="width:80px" value="${esc(ad2)}"></label>` : ""}
      <label><input type="checkbox" id="eInv"${T.invert ? " checked" : ""}> Invertido</label></div>` : ""}
    <div class="row" style="margin-top:8px"><label>Sensor nº <input id="eSid" type="number" min="1" style="width:80px" value="${S.id || ""}"></label>
      <label>Nombre <input id="eSname" size="12" value="${esc(S.name || "")}" placeholder="anden"></label></div>
    <div class="row" style="margin-top:10px"><button class="act" onclick="applyEdit()">Aplicar</button>
      <button onclick="clearEdit()">Quitar asignación</button><span class="sp"></span>
      <button onclick="PLAN.sel=null;renderEdit();updatePlanState()">Cerrar</button></div>`;
}

function applyEdit() {
  const p = pieceById(PLAN.sel), M = PLAN.L.marklin;
  if (!p) return;
  if (CT.isSwitch(p.t)) {
    const addr = $("#eAddr").value.trim(), name = $("#eName").value.trim();
    M.turnouts = M.turnouts.filter((t) => t.piece !== p.id);
    if (addr || name) {
      const T = { piece: p.id, name: name ? slug(name) : "d" + (addr || p.id), addr: addr ? $("#eProto").value + ":" + addr : "" };
      if ($("#eAddr2") && $("#eAddr2").value.trim()) T.addr2 = $("#eProto2").value + ":" + $("#eAddr2").value.trim();
      if ($("#eInv").checked) T.invert = true;
      if (M.turnouts.some((t) => t.name === T.name)) return toast("Ya hay otro desvío llamado " + T.name, true);
      M.turnouts.push(T);
    }
  }
  const sid = parseInt($("#eSid").value, 10), sname = $("#eSname").value.trim();
  M.sensors = M.sensors.filter((s) => s.piece !== p.id);
  if (sid > 0) M.sensors.push(Object.assign({ piece: p.id, id: sid }, sname ? { name: slug(sname) } : {}));
  PLAN.dirty = true;
  drawPlan();
  renderMap();
  renderEdit();
}

function clearEdit() {
  const M = PLAN.L.marklin;
  M.turnouts = M.turnouts.filter((t) => t.piece !== PLAN.sel);
  M.sensors = M.sensors.filter((s) => s.piece !== PLAN.sel);
  PLAN.dirty = true;
  drawPlan();
  renderMap();
  renderEdit();
}

// Da dirección (MM) y nombre a todos los desvíos que no la tengan.
function autoNumber() {
  if (!PLAN.L) return;
  const M = PLAN.L.marklin;
  const used = new Set();
  M.turnouts.forEach((t) => [t.addr, t.addr2].forEach((a) => a && used.add(accRef(a))));
  let next = 1;
  const take = () => {
    while (used.has("mm:" + next)) next++;
    used.add("mm:" + next);
    return "mm:" + next;
  };
  let n = 0;
  for (const p of PLAN.L.pieces) {
    if (!CT.isSwitch(p.t)) continue;
    let T = turnoutOf(p.id);
    if (!T) {
      T = { piece: p.id, name: "" };
      M.turnouts.push(T);
    }
    if (!T.addr) {
      T.addr = take();
      n++;
    }
    if (CT.LIB[p.t].kind === "threeway" && !T.addr2) T.addr2 = take();
    if (!T.name) {
      let k = 1;
      while (M.turnouts.some((t) => t.name === "d" + k)) k++;
      T.name = "d" + k;
    }
  }
  PLAN.dirty = n > 0 || PLAN.dirty;
  drawPlan();
  renderMap();
  toast(n ? `${n} desvíos numerados. Pulsa Guardar.` : "Todos los desvíos ya tenían dirección");
}

function renderMap() {
  const L = PLAN.L;
  $("#mapBox").hidden = !L;
  if (!L) return;
  const M = L.marklin;
  const sw = L.pieces.filter((p) => CT.isSwitch(p.t)).length;
  $("#mapTurnouts").innerHTML = M.turnouts.map((t) => `<tr><td><b>${esc(t.name)}</b></td><td>${esc(accRef(t.addr) || "—")}${t.addr2 ? " + " + esc(accRef(t.addr2)) : ""}${t.invert ? " (invertido)" : ""}</td><td class="dir">pieza ${t.piece}</td></tr>`).join("")
    || `<tr><td class="dir">Ningún desvío asignado (${sw} en la maqueta).</td></tr>`;
  $("#mapSensors").innerHTML = M.sensors.map((s) => `<tr><td><b>S${s.id}</b></td><td>${esc(s.name || "")}</td><td class="dir">pieza ${s.piece}</td></tr>`).join("")
    || '<tr><td class="dir">Ningún sensor colocado.</td></tr>';
  const ex = M.turnouts[0], es = M.sensors.find((s) => s.name);
  $("#mapHint").textContent = ex || es
    ? `En los scripts: ${ex ? `switch ${ex.name} round` : ""}${ex && es ? "  ·  " : ""}${es ? `wait sensor ${es.name} on` : ""}`
    : "";
  $("#saveMap").classList.toggle("act", PLAN.dirty);
}

async function saveMap() {
  if (!PLAN.L) return;
  try {
    await post("/api/layout", { name: PLAN.name }, JSON.stringify(PLAN.L));
    PLAN.dirty = false;
    renderMap();
  } catch (e) {}
}

async function delLayout() {
  if (!PLAN.name || !confirm(`¿Borrar la maqueta "${PLAN.name}" del ESP32?`)) return;
  try {
    await post("/api/layout/delete", { name: PLAN.name });
    PLAN.dirty = false;
    PLAN.name = "";
    await loadLayouts("");
  } catch (e) {}
}

// Importar un fichero exportado por C-Track Planner (.json o app .html).
async function importLayoutFile(file) {
  const { layout, unknown } = CT.parseLayoutFile(await file.text());
  const name = slug(layout.name || file.name.replace(/\.(json|html?)$/i, ""));
  if (PLAN.list.some((l) => l.name === name)) {
    if (!confirm(`Ya existe la maqueta "${name}". ¿Sustituirla? Se conservan las asignaciones de las piezas que sigan existiendo.`)) return null;
    if (!layout.marklin) {
      try {
        const old = await api("/api/layout?name=" + encodeURIComponent(name));
        if (old.marklin) {
          const ids = new Set(layout.pieces.map((p) => p.id));
          layout.marklin = {
            turnouts: (old.marklin.turnouts || []).filter((t) => ids.has(t.piece)),
            sensors: (old.marklin.sensors || []).filter((s) => ids.has(s.piece)),
          };
        }
      } catch (e) {}
    }
  }
  const txt = JSON.stringify(layout);
  if (txt.length > 48 * 1024) throw new Error("La maqueta es demasiado grande (máx. 48 KB)");
  await post("/api/layout", { name }, txt);
  await post("/api/layout/activate", { name });
  if (unknown.length) toast("Piezas no reconocidas (no se dibujan): " + unknown.join(", "), true);
  return name;
}

$("#layoutFile").onchange = async (e) => {
  const f = e.target.files[0];
  e.target.value = "";
  if (!f) return;
  try {
    PLAN.dirty = false;
    const name = await importLayoutFile(f);
    if (name) {
      PLAN.L = null;  // forzar recarga aunque sea la maqueta que se está viendo
      await loadLayouts(name);
      if (typeof wzLayoutsChanged === "function") wzLayoutsChanged(name);
    }
  } catch (err) {
    toast(err.message, true);
  }
};
$("#layoutSel").onchange = (e) => openLayout(e.target.value);
