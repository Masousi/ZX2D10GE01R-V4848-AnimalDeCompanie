// ⚫ AJOUTÉ (25/10/2026) : accessoires des jeux propres à chaque espèce, des gestes (gronder, punir) et de la météo.
// Jeux : pelote de laine, canne à plume (tenue par la main), haie, carotte (tendue par la main), botte de foin,
// flaque de boue, brosse (tenue par la main), touffe d'herbe. Gestes : main qui gronde (index levé), main qui
// montre le coin, sablier (temps mort : la punition sans violence). Météo : goutte, éclaboussure, flocon, nuage sombre.
const svg = (w, h, body, defs = '') =>
    `<svg xmlns="http://www.w3.org/2000/svg" width="${w}" height="${h}" viewBox="0 0 ${w} ${h}"><defs>${defs}</defs>${body}</svg>`;

function rng(seed) {
    let s = seed >>> 0;
    return () => ((s = (s * 1664525 + 1013904223) >>> 0) / 4294967296);
}

const SKIN = `<linearGradient id="skin" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#f6cdb4"/><stop offset="0.6" stop-color="#e8b096"/><stop offset="1" stop-color="#d4967c"/></linearGradient>
  <linearGradient id="skinD" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#eab89c"/><stop offset="1" stop-color="#c4866a"/></linearGradient>
  <linearGradient id="sleeve" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#4a74c8"/><stop offset="1" stop-color="#2a4a8a"/></linearGradient>`;

// Poing vu de côté (doigts repliés), poignet à droite ; base commune des mains qui tiennent un objet
function fist(cx, cy) {
    let f = '';
    for (let i = 0; i < 4; i++) {   // phalanges repliées
        const y = cy - 9 + i * 6.2;
        f += `<path d="M${cx - 12} ${y} q-7 0.5 -7 3.1 q0 2.6 7 3.1 z" fill="url(#skinD)" stroke="#b5785a" stroke-width="0.6"/>`;
    }
    return `<path d="M${cx - 13} ${cy - 12} Q${cx - 2} ${cy - 16} ${cx + 18} ${cy - 13} L${cx + 26} ${cy - 12} L${cx + 26} ${cy + 14} L${cx + 14} ${cy + 15} Q${cx - 6} ${cy + 16} ${cx - 13} ${cy + 12} Z" fill="url(#skin)" stroke="#b5785a" stroke-width="0.7"/>`
        + f
        + `<path d="M${cx - 6} ${cy + 12} Q${cx - 14} ${cy + 8} ${cx - 15} ${cy + 2} Q${cx - 12} ${cy} ${cx - 4} ${cy + 4} Z" fill="url(#skin)" stroke="#b5785a" stroke-width="0.6"/>`
        + `<rect x="${cx + 24}" y="${cy - 15}" width="16" height="32" rx="5" fill="url(#sleeve)"/>`;
}

/* ---------- gestes ---------- */

// Gronder : poing, index levé vers le haut (agité par le firmware)
export function handScold() {
    const cx = 30, cy = 52;
    const finger = `<path d="M${cx - 10} ${cy - 12} L${cx - 13} ${cy - 44} Q${cx - 12} ${cy - 50} ${cx - 6} ${cy - 49} Q${cx - 2} ${cy - 47} ${cx - 2} ${cy - 42} L${cx - 1} ${cy - 13} Z" fill="url(#skin)" stroke="#b5785a" stroke-width="0.7"/>
      <path d="M${cx - 11.5} ${cy - 44} q3 -3 7 -1.5" stroke="#f8dccc" stroke-width="1.6" fill="none"/>
      <path d="M${cx - 12} ${cy - 30} q4 1.5 9 0 M${cx - 12} ${cy - 21} q4 1.5 9 0" stroke="#c88a6a" stroke-width="0.6" fill="none"/>`;
    return svg(72, 72, fist(cx, cy) + finger, SKIN);
}

// Punir : main qui montre le coin (index tendu vers la gauche)
export function handPoint() {
    const cx = 40, cy = 26;
    const finger = `<path d="M${cx - 12} ${cy - 11} L${cx - 38} ${cy - 9} Q${cx - 43} ${cy - 7.5} ${cx - 42} ${cy - 4} Q${cx - 41} ${cy - 1} ${cx - 37} ${cy - 1.5} L${cx - 12} ${cy - 3} Z" fill="url(#skin)" stroke="#b5785a" stroke-width="0.7"/>
      <path d="M${cx - 40} ${cy - 7} q1 3 3 4" stroke="#f8dccc" stroke-width="1.6" fill="none"/>`;
    return svg(84, 46, fist(cx, cy) + finger, SKIN);
}

