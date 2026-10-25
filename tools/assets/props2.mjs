// ⚫ AJOUTÉ (25/10/2026) : accessoires de la version "plusieurs animaux" : gamelles (normales / renversées),
// main qui caresse, oiseaux, besoins. Séparé de props.mjs pour garder chaque fichier lisible.
const svg = (w, h, body, defs = '') =>
    `<svg xmlns="http://www.w3.org/2000/svg" width="${w}" height="${h}" viewBox="0 0 ${w} ${h}"><defs>${defs}</defs>${body}</svg>`;

function rng(seed) {
    let s = seed >>> 0;
    return () => ((s = (s * 1664525 + 1013904223) >>> 0) / 4294967296);
}

/* ---------- gamelles ---------- */

const BOWL_DEFS = `
  <linearGradient id="redBody" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#9e1c1c"/><stop offset="0.35" stop-color="#e8443c"/><stop offset="1" stop-color="#8a1414"/></linearGradient>
  <linearGradient id="redRim" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#ff7a6a"/><stop offset="1" stop-color="#c22a24"/></linearGradient>
  <linearGradient id="blueBody" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#1c4e9e"/><stop offset="0.35" stop-color="#4a90e8"/><stop offset="1" stop-color="#163e86"/></linearGradient>
  <linearGradient id="blueRim" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#9cc8ff"/><stop offset="1" stop-color="#2e6cc8"/></linearGradient>
  <radialGradient id="water" cx="0.4" cy="0.35" r="0.7"><stop offset="0" stop-color="#d4f2ff"/><stop offset="0.5" stop-color="#6cc4f0"/><stop offset="1" stop-color="#2a86c8"/></radialGradient>
  <radialGradient id="puddle" cx="0.5" cy="0.5" r="0.5"><stop offset="0" stop-color="#8fd4f4" stop-opacity="0.85"/><stop offset="1" stop-color="#4aa0d8" stop-opacity="0.55"/></radialGradient>
  <filter id="sh" x="-30%" y="-80%" width="160%" height="260%"><feGaussianBlur stdDeviation="3"/></filter>`;

// kind = 'food' (rouge, croquettes) ou 'water' (bleue) ; tipped = renversée, contenu répandu
export function bowl(kind, tipped) {
    const red = kind === 'food';
    const body = red ? 'redBody' : 'blueBody', rim = red ? 'redRim' : 'blueRim';
    const W = 110, H = 56, cx = 55, cy = 30, R = 34;
    if (!tipped) {
        const inner = red ? '#3a0c0c' : 'url(#water)';
        const deco = red
            ? `<path d="M${cx - 7} ${cy + 6.5} h14 a2 2 0 1 1 1.5 2.5 a2 2 0 1 1 -1.5 2.5 h-14 a2 2 0 1 1 -1.5 -2.5 a2 2 0 1 1 1.5 -2.5 z" fill="#ffd9d0" opacity="0.8"/>`
            : `<ellipse cx="${cx - 9}" cy="${cy - 5}" rx="10" ry="2" fill="#fff" opacity="0.8"/><ellipse cx="${cx + 8}" cy="${cy - 2}" rx="5" ry="1" fill="#fff" opacity="0.5"/>`;
        return svg(W, H, `<ellipse cx="${cx}" cy="${cy + 12}" rx="${R + 8}" ry="9" fill="#14300c" opacity="0.5" filter="url(#sh)"/>
          <path d="M${cx - R} ${cy - 4} L${cx - R + 7} ${cy + 11} Q${cx} ${cy + 19} ${cx + R - 7} ${cy + 11} L${cx + R} ${cy - 4} Z" fill="url(#${body})"/>
          <ellipse cx="${cx}" cy="${cy - 4}" rx="${R}" ry="9.5" fill="url(#${rim})"/>
          <ellipse cx="${cx}" cy="${cy - 3.5}" rx="28" ry="7.2" fill="${inner}"/>
          <path d="M${cx - R + 4} ${cy - 7} Q${cx - 12} ${cy - 13} ${cx + 6} ${cy - 13}" stroke="#fff" stroke-width="1.4" fill="none" opacity="0.55"/>${deco}`, BOWL_DEFS);
    }
    const r = rng(red ? 5 : 6);
    let spill = '';
    if (red) {
        for (let i = 0; i < 26; i++) {
            const x = cx - 40 + r() * 70, y = cy + 6 + r() * 14;
            spill += `<ellipse cx="${x.toFixed(1)}" cy="${y.toFixed(1)}" rx="2.6" ry="1.8" fill="${['#8a4a1c', '#a65e26', '#6e3812'][Math.floor(r() * 3)]}" transform="rotate(${Math.floor(r() * 180)} ${x.toFixed(1)} ${y.toFixed(1)})"/>`;
        }
    } else {
        spill = `<path d="M${cx - 48} ${cy + 14} q10 -12 34 -8 q20 -6 34 4 q14 12 -6 16 q-30 8 -50 0 q-16 -2 -12 -12 z" fill="url(#puddle)"/>`
            + `<ellipse cx="${cx - 18}" cy="${cy + 12}" rx="8" ry="1.6" fill="#fff" opacity="0.6"/>`;
    }
    const gx = cx + 22, gy = cy + 6;
    return svg(W, H, `${spill}
      <ellipse cx="${cx + 18}" cy="${cy + 15}" rx="26" ry="6" fill="#14300c" opacity="0.45" filter="url(#sh)"/>
      <g transform="rotate(-62 ${gx} ${gy})">
        <path d="M${gx - 24} ${cy - 2} L${gx - 19} ${cy + 9} Q${gx} ${cy + 15} ${gx + 19} ${cy + 9} L${gx + 24} ${cy - 2} Z" fill="url(#${body})"/>
        <ellipse cx="${gx}" cy="${cy - 2}" rx="24" ry="7" fill="url(#${rim})"/>
        <ellipse cx="${gx}" cy="${cy - 1.6}" rx="20" ry="5.2" fill="${red ? '#3a0c0c' : '#2a5a88'}"/>
      </g>`, BOWL_DEFS);
}

