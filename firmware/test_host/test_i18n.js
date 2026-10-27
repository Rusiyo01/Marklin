// Comprueba que todas las traducciones de la web están completas:
//   node firmware/test_host/test_i18n.js            comprobar
//   node firmware/test_host/test_i18n.js --keys     listar los textos en inglés
// Extrae los textos de index.html (data-i18n*) y de los t("...") de los .js;
// cada idioma de web/i18n_*.js debe traducirlos todos, con las mismas claves
// que los demás idiomas y los mismos marcadores {0}, {1}...
const fs = require("fs");
const path = require("path");
const vm = require("vm");

const WEB = path.join(__dirname, "../web");
const norm = (s) => String(s).replace(/\s+/g, " ").trim();
const keys = new Set();

// ---- index.html ----
const html = fs.readFileSync(path.join(WEB, "index.html"), "utf8");
const elRe = /<(\w+)((?:[^>"]|"[^"]*")*?)\bdata-i18n(?![-\w])(?:="(pre)")?((?:[^>"]|"[^"]*")*)>([\s\S]*?)<\/\1>/g;
for (const m of html.matchAll(elRe)) keys.add(m[3] === "pre" ? m[5].trim() : norm(m[5]));
for (const m of html.matchAll(/<[^>]*\bdata-i18n-ph[^>]*>/g)) keys.add(m[0].match(/placeholder="([^"]*)"/)[1]);
for (const m of html.matchAll(/<[^>]*\bdata-i18n-title[^>]*>/g)) keys.add(m[0].match(/title="([^"]*)"/)[1]);

// ---- t("...") en los .js ----
const jsFiles = fs.readdirSync(WEB).filter((f) => f.endsWith(".js") && !f.startsWith("i18n"));
const lit = /\bt\(\s*("(?:[^"\\]|\\.)*"|'(?:[^'\\]|\\.)*'|`[^`$\\]*`)/g;
for (const f of jsFiles) {
  const src = fs.readFileSync(path.join(WEB, f), "utf8");
  for (const m of src.matchAll(lit)) keys.add(norm(vm.runInNewContext(m[1])));
  // Nombres que se traducen indirectamente.
  if (f === "desk.js") for (const m of src.matchAll(/\["[^"]*", "([^"]+)"\]/g)) keys.add(m[1]);
  if (f === "wizard.js") {
    const steps = src.match(/WZ_STEPS = (\[[^\]]*\])/);
    for (const s of JSON.parse(steps[1])) keys.add(s);
  }
}

if (process.argv.includes("--keys")) {
  console.log([...keys].sort().join("\n"));
  process.exit(0);
}

// ---- Diccionarios ----
const ctx = { TR: {} };
vm.createContext(ctx);
const langs = fs.readdirSync(WEB).filter((f) => /^i18n_\w+\.js$/.test(f));
for (const f of langs) vm.runInContext(fs.readFileSync(path.join(WEB, f), "utf8"), ctx, { filename: f });
const codes = Object.keys(ctx.TR);
let errors = 0;
const err = (m) => {
  errors++;
  if (errors <= 60) console.log("  " + m);
};
const marks = (s) => (s.match(/\{\d\}/g) || []).sort().join("");
if (codes.length < 4) err(`se esperaban 4 idiomas, hay ${codes.length}: ${codes}`);
const all = new Set();
for (const c of codes) Object.keys(ctx.TR[c]).forEach((k) => all.add(k));
for (const c of codes) {
  const d = ctx.TR[c];
  for (const k of keys) if (!(k in d)) err(`${c}: falta "${k}"`);
  for (const k of all) if (!(k in d)) err(`${c}: falta la clave de otro idioma "${k}"`);
  for (const [k, v] of Object.entries(d)) {
    if (typeof v !== "string" || !v) err(`${c}: traducción vacía de "${k}"`);
    else if (marks(k) !== marks(v)) err(`${c}: marcadores distintos en "${k}"`);
  }
}
if (errors) {
  console.log(`${errors} problemas en las traducciones`);
  process.exit(1);
}
console.log(`traducciones OK: ${codes.join(", ")} · ${keys.size} textos de la interfaz, ${all.size} claves`);