// Sablier (temps mort), sable qui s'écoule
export function hourglass(t) {
    const top = 14 * (1 - t), bot = 14 * t;
    return svg(28, 40, `<rect x="2" y="1" width="24" height="4" rx="1.5" fill="#8a5a2c"/><rect x="2" y="35" width="24" height="4" rx="1.5" fill="#8a5a2c"/>
      <path d="M5 5 L23 5 Q23 14 15 20 Q23 26 23 35 L5 35 Q5 26 13 20 Q5 14 5 5 Z" fill="#dff2ff" fill-opacity="0.55" stroke="#8ab0c8" stroke-width="0.8"/>
      <path d="M${7 + (14 - top) * 0.4} ${20 - top} L${21 - (14 - top) * 0.4} ${20 - top} L14 20 Z" fill="#e8c068"/>
      <path d="M14 20 L14 ${35 - bot}" stroke="#e8c068" stroke-width="1"/>
      <path d="M6 35 Q14 ${35 - bot * 1.3} 22 35 Z" fill="#e8c068"/>`);
}

/* ---------- jeux ---------- */

export function yarn() {   // pelote de laine (chat)
    let w = '';
    for (let i = 0; i < 7; i++) w += `<path d="M${3 + i * 2.6} ${4 + i} Q12 ${12 - i} ${21 - i * 2.2} ${20 - i * 0.4}" stroke="#c23a6a" stroke-width="1" fill="none" opacity="0.8"/>`;
    return svg(28, 26, `<circle cx="12" cy="12" r="10" fill="url(#yg)"/>${w}<path d="M20 18 q5 2 7 7" stroke="#e05080" stroke-width="1.2" fill="none"/>`,
        `<radialGradient id="yg" cx="0.35" cy="0.3" r="0.75"><stop offset="0" stop-color="#ff8ab0"/><stop offset="1" stop-color="#c02a5a"/></radialGradient>`);
}

export function wand(swing) {   // main qui tient une canne à plume ; swing = la plume penche
    const tipx = 8 + swing * 10, tipy = 6;
    return svg(96, 74, `${fist(66, 50)}
      <path d="M58 46 L${tipx + 4} ${tipy + 8}" stroke="#8a6a3a" stroke-width="2.2" stroke-linecap="round"/>
      <path d="M${tipx + 4} ${tipy + 8} q-4 6 -6 14" stroke="#555" stroke-width="0.8" fill="none"/>
      <g transform="translate(${tipx - 2} ${tipy + 22}) rotate(${swing * 25})">
        <path d="M0 0 Q-7 7 -3 18 Q2 9 0 0 Z" fill="#2aa8d8"/><path d="M0 0 Q6 8 2 18 Q-1 9 0 0 Z" fill="#7a3ad8"/>
        <path d="M0 0 L0.5 18" stroke="#fff" stroke-width="0.6" opacity="0.7"/></g>`, SKIN);
}

export function hurdle() {   // haie de saut d'obstacle (cheval, âne, chèvre, lama)
    return svg(64, 46, `<ellipse cx="32" cy="43" rx="30" ry="3" fill="#14300c" opacity="0.4"/>
      <rect x="6" y="6" width="5" height="38" fill="#f4f0e8" stroke="#8a8478" stroke-width="0.6"/><rect x="53" y="6" width="5" height="38" fill="#f4f0e8" stroke="#8a8478" stroke-width="0.6"/>
      <rect x="4" y="12" width="56" height="5" rx="2" fill="url(#bar)"/><rect x="4" y="26" width="56" height="5" rx="2" fill="url(#bar)"/>`,
        `<linearGradient id="bar" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#e83a2a"/><stop offset="0.25" stop-color="#ffffff"/><stop offset="0.5" stop-color="#e83a2a"/><stop offset="0.75" stop-color="#ffffff"/><stop offset="1" stop-color="#e83a2a"/></linearGradient>`);
}