/* ---------- main qui caresse ---------- */

// Vue de côté, paume vers le bas, poignet à droite ; curl = doigts un peu repliés (2e image de la caresse)
export function hand(curl) {
    const fingers = [   // [x base, y base, longueur, épaisseur, angle]
        [40, 26, 30, 7.4, 4], [42, 32, 32, 7.8, 0], [44, 38, 29, 7.4, -4], [47, 43, 23, 6.4, -9],
    ];
    let fg = '';
    for (const [bx, by, L, w, a] of fingers) {
        const k = curl ? 0.82 : 1, ang = (a + (curl ? 14 : 0)) * Math.PI / 180;
        const tx = bx - Math.cos(ang) * L * k, ty = by + Math.sin(ang) * L * k + (curl ? 4 : 0);
        const mx = (bx + tx) / 2, my = (by + ty) / 2 + (curl ? 2 : 0.5);
        fg += `<path d="M${bx} ${by - w / 2} Q${mx.toFixed(1)} ${(my - w / 2 - 1).toFixed(1)} ${tx.toFixed(1)} ${(ty - w / 2 + 1.2).toFixed(1)} Q${(tx - 4).toFixed(1)} ${ty.toFixed(1)} ${tx.toFixed(1)} ${(ty + w / 2 - 0.4).toFixed(1)} Q${mx.toFixed(1)} ${(my + w / 2).toFixed(1)} ${bx} ${by + w / 2} Z" fill="url(#skin)" stroke="#b5785a" stroke-width="0.6"/>`
            + `<path d="M${(tx + 1.2).toFixed(1)} ${(ty - w / 2 + 2).toFixed(1)} q-3 ${(w / 2 - 2).toFixed(1)} 0 ${(w - 3.5).toFixed(1)}" stroke="#f6d6c8" stroke-width="1.6" fill="none" opacity="0.9"/>`
            + `<path d="M${(mx + 3).toFixed(1)} ${(my - w / 2 + 0.6).toFixed(1)} q-1.5 ${(w / 2).toFixed(1)} 0 ${(w - 1).toFixed(1)}" stroke="#c88a6a" stroke-width="0.6" fill="none" opacity="0.6"/>`;
    }
    const thumb = `<path d="M50 46 Q40 54 30 52 Q26 50 29 47 Q38 46 46 41 Z" fill="url(#skinDark)" stroke="#b5785a" stroke-width="0.6"/>`;
    const palm = `<path d="M40 22 Q58 16 80 20 L96 22 L96 50 L80 52 Q60 54 46 47 Q36 40 40 22 Z" fill="url(#skin)" stroke="#b5785a" stroke-width="0.7"/>
      <path d="M58 24 q4 8 2 18 M66 23 q3 9 1 20" stroke="#d6a088" stroke-width="0.8" fill="none" opacity="0.6"/>
      <rect x="92" y="18" width="18" height="36" rx="5" fill="url(#sleeve)"/>
      <path d="M92 20 v32" stroke="#2a4a8a" stroke-width="2"/>`;
    return svg(112, 64, `${thumb}${palm}${fg}`,
        `<linearGradient id="skin" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#f6cdb4"/><stop offset="0.6" stop-color="#e8b096"/><stop offset="1" stop-color="#d4967c"/></linearGradient>
         <linearGradient id="skinDark" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#e0a488"/><stop offset="1" stop-color="#c4866a"/></linearGradient>
         <linearGradient id="sleeve" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#4a74c8"/><stop offset="1" stop-color="#2a4a8a"/></linearGradient>`);
}

