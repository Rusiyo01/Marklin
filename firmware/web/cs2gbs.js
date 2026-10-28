// ---- Plano de vías para la Central Station (gleisbilder/*.cs2) ----
// Convierte una maqueta de C-Track Planner en una página de plano CS2: una
// rejilla de casillas con rectas, curvas, desvíos, topes y contactos S88,
// para que las apps (RailControl Pro...) dibujen la maqueta y accionen los
// desvíos. El navegador la calcula y la guarda en el ESP32.
//
// Formato CS2: element .id=0xXXYY (columna, fila), .typ, .drehung (0-3 en
// sentido horario), .artikel (dirección del desvío o contacto, -1 = nada).
// Orientación base (drehung 0), como en RailControl:
//   gerade: norte-sur · bogen: oeste-sur · prellbock: abierto al sur
//   linksweiche/rechtsweiche/dreiwegweiche: entrada por el sur, recto al
//   norte, desvío al oeste (izquierda) o al este (derecha).
const CS2GBS = (function () {
  const CTX = typeof CT !== "undefined" ? CT : require("./ctrack.js");
  const N = 0, E = 1, S = 2, W = 3;
  const opp = (s) => (s + 2) % 4;
  const MAX_W = 64, MAX_H = 40;

  // Lado de la casilla hacia el que apunta un ángulo (grados, 0 = este, y hacia abajo).
  function sideOfAngle(a) {
    a = ((a % 360) + 360) % 360;
    return a >= 315 || a < 45 ? E : a < 135 ? S : a < 225 ? W : N;
  }

  // Casillas que recorre una polilínea, sin saltos en diagonal.
  function cellsOf(pts, toCell) {
    const out = [];
    const push = (c) => {
      const last = out[out.length - 1];
      if (last && last[0] === c[0] && last[1] === c[1]) return;
      if (last && last[0] !== c[0] && last[1] !== c[1]) out.push([c[0], last[1]]);
      out.push(c);
    };
    for (let i = 0; i < pts.length; i++) {
      const [x0, y0] = pts[i];
      push(toCell(x0, y0));
      if (i + 1 < pts.length) {
        const [x1, y1] = pts[i + 1], n = Math.ceil(Math.hypot(x1 - x0, y1 - y0) / 5);
        for (let k = 1; k < n; k++) push(toCell(x0 + ((x1 - x0) * k) / n, y0 + ((y1 - y0) * k) / n));
      }
    }
    return out;
  }
  const stepSide = (a, b) => (b[0] > a[0] ? E : b[0] < a[0] ? W : b[1] > a[1] ? S : N);

  function addrNum(ref) {
    const m = /(\d+)$/.exec(ref || "");
    return m ? +m[1] : -1;
  }

  function convert(L) {
    const pieces = (L.pieces || []).filter((p) => CTX.LIB[p.t]);
    const bb = CTX.bounds(pieces);
    const cell = Math.max(64, bb.w / (MAX_W - 2), bb.h / (MAX_H - 2));
    const toCell = (x, y) => [1 + Math.floor((x - bb.x) / cell), 1 + Math.floor((y - bb.y) / cell)];
    const key = (c) => c[0] + "," + c[1];
    const sides = new Map(); // casilla -> Set de lados unidos
    const addSide = (c, s) => {
      const k = key(c);
      if (!sides.has(k)) sides.set(k, new Set());
      sides.get(k).add(s);
    };
    const link = (seq) => {
      for (let i = 0; i + 1 < seq.length; i++) {
        const s = stepSide(seq[i], seq[i + 1]);
        addSide(seq[i], s);
        addSide(seq[i + 1], opp(s));
      }
    };
    for (const p of pieces) for (const pts of CTX.paths(p)) link(cellsOf(pts, toCell));
    // Extremos unidos que caen en casillas distintas (justo en el borde).
    const byId = new Map(pieces.map((p) => [p.id, p]));
    for (const [k, o] of CTX.connections(pieces, L.joins)) {
      const [pid, i] = k.split(":").map(Number), q = byId.get(o.pid);
      if (!q) continue;
      const a = CTX.wep(byId.get(pid), i), b = CTX.wep(q, o.i);
      link(cellsOf([[a.x, a.y], [b.x, b.y]], toCell));
    }

    // Desvíos: la casilla donde se separa la vía (3 o 4 lados) más cercana a
    // cada pieza de desvío. Cerca = a algún punto de sus recorridos.
    const M = L.marklin || {};
    const turnouts = new Map((M.turnouts || []).map((t) => [t.piece, t]));
    const junctions = [...sides].filter(([, set]) => set.size >= 3).map(([k]) => k);
    const cand = [];
    for (const p of pieces) {
      const kind = CTX.LIB[p.t].kind;
      if (!["turnout", "threeway", "dslip"].includes(kind)) continue;
      const pts = CTX.paths(p).flat();
      for (const k of junctions) {
        const [x, y] = k.split(",").map(Number), cx = bb.x + (x - 0.5) * cell, cy = bb.y + (y - 0.5) * cell;
        const d = Math.min(...pts.map(([px, py]) => Math.hypot(px - cx, py - cy)));
        if (d < cell * 1.5) cand.push({ d, k, p, kind });
      }
    }
    cand.sort((a, b) => a.d - b.d);
    const special = new Map(), placed = new Set();
    for (const c of cand) {
      if (special.has(c.k) || (placed.has(c.p.id) && c.kind !== "threeway")) continue;
      if (c.kind === "threeway" && [...special.values()].filter((s) => s.p === c.p).length >= 2) continue;
      special.set(c.k, c);
      placed.add(c.p.id);
    }

    const els = new Map();
    for (const [k, set] of sides) {
      const [x, y] = k.split(",").map(Number), sp = special.get(k);
      const has = (a, b) => set.has(a) && set.has(b);
      let el;
      if (sp && set.size === 3) {
        // Entrada: el lado hacia donde apunta el extremo común (0) de la pieza.
        const B = [0, 1, 2, 3].find((s) => set.has(s) && !set.has(opp(s)));
        let C = sideOfAngle(CTX.wep(sp.p, 0).a);
        if (C === B || C === opp(B) || !set.has(C)) C = (B + 1) % 4;
        const T = turnouts.get(sp.p.id) || {}, left = B === (C + 1) % 4;
        const two = sp.kind === "threeway" && [...special.values()].filter((s) => s.p === sp.p).length > 1;
        const addr = two && !left && T.addr2 ? T.addr2 : T.addr;
        el = { typ: left ? "linksweiche" : "rechtsweiche", drehung: (C + 2) % 4, artikel: addrNum(addr), text: T.name };
      } else if (sp && set.size === 4) {
        const T = turnouts.get(sp.p.id) || {};
        const C = sideOfAngle(CTX.wep(sp.p, 0).a);
        el = sp.kind === "threeway"
          ? { typ: "dreiwegweiche", drehung: (C + 2) % 4, artikel: addrNum(T.addr), text: T.name }
          : { typ: sp.kind === "dslip" ? "dkweiche" : "kreuzung", drehung: C % 2, artikel: addrNum(T.addr), text: T.name };
      } else if (set.size === 4) {
        el = { typ: "kreuzung", drehung: 0 };
      } else if (set.size === 1) {
        el = { typ: "prellbock", drehung: ([...set][0] + 2) % 4 };
      } else if (has(N, S) || has(E, W)) {
        // Recta (también una casilla de 3 lados sin desvío: se queda la recta).
        el = { typ: "gerade", drehung: has(N, S) ? 0 : 1 };
      } else {
        // bogen 0: oeste-sur, 1: norte-oeste, 2: norte-este, 3: este-sur.
        el = { typ: "bogen", drehung: has(W, S) ? 0 : has(N, W) ? 1 : has(N, E) ? 2 : 3 };
      }
      els.set(k, Object.assign({ x, y, artikel: -1 }, el));
    }
    // Contactos S88 sobre la vía de su pieza.
    for (const sn of M.sensors || []) {
      const p = byId.get(sn.piece);
      if (!p) continue;
      const el = els.get(key(toCell(...CTX.center(p))));
      if (!el || !["gerade", "bogen"].includes(el.typ)) continue;
      el.typ = el.typ === "gerade" ? "s88kontakt" : "s88bogen";
      el.artikel = sn.id;
      el.text = sn.name || "";
    }
    let w = 0, h = 0;
    for (const el of els.values()) {
      w = Math.max(w, el.x + 2);
      h = Math.max(h, el.y + 2);
    }
    const lines = ["[gleisbildseite]", "version", " .major=1"];
    for (const el of [...els.values()].sort((a, b) => a.y - b.y || a.x - b.x)) {
      lines.push("element", " .id=0x" + ((el.x << 8) | el.y).toString(16), " .typ=" + el.typ);
      if (el.drehung) lines.push(" .drehung=" + el.drehung);
      lines.push(" .artikel=" + el.artikel);
      if (el.text) lines.push(" .text=" + String(el.text).replace(/[\r\n]/g, " "));
    }
    return { text: lines.join("\n") + "\n", width: w, height: h, elements: [...els.values()] };
  }

  return { convert };
})();
if (typeof module !== "undefined") module.exports = CS2GBS;