export function handCarrot() {   // main qui tend une carotte (cheval, âne, lapin, cochon)
    return svg(100, 64, `${fist(70, 34)}
      <path d="M57 26 L6 34 Q2 35 5 38 L57 40 Z" fill="url(#car)" stroke="#b8501a" stroke-width="0.6"/>
      <path d="M20 31 l3 4 M32 30 l2 5 M44 28 l2 6" stroke="#c0601e" stroke-width="0.7"/>
      <path d="M58 32 q8 -12 14 -16 M58 34 q10 -8 18 -8 M58 33 q6 -14 6 -22" stroke="#3a9a2a" stroke-width="2" fill="none" stroke-linecap="round"/>`,
        SKIN + `<linearGradient id="car" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#ffa040"/><stop offset="1" stop-color="#e06a10"/></linearGradient>`);
}

export function carrotBit() {   // morceau de carotte (dans la gueule)
    return svg(22, 10, `<path d="M20 2 L3 4 Q0 5 3 7 L20 8 Z" fill="#f08a2a" stroke="#b8501a" stroke-width="0.5"/>`);
}

export function hay() {   // botte de foin (la chèvre grimpe dessus)
    const r = rng(12);
    let st = '';
    for (let i = 0; i < 90; i++) {
        const x = 4 + r() * 72, y = 6 + r() * 34;
        st += `<path d="M${x.toFixed(1)} ${y.toFixed(1)} l${(r() * 8 - 4).toFixed(1)} ${(r() * 3).toFixed(1)}" stroke="${['#e8c868', '#c49a34', '#f2da8a'][Math.floor(r() * 3)]}" stroke-width="1" stroke-linecap="round"/>`;
    }
    return svg(80, 52, `<ellipse cx="40" cy="48" rx="40" ry="4" fill="#14300c" opacity="0.45"/>
      <rect x="2" y="4" width="76" height="42" rx="6" fill="url(#hy)"/>${st}
      <rect x="20" y="4" width="4" height="42" fill="#8a4a20"/><rect x="56" y="4" width="4" height="42" fill="#8a4a20"/>`,
        `<linearGradient id="hy" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#f0d37a"/><stop offset="1" stop-color="#b88a30"/></linearGradient>`);
}

export function mud() {   // flaque de boue (cochon)
    const r = rng(8);
    let b = '';
    for (let i = 0; i < 10; i++) b += `<circle cx="${(14 + r() * 92).toFixed(1)}" cy="${(10 + r() * 16).toFixed(1)}" r="${(1.5 + r() * 2.5).toFixed(1)}" fill="#7a5030" opacity="0.8"/>`;
    return svg(120, 36, `<path d="M6 18 Q10 4 40 6 Q70 0 96 6 Q118 10 114 22 Q110 34 70 32 Q30 36 12 30 Q2 26 6 18 Z" fill="url(#md)"/>${b}
      <ellipse cx="50" cy="13" rx="16" ry="2.5" fill="#fff" opacity="0.25"/>`,
        `<radialGradient id="md" cx="0.5" cy="0.5" r="0.6"><stop offset="0" stop-color="#5a3a1e"/><stop offset="1" stop-color="#7a5232"/></radialGradient>`);
}

export function handBrush(stroke) {   // main qui tient une brosse (cheval, âne, vache, mouton, lama)
    const dx = stroke ? -4 : 0;
    let bristles = '';
    for (let i = 0; i < 12; i++) bristles += `<path d="M${10 + i * 3.2 + dx} 44 l0 7" stroke="#3a2a1a" stroke-width="1.4"/>`;
    return svg(96, 58, `<rect x="${6 + dx}" y="34" width="44" height="11" rx="4" fill="url(#br)" stroke="#6a4020" stroke-width="0.7"/>${bristles}${fist(68, 32)}`,
        SKIN + `<linearGradient id="br" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#c8864a"/><stop offset="1" stop-color="#8a5228"/></linearGradient>`);
}

export function grass() {   // touffe d'herbe fraîche (brouter)
    const r = rng(5);
    let g = '';
    for (let i = 0; i < 26; i++) {
        const x = 6 + r() * 36, h = 10 + r() * 16, lean = (r() - 0.5) * 10;
        g += `<path d="M${x.toFixed(1)} 30 q${(lean / 2).toFixed(1)} ${(-h / 2).toFixed(1)} ${lean.toFixed(1)} ${(-h).toFixed(1)}" stroke="${['#4f9a2a', '#3f8a22', '#62b034', '#78c040', '#2f7a1c'][Math.floor(r() * 5)]}" stroke-width="1.8" fill="none" stroke-linecap="round"/>`;
    }
    return svg(48, 34, `<ellipse cx="24" cy="30" rx="20" ry="3.5" fill="#14300c" opacity="0.4"/>${g}
      <circle cx="16" cy="12" r="2" fill="#fff"/><circle cx="16" cy="12" r="0.9" fill="#f2c200"/><circle cx="33" cy="9" r="2" fill="#ffe45c"/>`);
}

