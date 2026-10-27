// ---- Inventario de locomotoras ----
// Fichas con foto, dirección y funciones; registro y edición; lectura de las
// locos de la MS2 (nombre, dirección y tipo de cada función).
const INV = { seq: -1, locos: [], auto: true, loading: false };

async function loadInventory() {
  if (INV.loading) return;
  INV.loading = true;
  try {
    const d = await api("/api/inventory");
    Object.assign(INV, d);
    renderInventory();
    renderDesk();
  } catch (e) {} finally {
    INV.loading = false;
  }
}

function fnIcons(fn) {
  return (fn || []).map((t, n) => {
    if (!t) return "";
    const [icon, name] = fnInfo(t, n);
    return `<span title="F${n} · ${esc(name)}">${icon || "F" + n}</span>`;
  }).join("");
}

function renderInventory() {
  const grid = $("#invGrid");
  if (!grid) return;
  const list = regLocos();
  grid.innerHTML = list.map((r) => {
    const l = locoByRef(r.ref) || { speed: 0 };
    return `<div class="invcard" data-ref="${esc(r.ref)}">
      <div class="invphoto">${r.photo ? `<img src="${photoUrl(r.ref)}" alt="">` : '<span>🚂<small>Sin foto</small></span>'}</div>
      <div class="invbody"><b>${esc(r.label || r.name)}</b>
        <small>${esc(r.ref)} · scripts: ${esc(r.name)}${r.mfxuid ? " · mfx " + esc(r.mfxuid) : ""}</small>
        <div class="invfn">${fnIcons(r.fn) || '<span class="dir">funciones genéricas F0–F15</span>'}</div>
        ${l.speed ? `<small class="run">en marcha · ${pct(l.speed)}</small>` : ""}
        <div class="row"><button class="small act" data-drive>Conducir</button><button class="small" data-edit>Editar</button></div>
      </div></div>`;
  }).join("") || '<div class="panel dir">Aún no hay locomotoras. Pulsa <b>+ Registrar locomotora</b> o <b>Leer locos de la MS2</b>.</div>';
  grid.querySelectorAll(".invcard").forEach((c) => {
    c.querySelector("[data-drive]").onclick = () => driveLoco(c.dataset.ref);
    c.querySelector("[data-edit]").onclick = () => openLocoDialog(c.dataset.ref);
    c.querySelector(".invphoto").onclick = () => openLocoDialog(c.dataset.ref);
  });
  $("#autoImp").checked = !!INV.auto;
}

// Locos que se han movido (MS2, apps) y no están en el inventario.
function renderDetected() {
  const box = $("#detList");
  if (!box || !state.locos) return;
  const det = state.locos.filter((l) => !invByRef(l.ref));
  const html = det.map((l) => `<div class="rl"><div class="rlt"><b>${esc(l.ref)}</b><small>detectada${l.speed ? " · " + pct(l.speed) : ""}</small></div>
    <button class="small" data-reg="${esc(l.ref)}">Registrar</button></div>`).join("");
  $("#detBox").hidden = !det.length;
  if (box._html === html) return;
  box._html = html;
  box.innerHTML = html;
  box.querySelectorAll("[data-reg]").forEach((b) => (b.onclick = () => openLocoDialog(b.dataset.reg)));
}

function renderImport() {
  const imp = state.imp || {};
  $("#impMsg").textContent = imp.msg || "";
  $("#ms2Btn").disabled = !!imp.busy;
  $("#ms2Btn").textContent = imp.busy ? "Leyendo la MS2..." : "Leer locos de la MS2";
}
function ms2Import() {
  post("/api/ms2import").catch(() => {});
}
function setAutoImport(on) {
  post("/api/autoimport", { on: on ? 1 : 0 }).then(loadInventory).catch(() => {});
}

// ---- Fotos ----
// Se reducen en el navegador (máx. 640×480, JPEG) antes de subirlas.
async function shrinkPhoto(file) {
  const url = URL.createObjectURL(file), img = new Image();
  try {
    await new Promise((ok, ko) => {
      img.onload = ok;
      img.onerror = () => ko(new Error("no es una imagen válida"));
      img.src = url;
    });
  } finally {
    URL.revokeObjectURL(url);
  }
  const k = Math.min(1, 640 / img.width, 480 / img.height);
  const cv = document.createElement("canvas");
  cv.width = Math.round(img.width * k);
  cv.height = Math.round(img.height * k);
  cv.getContext("2d").drawImage(img, 0, 0, cv.width, cv.height);
  return new Promise((ok) => cv.toBlob(ok, "image/jpeg", 0.82));
}
async function uploadPhoto(ref, blob) {
  const fd = new FormData();
  fd.append("photo", blob, "foto.jpg");
  await api("/api/photo?loco=" + encodeURIComponent(ref), { method: "POST", body: fd });
}

