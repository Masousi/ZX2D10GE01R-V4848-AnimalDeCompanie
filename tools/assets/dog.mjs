// ⚫ AJOUTÉ (25/10/2026) : chien (chiot type golden retriever) vu de profil, tourné vers la droite.
// Personnage articulé : corps, cou, tête (museau, truffe, œil, oreille tombante), 4 pattes à deux segments,
// queue en panache, collier. Chaque pose = un jeu d'angles (POSES) ; toutes les images viennent du même modèle.
// Canevas W x H, sol à GROUND_Y, point d'ancrage (pied du chien) en (ANCHOR_X, GROUND_Y).

export const W = 200, H = 150, ANCHOR_X = 100, GROUND_Y = 142;

const D2R = Math.PI / 180;
const add = (a, b) => [a[0] + b[0], a[1] + b[1]];
const pol = (len, deg) => [len * Math.cos(deg * D2R), -len * Math.sin(deg * D2R)];   // 0° = avant, 90° = haut
// rotation d'un point du repère du corps (origine = hanche) dans le monde
const rot = (p, deg) => {
    const c = Math.cos(-deg * D2R), s = Math.sin(-deg * D2R);
    return [p[0] * c - p[1] * s, p[0] * s + p[1] * c];
};
const f = n => n.toFixed(1);

// Couleurs du pelage
const FUR = { back: '#b8772f', mid: '#dea253', belly: '#f2cf8e', cream: '#f8e6bf', ear: '#a9662a', dark: '#7a4a1c' };

function defs() {
    return `<defs>
    <linearGradient id="furBody" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="${FUR.back}"/><stop offset="0.45" stop-color="${FUR.mid}"/><stop offset="1" stop-color="${FUR.belly}"/>
    </linearGradient>
    <linearGradient id="furLeg" x1="0" y1="0" x2="1" y2="0">
      <stop offset="0" stop-color="${FUR.back}"/><stop offset="0.5" stop-color="${FUR.mid}"/><stop offset="1" stop-color="#c88a40"/>
    </linearGradient>
    <linearGradient id="furLegFar" x1="0" y1="0" x2="1" y2="0">
      <stop offset="0" stop-color="#9a5f24"/><stop offset="1" stop-color="#b57434"/>
    </linearGradient>
    <radialGradient id="furHead" cx="0.4" cy="0.35" r="0.75">
      <stop offset="0" stop-color="#ecb66a"/><stop offset="0.7" stop-color="${FUR.mid}"/><stop offset="1" stop-color="${FUR.back}"/>
    </radialGradient>
    <radialGradient id="muzzle" cx="0.45" cy="0.4" r="0.7">
      <stop offset="0" stop-color="#fbeccb"/><stop offset="1" stop-color="#e8c58a"/>
    </radialGradient>
    <linearGradient id="earGrad" x1="0" y1="0" x2="0.3" y2="1">
      <stop offset="0" stop-color="${FUR.ear}"/><stop offset="1" stop-color="#8a4f1c"/>
    </linearGradient>
    <linearGradient id="tailGrad" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="${FUR.back}"/><stop offset="1" stop-color="${FUR.belly}"/>
    </linearGradient>
    <radialGradient id="nose" cx="0.35" cy="0.3" r="0.8">
      <stop offset="0" stop-color="#6a5248"/><stop offset="0.5" stop-color="#2a1a14"/><stop offset="1" stop-color="#120a08"/>
    </radialGradient>
    <radialGradient id="eye" cx="0.45" cy="0.45" r="0.6">
      <stop offset="0" stop-color="#1a0c04"/><stop offset="0.55" stop-color="#3a1e0c"/><stop offset="1" stop-color="#6a3a16"/>
    </radialGradient>
    <linearGradient id="tongue" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#ff8a9a"/><stop offset="1" stop-color="#d84a62"/>
    </linearGradient>
    <filter id="dshadow" x="-30%" y="-100%" width="160%" height="300%"><feGaussianBlur stdDeviation="3"/></filter>
    <filter id="soft1" x="-20%" y="-20%" width="140%" height="140%"><feGaussianBlur stdDeviation="1.2"/></filter>
    <!-- texture de poils : bruit étiré, appliqué seulement à l'intérieur de la silhouette -->
    <filter id="fur" x="0" y="0" width="100%" height="100%" color-interpolation-filters="sRGB">
      <feTurbulence type="fractalNoise" baseFrequency="0.45 1.1" numOctaves="2" seed="8" result="n"/>
      <feColorMatrix in="n" type="matrix" values="0 0 0 0 0.72  0 0 0 0 0.52  0 0 0 0 0.3  0 0 0 -1.3 0.75" result="d"/>
      <feComposite in="d" in2="SourceAlpha" operator="in" result="dm"/>
      <feBlend in="dm" in2="SourceGraphic" mode="multiply"/>
    </filter>
  </defs>`;
}

