// ---- Languages / Idiomas ----
// The interface is written in English; TR[lang] maps each English text to
// its translation. In the HTML: data-i18n (inner HTML), data-i18n="pre"
// (keeps line breaks), data-i18n-ph (placeholder), data-i18n-title (title).
// In JS: t("Text with {0}", value). Messages that come from the ESP32
// (errors, log, script status) are translated with tr(), which also
// understands keys with {0}, {1}... placeholders.
const LANGS = { en: "English", es: "Español", de: "Deutsch", it: "Italiano", fr: "Français" };
const TR = {};
let LANG = "en";
try {
  LANG = localStorage.getItem("lang") || "en";
} catch (e) {}
if (!LANGS[LANG]) LANG = "en";

const norm = (s) => String(s).replace(/\s+/g, " ").trim();
const fmt = (s, args) => s.replace(/\{(\d+)\}/g, (m, i) => (args[i] !== undefined ? args[i] : m));

function t(key, ...args) {
  const d = TR[LANG];
  const k = key.includes("\n") ? key : norm(key);
  return fmt((d && d[k]) || k, args);
}

// Messages from the ESP32: exact text or a key with placeholders.
let trPatterns = null;
const trCache = new Map();
function tr(msg) {
  if (LANG === "en" || !msg) return msg;
  msg = String(msg);
  if (trCache.has(msg)) return trCache.get(msg);
  const d = TR[LANG] || {};
  let out = d[msg];
  if (out === undefined) {
    if (!trPatterns) {
      trPatterns = Object.keys(d).filter((k) => /\{\d\}/.test(k)).map((k) => {
        const order = [];
        const re = k.split(/(\{\d\})/).map((part) => {
          const m = part.match(/^\{(\d)\}$/);
          if (m) {
            order.push(+m[1]);
            return "(.*?)";
          }
          return part.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
        }).join("");
        return { re: new RegExp("^" + re + "$"), order, value: d[k] };
      }).sort((a, b) => b.re.source.length - a.re.source.length);
    }
    for (const p of trPatterns) {
      const m = msg.match(p.re);
      if (!m) continue;
      const args = [];
      p.order.forEach((n, i) => (args[n] = tr(m[i + 1])));
      out = fmt(p.value, args);
      break;
    }
  }
  if (out === undefined) out = msg;
  if (trCache.size > 2000) trCache.clear();
  trCache.set(msg, out);
  return out;
}

function applyI18n(root) {
  (root || document).querySelectorAll("[data-i18n]").forEach((el) => {
    if (el._i18nKey === undefined) el._i18nKey = el.dataset.i18n === "pre" ? el.innerHTML.trim() : norm(el.innerHTML);
    el.innerHTML = t(el._i18nKey);
  });
  (root || document).querySelectorAll("[data-i18n-ph]").forEach((el) => {
    if (el._i18nPh === undefined) el._i18nPh = el.getAttribute("placeholder");
    el.setAttribute("placeholder", t(el._i18nPh));
  });
  (root || document).querySelectorAll("[data-i18n-title]").forEach((el) => {
    if (el._i18nTitle === undefined) el._i18nTitle = el.getAttribute("title");
    el.setAttribute("title", t(el._i18nTitle));
  });
}

function setLang(lang) {
  try {
    localStorage.setItem("lang", lang);
  } catch (e) {}
  location.reload();
}

function initLangSelector() {
  const sel = document.getElementById("langSel");
  if (!sel) return;
  sel.innerHTML = Object.entries(LANGS).map(([k, v]) => `<option value="${k}"${k === LANG ? " selected" : ""}>${v}</option>`).join("");
  sel.onchange = () => setLang(sel.value);
  document.documentElement.lang = LANG;
}
