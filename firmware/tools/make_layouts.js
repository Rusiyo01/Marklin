// Genera las maquetas de ejemplo (formato de C-Track Planner) que vienen
// preinstaladas en el ESP32, con los desvíos y sensores ya asignados para
// que coincidan con los scripts de ejemplo.
//
//   node firmware/tools/make_layouts.js      (escribe firmware/layouts/*.json)
"use strict";
const fs = require("fs");
const path = require("path");
const CT = require("../web/ctrack.js");

function builder(name, board) {
  const L = { app: "ctrack-planner", v: 1, name, board, pieces: [], iso: [], joins: [], labels: [] };
  let nextId = 1, active = null;
  const make = (t) => ({ id: nextId++, t, x: 0, y: 0, r: 0, z: CT.LIB[t].eps.map(() => 0) });
  const conn = () => CT.connections(L.pieces, L.joins);
  const nextFree = (p, except) => {
    const c = conn();
    const eps = CT.LIB[p.t].eps;
    for (let i = 0; i < eps.length; i++) if (i !== except && !eps[i].closed && !c.has(p.id + ":" + i)) return i;
    return null;
  };
  return {
    L,
    // Primera pieza en (x, y) con giro r.
    add(t, x, y, r) {
      const p = make(t);
      Object.assign(p, { x, y, r });
      L.pieces.push(p);
      active = { pid: p.id, i: 1 };
      return p;
    },
    // Une una pieza al extremo activo por su extremo k (k=1 en una curva = gira al otro lado).
    go(t, k = 0) {
      const p = make(t);
      CT.attachTo(p, L.pieces, active, k);
      L.pieces.push(p);
      const n = nextFree(p, k);
      active = n != null ? { pid: p.id, i: n } : null;
      return p;
    },
    from(p, i) { active = { pid: p.id, i }; },
    openEnds() {
      const c = conn(), out = [];
      for (const p of L.pieces)
        CT.LIB[p.t].eps.forEach((d, i) => { if (!d.closed && !c.has(p.id + ":" + i)) out.push(p.id + ":" + i); });
      return out;
    },
  };
}

function round(L) {
  for (const p of L.pieces) {
    p.x = +p.x.toFixed(2);
    p.y = +p.y.toFixed(2);
    p.r = +p.r.toFixed(3);
  }
  return L;
}

function check(b, expectedOpen) {
  const open = b.openEnds();
  if (open.length !== expectedOpen) {
    throw new Error(`${b.L.name}: ${open.length} extremos libres (esperados ${expectedOpen}): ${open.join(" ")}`);
  }
}

const layouts = {};

// 1) Lanzadera: vía recta con toperas en los dos extremos y un sensor en
//    cada estación (sensores 1 y 2, como el script "shuttle").
{
  const b = builder("Shuttle between two stations", { w: 2000, h: 400 });
  const tope1 = b.add("24977", 1700, 200, 0);
  b.from(tope1, 0);
  const sA = b.go("24188", 1);   // extremo derecho (estación B)
  const mid = [];
  for (let i = 0; i < 6; i++) mid.push(b.go("24188", 1));
  const sB = b.go("24188", 1);   // extremo izquierdo (estación A)
  b.go("24977", 0);
  check(b, 0);
  b.L.marklin = {
    turnouts: [],
    sensors: [{ piece: sB.id, id: 1, name: "station_a" }, { piece: sA.id, id: 2, name: "station_b" }],
  };
  layouts.shuttle = round(b.L);
}

// 2) Óvalo con estación de dos vías (desvíos 1 y 2, sensores 7 y 8, como el
//    script "random_station").
{
  const b = builder("Oval with a two-track station", { w: 2600, h: 1300 });
  const start = b.add("24188", 700, 1100, 0);
  const t1 = b.go("24611");                  // desvío 1: la vía 2 va hacia el interior
  b.go("24172");
  const main = [b.go("24188"), b.go("24188"), b.go("24188")];
  b.go("24172");
  const t2 = b.go("24612", 1);              // desvío 2 (entra por el lado de la vía recta)
  b.go("24188");
  for (let i = 0; i < 6; i++) b.go("24230");
  // Lado de atrás: misma longitud que el de delante para cerrar el óvalo.
  for (let i = 0; i < 7; i++) b.go("24188");
  b.go("24172"); b.go("24172");
  for (let i = 0; i < 6; i++) b.go("24230");
  // Vía 2 (apartadero) desde la rama desviada del desvío 1.
  b.from(t1, 2);
  b.go("24224", 1);
  const sid = [b.go("24188"), b.go("24188"), b.go("24188")];
  b.go("24224", 1);
  check(b, 0);
  b.L.marklin = {
    turnouts: [{ piece: t1.id, name: "entry", addr: "mm:1" }, { piece: t2.id, name: "exit", addr: "mm:2" }],
    sensors: [{ piece: start.id, id: 7, name: "arrival" }, { piece: main[1].id, id: 8, name: "platform" },
              { piece: sid[1].id, id: 9, name: "platform_track2" }],
  };
  layouts.station = round(b.L);
}

// 3) El ejemplo del planificador: óvalo con apartadero y ramal elevado.
{
  const b = builder("Oval with a raised branch line", { w: 2400, h: 1300 });
  b.add("24188", 450, 1000, 0);
  const t1 = b.go("24612");
  b.go("24188"); b.go("24188");
  for (let i = 0; i < 6; i++) b.go("24230");
  for (let i = 0; i < 4; i++) b.go("24188");
  const oval = [];
  for (let i = 0; i < 6; i++) oval.push(b.go("24230"));
  b.from(t1, 2);
  b.go("24224");
  const run = [];
  for (let i = 0; i < 5; i++) run.push(b.go("24188"));
  const top = [b.go("24188"), b.go("24977")];
  // Rampa de 0 a 28 mm a lo largo del tramo, como el ejemplo original.
  const total = run.length;
  run.forEach((p, k) => { p.z = [Math.round((28 * k) / total * 10) / 10, Math.round((28 * (k + 1)) / total * 10) / 10]; });
  for (const p of top) p.z = p.z.map(() => 28);
  check(b, 0);
  b.L.marklin = {
    turnouts: [{ piece: t1.id, name: "branch", addr: "mm:3" }],
    sensors: [{ piece: top[0].id, id: 10, name: "branch_end" }],
  };
  layouts.branch_line = round(b.L);
}

const out = path.join(__dirname, "..", "layouts");
fs.mkdirSync(out, { recursive: true });
for (const [name, L] of Object.entries(layouts)) {
  fs.writeFileSync(path.join(out, name + ".json"), JSON.stringify(L) + "\n");
  console.log(`${name}.json: ${L.pieces.length} piezas`);
}
