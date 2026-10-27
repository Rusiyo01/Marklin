// ---- Programación de decodificadores (CV) ----
// Lectura y escritura de CV a través de la Gleisbox, como la MS2/CS3:
// DCC en modo programación o en vía principal (POM), MM2 programable y mfx.
const PG_CVS = {
  dcc: [[1, "Short address"], [2, "Start voltage"], [3, "Acceleration"], [4, "Deceleration"], [5, "Maximum speed"],
    [6, "Middle speed"], [7, "Version", "r"], [8, "Manufacturer (write 8 = reset)"], [17, "Long address (high)"],
    [18, "Long address (low)"], [29, "Configuration"]],
  mm: [[1, "Address"], [2, "Start voltage"], [3, "Acceleration"], [4, "Deceleration"], [5, "Maximum speed"],
    [8, "Reset (write 8)"]],
  mfx: [],
};
const pgMode = () => $("#pgMode").value;
const pgCanRead = () => pgMode() === "dcc" || pgMode() === "mfx";
function pgRef() {
  const m = pgMode(), a = +$("#pgAddr").value || 0;
  return m === "mm" ? "mm2:" + a : m === "mfx" ? "mfx:" + a : "dcc:" + (m === "dcc" ? a || 3 : a);
}
function pgLogLine(txt, ok) {
  const box = $("#pgLog");
  box.innerHTML = `<div class="${ok ? "" : "k-error"}">${esc(new Date().toLocaleTimeString())}  ${esc(txt)}</div>` + box.innerHTML;
}

// Envía la orden y espera la respuesta de la Gleisbox (hasta 15 s).
async function pgOp(params) {
  const d = await api("/api/cv?" + new URLSearchParams(Object.assign({ loco: pgRef(), index: +$("#pgIdx").value || 0 }, params)), { method: "POST" });
  for (let t0 = Date.now(); Date.now() - t0 < 15000;) {
    await new Promise((r) => setTimeout(r, 200));
    const r = await api("/api/cv");
    if (r.seq !== d.seq) return r;
  }
  return null;
}
async function pgRead(cv, out) {
  try {
    const r = await pgOp({ op: "read", cv });
    if (r && r.ok) {
      if (out) out.value = r.value;
      pgLogLine(t("CV {0} = {1}", cv, r.value), true);
      return r.value;
    }
    pgLogLine(t("CV {0}: no answer from the decoder", cv), false);
  } catch (e) {
    pgLogLine(e.message, false);
  }
  return null;
}
async function pgWrite(cv, value) {
  if (!(value >= 0 && value <= 255)) return toast(t("Value 0-255"), true);
  try {
    const r = await pgOp({ op: "write", cv, value, main: pgMode() === "pom" ? 1 : 0 });
    const ok = !!(r && r.ok);
    pgLogLine(t("CV {0} ← {1}: {2}", cv, value, !r ? t("no answer") : r.verified ? t("written and verified") : ok ? t("written") : t("not confirmed")), ok);
    return ok;
  } catch (e) {
    pgLogLine(e.message, false);
    return false;
  }
}

// Cambio de dirección: DCC corta (CV1) o larga (CV17/18) ajustando CV29; MM2 en CV1.
async function pgChangeAddr() {
  const n = +$("#pgNew").value;
  if (pgMode() === "mm") return n >= 1 && n <= 255 ? pgWrite(1, n) : toast(t("Address 1-255"), true);
  if (!(n >= 1 && n <= 10239)) return toast(t("Address 1-10239"), true);
  const cv29 = await pgRead(29);
  if (cv29 === null) return;
  if (n <= 127) {
    if (await pgWrite(1, n)) await pgWrite(29, cv29 & ~0x20);
  } else if (await pgWrite(17, 0xc0 | (n >> 8)) && await pgWrite(18, n & 0xff)) {
    await pgWrite(29, cv29 | 0x20);
  }
}

function pgRender() {
  const m = pgMode(), canRead = pgCanRead();
  $("#pgAddrBox").hidden = m === "dcc";
  $("#pgIdxBox").hidden = m !== "mfx";
  $("#pgAddrPanel").hidden = m === "pom" || m === "mfx";
  document.querySelectorAll(".pgRead").forEach((b) => (b.disabled = !canRead));
  $("#pgHint").textContent = {
    dcc: t("Programming mode: put ONLY the loco to program on the track. Reading and writing take a few seconds."),
    pom: t("Main track: the loco is addressed by its address; other locos can stay on the track. Values cannot be read."),
    mm: t("Programmable MM2 decoders (e.g. Märklin mSD/mLD, ESU): put ONLY this loco on the track. Values cannot be read."),
    mfx: t("mfx: use the SID the MS2 gave the loco. The structure of the mfx values depends on each decoder (CV + index)."),
  }[m];
  $("#pgTable").innerHTML = PG_CVS[m === "pom" ? "dcc" : m].map(([cv, name, ro]) => `<tr><td>CV ${cv}</td><td>${esc(t(name))}</td>
    <td><input type="number" min="0" max="255" style="width:80px" data-cv="${cv}"></td>
    <td style="text-align:right"><button class="small pgRead" onclick="pgRead(${cv}, $('[data-cv=\\'${cv}\\']'))"${canRead ? "" : " disabled"}>${t("Read")}</button>
    ${ro ? "" : `<button class="small" onclick="pgWrite(${cv}, +$('[data-cv=\\'${cv}\\']').value)">${t("Write")}</button>`}</td></tr>`).join("")
    || `<tr><td class="dir">${t("Use “Any CV” with the CV and index of your decoder.")}</td></tr>`;
}
function pgLocos() {
  const sel = $("#pgLoco"), opts = `<option value="">— ${t("from the inventory")} —</option>` +
    regLocos().map((l) => `<option value="${esc(l.ref)}">${esc(l.label || l.name)} (${esc(l.ref)})</option>`).join("");
  if (sel._opts !== opts) sel.innerHTML = sel._opts = opts;
}
$("#pgLoco").onchange = (e) => {
  const [p, a] = (e.target.value || ":").split(":");
  if (!p) return;
  $("#pgMode").value = p === "mm2" ? "mm" : p === "mfx" ? "mfx" : "pom";
  $("#pgAddr").value = a;
  pgRender();
};
$("#pgMode").onchange = pgRender;
pgRender();
