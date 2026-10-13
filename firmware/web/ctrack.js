// Geometría de las vías C de Märklin, compatible con los ficheros de
// "C-Track Layout Planner" (app:"ctrack-planner").
//
// Una pieza es {id, t: referencia, x, y (mm), r (grados), z: [alturas]}.
// (x, y) es la posición de su extremo 0 y r su giro. Cada referencia de la
// biblioteca tiene extremos (eps) y recorridos (paths) en coordenadas
// locales. Mismo modelo que el planificador, para que las maquetas
// importadas se dibujen exactamente igual.
"use strict";
const CT = (function () {
  const D2R = Math.PI / 180;
  const LIB = {};

  function sampleArc(R, deg, side, x0 = 0) {
    const n = Math.max(4, Math.ceil((R * deg * D2R) / 12)), pts = [];
    for (let k = 0; k <= n; k++) {
      const f = (deg * D2R * k) / n;
      pts.push([x0 + R * Math.sin(f), side * R * (1 - Math.cos(f))]);
    }
    return pts;
  }
  function sampleLine(x0, y0, x1, y1) {
    return [[x0, y0], [x1, y1]];
  }
  function add(def) { LIB[def.art] = def; }

  function straight(art, L, name) {
    add({ art, kind: "straight", name: name || `Recta ${(L / 10).toFixed(1)} cm`,
      eps: [{ x: 0, y: 0, a: 180 }, { x: L, y: 0, a: 0 }],
      paths: [{ a: 0, b: 1, pts: sampleLine(0, 0, L, 0) }] });
  }
  function curve(art, rn, R, deg) {
    add({ art, kind: "curve", name: `Curva ${rn} ${deg}°`,
      eps: [{ x: 0, y: 0, a: 180 }, { x: R * Math.sin(deg * D2R), y: -R * (1 - Math.cos(deg * D2R)), a: -deg }],
      paths: [{ a: 0, b: 1, pts: sampleArc(R, deg, -1) }] });
  }
  // Desvío: recorrido 0 = recto, recorrido 1 = desviado.
  function turnout(art, name, L, R, deg, side) {
    const s = side === "L" ? -1 : 1;
    add({ art, kind: "turnout", name: `${name} ${side === "L" ? "izquierdo" : "derecho"}`,
      eps: [{ x: 0, y: 0, a: 180 }, { x: L, y: 0, a: 0 }, { x: R * Math.sin(deg * D2R), y: s * R * (1 - Math.cos(deg * D2R)), a: s * deg }],
      paths: [{ a: 0, b: 1, pts: sampleLine(0, 0, L, 0) }, { a: 0, b: 2, pts: sampleArc(R, deg, s) }] });
  }
  // Desvío triple: recorridos 0 recto, 1 izquierda, 2 derecha.
  function threeway(art, L, R, deg) {
    const ex = R * Math.sin(deg * D2R), ey = R * (1 - Math.cos(deg * D2R));
    add({ art, kind: "threeway", name: "Desvío triple",
      eps: [{ x: 0, y: 0, a: 180 }, { x: L, y: 0, a: 0 }, { x: ex, y: -ey, a: -deg }, { x: ex, y: ey, a: deg }],
      paths: [{ a: 0, b: 1, pts: sampleLine(0, 0, L, 0) }, { a: 0, b: 2, pts: sampleArc(R, deg, -1) }, { a: 0, b: 3, pts: sampleArc(R, deg, 1) }] });
  }
  function crossing(art, L, deg, name) {
    const c = L / 2, dx = c * Math.cos(deg * D2R), dy = c * Math.sin(deg * D2R);
    add({ art, kind: "crossing", name: name || `Cruce ${deg}°`,
      eps: [{ x: 0, y: 0, a: 180 }, { x: L, y: 0, a: 0 }, { x: c - dx, y: dy, a: 180 - deg }, { x: c + dx, y: -dy, a: -deg }],
      paths: [{ a: 0, b: 1, pts: sampleLine(0, 0, L, 0) }, { a: 2, b: 3, pts: sampleLine(c - dx, dy, c + dx, -dy) }] });
  }
  function curvedTurnout(art, side, S0, R, deg) {
    const s = side === "L" ? -1 : 1, ex = R * Math.sin(deg * D2R), ey = s * R * (1 - Math.cos(deg * D2R));
    add({ art, kind: "turnout", name: `Desvío curvo ${side === "L" ? "izquierdo" : "derecho"}`,
      eps: [{ x: 0, y: 0, a: 180 }, { x: ex, y: ey, a: s * deg }, { x: S0 + ex, y: ey, a: s * deg }],
      paths: [{ a: 0, b: 1, pts: sampleArc(R, deg, s) },
              { a: 0, b: 2, pts: sampleLine(0, 0, S0, 0).concat(sampleArc(R, deg, s, S0).slice(1)) }] });
  }

  [[24360, 360], [24236, 236.1], [24229, 229.3], [24188, 188.3], [24172, 171.7], [24094, 94.2],
   [24077, 77.5], [24071, 70.8], [24064, 64.3]].forEach(([a, l]) => straight(String(a), l));
  straight("24088", 188.3, "Vía de alimentación");
  LIB["24088"].feeder = true;
  [["24130", "R1", 360, 30], ["24115", "R1", 360, 15], ["24107", "R1", 360, 7.5], ["24230", "R2", 437.5, 30],
   ["24215", "R2", 437.5, 15], ["24224", "R2", 437.5, 24.3], ["24206", "R2", 437.5, 5.7], ["24207", "R2", 437.5, 7.5],
   ["24330", "R3", 515, 30], ["24315", "R3", 515, 15], ["24430", "R4", 579.3, 30], ["24530", "R5", 643.6, 30],
   ["24912", "R9", 1114.6, 12.1]].forEach((c) => curve(...c));
  turnout("24611", "Desvío", 188.3, 437.5, 24.3, "L");
  turnout("24612", "Desvío", 188.3, 437.5, 24.3, "R");
  turnout("24711", "Desvío esbelto", 236.1, 1114.6, 12.1, "L");
  turnout("24712", "Desvío esbelto", 236.1, 1114.6, 12.1, "R");
  threeway("24630", 188.3, 437.5, 24.3);
  crossing("24640", 188.3, 24.3);
  crossing("24649", 103.3, 48.6);
  // Travesía doble: cruce + dos recorridos curvos (recorridos 2 y 3).
  (function () {
    crossing("24620", 188.3, 24.3, "Travesía doble");
    const d = LIB["24620"];
    d.kind = "dslip";
    const bez = (e0, e1) => {
      const L = Math.hypot(e1.x - e0.x, e1.y - e0.y), k = L / 3, a0 = (e0.a + 180) * D2R, a1 = (e1.a + 180) * D2R;
      const P = [[e0.x, e0.y], [e0.x + Math.cos(a0) * k, e0.y + Math.sin(a0) * k],
                 [e1.x + Math.cos(a1) * k, e1.y + Math.sin(a1) * k], [e1.x, e1.y]], pts = [];
      for (let t = 0; t <= 1.0001; t += 1 / 16) {
        const u = 1 - t;
        pts.push([0, 1].map((j) => u * u * u * P[0][j] + 3 * u * u * t * P[1][j] + 3 * u * t * t * P[2][j] + t * t * t * P[3][j]));
      }
      return pts;
    };
    const e = d.eps;
    d.paths.push({ a: 0, b: 3, pts: bez(e[0], e[3]) }, { a: 2, b: 1, pts: bez(e[2], e[1]) });
  })();
  curvedTurnout("24671", "L", 77.5, 360, 30);
  curvedTurnout("24672", "R", 77.5, 360, 30);
  curvedTurnout("24771", "L", 64.3, 515, 30);
  curvedTurnout("24772", "R", 64.3, 515, 30);
  add({ art: "24977", kind: "buffer", name: "Topera",
    eps: [{ x: 0, y: 0, a: 180 }, { x: 77.5, y: 0, a: 0, closed: true }],
    paths: [{ a: 0, b: 1, pts: sampleLine(0, 0, 77.5, 0) }] });

  const norm = (a) => ((((a % 360) + 540) % 360) - 180);
  function rot(x, y, deg) {
    const c = Math.cos(deg * D2R), s = Math.sin(deg * D2R);
    return [x * c - y * s, x * s + y * c];
  }
  // Extremo i de la pieza p en coordenadas del tablero.
  function wep(p, i) {
    const d = LIB[p.t].eps[i], [x, y] = rot(d.x, d.y, p.r);
    return { x: p.x + x, y: p.y + y, a: norm(d.a + p.r), closed: !!d.closed };
  }
  function wpt(p, pt) {
    const [x, y] = rot(pt[0], pt[1], p.r);
    return [p.x + x, p.y + y];
  }
  // Puntos (del tablero) de cada recorrido de la pieza.
  function paths(p) {
    return LIB[p.t].paths.map((pa) => pa.pts.map((pt) => wpt(p, pt)));
  }
  function center(p) {
    const ps = paths(p)[0], m = ps[Math.floor(ps.length / 2)];
    if (ps.length > 2) return m;
    return [(ps[0][0] + ps[ps.length - 1][0]) / 2, (ps[0][1] + ps[ps.length - 1][1]) / 2];
  }
  // Conexiones entre extremos: Map "id:i" -> {pid, i}.
  function connections(pieces, joins) {
    const conn = new Map(), all = [];
    for (const p of pieces) {
      if (!LIB[p.t]) continue;
      LIB[p.t].eps.forEach((d, i) => {
        if (d.closed) return;
        const e = wep(p, i);
        e.pid = p.id; e.i = i; e.z = (p.z && p.z[i]) || 0;
        all.push(e);
      });
    }
    for (const e of all)
      for (const o of all) {
        if (o.pid === e.pid || Math.hypot(o.x - e.x, o.y - e.y) > 2 || Math.abs(o.z - e.z) > 0.5) continue;
        if (Math.abs(norm(o.a - e.a + 180)) > 3) continue;
        conn.set(e.pid + ":" + e.i, { pid: o.pid, i: o.i });
      }
    for (const [a, b] of joins || []) {
      const [pa, ia] = a.split(":").map(Number), [pb, ib] = b.split(":").map(Number);
      if (!conn.has(a) && !conn.has(b)) {
        conn.set(a, { pid: pb, i: ib });
        conn.set(b, { pid: pa, i: ia });
      }
    }
    return conn;
  }
  // Coloca la pieza p uniendo su extremo k al extremo libre tgt {pid, i}.
  function attachTo(p, pieces, tgt, k) {
    const tp = pieces.find((q) => q.id === tgt.pid), E = wep(tp, tgt.i), d = LIB[p.t].eps[k];
    p.r = norm(E.a + 180 - d.a);
    const [rx, ry] = rot(d.x, d.y, p.r);
    p.x = E.x - rx;
    p.y = E.y - ry;
    const z = (tp.z && tp.z[tgt.i]) || 0;
    p.z = LIB[p.t].eps.map(() => z);
  }
  function bounds(pieces) {
    let x0 = Infinity, y0 = Infinity, x1 = -Infinity, y1 = -Infinity;
    for (const p of pieces) {
      if (!LIB[p.t]) continue;
      for (const ps of paths(p))
        for (const [x, y] of ps) {
          x0 = Math.min(x0, x); y0 = Math.min(y0, y); x1 = Math.max(x1, x); y1 = Math.max(y1, y);
        }
    }
    if (x0 === Infinity) return { x: 0, y: 0, w: 1000, h: 600 };
    return { x: x0, y: y0, w: x1 - x0, h: y1 - y0 };
  }
  const isSwitch = (t) => LIB[t] && ["turnout", "threeway", "dslip"].includes(LIB[t].kind);

  // Lee un fichero exportado por C-Track Planner: .json o la app .html sin conexión.
  function parseLayoutFile(text) {
    let o;
    if (/^\s*</.test(text)) {
      const m = text.match(/window\.__CTP_EMBED=(\{[\s\S]*?\});<\/script>/);
      if (!m) throw new Error("el fichero .html no contiene ninguna maqueta");
      o = JSON.parse(m[1]).layout;
      if (typeof o === "string") o = JSON.parse(o);
    } else {
      o = JSON.parse(text);
    }
    if (!o || !Array.isArray(o.pieces)) throw new Error("no es una maqueta de C-Track Planner");
    const unknown = o.pieces.filter((p) => !LIB[p.t]).map((p) => p.t);
    o.pieces = o.pieces.filter((p) => LIB[p.t]);
    if (!o.pieces.length) throw new Error("la maqueta no tiene vías reconocidas");
    delete o.bg;  // la imagen de calco no hace falta (y ocupa mucho)
    return { layout: o, unknown: [...new Set(unknown)] };
  }

  return { LIB, wep, wpt, paths, center, connections, attachTo, bounds, isSwitch, norm, parseLayoutFile };
})();
if (typeof module !== "undefined") module.exports = CT;
