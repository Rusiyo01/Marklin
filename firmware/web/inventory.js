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
  return (fn || []).map((ty, n) => {
    if (!ty) return "";
    const [icon, name] = fnInfo(ty, n);
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
      <div class="invphoto">${r.photo ? `<img src="${photoUrl(r.ref)}" alt="">` : `<span>🚂<small>${t("No photo")}</small></span>`}</div>
      <div class="invbody"><b>${esc(r.label || r.name)}</b>
        <small>${esc(r.ref)} · ${t("scripts")}: ${esc(r.name)}${r.mfxuid ? " · mfx " + esc(r.mfxuid) : ""}</small>
        <div class="invfn">${fnIcons(r.fn) || `<span class="dir">${t("generic functions F0–F15")}</span>`}</div>
        ${l.speed ? `<small class="run">${t("moving")} · ${pct(l.speed)}</small>` : ""}
        <div class="row"><button class="small act" data-drive>${t("Drive")}</button><button class="small" data-edit>${t("Edit")}</button></div>
      </div></div>`;
  }).join("") || `<div class="panel dir">${t("No locos yet. Press <b>+ Add loco</b> or <b>Read locos from the MS2</b>.")}</div>`;
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
  const html = det.map((l) => `<div class="rl"><div class="rlt"><b>${esc(l.ref)}</b><small>${t("detected")}${l.speed ? " · " + pct(l.speed) : ""}</small></div>
    <button class="small" data-reg="${esc(l.ref)}">${t("Register")}</button></div>`).join("");
  $("#detBox").hidden = !det.length;
  if (box._html === html) return;
  box._html = html;
  box.innerHTML = html;
  box.querySelectorAll("[data-reg]").forEach((b) => (b.onclick = () => openLocoDialog(b.dataset.reg)));
}

function renderImport() {
  const imp = state.imp || {};
  $("#impMsg").textContent = tr(imp.msg || "");
  $("#ms2Btn").disabled = !!imp.busy;
  $("#ms2Btn").textContent = imp.busy ? t("Reading the MS2...") : t("Read locos from the MS2");
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
      img.onerror = () => ko(new Error(t("not a valid image")));
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
const FN_CHOICES = Object.keys(FN_TYPES).map(Number).sort((a, b) => t(FN_TYPES[a][1]).localeCompare(t(FN_TYPES[b][1])));

function fnRow(n, ty) {
  const base = ty & 0x7f, generic = 50 + n;
  const opts = `<option value="0">— ${t("none")} —</option><option value="${generic}"${base === generic ? " selected" : ""}>F${n} (${t("generic")})</option>` +
    FN_CHOICES.map((c) => `<option value="${c}"${c === base ? " selected" : ""}>${FN_TYPES[c][0]} ${esc(t(FN_TYPES[c][1]))}</option>`).join("");
  return `<div class="fnrow"><span>F${n}</span><select data-fnt="${n}">${opts}</select>
    <label title="${t("Only while held down (horn, whistle...)")}"><input type="checkbox" data-fnm="${n}"${ty & 0x80 ? " checked" : ""}> ${t("momentary")}</label></div>`;
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
    const n = +s.dataset.fnt, ty = +s.value;
    fn[n] = ty ? ty | ($(`#dlgFns [data-fnm="${n}"]`).checked ? 0x80 : 0) : 0;
  });
  while (fn.length && !fn[fn.length - 1]) fn.pop();
  return fn;
}

function setDlgPhoto(src) {
  $("#dlgImg").innerHTML = src ? `<img src="${src}" alt="">` : `<span>🚂<small>${t("No photo")}</small></span>`;
  $("#dlgPhotoDel").hidden = !src;
}

function openLocoDialog(ref) {
  const r = ref ? invByRef(ref) : null, l = ref ? locoByRef(ref) : null;
  dlgOld = r ? ref : null;
  dlgPhoto = null;
  dlgPhotoDel = false;
  const [pr, ad] = ref ? ref.split(":") : ["mfx", ""];
  $("#dlgTitle").textContent = r ? t("Edit loco") : t("Add loco");
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
  if (!label) return toast(t("Write a name for the loco"), true);
  if (!(addr > 0)) return toast(t("Write the decoder address"), true);
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
  if (!dlgOld || !confirm(t("Remove this loco from the inventory?"))) return;
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