// Patte : articulation `j`, angles des deux segments (0° = vertical vers le bas, + = vers l'avant)
function leg(j, a1, a2, L1, L2, w1, w2, w3, fill, pawFwd = 1) {
    const dir = a => [Math.sin(a * D2R), Math.cos(a * D2R)];
    const d1 = dir(a1), d2 = dir(a2);
    const k = [j[0] + d1[0] * L1, j[1] + d1[1] * L1];
    const p = [k[0] + d2[0] * L2, k[1] + d2[1] * L2];
    const n1 = [-d1[1], d1[0]], n2 = [-d2[1], d2[0]];
    const pts = [
        [j[0] + n1[0] * w1, j[1] + n1[1] * w1], [k[0] + n1[0] * w2, k[1] + n1[1] * w2], [p[0] + n2[0] * w3, p[1] + n2[1] * w3],
        [p[0] - n2[0] * w3, p[1] - n2[1] * w3], [k[0] - n1[0] * w2, k[1] - n1[1] * w2], [j[0] - n1[0] * w1, j[1] - n1[1] * w1],
    ];
    const path = `M${f(pts[0][0])} ${f(pts[0][1])} Q${f(k[0] + n1[0] * w2 * 1.15)} ${f(k[1] + n1[1] * w2 * 1.15)} ${f(pts[2][0])} ${f(pts[2][1])} `
        + `L${f(pts[3][0])} ${f(pts[3][1])} Q${f(k[0] - n1[0] * w2 * 1.15)} ${f(k[1] - n1[1] * w2 * 1.15)} ${f(pts[5][0])} ${f(pts[5][1])} Z`;
    const paw = `<ellipse cx="${f(p[0] + pawFwd * 2.5)}" cy="${f(p[1] + 1)}" rx="${f(w3 * 1.55)}" ry="${f(w3 * 0.95)}" fill="${fill === 'url(#furLegFar)' ? '#b9783a' : '#e9b875'}"/>`
        + `<path d="M${f(p[0] + pawFwd * 2)} ${f(p[1] - 1)} l${f(pawFwd * 2.2)} 2.4 M${f(p[0] + pawFwd * 4.5)} ${f(p[1] - 1.5)} l${f(pawFwd * 1.6)} 2.6" stroke="#8a5a24" stroke-width="0.6" opacity="0.6"/>`;
    return { svg: `<path d="${path}" fill="${fill}"/>${paw}`, paw: p };
}

// Patte arrière repliée (assis / couché) : grosse cuisse + jarret au sol
function foldedHind(hip, far) {
    const fill = far ? 'url(#furLegFar)' : 'url(#furLeg)';
    const t = `<ellipse cx="${f(hip[0] + 4)}" cy="${f(hip[1] - 2)}" rx="19" ry="17" fill="${far ? '#a8692c' : 'url(#furBody)'}" transform="rotate(-20 ${f(hip[0] + 4)} ${f(hip[1] - 2)})"/>`;
    const foot = `<path d="M${f(hip[0] - 4)} ${f(hip[1] + 8)} Q${f(hip[0] + 6)} ${f(GROUND_Y - 1)} ${f(hip[0] + 26)} ${f(GROUND_Y - 4)} L${f(hip[0] + 27)} ${f(GROUND_Y + 1)} Q${f(hip[0] + 4)} ${f(GROUND_Y + 3)} ${f(hip[0] - 12)} ${f(hip[1] + 12)} Z" fill="${fill}"/>`
        + `<ellipse cx="${f(hip[0] + 27)}" cy="${f(GROUND_Y - 2)}" rx="7" ry="4.2" fill="${far ? '#b9783a' : '#e9b875'}"/>`;
    return foot + t;
}

