// Tests de la geometría de vías C y de la importación de maquetas.
//   node firmware/test_host/test_layouts.js
"use strict";
const assert = require("assert");
const fs = require("fs");
const path = require("path");
const CT = require("../web/ctrack.js");

let n = 0;
function test(name, fn) {
  fn();
  n++;
  console.log("[ OK ] " + name);
}
const near = (a, b, tol = 0.05) => Math.abs(a - b) <= tol;

test("curva R2 30 grados", () => {
  const p = { id: 1, t: "24230", x: 0, y: 0, r: 0, z: [0, 0] };
  const e = CT.wep(p, 1);
  assert(near(e.x, 437.5 * Math.sin(Math.PI / 6)) && near(e.y, -437.5 * (1 - Math.cos(Math.PI / 6))));
  assert(near(e.a, -30));
});

test("doce curvas R2 forman un circulo cerrado", () => {
  const pieces = [{ id: 1, t: "24230", x: 0, y: 0, r: 0, z: [0, 0] }];
  for (let i = 2; i <= 12; i++) {
    const p = { id: i, t: "24230", x: 0, y: 0, r: 0, z: [0, 0] };
    CT.attachTo(p, pieces, { pid: i - 1, i: 1 }, 0);
    pieces.push(p);
  }
  const c = CT.connections(pieces, []);
  assert.strictEqual(c.size, 24);  // todos los extremos unidos
});

test("desvios reconocidos", () => {
  for (const t of ["24611", "24612", "24711", "24712", "24630", "24620", "24671", "24772"]) assert(CT.isSwitch(t), t);
  for (const t of ["24188", "24230", "24640", "24977"]) assert(!CT.isSwitch(t), t);
  assert.strictEqual(CT.LIB["24630"].paths.length, 3);
  assert.strictEqual(CT.LIB["24620"].paths.length, 4);
});

test("maquetas de ejemplo cerradas y con asignaciones validas", () => {
  const dir = path.join(__dirname, "..", "layouts");
  const files = fs.readdirSync(dir).filter((f) => f.endsWith(".json"));
  assert(files.length >= 3);
  for (const f of files) {
    const L = JSON.parse(fs.readFileSync(path.join(dir, f), "utf8"));
    assert.strictEqual(L.app, "ctrack-planner");
    const c = CT.connections(L.pieces, L.joins);
    let open = 0;
    for (const p of L.pieces) CT.LIB[p.t].eps.forEach((d, i) => { if (!d.closed && !c.has(p.id + ":" + i)) open++; });
    assert.strictEqual(open, 0, f + ": extremos libres");
    const ids = new Set(L.pieces.map((p) => p.id));
    for (const t of L.marklin.turnouts) {
      assert(ids.has(t.piece) && CT.isSwitch(L.pieces.find((p) => p.id === t.piece).t), f + ": desvio " + t.name);
      assert(/^[a-z][a-z0-9_-]*$/.test(t.name));
    }
    for (const s of L.marklin.sensors) assert(ids.has(s.piece) && s.id > 0);
  }
});

test("importar .json y app .html de C-Track Planner", () => {
  const L = { app: "ctrack-planner", v: 1, name: "Prueba", bg: { img: "data:..." },
    pieces: [{ id: 1, t: "24188", x: 0, y: 0, r: 0, z: [0, 0] }, { id: 2, t: "99999", x: 0, y: 0, r: 0, z: [0] }] };
  const a = CT.parseLayoutFile(JSON.stringify(L));
  assert.strictEqual(a.layout.pieces.length, 1);
  assert.deepStrictEqual(a.unknown, ["99999"]);
  assert(!("bg" in a.layout));
  const embed = { layout: L, id: "E1", exported: "2026-10-06" };
  const html = `<!doctype html><html><head></head><body><script>window.__CTP_EMBED=${JSON.stringify(embed)};</script><script>/* app */</script></body></html>`;
  const b = CT.parseLayoutFile(html);
  assert.strictEqual(b.layout.name, "Prueba");
  assert.throws(() => CT.parseLayoutFile('{"hola":1}'));
  assert.throws(() => CT.parseLayoutFile("<html>sin maqueta</html>"));
});

test("plano CS2 de la estacion de ejemplo", () => {
  const G = require("../web/cs2gbs.js");
  const L = JSON.parse(fs.readFileSync(path.join(__dirname, "../layouts/station.json")));
  const r = G.convert(L);
  const typ = (t) => r.elements.filter((e) => e.typ === t);
  assert(r.text.startsWith("[gleisbildseite]\n"));
  assert.strictEqual(typ("linksweiche").length + typ("rechtsweiche").length, 2);
  for (const e of typ("linksweiche").concat(typ("rechtsweiche"))) assert(e.artikel > 0);
  assert(typ("s88kontakt").length >= 2);
  assert(typ("bogen").length >= 8);
  // Cada vía une con su vecina: ningún tope en un óvalo cerrado.
  assert.strictEqual(typ("prellbock").length, 0);
  assert(r.text.length < 32 * 1024);
});

console.log(`\n${n} tests de maquetas OK`);
