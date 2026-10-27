// ---- Asistente de primera configuración ----
// Aparece la primera vez (setupDone = false): comprueba el bus CAN, recoge
// las locos (detectadas desde la MS2 o a mano), elige la maqueta y la WiFi.
const WZ = { open: false, step: 0, locos: [], removed: new Set(), layouts: [], layout: "", wifi: { mode: "ap", ssid: "", password: "" }, test: true, done: false };
const WZ_STEPS = ["Connection", "Locomotives", "Layout", "WiFi", "Done"];

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
  $("#wzSteps").innerHTML = WZ_STEPS.map((name, i) => `<span class="${i === s ? "cur" : i < s ? "past" : ""}">${i + 1}. ${t(name)}</span>`).join("");
  $("#wzBack").style.visibility = s > 0 && !WZ.done ? "visible" : "hidden";
  $("#wzNext").textContent = s === WZ_STEPS.length - 1 ? t("Finish") : t("Next");
  $("#wzNext").style.display = WZ.done ? "none" : "";
  $("#wzLater").style.display = WZ.done ? "none" : "";
  if (WZ.done) return;
  if (s === 0) {
    const c = state.can || {}, dev = state.devices || [];
    const gb = dev.some((d) => d.type === "Gleisbox");
    const canOk = c.state === "ok" && !(c.tec >= 128);
    b.innerHTML = `<h2>${t("Welcome!")}</h2>
      <p>${t("This wizard gets the system ready in a minute (from the laptop or the phone). First we check the connection with the Gleisbox (switch it on and, if you have it, connect the MS2 too).")}</p>
      ${wzCheck(canOk, t("CAN bus working"), t("Check the CAN-H / CAN-L / GND wiring (see docs/hardware.md). You can continue and check it later."))}
      ${wzCheck(gb, t("Gleisbox found"), t('Press "Find devices". If it does not appear, check the cable and that the Gleisbox is plugged in.'))}
      ${wzCheck(state.ms2, t("MS2 connected"), t("Optional: without an MS2 the ESP32 wakes the Gleisbox up by itself (MM2 and DCC locos)."))}
      <p class="dir">${t("Language")}: <select onchange="setLang(this.value)">${Object.entries(LANGS).map(([k, v]) => `<option value="${k}"${k === LANG ? " selected" : ""}>${v}</option>`).join("")}</select></p>
      <div class="row" style="margin-top:10px"><button onclick="post('/api/ping')">${t("Find devices")}</button></div>`;
  } else if (s === 1) {
    b.innerHTML = `<h2>${t("Your locomotives")}</h2>
      <p>${t('Drive each loco for a moment with the MS2 and it will appear here by itself. You can also add them with their address. Give them any name you like (e.g. "Crocodile SBB").')}</p>
      <div id="wzLocoList"></div>
      <div class="row" style="margin-top:10px">
        <select id="wzProto"><option value="mm2">MM2</option><option value="dcc">DCC</option><option value="mfx">mfx (SID)</option></select>
        <input id="wzAddr" type="number" min="1" placeholder="${t("address")}" style="width:110px">
        <button onclick="wzAddLoco()">${t("Add")}</button></div>`;
    wzRenderLocoList();
  } else if (s === 2) {
    const opt = (v, title, sub) => `<label class="opt"><input type="radio" name="wzLay" value="${esc(v)}"${WZ.layout === v ? " checked" : ""}> <span><b>${esc(title)}</b>${sub ? `<span class="dir"> ${esc(sub)}</span>` : ""}</span></label>`;
    b.innerHTML = `<h2>${t("Your layout")}</h2>
      <p>${t("Choose an example layout or upload yours from <b>C-Track Planner</b> (Save button → Export .json file, or the offline .html app). You can assign turnouts and sensors in the Layout tab.")}</p>
      ${WZ.layouts.map((l) => opt(l.name, l.title, l.name)).join("")}
      ${opt("", t("None for now"), "")}
      <div class="row" style="margin-top:10px"><button onclick="$('#layoutFile').click()">${t("Upload a C-Track Planner layout…")}</button></div>`;
    b.querySelectorAll('input[name="wzLay"]').forEach((r) => (r.onchange = () => (WZ.layout = r.value)));
  } else if (s === 3) {
    const w = WZ.wifi;
    b.innerHTML = `<h2>${t("WiFi network")}</h2>
      <label class="opt"><input type="radio" name="wzW" value="ap"${w.mode === "ap" ? " checked" : ""}> <span><b>${t("The ESP32's own network")}</b>
        <span class="dir"> ${t("Connect the laptop or phone to «{0}». That device will have no Internet meanwhile.", esc((state.wifi && state.wifi.apSsid) || "Marklin-ESP32"))}</span></span></label>
      <label class="opt"><input type="radio" name="wzW" value="sta"${w.mode === "sta" ? " checked" : ""}> <span><b>${t("My home WiFi")}</b>
        <span class="dir"> ${t("Recommended for the laptop: the ESP32 joins your network (it must be 2.4 GHz), you keep Internet and open it with http://marklin.local or its IP. The own network stays on just in case.")}</span></span></label>
      <div id="wzSta" class="row" style="margin-top:8px"${w.mode === "sta" ? "" : " hidden"}>
        <input id="wzSsid" placeholder="${t("Network name")}" value="${esc(w.ssid)}">
        <input id="wzPass" type="password" placeholder="${t("Password")}" value="${esc(w.password)}"></div>`;
    b.querySelectorAll('input[name="wzW"]').forEach((r) => (r.onchange = () => {
      w.mode = r.value;
      $("#wzSta").hidden = w.mode !== "sta";
    }));
  } else {
    const first = WZ.locos[0];
    const lay = WZ.layouts.find((l) => l.name === WZ.layout);
    b.innerHTML = `<h2>${t("All set")}</h2>
      <ul class="sum"><li>${t("{0} loco(s): {1}", WZ.locos.length, WZ.locos.map((l) => `<b>${esc(wzName(l))}</b> (${esc(l.ref)})`).join(", ") || t("none"))}</li>
      <li>${t("Layout")}: <b>${esc(lay ? lay.title : t("none"))}</b></li>
      <li>WiFi: <b>${WZ.wifi.mode === "sta" ? t("home network «{0}»", esc(WZ.wifi.ssid)) : t("the ESP32's own network")}</b></li></ul>
      ${first ? `<label class="opt"><input type="checkbox" id="wzTest"${WZ.test ? " checked" : ""}> <span>${t("Create the script <b>{0}</b>: a short, slow back-and-forth run for the first test.", "test_" + esc(wzName(first)))}</span></label>` : ""}`;
    const chk = $("#wzTest");
    if (chk) chk.onchange = () => (WZ.test = chk.checked);
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
    || `<p class="dir">${t("None yet. Drive a loco with the MS2 or add it below.")}</p>`;
}
function wzAddLoco() {
  const a = parseInt($("#wzAddr").value, 10);
  if (!(a > 0)) return toast(t("Write the loco address"), true);
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
      if (!w.ssid) return toast(t("Write the name of your WiFi network"), true);
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
      const src = `# Test script created by the setup wizard.\n# Put the loco "${n}" on a straight section and keep the MS2 STOP button at hand.\n` +
        `power on\nfunc ${n} 0 on\ndir ${n} fwd\nramp ${n} 25% 2s\nwait 3s\nramp ${n} 0 2s\nwait 2s\n` +
        `dir ${n} rev\nramp ${n} 25% 2s\nwait 3s\nramp ${n} 0 2s\nfunc ${n} 0 off\nlog test finished\n`;
      await api("/api/script?name=" + encodeURIComponent(("test_" + n).slice(0, 31)), { method: "POST", body: src });
    }
    const body = { locos, layout: WZ.layout, wifi: WZ.wifi };
    const d = await api("/api/setup", { method: "POST", body: JSON.stringify(body) });
    WZ.done = true;
    wzRender();
    if (d.restart) {
      $("#wzBody").innerHTML = `<h2>${t("Connecting to «{0}»…", esc(WZ.wifi.ssid))}</h2>
        <p>${t("The ESP32 restarts and joins your network. Connect this device to that same network and open <b>http://marklin.local</b> (or look up the ESP32's IP in your router).")}</p>
        <p class="dir">${t("If <b>marklin.local</b> does not open (it happens on some Windows PCs), connect to the «Marklin-ESP32» network, which is always on, open <b>http://192.168.4.1</b> and you will see the ESP32's home IP in <b>Settings</b>. Remember: the ESP32 only works with <b>2.4 GHz</b> networks.")}</p>`;
    } else {
      $("#wzBody").innerHTML = `<h2>${t("Configured!")}</h2>
        <p>${t("Press <b>GO</b> at the top to power the track. In the <b>Locomotives</b> tab you have your locos on the left: click one to put it on a throttle (up to 8 at once). Turnouts are switched in the <b>Turnouts</b> tab, and in <b>Inventory</b> you add photos and functions. In <b>Layout</b> you assign turnouts and sensors; the example scripts are in <b>Automation</b>.")}</p>
        <div class="row"><span class="sp"></span><button class="act" onclick="closeWizard();refresh()">${t("Start")}</button></div>`;
      if (typeof loadLayouts === "function") loadLayouts();
    }
  } catch (e) {
    toast(e.message, true);
  }
}