function tail(base, angle, wag, len = 38) {
    // ⚫ 25/10/2026 : panache fourni : forme pleine + mèches plus claires sur le dessous
    const dirBack = 180 - (angle + wag);   // direction de la queue (vers l'arrière)
    const p1 = add(base, pol(len * 0.5, dirBack - 18));
    const p2 = add(base, pol(len, dirBack + 2));
    const n = 7;
    // normale à la queue (côté "dessous" = vers le bas de l'image)
    const nx = Math.sin(dirBack * D2R), ny = Math.cos(dirBack * D2R);
    const sgn = ny >= 0 ? 1 : -1;
    const P = (p, k) => [p[0] + nx * k * sgn, p[1] + ny * k * sgn];
    const top0 = P(base, -n * 0.8), bot0 = P(base, n), topM = P(p1, -n * 0.7), botM = P(p1, n * 1.6);
    const d = `M${f(top0[0])} ${f(top0[1])} Q${f(topM[0])} ${f(topM[1])} ${f(p2[0])} ${f(p2[1])} Q${f(botM[0])} ${f(botM[1])} ${f(bot0[0])} ${f(bot0[1])} Z`;
    let fr = '';
    for (let i = 1; i <= 5; i++) {
        const t = i / 6;
        const q = [base[0] + (p2[0] - base[0]) * t, base[1] + (p2[1] - base[1]) * t];
        const q1 = P(q, n * 0.4), q2 = P(add(q, pol(5, dirBack)), n * 1.25 + 3 * t);
        fr += `M${f(q1[0])} ${f(q1[1])} Q${f((q1[0] + q2[0]) / 2 + 2)} ${f((q1[1] + q2[1]) / 2)} ${f(q2[0])} ${f(q2[1])} `;
    }
    // mèches dessinées sous le panache : seules leurs pointes dépassent
    return `<path d="${fr}" stroke="#e9bf7c" stroke-width="2.6" fill="none" stroke-linecap="round"/>`
        + `<path d="${d}" fill="url(#tailGrad)" stroke="#8a5522" stroke-width="0.6" stroke-opacity="0.5"/>`;
}

function eyeSvg(c, state) {
    const [x, y] = c;
    switch (state) {
    case 'closed':   // dort : paupière fermée vers le bas
        return `<path d="M${f(x - 5)} ${f(y)} Q${f(x)} ${f(y + 3.5)} ${f(x + 5)} ${f(y)}" stroke="#2a160a" stroke-width="1.6" fill="none" stroke-linecap="round"/>`;
    case 'happy':    // yeux plissés de bonheur
        return `<path d="M${f(x - 5)} ${f(y + 1.5)} Q${f(x)} ${f(y - 4)} ${f(x + 5)} ${f(y + 1.5)}" stroke="#2a160a" stroke-width="1.8" fill="none" stroke-linecap="round"/>`;
    case 'half':
        return `<ellipse cx="${f(x)}" cy="${f(y + 1)}" rx="4.6" ry="2.6" fill="url(#eye)"/>`
            + `<path d="M${f(x - 5.5)} ${f(y - 0.5)} Q${f(x)} ${f(y - 2)} ${f(x + 5.5)} ${f(y - 0.5)}" stroke="${FUR.dark}" stroke-width="1.6" fill="none"/>`
            + `<circle cx="${f(x + 1.5)}" cy="${f(y + 0.3)}" r="0.9" fill="#fff" opacity="0.8"/>`;
    default: {       // ouvert (open / sad)
        const brow = state === 'sad'
            ? `<path d="M${f(x - 5)} ${f(y - 6.5)} Q${f(x)} ${f(y - 9)} ${f(x + 5)} ${f(y - 9.5)}" stroke="${FUR.dark}" stroke-width="1.3" fill="none" stroke-linecap="round"/>`
            : `<path d="M${f(x - 5)} ${f(y - 7)} Q${f(x)} ${f(y - 9)} ${f(x + 5)} ${f(y - 7)}" stroke="${FUR.dark}" stroke-width="1.1" fill="none" opacity="0.6" stroke-linecap="round"/>`;
        return `<ellipse cx="${f(x)}" cy="${f(y)}" rx="5" ry="5.4" fill="#f4e4cc"/>`
            + `<ellipse cx="${f(x + 0.6)}" cy="${f(y + (state === 'sad' ? 0.8 : 0))}" rx="4.4" ry="4.8" fill="url(#eye)"/>`
            + `<circle cx="${f(x + 2)}" cy="${f(y - 1.8)}" r="1.5" fill="#fff"/><circle cx="${f(x - 1.2)}" cy="${f(y + 1.8)}" r="0.6" fill="#fff" opacity="0.7"/>`
            + `<path d="M${f(x - 5)} ${f(y - 2.5)} Q${f(x)} ${f(y - 6.6)} ${f(x + 5.2)} ${f(y - 2)}" stroke="#3a1e0c" stroke-width="1" fill="none"/>`
            + brow;
    }
    }
}