/* ---------- météo ---------- */

export function drop() {
    return svg(4, 16, `<path d="M2 0 L2.8 13 Q2 16 1.2 13 Z" fill="#cfe6ff" opacity="0.85"/>`);
}

export function splash() {
    return svg(16, 8, `<path d="M2 7 q2 -5 4 -1 M8 7 l0 -5 M14 7 q-2 -5 -4 -1" stroke="#dcefff" stroke-width="1" fill="none" opacity="0.9"/><ellipse cx="8" cy="7" rx="6" ry="1.2" fill="#dcefff" opacity="0.5"/>`);
}

export function flake() {
    let a = '';
    for (let i = 0; i < 3; i++) a += `<path d="M5 0.5 L5 9.5" stroke="#fff" stroke-width="1" transform="rotate(${i * 60} 5 5)"/>`;
    return svg(10, 10, `${a}<circle cx="5" cy="5" r="1.4" fill="#fff"/>`);
}

export function cloudDark(seed, w, h) {
    const r = rng(seed);
    let c = '';
    for (let i = 0; i < 8; i++) {
        const x = w * (0.15 + 0.7 * r()), y = h * (0.42 + 0.25 * r()), rx = w * (0.13 + 0.12 * r()), ry = h * (0.28 + 0.2 * r());
        c += `<ellipse cx="${x.toFixed(1)}" cy="${y.toFixed(1)}" rx="${rx.toFixed(1)}" ry="${ry.toFixed(1)}"/>`;
    }
    return svg(w, h, `<g fill="#7d8594" filter="url(#b)">${c}<ellipse cx="${w / 2}" cy="${h * 0.7}" rx="${w * 0.44}" ry="${h * 0.2}"/></g>
      <g fill="#5d6472" opacity="0.7" filter="url(#b)"><ellipse cx="${w / 2}" cy="${h * 0.82}" rx="${w * 0.38}" ry="${h * 0.12}"/></g>`,
        `<filter id="b" x="-10%" y="-20%" width="120%" height="140%"><feGaussianBlur stdDeviation="2.2"/></filter>`);
}

// ⚫ AJOUTÉ (25/10/2026) : trou rebouché : terre fraîche tassée, un peu bombée (56 x 22), puis jeunes pousses
// de gazon qui la recouvrent peu à peu (le firmware fait disparaître la terre et les pousses au fil des heures)
export function dirt() {
    const r = rng(21);
    let crumbs = '';
    for (let i = 0; i < 26; i++) {
        const a = r() * Math.PI * 2, d = Math.sqrt(r()) * 20;
        const x = 28 + Math.cos(a) * d, y = 11 + Math.sin(a) * d * 0.34;
        crumbs += `<ellipse cx="${x.toFixed(1)}" cy="${y.toFixed(1)}" rx="${(1 + r() * 1.6).toFixed(1)}" ry="${(0.8 + r() * 0.8).toFixed(1)}" fill="${['#5e3a1e', '#8a5c38', '#6e4628', '#a07048'][Math.floor(r() * 4)]}"/>`;
    }
    return svg(56, 22, `<ellipse cx="28" cy="11.5" rx="24" ry="8.4" fill="#6a4426" filter="url(#e)"/>`
        + `<ellipse cx="28" cy="10" rx="19" ry="5.6" fill="url(#m)"/>${crumbs}`,
        `<radialGradient id="m" cx="0.45" cy="0.35" r="0.7"><stop offset="0" stop-color="#9a6a42"/><stop offset="1" stop-color="#6e4628"/></radialGradient>`
        + `<filter id="e" x="-10%" y="-30%" width="120%" height="160%"><feGaussianBlur stdDeviation="1.1"/></filter>`);
}
export function sprouts() {
    const r = rng(37);
    let b = '';
    for (let i = 0; i < 46; i++) {
        const a = r() * Math.PI * 2, d = Math.sqrt(r()) * 21;
        const x = 28 + Math.cos(a) * d, y = 13 + Math.sin(a) * d * 0.34, hgt = 3 + r() * 5, lean = (r() - 0.5) * 4;
        b += `<path d="M${x.toFixed(1)} ${y.toFixed(1)} q${(lean * 0.4).toFixed(1)} ${(-hgt * 0.6).toFixed(1)} ${lean.toFixed(1)} ${(-hgt).toFixed(1)}" stroke="${['#4f8a2a', '#5f9e34', '#3f7420', '#6fae3c'][Math.floor(r() * 4)]}" stroke-width="1.2" fill="none" stroke-linecap="round"/>`;
    }
    return svg(56, 22, b);
}