/* ---------- oiseaux ---------- */

// Moineau tourné vers la droite : 'up' / 'down' (vol, ailes hautes / basses), 'stand' (posé), 'peck' (picore)
export function bird(pose) {
    const wing = pose === 'up'
        ? `<path d="M15 12 Q12 2 4 0 Q10 6 11 12 Z" fill="url(#bw)" stroke="#4a3220" stroke-width="0.5"/>`
        : pose === 'down'
            ? `<path d="M15 13 Q10 22 3 24 Q9 17 11 13 Z" fill="url(#bw)" stroke="#4a3220" stroke-width="0.5"/>`
            : `<path d="M9 12 Q16 9 21 13 Q15 16 9 15 Z" fill="url(#bw)" stroke="#4a3220" stroke-width="0.5"/>`;
    const peck = pose === 'peck';
    const hx = peck ? 24 : 23, hy = peck ? 17 : 9;
    const legs = (pose === 'up' || pose === 'down') ? '' : `<path d="M14 17 l-1 5 M17 17 l1 5" stroke="#8a5a3a" stroke-width="0.9"/>`;
    return svg(30, 26, `${legs}
      <path d="M3 13 L9 12 L9 15 Z" fill="#6a4a30"/>
      <ellipse cx="15" cy="14" rx="8" ry="5" fill="url(#bb)"/>
      <circle cx="${hx - 2}" cy="${hy}" r="4.2" fill="#8a5a36"/>
      <circle cx="${hx - 1}" cy="${hy - 0.8}" r="0.9" fill="#111"/>
      <path d="M${hx + 2} ${hy - 0.5} l3 ${peck ? 2.5 : 1} l-3 1 z" fill="#e8b040"/>
      <path d="M${hx - 4} ${hy + 2.5} q2 1.5 4 0" stroke="#f4e8d8" stroke-width="1.2" fill="none"/>
      ${wing}`,
        `<radialGradient id="bb" cx="0.5" cy="0.6" r="0.6"><stop offset="0" stop-color="#e8d4b8"/><stop offset="1" stop-color="#a07850"/></radialGradient>
         <linearGradient id="bw" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#8a6040"/><stop offset="1" stop-color="#5a3c24"/></linearGradient>`);
}

/* ---------- besoins ---------- */

export function poop() {
    return svg(30, 22, `<ellipse cx="15" cy="19" rx="12" ry="2.6" fill="#14300c" opacity="0.4"/>
      <path d="M5 18 Q4 13 10 13 Q8 9 13 8 Q12 4 16 3 Q19 5 18 8 Q23 9 21 13 Q27 13 25 18 Z" fill="url(#pp)" stroke="#3a2412" stroke-width="0.6"/>
      <path d="M9 14 q4 2 9 0 M11 10 q3 1.5 6 0" stroke="#3a2412" stroke-width="0.6" fill="none" opacity="0.7"/>
      <ellipse cx="13" cy="7" rx="1.6" ry="0.8" fill="#fff" opacity="0.4"/>`,
        `<radialGradient id="pp" cx="0.4" cy="0.3" r="0.8"><stop offset="0" stop-color="#8a5a2c"/><stop offset="1" stop-color="#4a2c12"/></radialGradient>`);
}