// Tête : centre `c`, inclinaison `a` (° , + = nez vers le haut), oreille `earA`
function head(c, a, P) {
    const R = p => add(c, rot(p, a));
    const muz = R([18, 7]), nose = R([31, 3]), eye = R([6, -5]), earJ = R([-8, -13]);
    let mouth;
    const m0 = R([29, 10]), m1 = R([21, 13]), m2 = R([12, 11]);
    if (P.mouth === 'open' || P.mouth === 'tongue' || P.mouth === 'eat') {
        const t1 = R([26, 14]), t2 = R([22, 25]), t3 = R([17, 15]);
        const tongue = P.mouth === 'eat'
            ? ''
            : `<path d="M${f(t3[0])} ${f(t3[1])} Q${f(t2[0] - 4)} ${f(t2[1] + 2)} ${f(t2[0])} ${f(t2[1])} Q${f(t2[0] + 5)} ${f(t2[1] - 1)} ${f(t1[0])} ${f(t1[1])} Z" fill="url(#tongue)"/>`
            + `<path d="M${f((t1[0] + t3[0]) / 2)} ${f((t1[1] + t3[1]) / 2 + 1)} L${f(t2[0])} ${f(t2[1] - 3)}" stroke="#b83a52" stroke-width="0.7" opacity="0.6"/>`;
        mouth = `<path d="M${f(m0[0])} ${f(m0[1])} Q${f(m1[0])} ${f(m1[1] + 6)} ${f(m2[0])} ${f(m2[1])} Q${f(m1[0])} ${f(m1[1] + 1)} ${f(m0[0])} ${f(m0[1])} Z" fill="#5a1a1a"/>${tongue}`;
    } else {
        mouth = `<path d="M${f(m0[0])} ${f(m0[1])} Q${f(m1[0])} ${f(m1[1] + (P.eye === 'sad' ? -1 : 2))} ${f(m2[0])} ${f(m2[1])}" stroke="#5a3418" stroke-width="1.3" fill="none" stroke-linecap="round"/>`;
    }
    // oreille tombante (goutte), pivot au sommet du crâne
    const earA = P.ear;   // 0 = pend verticalement, + = vers l'arrière (relevée), - = plaquée
    const ea = a + earA;
    const E = p => add(earJ, rot(p, ea));
    const e1 = E([-9, 4]), e2 = E([-6, 26]), e3 = E([4, 24]), e4 = E([8, 3]);
    const ear = `<path d="M${f(earJ[0])} ${f(earJ[1])} C${f(e1[0])} ${f(e1[1])} ${f(e2[0] - 3)} ${f(e2[1])} ${f(e2[0])} ${f(e2[1] + 1)} C${f(e3[0])} ${f(e3[1] + 3)} ${f(e4[0] + 2)} ${f(e4[1] + 8)} ${f(e4[0])} ${f(e4[1])} Z" fill="url(#earGrad)"/>`
        + `<path d="M${f(earJ[0] - 1)} ${f(earJ[1] + 6)} Q${f(e2[0] - 1)} ${f(e2[1] - 8)} ${f(e2[0] + 1)} ${f(e2[1] - 1)}" stroke="#7a4214" stroke-width="0.8" fill="none" opacity="0.5"/>`;
    const tr = `transform="rotate(${f(-a)} ${f(c[0])} ${f(c[1])})"`;
    const tear = P.tear
        ? `<path d="M${f(eye[0] + 1)} ${f(eye[1] + 5)} q-2.5 5 0 7 q2.5 -2 0 -7z" fill="#9ad8ff" stroke="#5aa8e0" stroke-width="0.5"/>` : '';
    return `
      <ellipse cx="${f(c[0])}" cy="${f(c[1])}" rx="21" ry="19" fill="url(#furHead)" stroke="#8a5522" stroke-width="0.5" stroke-opacity="0.3" ${tr}/>
      <ellipse cx="${f(muz[0])}" cy="${f(muz[1])}" rx="15" ry="10.5" fill="url(#muzzle)" transform="rotate(${f(-a - 6)} ${f(muz[0])} ${f(muz[1])})"/>
      <path d="M${f(R([10, -2])[0])} ${f(R([10, -2])[1])} Q${f(R([18, -4])[0])} ${f(R([18, -4])[1])} ${f(R([28, -2])[0])} ${f(R([28, -2])[1])}" stroke="#c88a40" stroke-width="1.4" fill="none" opacity="0.5"/>
      ${mouth}
      <ellipse cx="${f(nose[0])}" cy="${f(nose[1])}" rx="5.6" ry="4.4" fill="url(#nose)" transform="rotate(${f(-a)} ${f(nose[0])} ${f(nose[1])})"/>
      <ellipse cx="${f(nose[0] - 1)}" cy="${f(nose[1] - 1.6)}" rx="2" ry="1" fill="#fff" opacity="0.45"/>
      <g fill="#6a4020" opacity="0.5"><circle cx="${f(R([24, 5])[0])}" cy="${f(R([24, 5])[1])}" r="0.6"/><circle cx="${f(R([21, 7])[0])}" cy="${f(R([21, 7])[1])}" r="0.6"/><circle cx="${f(R([25, 8])[0])}" cy="${f(R([25, 8])[1])}" r="0.6"/></g>
      ${eyeSvg(eye, P.eye)}
      ${tear}
      ${ear}`;
}