// ---- Registro / edición ----
let dlgOld = null, dlgPhoto = null, dlgPhotoDel = false;
const FN_CHOICES = Object.keys(FN_TYPES).map(Number).sort((a, b) => FN_TYPES[a][1].localeCompare(FN_TYPES[b][1]));

function fnRow(n, t) {
  const base = t & 0x7f, generic = 50 + n;
  const opts = `<option value="0">— no tiene —</option><option value="${generic}"${base === generic ? " selected" : ""}>F${n} (genérica)</option>` +
    FN_CHOICES.map((c) => `<option value="${c}"${c === base ? " selected" : ""}>${FN_TYPES[c][0]} ${esc(FN_TYPES[c][1])}</option>`).join("");
  return `<div class="fnrow"><span>F${n}</span><select data-fnt="${n}">${opts}</select>
    <label title="Solo mientras se mantiene pulsado (bocina, silbato...)"><input type="checkbox" data-fnm="${n}"${t & 0x80 ? " checked" : ""}> momentánea</label></div>`;
}
function renderFnEditor(fn, all) {
  const rows = [];
  for (let n = 0; n < (all ? 32 : 16); n++) rows.push(fnRow(n, fn[n] || 0));
  $("#dlgFns").innerHTML = rows.join("");
  $("#dlgFnMore").hidden = all;
}
function readFnEditor() {
  const fn = [];
  document.querySelectorAll("#dlgFns [data-fnt]").forEach((s) => {
    const n = +s.dataset.fnt, t = +s.value;
    fn[n] = t ? t | ($(`#dlgFns [data-fnm="${n}"]`).checked ? 0x80 : 0) : 0;
  });
  while (fn.length && !fn[fn.length - 1]) fn.pop();
  return fn;
}

function setDlgPhoto(src) {
  $("#dlgImg").innerHTML = src ? `<img src="${src}" alt="">` : "<span>🚂<small>Sin foto</small></span>";
  $("#dlgPhotoDel").hidden = !src;
}

function openLocoDialog(ref) {
  const r = ref ? invByRef(ref) : null, l = ref ? locoByRef(ref) : null;
  dlgOld = r ? ref : null;
  dlgPhoto = null;
  dlgPhotoDel = false;
  const [pr, ad] = ref ? ref.split(":") : ["mfx", ""];
  $("#dlgTitle").textContent = r ? "Editar locomotora" : "Registrar locomotora";
  $("#dlgLabel").value = r ? r.label || r.name : l ? l.label || "" : "";
  $("#dlgProto").value = ["mm2", "dcc", "mfx"].includes(pr) ? pr : "mm2";
  $("#dlgAddr").value = ad || "";
  $("#dlgName").value = r ? r.name : "";
  $("#dlgName").placeholder = slug($("#dlgLabel").value || "loco");
  $("#dlgDel").hidden = !r;
  let fn = r && r.fn && r.fn.length ? r.fn.slice() : [1];
  if (!(r && r.fn && r.fn.length)) for (let n = 1; n < 16; n++) fn[n] = 50 + n;
  renderFnEditor(fn, fn.length > 16);
  setDlgPhoto(r && r.photo ? photoUrl(ref) : "");
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
    await post("/api/locoreg", { loco: ref, name, label, old: dlgOld || "", fn: readFnEditor().join(",") });
    if (dlgOld && dlgOld !== ref) {
      DESK.cabs = DESK.cabs.map((r) => (r === dlgOld ? ref : r));
      if (DESK.sw === dlgOld) DESK.sw = ref;
    }
    saveDesk();
    if (dlgPhoto) await uploadPhoto(ref, dlgPhoto);
    else if (dlgPhotoDel) await api("/api/photo/delete?loco=" + encodeURIComponent(ref), { method: "POST" });
    closeLocoDialog();
    loadInventory();
  } catch (e) {
    toast(e.message, true);
  }
}
async function deleteLocoDialog() {
  if (!dlgOld || !confirm("¿Quitar esta locomotora del inventario?")) return;
  try {
    await post("/api/locoreg", { loco: dlgOld, delete: 1 });
    DESK.cabs = DESK.cabs.map((r) => (r === dlgOld ? null : r));
    if (DESK.sw === dlgOld) DESK.sw = null;
    saveDesk();
    closeLocoDialog();
    loadInventory();
  } catch (e) {}
}
$("#dlgLabel").addEventListener("input", () => ($("#dlgName").placeholder = slug($("#dlgLabel").value || "loco")));
$("#dlgPhotoFile").addEventListener("change", async (e) => {
  const f = e.target.files[0];
  e.target.value = "";
  if (!f) return;
  try {
    dlgPhoto = await shrinkPhoto(f);
    dlgPhotoDel = false;
    setDlgPhoto(URL.createObjectURL(dlgPhoto));
  } catch (err) {
    toast(err.message, true);
  }
});
$("#dlgPhotoDel").onclick = () => {
  dlgPhoto = null;
  dlgPhotoDel = true;
  setDlgPhoto("");
};