// ⚫ AJOUTÉ (25/10/2026) : cœur brisé (punition) : fendu, puis les deux moitiés qui s'écartent (44 x 40)
export function heartBroken(split) {
    const heart = 'M22 37 C10 28 2 21 2 12 C2 6 7 2 12 2 C16 2 19.5 4.5 22 8 C24.5 4.5 28 2 32 2 C37 2 42 6 42 12 C42 21 34 28 22 37 Z';
    const crack = 'M22 8 L18.5 15 L24 19 L19.5 26 L23 31 L22 37';
    const defs = `<radialGradient id="hb" cx="0.35" cy="0.3" r="0.8"><stop offset="0" stop-color="#ff7a8a"/><stop offset="1" stop-color="#c8102e"/></radialGradient>`
        + `<clipPath id="L"><path d="M0 0 H22 L18.5 15 L24 19 L19.5 26 L23 31 L22 40 H0 Z"/></clipPath>`
        + `<clipPath id="R"><path d="M44 0 H22 L18.5 15 L24 19 L19.5 26 L23 31 L22 40 H44 Z"/></clipPath>`;
    const shine = `<ellipse cx="12" cy="9" rx="5" ry="3" fill="#fff" opacity="0.45" transform="rotate(-30 12 9)"/>`;
    if (!split)
        return svg(44, 40, `<path d="${heart}" fill="url(#hb)" stroke="#7a0a1c" stroke-width="1"/>${shine}`
            + `<path d="${crack}" stroke="#4a0610" stroke-width="2" fill="none" stroke-linejoin="round"/>`, defs);
    return svg(44, 40, `<g transform="translate(-4 2) rotate(-12 12 20)" clip-path="url(#L)"><path d="${heart}" fill="url(#hb)" stroke="#7a0a1c" stroke-width="1"/>${shine}</g>`
        + `<g transform="translate(4 3) rotate(12 32 20)" clip-path="url(#R)"><path d="${heart}" fill="url(#hb)" stroke="#7a0a1c" stroke-width="1"/></g>`, defs);
}

// ⚫ AJOUTÉ (25/10/2026) : cône de signalisation de la piste (séparé du décor : il peut le renverser)
const CONE_DEF = `<linearGradient id="cg" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#ff8a3a"/><stop offset="0.5" stop-color="#ff6a10"/><stop offset="1" stop-color="#c84a00"/></linearGradient>`;
export function coneUp() {   // debout (22 x 26), base en bas au milieu
    const x = 11, y = 23;
    return svg(22, 26, `<ellipse cx="${x}" cy="${y + 1}" rx="9" ry="2.6" fill="#2a3a14" opacity="0.4"/>
    <path d="M${x - 8} ${y} L${x - 2} ${y - 20} L${x + 2} ${y - 20} L${x + 8} ${y} Z" fill="url(#cg)"/>
    <path d="M${x - 5.6} ${y - 7} L${x + 5.6} ${y - 7} L${x + 4.4} ${y - 11} L${x - 4.4} ${y - 11} Z" fill="#fff" opacity="0.9"/>
    <rect x="${x - 10}" y="${y - 1}" width="20" height="3" rx="1" fill="#e2550f"/>`, CONE_DEF);
}
export function coneDown() { // couché sur le flanc (30 x 16), base à gauche, pointe à droite
    return svg(30, 16, `<ellipse cx="15" cy="13.5" rx="13" ry="2.2" fill="#2a3a14" opacity="0.4"/>
    <path d="M4 1.5 L26 6.5 L26 9 L4 13.5 Z" fill="url(#cg)"/>
    <path d="M12 3.3 L16 4.2 L16 11.2 L12 12 Z" fill="#fff" opacity="0.9"/>
    <rect x="1" y="0.5" width="3.4" height="14" rx="1" fill="#e2550f"/>`, CONE_DEF);
}