// Silhouette du torse dans le repère du corps (origine = hanche, x vers l'avant)
// ⚫ 25/10/2026 : torse raccourci et arrondi (silhouette de chiot)
const TORSO = [
    ['M', [-20, -17]], ['C', [0, -26], [30, -27], [52, -23]], ['C', [72, -19], [78, -2], [68, 16]],
    ['C', [54, 26], [28, 20], [10, 19]], ['C', [-10, 23], [-27, 11], [-27, -3]], ['C', [-27, -11], [-25, -15], [-20, -17]],
];

function torso(hip, a) {
    const T = p => add(hip, rot(p, a));
    let d = '';
    for (const [cmd, ...pts] of TORSO) d += cmd + pts.map(p => T(p).map(f).join(' ')).join(' ') + ' ';
    const chest = [T([50, -6]), T([72, 2]), T([66, 17]), T([50, 19])];
    const chestD = `M${f(chest[0][0])} ${f(chest[0][1])} Q${f(chest[1][0])} ${f(chest[1][1])} ${f(chest[2][0])} ${f(chest[2][1])} Q${f(chest[3][0] + 4)} ${f(chest[3][1] + 3)} ${f(chest[3][0])} ${f(chest[3][1])} Z`;
    const hl = T([20, -19]);
    return `<path d="${d}Z" fill="url(#furBody)" stroke="#8a5522" stroke-width="0.8" stroke-opacity="0.55"/>`
        + `<path d="${chestD}" fill="${FUR.cream}" opacity="0.85"/>`
        + `<ellipse cx="${f(hl[0])}" cy="${f(hl[1])}" rx="30" ry="5" fill="#f6c27a" opacity="0.45" filter="url(#soft1)" transform="rotate(${f(-a)} ${f(hl[0])} ${f(hl[1])})"/>`;
}

/* ---------- poses ---------- */

const BASE = {
    hip: [64, 92], bodyA: 2, neckA: 62, neckL: 26, headA: -4, ear: 10, eye: 'open', mouth: 'closed',
    tail: 40, wag: 0, legs: { hf: [18, -14], hn: [12, -10], ff: [-2, 0], fn: [4, 2] }, mode: 'stand',
};

const P = o => ({ ...BASE, ...o, legs: { ...BASE.legs, ...(o.legs || {}) } });

// Galop : 4 images
const RUN = [
    { legs: { hf: [-25, 15], hn: [-35, 10], ff: [40, 30], fn: [55, 45] }, bodyA: 4, hip: [62, 88] },
    { legs: { hf: [10, -5], hn: [0, 0], ff: [20, 10], fn: [10, 0] }, bodyA: 0, hip: [62, 84] },
    { legs: { hf: [40, 20], hn: [50, 30], ff: [-30, -20], fn: [-40, -30] }, bodyA: -4, hip: [62, 88] },
    { legs: { hf: [25, -10], hn: [15, -20], ff: [-10, -5], fn: [0, 5] }, bodyA: 0, hip: [62, 90] },
];

export const POSES = {
    stand_1: P({ wag: 0, mouth: 'tongue' }),
    stand_2: P({ wag: 18, mouth: 'tongue' }),
    stand_3: P({ wag: -14, mouth: 'tongue' }),
    calm_1: P({ wag: 0, mouth: 'closed', tail: 20 }),
    calm_2: P({ wag: 8, mouth: 'closed', tail: 20 }),
    run_1: P({ ...RUN[0], tail: 30, mouth: 'tongue', ear: 45, neckA: 40, headA: -8 }),
    run_2: P({ ...RUN[1], tail: 34, mouth: 'tongue', ear: 55, neckA: 42, headA: -6 }),
    run_3: P({ ...RUN[2], tail: 26, mouth: 'tongue', ear: 40, neckA: 44, headA: -10 }),
    run_4: P({ ...RUN[3], tail: 30, mouth: 'tongue', ear: 50, neckA: 40, headA: -8 }),
    sit: P({ mode: 'sit', bodyA: 38, hip: [70, 120], neckA: 70, headA: 4, tail: -40, mouth: 'closed' }),
    sit_happy: P({ mode: 'sit', bodyA: 38, hip: [70, 120], neckA: 72, headA: 10, tail: -35, wag: 10, mouth: 'tongue', eye: 'open' }),
    sit_love: P({ mode: 'sit', bodyA: 38, hip: [70, 120], neckA: 75, headA: 18, tail: -30, mouth: 'tongue', eye: 'happy', ear: 25 }),
    sit_sad: P({ mode: 'sit', bodyA: 34, hip: [70, 121], neckA: 40, headA: -22, tail: -55, mouth: 'closed', eye: 'sad', ear: -8 }),
    sit_cry: P({ mode: 'sit', bodyA: 36, hip: [70, 121], neckA: 70, headA: 22, tail: -55, mouth: 'open', eye: 'closed', ear: -8, tear: true }),
    lie: P({ mode: 'lie', bodyA: 0, hip: [58, 120], neckA: 70, headA: 0, tail: -12, mouth: 'closed', eye: 'half' }),
    lie_sick: P({ mode: 'lie', bodyA: 0, hip: [58, 121], neckA: 18, neckL: 24, headA: -14, tail: -14, mouth: 'closed', eye: 'half', ear: -6 }),
    sleep_1: P({ mode: 'lie', bodyA: 0, hip: [58, 121], neckA: 10, neckL: 22, headA: -16, tail: -16, mouth: 'closed', eye: 'closed', ear: -4 }),
    sleep_2: P({ mode: 'lie', bodyA: 1.5, hip: [58, 120], neckA: 12, neckL: 22, headA: -15, tail: -16, mouth: 'closed', eye: 'closed', ear: -4 }),
    eat_1: P({ neckA: -58, neckL: 34, headA: -62, tail: 30, wag: 10, mouth: 'eat', eye: 'happy', bodyA: -6, legs: { ff: [12, 6], fn: [16, 8] } }),
    eat_2: P({ neckA: -62, neckL: 35, headA: -70, tail: 30, wag: -10, mouth: 'closed', eye: 'happy', bodyA: -7, legs: { ff: [12, 6], fn: [16, 8] } }),
    drink_1: P({ neckA: -58, neckL: 34, headA: -60, tail: 25, mouth: 'tongue', eye: 'half', bodyA: -6, legs: { ff: [12, 6], fn: [16, 8] } }),
    drink_2: P({ neckA: -61, neckL: 35, headA: -66, tail: 25, mouth: 'open', eye: 'half', bodyA: -7, legs: { ff: [12, 6], fn: [16, 8] } }),
    dig_1: P({ bodyA: -16, hip: [56, 86], neckA: 10, headA: -30, tail: 55, wag: 10, mouth: 'tongue', ear: 20, legs: { ff: [40, 10], fn: [-10, 20], hf: [20, -20], hn: [14, -16] } }),
    dig_2: P({ bodyA: -16, hip: [56, 86], neckA: 10, headA: -30, tail: 55, wag: -10, mouth: 'tongue', ear: 20, legs: { ff: [-5, 25], fn: [45, 5], hf: [20, -20], hn: [14, -16] } }),
    jump: P({ bodyA: 24, hip: [56, 82], neckA: 70, headA: 10, tail: 50, mouth: 'tongue', eye: 'happy', ear: 60, legs: { hf: [-40, -20], hn: [-50, -30], ff: [60, 20], fn: [70, 30] } }),
    guilty: P({ bodyA: -4, hip: [64, 98], neckA: 18, neckL: 22, headA: -24, tail: -55, mouth: 'closed', eye: 'sad', ear: -16, legs: { hf: [26, -24], hn: [22, -22], ff: [8, -6], fn: [12, -4] } }),
};

// Fond d'ancrage commun : on pose les pattes au sol en ajustant la hauteur de la hanche (mode stand)
function render(Pz) {
    const hip = Pz.hip.slice();
    const a = Pz.bodyA;
    const T = p => add(hip, rot(p, a));
    const shoulder = T([48, 6]);
    const hipJ = T([-4, 4]);
    const neckBase = T([56, -12]);
    const headC = add(neckBase, pol(Pz.neckL, Pz.neckA));
    const tailBase = T([-24, -8]);

    let far = '', near = '', back = '';
    const shadowW = Pz.mode === 'lie' ? 64 : 52;
    const shadow = `<ellipse cx="${f(hip[0] + 30)}" cy="${GROUND_Y + 1}" rx="${shadowW}" ry="6" fill="#0c2006" opacity="0.45" filter="url(#dshadow)"/>`;

    if (Pz.mode === 'stand') {
        const L = Pz.legs;
        const lf = leg(add(hipJ, [-2, 0]), L.hf[0], L.hf[1], 24, 25, 10, 5.8, 4, 'url(#furLegFar)');
        const ff = leg(add(shoulder, [-3, 0]), L.ff[0], L.ff[1], 22, 24, 9, 5.4, 4, 'url(#furLegFar)');
        const ln = leg(hipJ, L.hn[0], L.hn[1], 24, 25, 11, 6.2, 4.2, 'url(#furLeg)');
        const fn = leg(shoulder, L.fn[0], L.fn[1], 22, 24, 10, 5.6, 4.2, 'url(#furLeg)');
        far = lf.svg + ff.svg;
        near = `<ellipse cx="${f(hipJ[0] + 2)}" cy="${f(hipJ[1] + 2)}" rx="15" ry="19" fill="url(#furBody)" transform="rotate(${f(-a - 15)} ${f(hipJ[0] + 2)} ${f(hipJ[1] + 2)})"/>` + ln.svg + fn.svg;
    } else if (Pz.mode === 'sit') {
        const ffar = leg(add(shoulder, [-3, 0]), 4, 0, 18, GROUND_Y - shoulder[1] - 22, 8, 5, 3.6, 'url(#furLegFar)');
        const fnear = leg(shoulder, 8, 2, 18, GROUND_Y - shoulder[1] - 22, 9, 5.2, 3.8, 'url(#furLeg)');
        far = foldedHind(add(hipJ, [-3, 0]), true) + ffar.svg;
        near = foldedHind(hipJ, false) + fnear.svg;
    } else {   // lie
        const fl = leg(add(shoulder, [-2, 2]), 70, 88, 14, 22, 8, 5, 3.6, 'url(#furLegFar)');
        const fr = leg(add(shoulder, [6, 4]), 74, 90, 14, 24, 9, 5.2, 3.8, 'url(#furLeg)');
        far = foldedHind(add(hipJ, [-3, 0]), true) + fl.svg;
        near = foldedHind(hipJ, false) + fr.svg;
    }
    back = tail(tailBase, Pz.tail, Pz.wag, Pz.mode === 'stand' ? 38 : 30);

    // cou : bande épaisse entre le poitrail et la tête
    const nb1 = T([42, -20]), nb2 = T([68, 6]);
    const neck = `<path d="M${f(nb1[0])} ${f(nb1[1])} Q${f(headC[0] - 14)} ${f(headC[1] - 4)} ${f(headC[0] - 6)} ${f(headC[1] - 12)} L${f(headC[0] + 10)} ${f(headC[1] + 6)} Q${f(headC[0] + 2)} ${f(headC[1] + 18)} ${f(nb2[0])} ${f(nb2[1])} Z" fill="url(#furBody)"/>`;
    // collier rouge + médaille
    const c1 = add(headC, rot([-12, 6], Pz.headA * 0.5 + Pz.neckA - 70)), c2 = add(headC, rot([8, 16], Pz.headA * 0.5 + Pz.neckA - 70));
    const tag = add([(c1[0] + c2[0]) / 2, (c1[1] + c2[1]) / 2], [2, 5]);
    const collar = `<path d="M${f(c1[0])} ${f(c1[1])} L${f(c2[0])} ${f(c2[1])}" stroke="#c4202a" stroke-width="5" stroke-linecap="round"/>`
        + `<path d="M${f(c1[0])} ${f(c1[1] - 1.4)} L${f(c2[0])} ${f(c2[1] - 1.4)}" stroke="#ff6a6a" stroke-width="1" opacity="0.7"/>`
        + `<circle cx="${f(tag[0])}" cy="${f(tag[1])}" r="3.2" fill="#ffd24a" stroke="#b8860b" stroke-width="0.8"/>`;

    const body = back + far + torso(hip, a) + near + neck + collar + head(headC, Pz.headA, Pz);
    return `<svg xmlns="http://www.w3.org/2000/svg" width="${W}" height="${H}" viewBox="0 0 ${W} ${H}">${defs()}
      ${shadow}
      <g filter="url(#fur)">${body}</g>
      <g fill="none" stroke="${FUR.dark}" stroke-width="0.6" opacity="0.35">${''}</g>
    </svg>`;
}

export function dogSvg(name) {
    return render(POSES[name]);
}

// Position de la bouche (pour aligner la gamelle) : approximée depuis la pose
export function mouthOf(name) {
    const Pz = POSES[name];
    const T = p => add(Pz.hip, rot(p, Pz.bodyA));
    const headC = add(T([56, -12]), pol(Pz.neckL, Pz.neckA));
    const m = add(headC, rot([28, 10], Pz.headA));
    return [Math.round(m[0] - ANCHOR_X), Math.round(m[1] - GROUND_Y)];
}
