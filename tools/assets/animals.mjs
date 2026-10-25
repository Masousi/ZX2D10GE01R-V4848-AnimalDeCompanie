// ⚫ AJOUTÉ (25/10/2026) : animaux vus de profil, tournés vers la droite (remplace dog.mjs, chien seul).
// Un seul squelette articulé (corps, cou, tête, 4 pattes à deux segments, queue) et une seule liste de poses ;
// chaque espèce (SPECIES) règle les proportions, les couleurs et les attributs : pattes ou sabots, oreilles
// tombantes / pointues / latérales, cornes, barbiche, crinière, laine, moustaches, groin, forme de la queue…
// Les animaux de ferme ne s'assoient pas : les poses "assis" deviennent debout (ou couché pour les pleurs).
// Canevas propre à chaque espèce ; point d'ancrage = sol sous le corps, en (anchorX, groundY).
// ⚫ MODIFIÉ (25/10/2026) : + vache, âne, lapin, lama ; allures calculées (pas à 4 temps sur 8 images, galop sur
// 6 images, corps et tête qui oscillent) ; respiration au repos (4 images) ; patte levée (chat qui joue).

const D2R = Math.PI / 180;
const add = (a, b) => [a[0] + b[0], a[1] + b[1]];
const pol = (len, deg) => [len * Math.cos(deg * D2R), -len * Math.sin(deg * D2R)];   // 0° = avant, 90° = haut
const rot = (p, deg) => {   // rotation d'un point d'un repère local (corps, tête) dans le monde
    const c = Math.cos(-deg * D2R), s = Math.sin(-deg * D2R);
    return [p[0] * c - p[1] * s, p[0] * s + p[1] * c];
};
const f = n => n.toFixed(1);
const lerp = (a, b, t) => a + (b - a) * t;

/* ---------- espèces ---------- */

const DOG_HEAD = { rx: 21, ry: 19, muz: [18, 7, 15, 10.5], nose: [31, 3], eye: [6, -5], earJ: [-8, -13], mouth: [[29, 10], [21, 13], [12, 11]] };

export const SPECIES = {
    chien: {
        label: 'Chien', sound: 'Ouaf', canSit: true, scale: 1, legMul: 1, legW: 1, torso: [1, 1], neck: 0,
        fur: { back: '#b8772f', mid: '#dea253', belly: '#f2cf8e', cream: '#f8e6bf', ear: '#a9662a', dark: '#7a4a1c', far: ['#9a5f24', '#b57434'], paw: '#e9b875', pawFar: '#b9783a', headHi: '#ecb66a', muzzle: ['#fbeccb', '#e8c58a'] },
        head: DOG_HEAD, nose: 'dog', ears: 'floppy', tail: 'plume', eyeType: 'round', iris: '#6a3a16', foot: 'paw',
        collar: '#c4202a', tongue: true, furTex: true,
    },
    chat: {
        label: 'Chat', sound: 'Miaou', canSit: true, scale: 0.86, legMul: 1.02, legW: 0.78, torso: [1.04, 0.86], neck: -4,
        fur: { back: '#5c5f68', mid: '#878a94', belly: '#c9cad0', cream: '#f2f2f4', ear: '#6a6d76', dark: '#33353b', far: ['#4b4d55', '#62656e'], paw: '#e6e6ea', pawFar: '#8f929a', headHi: '#a2a5ae', muzzle: ['#f6f6f8', '#d8d9de'] },
        head: { rx: 18, ry: 16.5, muz: [13, 6, 9.5, 7.5], nose: [21, 2], eye: [7, -4], earJ: [-2, -14], mouth: [[20, 7], [15, 10], [10, 8]] },
        nose: 'cat', ears: 'pointy', tail: 'cat', eyeType: 'slit', iris: '#9ccc3a', foot: 'paw', whiskers: true, stripes: true,
        collar: '#2a7de1', bell: true, tongue: false, furTex: true,
    },
    mouton: {
        label: 'Mouton', sound: 'Bêêê', canSit: false, scale: 1.0, legMul: 0.86, legW: 0.62, torso: [1.06, 1.12], neck: -6,
        fur: { back: '#e9e2d0', mid: '#f4efe2', belly: '#ddd4be', cream: '#fbf8f0', ear: '#3a3532', dark: '#1e1b19', far: ['#2a2624', '#3a3532'], leg: ['#2e2a27', '#4a4440'], paw: '#2a2624', pawFar: '#1c1a18', headHi: '#4a4440', muzzle: ['#4a4440', '#2e2a27'] },
        head: { rx: 15, ry: 13, muz: [14, 7, 12, 9], nose: [24, 5], eye: [6, -3], earJ: [-4, -6], mouth: [[23, 10], [18, 12], [13, 11]] },
        darkHead: true, nose: 'sheep', ears: 'side', tail: 'tuft', eyeType: 'goat', iris: '#a8742a', foot: 'hoof', wool: true, tongue: false,
    },
    chevre: {
        label: 'Chèvre', sound: 'Mêêê', canSit: false, scale: 1.0, legMul: 1.08, legW: 0.66, torso: [1.0, 0.92], neck: 6,
        fur: { back: '#8a6a4a', mid: '#b89a74', belly: '#e6d8c0', cream: '#f4ecdc', ear: '#8a6a4a', dark: '#3a2a1c', far: ['#6e5236', '#856546'], paw: '#3a2e26', pawFar: '#2a201a', headHi: '#cdb08a', muzzle: ['#eadcc4', '#cbb490'] },
        head: { rx: 15, ry: 13, muz: [16, 8, 13, 8.5], nose: [28, 6], eye: [5, -3], earJ: [-4, -6], mouth: [[27, 11], [21, 13], [15, 12]] },
        nose: 'sheep', ears: 'side', horns: true, beard: true, tail: 'tuft', eyeType: 'goat', iris: '#d8a23a', foot: 'hoof', tongue: false, furTex: true,
    },
    cheval: {
        label: 'Cheval', sound: 'Hiiii', canSit: false, scale: 1.18, legMul: 1.42, legW: 0.78, torso: [1.16, 0.98], neck: 18,
        fur: { back: '#6a3a1c', mid: '#9a5a2c', belly: '#b97a48', cream: '#f4ece0', ear: '#6a3a1c', dark: '#24140a', far: ['#4e2a14', '#64361a'], paw: '#2a2420', pawFar: '#1e1a16', headHi: '#a86a3a', muzzle: ['#7a4a2a', '#4a2a16'] },
        head: { rx: 15, ry: 13, muz: [24, 10, 18, 9.5], nose: [38, 11], eye: [3, -4], earJ: [-6, -12], mouth: [[38, 16], [31, 18], [24, 17]] },
        nose: 'horse', ears: 'horse', tail: 'horse', mane: '#24140a', blaze: true, eyeType: 'round', iris: '#2a160a', foot: 'hoof', tongue: false, furTex: true,
    },
    cochon: {
        label: 'Cochon', sound: 'Groin', canSit: false, scale: 0.98, legMul: 0.66, legW: 0.95, torso: [1.08, 1.12], neck: -10,
        fur: { back: '#e89a9a', mid: '#f6b8b4', belly: '#fbd0c8', cream: '#fde2da', ear: '#e08a8c', dark: '#a04a52', far: ['#d0807e', '#e08e8c'], paw: '#8a5a52', pawFar: '#6e4640', headHi: '#fcc6c0', muzzle: ['#f8a8a8', '#e88a8e'] },
        head: { rx: 19, ry: 16, muz: [18, 7, 9, 9], nose: [24, 6], eye: [5, -5], earJ: [-2, -13], mouth: [[21, 12], [15, 14], [9, 12]] },
        nose: 'pig', ears: 'pigear', tail: 'curly', eyeType: 'round', iris: '#5a8ab0', foot: 'hoof', tongue: false,
    },
    // ⚫ AJOUTÉ (25/10/2026) : vache (Prim'Holstein : robe blanche à taches noires)
    vache: {
        label: 'Vache', sound: 'Meuh', canSit: false, scale: 1.16, legMul: 1.1, legW: 0.9, torso: [1.2, 1.12], neck: 2,
        fur: { back: '#e8e4dc', mid: '#f6f3ee', belly: '#e2dcd2', cream: '#ffffff', ear: '#2a2624', dark: '#1a1816', far: ['#cfc8bc', '#ddd6ca'], paw: '#2a2420', pawFar: '#1e1a16', headHi: '#ffffff', muzzle: ['#f4d4cc', '#e2b2aa'] },
        head: { rx: 17, ry: 14, muz: [18, 9, 12.5, 9.5], nose: [29, 10], eye: [3, -3], earJ: [-5, -8], mouth: [[28, 16], [23, 18], [17, 17]] },   // ⚫ 25/10/2026 : mufle plus petit
        nose: 'horse', ears: 'side', horns: 'cow', spots: true, tail: 'cow', eyeType: 'round', iris: '#2a160a', foot: 'hoof', tongue: false, furTex: true,
    },
    // ⚫ AJOUTÉ (25/10/2026) : âne (gris, grandes oreilles, museau blanc, crinière courte)
    ane: {
        label: 'Âne', sound: 'Hi-han', canSit: false, scale: 1.02, legMul: 1.18, legW: 0.74, torso: [1.06, 0.98], neck: 8,
        fur: { back: '#6e6a66', mid: '#8e8a84', belly: '#c8c2b8', cream: '#efebe4', ear: '#5e5a56', dark: '#2a2826', far: ['#55524e', '#686460'], paw: '#2a2624', pawFar: '#1e1c1a', headHi: '#a29e98', muzzle: ['#f2eee8', '#cfc8be'] },
        head: { rx: 15, ry: 13, muz: [22, 10, 16, 9.5], nose: [35, 11], eye: [3, -4], earJ: [-6, -12], mouth: [[35, 16], [28, 18], [22, 17]] },
        nose: 'horse', ears: 'long', earLen: 30, earTip: '#2a2826', tail: 'tuft', mane: '#3a3632', eyeType: 'round', iris: '#2a160a', foot: 'hoof', tongue: false, furTex: true,
    },
    // ⚫ AJOUTÉ (25/10/2026) : lapin (petit, longues oreilles dressées, queue en pompon)
    lapin: {
        label: 'Lapin', sound: 'Couic', canSit: true, scale: 0.62, legMul: 0.72, legW: 0.95, torso: [0.92, 1.08], neck: -10,
        fur: { back: '#9a7a5a', mid: '#bea078', belly: '#ece0cc', cream: '#fbf6ee', ear: '#a8886a', dark: '#4a3826', far: ['#86684a', '#9a7c5c'], paw: '#e8dcc8', pawFar: '#b8a488', headHi: '#d2b892', muzzle: ['#f6eee2', '#ddcdb4'] },
        head: { rx: 17, ry: 15, muz: [13, 6, 9, 7.5], nose: [20, 3], eye: [5, -4], earJ: [-4, -13], mouth: [[19, 8], [15, 10], [11, 9]] },
        nose: 'cat', ears: 'long', earLen: 34, tail: 'pompon', eyeType: 'round', iris: '#3a2210', foot: 'paw', whiskers: true, tongue: false, furTex: true,
    },
    // ⚫ AJOUTÉ (25/10/2026) : lama (long cou dressé, toison crème, oreilles en banane)
    lama: {
        label: 'Lama', sound: 'Mmmh', canSit: false, scale: 1.06, legMul: 1.25, legW: 0.7, torso: [0.98, 0.94], neck: 30,
        fur: { back: '#d8c4a4', mid: '#eadcc4', belly: '#f2e8d6', cream: '#fbf6ec', ear: '#c8b090', dark: '#5a4632', far: ['#bfa886', '#cdb898'], paw: '#4a3a2a', pawFar: '#3a2c20', headHi: '#f6ecda', muzzle: ['#f6eee0', '#dccab0'] },
        head: { rx: 13, ry: 12, muz: [16, 7, 11, 8], nose: [26, 6], eye: [4, -3], earJ: [-3, -11], mouth: [[25, 11], [20, 13], [15, 12]] },
        nose: 'sheep', ears: 'long', earLen: 20, banana: true, tail: 'tuft', eyeType: 'round', iris: '#2a160a', foot: 'hoof', tongue: false, furTex: true, fluffy: true,
    },
};

/* ---------- ⚫ AJOUTÉ (25/10/2026) : âges (bébé, adolescent, adulte) ---------- */
// Un petit n'est pas un adulte en réduction : tête plus grosse par rapport au corps, museau court, grands yeux,
// corps court et rond, queue courte, pas de cornes ni de barbiche. Les poulains, ânons, veaux, chevreaux,
// agneaux et petits lamas sont tout en jambes ; chiots, chatons, porcelets et lapereaux ont les pattes courtes.
// L'adolescent est presque à la taille adulte, élancé (pattes longues, corps fin), petites cornes.
export const STAGES = ['bebe', 'ado', 'adulte'];
const LEGGY = new Set(['cheval', 'ane', 'vache', 'chevre', 'mouton', 'lama']);
const AGE = [
    { scale: 0.56, head: 1.32, snout: 0.72, eye: 1.28, ear: 1.12, leg: 0.8, leggy: 1.02, legW: 1.12, torso: [0.8, 1.0], neck: -5, tail: 0.6, horn: 0 },
    { scale: 0.8, head: 1.1, snout: 0.9, eye: 1.08, ear: 1.05, leg: 1.0, leggy: 1.08, legW: 0.92, torso: [0.93, 0.9], neck: -1, tail: 0.85, horn: 0.55 },
    { scale: 1, head: 1, snout: 1, eye: 1, ear: 1, leg: 1, leggy: 1, legW: 1, torso: [1, 1], neck: 0, tail: 1, horn: 1 },
];
const ageCache = new Map();
export function speciesAt(sp, stage = 2) {
    const key = sp + stage;
    if (ageCache.has(key)) return ageCache.get(key);
    const S = SPECIES[sp], A = AGE[stage], H = S.head, hk = A.head, sn = A.snout;
    const out = {
        ...S,
        scale: S.scale * A.scale,
        legMul: S.legMul * (LEGGY.has(sp) ? A.leggy : A.leg),
        legW: S.legW * A.legW,
        torso: [S.torso[0] * A.torso[0], S.torso[1] * A.torso[1]],
        neck: S.neck * (stage === 0 ? 0.6 : 1) + A.neck,
        head: {
            ...H,
            rx: H.rx * hk, ry: H.ry * hk,
            muz: [H.muz[0] * hk * sn, H.muz[1] * hk, H.muz[2] * hk * (0.6 + 0.4 * sn), H.muz[3] * hk],
            nose: [H.nose[0] * hk * sn, H.nose[1] * hk],
            eye: [H.eye[0] * hk, H.eye[1] * hk],
            earJ: [H.earJ[0] * hk, H.earJ[1] * hk],
            mouth: H.mouth.map(m => [m[0] * hk * sn, m[1] * hk]),
        },
        eyeK: A.eye, earK: A.ear * (stage === 0 && S.ears === 'long' ? 0.9 : 1), tailK: A.tail, hornK: A.horn,
        horns: A.horn ? S.horns : false,
        beard: stage === 2 ? S.beard : false,
        mane: S.mane && stage === 0 ? S.fur.dark : S.mane,
    };
    ageCache.set(key, out);
    return out;
}
// Agrandit une partie (yeux, oreilles, cornes, queue) autour d'un point
const sc = (k, c, s) => (k === undefined || k === 1) ? s
    : `<g transform="translate(${f(c[0])} ${f(c[1])}) scale(${k}) translate(${f(-c[0])} ${f(-c[1])})">${s}</g>`;

/* ---------- canevas ---------- */

const BASE_W = 200, BASE_H = 150, BASE_AX = 100, BASE_GY = 142;
// Canevas agrandi pour les grandes espèces (cheval) : même ancrage relatif
// export function canvasOf(sp) {
//     const S = SPECIES[sp];
export function canvasOf(sp, stage = 2) {   // ⚫ MODIFIÉ (25/10/2026) : selon l'âge
    const S = speciesAt(sp, stage);
    const k = S.scale * Math.max(1, (0.55 + 0.45 * S.legMul));
    const W = Math.ceil(BASE_W * Math.max(1, S.scale) + (S.torso[0] - 1) * 60 + 20);
    const H = Math.ceil(BASE_H * k + 10);
    return { W, H, ax: Math.round(W / 2), gy: H - 8 };
}

/* ---------- définitions SVG (dégradés, filtres) ---------- */

function defs(S) {
    const F = S.fur;
    return `<defs>
    <linearGradient id="furBody" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="${F.back}"/><stop offset="0.45" stop-color="${F.mid}"/><stop offset="1" stop-color="${F.belly}"/>
    </linearGradient>
    <linearGradient id="furLeg" x1="0" y1="0" x2="1" y2="0">
      <stop offset="0" stop-color="${F.leg ? F.leg[0] : F.back}"/><stop offset="0.5" stop-color="${F.leg ? F.leg[1] : F.mid}"/><stop offset="1" stop-color="${F.leg ? F.leg[0] : F.back}"/>
    </linearGradient>
    <linearGradient id="furLegFar" x1="0" y1="0" x2="1" y2="0">
      <stop offset="0" stop-color="${F.far[0]}"/><stop offset="1" stop-color="${F.far[1]}"/>
    </linearGradient>
    <radialGradient id="furHead" cx="0.4" cy="0.35" r="0.75">
      <stop offset="0" stop-color="${F.headHi}"/><stop offset="0.7" stop-color="${S.darkHead ? F.ear : F.mid}"/><stop offset="1" stop-color="${S.darkHead ? F.dark : F.back}"/>
    </radialGradient>
    <radialGradient id="muzzle" cx="0.45" cy="0.4" r="0.7">
      <stop offset="0" stop-color="${F.muzzle[0]}"/><stop offset="1" stop-color="${F.muzzle[1]}"/>
    </radialGradient>
    <linearGradient id="earGrad" x1="0" y1="0" x2="0.3" y2="1">
      <stop offset="0" stop-color="${F.ear}"/><stop offset="1" stop-color="${F.dark}"/>
    </linearGradient>
    <linearGradient id="tailGrad" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="${F.back}"/><stop offset="1" stop-color="${F.belly}"/>
    </linearGradient>
    <linearGradient id="horn" x1="0" y1="0" x2="1" y2="1">
      <stop offset="0" stop-color="#e8dcc0"/><stop offset="1" stop-color="#8a7a5a"/>
    </linearGradient>
    <radialGradient id="nose" cx="0.35" cy="0.3" r="0.8">
      <stop offset="0" stop-color="#6a5248"/><stop offset="0.5" stop-color="#2a1a14"/><stop offset="1" stop-color="#120a08"/>
    </radialGradient>
    <radialGradient id="eye" cx="0.45" cy="0.45" r="0.6">
      <stop offset="0" stop-color="#1a0c04"/><stop offset="0.55" stop-color="${S.iris}"/><stop offset="1" stop-color="${S.iris}"/>
    </radialGradient>
    <linearGradient id="tongue" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#ff8a9a"/><stop offset="1" stop-color="#d84a62"/>
    </linearGradient>
    <radialGradient id="wool" cx="0.4" cy="0.35" r="0.7">
      <stop offset="0" stop-color="#ffffff"/><stop offset="0.7" stop-color="${F.mid}"/><stop offset="1" stop-color="${F.belly}"/>
    </radialGradient>
    <filter id="dshadow" x="-30%" y="-100%" width="160%" height="300%"><feGaussianBlur stdDeviation="3"/></filter>
    <filter id="soft1" x="-20%" y="-20%" width="140%" height="140%"><feGaussianBlur stdDeviation="1.2"/></filter>
    <filter id="fur" x="0" y="0" width="100%" height="100%" color-interpolation-filters="sRGB">
      <feTurbulence type="fractalNoise" baseFrequency="0.45 1.1" numOctaves="2" seed="8" result="n"/>
      <feColorMatrix in="n" type="matrix" values="0 0 0 0 0.72  0 0 0 0 0.52  0 0 0 0 0.3  0 0 0 -1.3 0.75" result="d"/>
      <feComposite in="d" in2="SourceAlpha" operator="in" result="dm"/>
      <feBlend in="dm" in2="SourceGraphic" mode="multiply"/>
    </filter>
    <filter id="woolTex" x="0" y="0" width="100%" height="100%" color-interpolation-filters="sRGB">
      <feTurbulence type="turbulence" baseFrequency="0.5" numOctaves="2" seed="4" result="n"/>
      <feColorMatrix in="n" type="matrix" values="0 0 0 0 0.75  0 0 0 0 0.7  0 0 0 0 0.6  0 0 0 -1.6 0.9" result="d"/>
      <feComposite in="d" in2="SourceAlpha" operator="in" result="dm"/>
      <feBlend in="dm" in2="SourceGraphic" mode="multiply"/>
    </filter>
  </defs>`;
}

/* ---------- parties du corps ---------- */

// Patte : articulation `j`, angles des deux segments (0° = vertical vers le bas, + = vers l'avant)
function leg(S, j, a1, a2, L1, L2, w1, w2, w3, far) {
    const fill = far ? 'url(#furLegFar)' : 'url(#furLeg)';
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
    let foot;
    if (S.foot === 'hoof') {   // sabot : trapèze sombre
        const h = w3 * 1.5;
        foot = `<path d="M${f(p[0] - w3 * 1.05)} ${f(p[1] - h)} L${f(p[0] + w3 * 1.05)} ${f(p[1] - h)} L${f(p[0] + w3 * 1.45)} ${f(p[1] + 1.2)} L${f(p[0] - w3 * 1.2)} ${f(p[1] + 1.2)} Z" fill="${far ? S.fur.pawFar : S.fur.paw}"/>`;
    } else {
        foot = `<ellipse cx="${f(p[0] + 2.5)}" cy="${f(p[1] + 1)}" rx="${f(w3 * 1.55)}" ry="${f(w3 * 0.95)}" fill="${far ? S.fur.pawFar : S.fur.paw}"/>`
            + `<path d="M${f(p[0] + 2)} ${f(p[1] - 1)} l2.2 2.4 M${f(p[0] + 4.5)} ${f(p[1] - 1.5)} l1.6 2.6" stroke="${S.fur.dark}" stroke-width="0.6" opacity="0.5"/>`;
    }
    return `<path d="${path}" fill="${fill}"/>${foot}`;
}

// Patte arrière repliée (assis / couché) : grosse cuisse + jarret au sol
function foldedHind(S, hip, far, gy) {
    const fill = far ? 'url(#furLegFar)' : 'url(#furLeg)';
    // la laine du mouton couvre déjà la cuisse
    const t = S.wool ? '' : `<ellipse cx="${f(hip[0] + 4)}" cy="${f(hip[1] - 2)}" rx="${f(19 * S.torso[1])}" ry="${f(17 * S.torso[1])}" fill="${S.wool ? 'url(#wool)' : far ? S.fur.far[1] : 'url(#furBody)'}" transform="rotate(-20 ${f(hip[0] + 4)} ${f(hip[1] - 2)})"/>`;
    const foot = `<path d="M${f(hip[0] - 4)} ${f(hip[1] + 8)} Q${f(hip[0] + 6)} ${f(gy - 1)} ${f(hip[0] + 26)} ${f(gy - 4)} L${f(hip[0] + 27)} ${f(gy + 1)} Q${f(hip[0] + 4)} ${f(gy + 3)} ${f(hip[0] - 12)} ${f(hip[1] + 12)} Z" fill="${fill}"/>`
        + (S.foot === 'hoof'
            ? `<path d="M${f(hip[0] + 24)} ${f(gy - 5)} l7 1 l1 5 l-9 0 z" fill="${far ? S.fur.pawFar : S.fur.paw}"/>`
            : `<ellipse cx="${f(hip[0] + 27)}" cy="${f(gy - 2)}" rx="7" ry="4.2" fill="${far ? S.fur.pawFar : S.fur.paw}"/>`);
    return foot + t;
}

function tail(S, base, angle, wag, len) {
    const dirBack = 180 - (angle + wag);
    const nx = Math.sin(dirBack * D2R), ny = Math.cos(dirBack * D2R);
    const sgn = ny >= 0 ? 1 : -1;
    const P = (p, k) => [p[0] + nx * k * sgn, p[1] + ny * k * sgn];
    switch (S.tail) {
    case 'cat': {   // longue queue fine en crosse
        const p1 = add(base, pol(len * 0.55, dirBack + 30)), p2 = add(base, pol(len * 1.05, dirBack + 55));
        const tip = add(p2, pol(8, dirBack - 30));
        return `<path d="M${f(base[0])} ${f(base[1])} Q${f(p1[0])} ${f(p1[1])} ${f(p2[0])} ${f(p2[1])} Q${f(tip[0] + 2)} ${f(tip[1] - 4)} ${f(tip[0])} ${f(tip[1])}" stroke="${S.fur.mid}" stroke-width="6" fill="none" stroke-linecap="round"/>`
            + `<path d="M${f(base[0])} ${f(base[1])} Q${f(p1[0])} ${f(p1[1])} ${f(p2[0])} ${f(p2[1])}" stroke="${S.fur.back}" stroke-width="2" fill="none" stroke-dasharray="3 4" opacity="0.7"/>`;
    }
    case 'tuft': {   // petite queue courte
        const p2 = add(base, pol(len * 0.32, dirBack + 40));
        return `<ellipse cx="${f((base[0] + p2[0]) / 2)}" cy="${f((base[1] + p2[1]) / 2)}" rx="${f(len * 0.2)}" ry="5" fill="${S.wool ? 'url(#wool)' : 'url(#tailGrad)'}" transform="rotate(${f(-(dirBack + 40))} ${f((base[0] + p2[0]) / 2)} ${f((base[1] + p2[1]) / 2)})"/>`;
    }
    case 'horse': {  // crins longs qui tombent
        let s = '';
        for (let i = 0; i < 6; i++) {
            const sw = (i - 2.5) * 2.2;
            const p1 = add(base, pol(14, dirBack + 30 + sw));
            const p2 = add(p1, [sw * 0.8 + (wag * 0.3), len * 1.1]);
            s += `<path d="M${f(base[0])} ${f(base[1] + i * 0.6)} Q${f(p1[0] - 4)} ${f(p1[1])} ${f(p2[0])} ${f(p2[1])}" stroke="${i % 2 ? S.mane : '#3a2414'}" stroke-width="3.2" fill="none" stroke-linecap="round"/>`;
        }
        return s;
    }
    case 'cow': {    // ⚫ AJOUTÉ (25/10/2026) : queue fine qui pend, touffe au bout
        const p1 = add(base, pol(10, dirBack + 40)), p2 = add(p1, [wag * 0.3, len * 0.9]);
        return `<path d="M${f(base[0])} ${f(base[1])} Q${f(p1[0] - 3)} ${f(p1[1])} ${f(p2[0])} ${f(p2[1])}" stroke="${S.fur.back}" stroke-width="2.6" fill="none" stroke-linecap="round"/>`
            + `<ellipse cx="${f(p2[0])}" cy="${f(p2[1] + 4)}" rx="3.2" ry="6" fill="${S.fur.dark}"/>`;
    }
    case 'pompon': { // ⚫ AJOUTÉ (25/10/2026) : lapin
        const c = add(base, pol(4, dirBack + 30));
        return `<circle cx="${f(c[0])}" cy="${f(c[1])}" r="7" fill="${S.fur.cream}" stroke="${S.fur.belly}" stroke-width="0.8"/>`;
    }
    case 'curly': {  // tire-bouchon
        const c = add(base, pol(6, dirBack + 35));
        return `<path d="M${f(base[0])} ${f(base[1])} q-6 -3 -5 -8 q1 -5 5 -3 q3 2 0 5 q-3 2 -4 -1" transform="translate(${f(c[0] - base[0])} ${f(c[1] - base[1])})" stroke="${S.fur.back}" stroke-width="2.6" fill="none" stroke-linecap="round"/>`;
    }
    default: {       // panache (chien)
        const p1 = add(base, pol(len * 0.5, dirBack - 18)), p2 = add(base, pol(len, dirBack + 2));
        const n = 7;
        const top0 = P(base, -n * 0.8), bot0 = P(base, n), topM = P(p1, -n * 0.7), botM = P(p1, n * 1.6);
        const d = `M${f(top0[0])} ${f(top0[1])} Q${f(topM[0])} ${f(topM[1])} ${f(p2[0])} ${f(p2[1])} Q${f(botM[0])} ${f(botM[1])} ${f(bot0[0])} ${f(bot0[1])} Z`;
        let fr = '';
        for (let i = 1; i <= 5; i++) {
            const t = i / 6;
            const q = [base[0] + (p2[0] - base[0]) * t, base[1] + (p2[1] - base[1]) * t];
            const q1 = P(q, n * 0.4), q2 = P(add(q, pol(5, dirBack)), n * 1.25 + 3 * t);
            fr += `M${f(q1[0])} ${f(q1[1])} Q${f((q1[0] + q2[0]) / 2 + 2)} ${f((q1[1] + q2[1]) / 2)} ${f(q2[0])} ${f(q2[1])} `;
        }
        return `<path d="${fr}" stroke="${S.fur.belly}" stroke-width="2.6" fill="none" stroke-linecap="round"/>`
            + `<path d="${d}" fill="url(#tailGrad)" stroke="${S.fur.dark}" stroke-width="0.6" stroke-opacity="0.5"/>`;
    }
    }
}

function eyeSvg(S, c, state) {
    const [x, y] = c;
    const lid = S.darkHead ? '#d8cfc0' : S.fur.dark;
    switch (state) {
    case 'closed':
        return `<path d="M${f(x - 5)} ${f(y)} Q${f(x)} ${f(y + 3.5)} ${f(x + 5)} ${f(y)}" stroke="${S.darkHead ? '#0e0c0a' : '#2a160a'}" stroke-width="1.6" fill="none" stroke-linecap="round"/>`;
    case 'happy':
        return `<path d="M${f(x - 5)} ${f(y + 1.5)} Q${f(x)} ${f(y - 4)} ${f(x + 5)} ${f(y + 1.5)}" stroke="${S.darkHead ? '#0e0c0a' : '#2a160a'}" stroke-width="1.8" fill="none" stroke-linecap="round"/>`;
    case 'half':
        return `<ellipse cx="${f(x)}" cy="${f(y + 1)}" rx="4.6" ry="2.6" fill="url(#eye)"/>`
            + `<path d="M${f(x - 5.5)} ${f(y - 0.5)} Q${f(x)} ${f(y - 2)} ${f(x + 5.5)} ${f(y - 0.5)}" stroke="${lid}" stroke-width="1.6" fill="none"/>`
            + `<circle cx="${f(x + 1.5)}" cy="${f(y + 0.3)}" r="0.9" fill="#fff" opacity="0.8"/>`;
    default: {
        const sad = state === 'sad';
        const brow = sad
            ? `<path d="M${f(x - 5)} ${f(y - 6.5)} Q${f(x)} ${f(y - 9)} ${f(x + 5)} ${f(y - 9.5)}" stroke="${lid}" stroke-width="1.3" fill="none" stroke-linecap="round"/>`
            : `<path d="M${f(x - 5)} ${f(y - 7)} Q${f(x)} ${f(y - 9)} ${f(x + 5)} ${f(y - 7)}" stroke="${lid}" stroke-width="1.1" fill="none" opacity="0.6" stroke-linecap="round"/>`;
        let pupil;
        if (S.eyeType === 'slit') pupil = `<ellipse cx="${f(x + 0.8)}" cy="${f(y + (sad ? 0.8 : 0))}" rx="1.2" ry="4" fill="#0a0604"/>`;
        else if (S.eyeType === 'goat') pupil = `<rect x="${f(x - 2.6)}" y="${f(y - 0.9 + (sad ? 0.8 : 0))}" width="5.6" height="1.8" rx="0.9" fill="#0a0604"/>`;
        else pupil = '';
        return `<ellipse cx="${f(x)}" cy="${f(y)}" rx="5" ry="5.4" fill="${S.eyeType === 'round' ? '#f4e4cc' : '#f6f0dc'}"/>`
            + `<ellipse cx="${f(x + 0.6)}" cy="${f(y + (sad ? 0.8 : 0))}" rx="4.4" ry="4.8" fill="url(#eye)"/>`
            + pupil
            + `<circle cx="${f(x + 2)}" cy="${f(y - 1.8)}" r="1.5" fill="#fff"/><circle cx="${f(x - 1.2)}" cy="${f(y + 1.8)}" r="0.6" fill="#fff" opacity="0.7"/>`
            + `<path d="M${f(x - 5)} ${f(y - 2.5)} Q${f(x)} ${f(y - 6.6)} ${f(x + 5.2)} ${f(y - 2)}" stroke="#2a1608" stroke-width="1" fill="none"/>`
            + brow;
    }
    }
}

function earSvg(S, earJ, a, earA) {
    const ea = a + earA;
    const E = p => add(earJ, rot(p, ea));
    const F = S.fur;
    switch (S.ears) {
    case 'pointy': {   // chat : deux triangles dressés
        let s = '';
        for (const [dx, far] of [[-9, true], [4, false]]) {
            const b0 = E([dx - 6, 2]), b1 = E([dx + 7, 3]), tip = E([dx - 1, -15 + earA * 0.05]);
            const i0 = E([dx - 3, 1]), i1 = E([dx + 4, 2]), it = E([dx - 0.5, -10]);
            s += `<path d="M${f(b0[0])} ${f(b0[1])} L${f(tip[0])} ${f(tip[1])} L${f(b1[0])} ${f(b1[1])} Z" fill="${far ? F.far[0] : F.ear}"/>`
                + `<path d="M${f(i0[0])} ${f(i0[1])} L${f(it[0])} ${f(it[1])} L${f(i1[0])} ${f(i1[1])} Z" fill="#f0a8b0" opacity="${far ? 0.5 : 0.85}"/>`;
        }
        return s;
    }
    case 'long': {     // ⚫ AJOUTÉ (25/10/2026) : âne, lapin, lama : longues oreilles dressées
        const L = S.earLen || 28, bend = S.banana ? 7 : 2;
        let out = '';
        for (const [dx, far] of [[-5, true], [3, false]]) {
            const b0 = E([dx - 4, 2]), b1 = E([dx + 4, 2]), tip = E([dx - 3 - bend * 0.4, -L]);
            const c0 = E([dx - 6 - bend, -L * 0.55]), c1 = E([dx + 5 - bend * 0.3, -L * 0.6]);
            const i0 = E([dx - 2, 0]), i1 = E([dx + 2, 0]), it = E([dx - 3 - bend * 0.4, -L * 0.85]);
            out += `<path d="M${f(b0[0])} ${f(b0[1])} Q${f(c0[0])} ${f(c0[1])} ${f(tip[0])} ${f(tip[1])} Q${f(c1[0])} ${f(c1[1])} ${f(b1[0])} ${f(b1[1])} Z" fill="${far ? F.far[0] : 'url(#earGrad)'}"/>`
                + `<path d="M${f(i0[0])} ${f(i0[1])} Q${f((i0[0] + it[0]) / 2 - 1)} ${f((i0[1] + it[1]) / 2)} ${f(it[0])} ${f(it[1])} Q${f((i1[0] + it[0]) / 2 + 1)} ${f((i1[1] + it[1]) / 2)} ${f(i1[0])} ${f(i1[1])} Z" fill="#e8a8a8" opacity="${far ? 0.4 : 0.75}"/>`
                + (S.earTip ? `<circle cx="${f(tip[0])}" cy="${f(tip[1] + 3)}" r="3" fill="${S.earTip}"/>` : '');
        }
        return out;
    }
    case 'horse': {    // oreilles fines dressées
        const b0 = E([-4, 2]), b1 = E([5, 2]), tip = E([0, -16]);
        return `<path d="M${f(b0[0])} ${f(b0[1])} Q${f(tip[0] - 4)} ${f(tip[1] + 6)} ${f(tip[0])} ${f(tip[1])} Q${f(tip[0] + 4)} ${f(tip[1] + 6)} ${f(b1[0])} ${f(b1[1])} Z" fill="url(#earGrad)"/>`;
    }
    case 'side': {     // mouton / chèvre : oreille horizontale
        const e1 = E([-4, -3]), e2 = E([-20, 2]), e3 = E([-4, 5]);
        return `<path d="M${f(earJ[0])} ${f(earJ[1])} Q${f(e1[0])} ${f(e1[1])} ${f(e2[0])} ${f(e2[1])} Q${f(e3[0])} ${f(e3[1])} ${f(earJ[0])} ${f(earJ[1] + 3)} Z" fill="url(#earGrad)"/>`
            + `<path d="M${f(earJ[0] - 2)} ${f(earJ[1] + 1)} L${f(e2[0] + 4)} ${f(e2[1])}" stroke="#f0b0a8" stroke-width="1.4" opacity="0.6"/>`;
    }
    case 'pigear': {   // cochon : oreille triangulaire rabattue vers l'avant
        const b0 = E([-6, 0]), b1 = E([6, 0]), tip = E([10, 8]);
        return `<path d="M${f(b0[0])} ${f(b0[1])} Q${f(b0[0] + 2)} ${f(b0[1] - 10)} ${f(b1[0])} ${f(b1[1] - 6)} L${f(tip[0])} ${f(tip[1])} Z" fill="url(#earGrad)"/>`;
    }
    default: {         // chien : oreille tombante
        const e1 = E([-9, 4]), e2 = E([-6, 26]), e3 = E([4, 24]), e4 = E([8, 3]);
        return `<path d="M${f(earJ[0])} ${f(earJ[1])} C${f(e1[0])} ${f(e1[1])} ${f(e2[0] - 3)} ${f(e2[1])} ${f(e2[0])} ${f(e2[1] + 1)} C${f(e3[0])} ${f(e3[1] + 3)} ${f(e4[0] + 2)} ${f(e4[1] + 8)} ${f(e4[0])} ${f(e4[1])} Z" fill="url(#earGrad)"/>`
            + `<path d="M${f(earJ[0] - 1)} ${f(earJ[1] + 6)} Q${f(e2[0] - 1)} ${f(e2[1] - 8)} ${f(e2[0] + 1)} ${f(e2[1] - 1)}" stroke="${F.dark}" stroke-width="0.8" fill="none" opacity="0.5"/>`;
    }
    }
}

// Tête : centre `c`, inclinaison `a` (° , + = nez vers le haut)
function head(S, c, a, P) {
    const H = S.head;
    const R = p => add(c, rot(p, a));
    const muz = R([H.muz[0], H.muz[1]]), nose = R(H.nose), eye = R(H.eye), earJ = R(H.earJ);
    const [m0, m1, m2] = H.mouth.map(R);
    const F = S.fur;
    let mouth;
    const open = P.mouth === 'open' || P.mouth === 'eat' || (P.mouth === 'tongue' && S.tongue);
    if (open) {
        const t1 = R([H.mouth[0][0] - 3, H.mouth[0][1] + 4]), t2 = R([H.mouth[1][0] + 1, H.mouth[1][1] + 12]), t3 = R([H.mouth[2][0] + 5, H.mouth[2][1] + 4]);
        const tongue = (P.mouth === 'eat' || !S.tongue) ? ''
            : `<path d="M${f(t3[0])} ${f(t3[1])} Q${f(t2[0] - 4)} ${f(t2[1] + 2)} ${f(t2[0])} ${f(t2[1])} Q${f(t2[0] + 5)} ${f(t2[1] - 1)} ${f(t1[0])} ${f(t1[1])} Z" fill="url(#tongue)"/>`;
        mouth = `<path d="M${f(m0[0])} ${f(m0[1])} Q${f(m1[0])} ${f(m1[1] + 6)} ${f(m2[0])} ${f(m2[1])} Q${f(m1[0])} ${f(m1[1] + 1)} ${f(m0[0])} ${f(m0[1])} Z" fill="#5a1a1a"/>${tongue}`;
    } else {
        mouth = `<path d="M${f(m0[0])} ${f(m0[1])} Q${f(m1[0])} ${f(m1[1] + (P.eye === 'sad' ? -1 : 2))} ${f(m2[0])} ${f(m2[1])}" stroke="${S.darkHead ? '#0e0c0a' : '#5a3418'}" stroke-width="1.3" fill="none" stroke-linecap="round"/>`;
    }

    let noseSvg;
    switch (S.nose) {
    case 'cat':
        noseSvg = `<path d="M${f(nose[0] - 2.6)} ${f(nose[1] - 1.6)} L${f(nose[0] + 2.6)} ${f(nose[1] - 1.6)} L${f(nose[0])} ${f(nose[1] + 1.8)} Z" fill="#e88a9a"/>`;
        break;
    case 'pig': {   // groin : disque rose et 2 narines
        const n2 = R([H.nose[0] + 6, H.nose[1]]);
        noseSvg = `<ellipse cx="${f(n2[0])}" cy="${f(n2[1])}" rx="4.5" ry="7.5" fill="#f08a96" stroke="#c06070" stroke-width="0.8" transform="rotate(${f(-a)} ${f(n2[0])} ${f(n2[1])})"/>`
            + `<ellipse cx="${f(R([H.nose[0] + 7, H.nose[1] - 2.5])[0])}" cy="${f(R([H.nose[0] + 7, H.nose[1] - 2.5])[1])}" rx="1.1" ry="1.8" fill="#8a3a48"/>`
            + `<ellipse cx="${f(R([H.nose[0] + 7, H.nose[1] + 2.5])[0])}" cy="${f(R([H.nose[0] + 7, H.nose[1] + 2.5])[1])}" rx="1.1" ry="1.8" fill="#8a3a48"/>`;
        break;
    }
    case 'horse':
    case 'sheep':   // naseau
        noseSvg = `<ellipse cx="${f(nose[0])}" cy="${f(nose[1])}" rx="2.4" ry="1.6" fill="#140c08" transform="rotate(${f(-a - 30)} ${f(nose[0])} ${f(nose[1])})"/>`;
        break;
    default:
        noseSvg = `<ellipse cx="${f(nose[0])}" cy="${f(nose[1])}" rx="5.6" ry="4.4" fill="url(#nose)" transform="rotate(${f(-a)} ${f(nose[0])} ${f(nose[1])})"/>`
            + `<ellipse cx="${f(nose[0] - 1)}" cy="${f(nose[1] - 1.6)}" rx="2" ry="1" fill="#fff" opacity="0.45"/>`;
    }

    const tr = `transform="rotate(${f(-a)} ${f(c[0])} ${f(c[1])})"`;
    const tear = P.tear ? `<path d="M${f(eye[0] + 1)} ${f(eye[1] + 5)} q-2.5 5 0 7 q2.5 -2 0 -7z" fill="#9ad8ff" stroke="#5aa8e0" stroke-width="0.5"/>` : '';
    let extra = '';
    if (S.whiskers) {
        const w0 = R([H.nose[0] - 3, H.nose[1] + 3]);
        for (const [dx, dy] of [[14, -3], [15, 1], [13, 5]]) {
            const w1 = R([H.nose[0] + dx, H.nose[1] + 3 + dy]);
            extra += `<path d="M${f(w0[0])} ${f(w0[1])} L${f(w1[0])} ${f(w1[1])}" stroke="#f4f4f6" stroke-width="0.6" opacity="0.9"/>`;
        }
    }
    if (S.beard) {   // barbiche : quelques mèches sous le menton
        for (const [dx, len] of [[-4, 11], [-1, 13], [2, 10]]) {
            const b0 = R([H.mouth[1][0] + dx, H.mouth[1][1] + 1]), b1 = R([H.mouth[1][0] + dx - 3, H.mouth[1][1] + len]);
            extra += `<path d="M${f(b0[0])} ${f(b0[1])} Q${f(b0[0] + 1)} ${f((b0[1] + b1[1]) / 2)} ${f(b1[0])} ${f(b1[1])}" stroke="${F.back}" stroke-width="2.2" fill="none" stroke-linecap="round"/>`;
        }
    }
    if (S.blaze) {   // liste blanche sur le chanfrein
        const z0 = R([4, -9]), z1 = R([H.nose[0] - 4, H.nose[1] - 3]);
        extra += `<path d="M${f(z0[0])} ${f(z0[1])} L${f(z1[0])} ${f(z1[1])}" stroke="${F.cream}" stroke-width="3.2" stroke-linecap="round" opacity="0.9"/>`;
    }
    let horns = '';
    if (S.horns === 'cow') {   // ⚫ AJOUTÉ (25/10/2026) : petites cornes de vache, vers l'avant
        const h0 = R([-1, -12]), h1 = R([4, -22]), h2 = R([10, -20]);
        horns = `<path d="M${f(h0[0])} ${f(h0[1])} Q${f(h1[0])} ${f(h1[1])} ${f(h2[0])} ${f(h2[1])}" stroke="url(#horn)" stroke-width="4" fill="none" stroke-linecap="round"/>`;
    } else if (S.horns) {   // cornes recourbées vers l'arrière
        const h0 = R([-2, -12]), h1 = R([-16, -22]), h2 = R([-24, -12]);
        horns = `<path d="M${f(h0[0])} ${f(h0[1])} Q${f(h1[0])} ${f(h1[1])} ${f(h2[0])} ${f(h2[1])}" stroke="url(#horn)" stroke-width="5" fill="none" stroke-linecap="round"/>`
            + `<path d="M${f(h0[0])} ${f(h0[1])} Q${f(h1[0])} ${f(h1[1])} ${f(h2[0])} ${f(h2[1])}" stroke="#6a5a3a" stroke-width="5" fill="none" stroke-dasharray="1 2.5" opacity="0.4"/>`;
    }
    if (horns) horns = sc(S.hornK, R([-1, -12]), horns);   // ⚫ AJOUTÉ (25/10/2026) : petites cornes de l'adolescent
    let woolTop = '';
    if (S.wool) {    // toupet de laine
        for (const [dx, dy, r] of [[-8, -11, 6], [-2, -14, 6], [-12, -5, 5]]) {
            const q = R([dx, dy]);
            woolTop += `<circle cx="${f(q[0])}" cy="${f(q[1])}" r="${r}" fill="url(#wool)"/>`;
        }
    }
    const stripes = S.stripes
        ? `<g stroke="${F.dark}" stroke-width="1.4" opacity="0.45" fill="none">${[[-4, -12, 2, -5], [1, -14, 5, -7], [-9, -9, -4, -3]].map(([x1, y1, x2, y2]) => {
            const p1 = R([x1, y1]), p2 = R([x2, y2]); return `<path d="M${f(p1[0])} ${f(p1[1])} L${f(p2[0])} ${f(p2[1])}"/>`; }).join('')}</g>` : '';
    // ⚫ MODIFIÉ (25/10/2026) : yeux et oreilles agrandis chez le petit (sc)
    return `
      ${S.ears === 'pointy' || S.ears === 'horse' || S.ears === 'long' ? sc(S.earK, earJ, earSvg(S, earJ, a, P.ear)) : ''}
      ${horns}
      <ellipse cx="${f(c[0])}" cy="${f(c[1])}" rx="${H.rx}" ry="${H.ry}" fill="url(#furHead)" stroke="${F.dark}" stroke-width="0.5" stroke-opacity="0.3" ${tr}/>
      <ellipse cx="${f(muz[0])}" cy="${f(muz[1])}" rx="${H.muz[2]}" ry="${H.muz[3]}" fill="url(#muzzle)" transform="rotate(${f(-a - 6)} ${f(muz[0])} ${f(muz[1])})"/>
      ${stripes}
      ${mouth}
      ${noseSvg}
      ${extra}
      ${sc(S.eyeK, eye, eyeSvg(S, eye, P.eye))}
      ${tear}
      ${woolTop}
      ${S.ears === 'pointy' || S.ears === 'horse' || S.ears === 'long' ? '' : sc(S.earK, earJ, earSvg(S, earJ, a, P.ear))}`;
}

// Silhouette du torse (repère du corps : origine = hanche, x vers l'avant)
const TORSO = [
    ['M', [-20, -17]], ['C', [0, -26], [30, -27], [52, -23]], ['C', [72, -19], [78, -2], [68, 16]],
    ['C', [54, 26], [28, 20], [10, 19]], ['C', [-10, 23], [-27, 11], [-27, -3]], ['C', [-27, -11], [-25, -15], [-20, -17]],
];

function torso(S, hip, a) {
    const [sx, sy] = S.torso;
    const T = p => add(hip, rot([p[0] * sx, p[1] * sy], a));
    let d = '';
    for (const [cmd, ...pts] of TORSO) d += cmd + pts.map(p => T(p).map(f).join(' ')).join(' ') + ' ';
    const F = S.fur;
    if (S.wool) {   // laine : silhouette bosselée + texture
        let bumps = '';
        for (let i = 0; i < 22; i++) {
            const t = i / 22 * Math.PI * 2;
            const q = T([24 + Math.cos(t) * 48, 0 + Math.sin(t) * 22]);
            bumps += `<circle cx="${f(q[0])}" cy="${f(q[1])}" r="${f(9 + (i % 3) * 1.5)}"/>`;
        }
        return `<g fill="url(#wool)" filter="url(#woolTex)"><path d="${d}Z"/>${bumps}</g>`;
    }
    const chest = [T([50, -6]), T([72, 2]), T([66, 17]), T([50, 19])];
    const chestD = `M${f(chest[0][0])} ${f(chest[0][1])} Q${f(chest[1][0])} ${f(chest[1][1])} ${f(chest[2][0])} ${f(chest[2][1])} Q${f(chest[3][0] + 4)} ${f(chest[3][1] + 3)} ${f(chest[3][0])} ${f(chest[3][1])} Z`;
    const hl = T([20, -19]);
    // ⚫ AJOUTÉ (25/10/2026) : taches noires (vache), découpées par la silhouette du torse
    let spots = '';
    if (S.spots) {
        const blobs = [[2, -10, 16, 12], [30, -16, 12, 10], [48, 2, 10, 9], [12, 10, 9, 7], [-18, 0, 9, 11]];
        spots = `<clipPath id="tclip"><path d="${d}Z"/></clipPath><g clip-path="url(#tclip)" fill="${F.dark}">`
            + blobs.map(([x, y, rx, ry], i) => { const q = T([x, y]); return `<ellipse cx="${f(q[0])}" cy="${f(q[1])}" rx="${rx}" ry="${ry}" transform="rotate(${i * 37} ${f(q[0])} ${f(q[1])})"/>`; }).join('')
            + '</g>';
    }
    let stripes = '';
    if (S.stripes) {   // chat tigré
        for (let i = 0; i < 6; i++) {
            const p1 = T([-10 + i * 11, -22]), p2 = T([-6 + i * 11, -4]);
            stripes += `<path d="M${f(p1[0])} ${f(p1[1])} Q${f(p2[0] + 3)} ${f((p1[1] + p2[1]) / 2)} ${f(p2[0])} ${f(p2[1])}" stroke="${F.dark}" stroke-width="2.2" fill="none" opacity="0.4" stroke-linecap="round"/>`;
        }
    }
    return `<path d="${d}Z" fill="url(#furBody)" stroke="${F.dark}" stroke-width="0.8" stroke-opacity="0.55"/>`
        + stripes + spots
        + `<path d="${chestD}" fill="${F.cream}" opacity="${S.nose === 'pig' ? 0.3 : 0.85}"/>`
        + `<ellipse cx="${f(hl[0])}" cy="${f(hl[1])}" rx="30" ry="5" fill="${F.headHi}" opacity="0.45" filter="url(#soft1)" transform="rotate(${f(-a)} ${f(hl[0])} ${f(hl[1])})"/>`;
}

/* ---------- poses (communes à toutes les espèces) ---------- */

const BASE = {
    hip: [64, 92], bodyA: 2, neckA: 62, neckL: 26, headA: -4, ear: 10, eye: 'open', mouth: 'closed',
    tail: 40, wag: 0, legs: { hf: [18, -14], hn: [12, -10], ff: [-2, 0], fn: [4, 2] }, mode: 'stand',
};
const P = o => ({ ...BASE, ...o, legs: { ...BASE.legs, ...(o.legs || {}) } });
// ⚫ RETIRÉ (25/10/2026) : 4 images de galop dessinées à la main (RUN) : remplacées par les allures calculées.

// ⚫ AJOUTÉ (25/10/2026) : allure calculée. Chaque patte suit un cycle (phase propre) : vers l'avant en l'air
// (genou / jarret fléchi), vers l'arrière au sol ; le corps monte et descend 2 fois par cycle, la tête balance.
//   pas    : 4 temps (postérieur, antérieur du même côté, puis l'autre côté), patte au sol 3/4 du temps ;
//   galop  : les deux postérieurs puis les deux antérieurs, grande amplitude, phase de suspension.
function gait(kind, i, n) {
    const ph = i / n, TAU = Math.PI * 2;
    const walk = kind === 'walk';
    const amp = walk ? 17 : 34;
    const phases = walk ? { hn: 0, fn: 0.25, hf: 0.5, ff: 0.75 } : { hn: 0, hf: 0.12, ff: 0.5, fn: 0.62 };
    const legs = {};
    for (const [k, p] of Object.entries(phases)) {
        const t = (ph + p) % 1;
        const swing = Math.sin(t * TAU);                 // + : vers l'avant
        const lift = Math.max(0, Math.cos(t * TAU));     // patte levée pendant le retour vers l'avant
        const hind = k[0] === 'h';
        const a1 = (hind ? 12 : 0) + amp * swing;
        const a2 = hind ? a1 - 14 - lift * (walk ? 24 : 38) : a1 + 2 - lift * (walk ? 30 : 46);
        legs[k] = [Math.round(a1), Math.round(a2)];
    }
    const bob = Math.sin(ph * TAU * 2);
    return {
        legs,
        hip: [walk ? 64 : 62, (walk ? 92 : 86) + (walk ? 1.4 : 3.5) * bob],
        bodyA: walk ? 1.5 * Math.sin(ph * TAU) : 5 * Math.sin(ph * TAU + 1),
        neckA: (walk ? 56 : 42) + (walk ? 3 : 4) * Math.sin(ph * TAU * 2 + 1.2),
        headA: walk ? -6 : -8,
        tail: walk ? 30 : 32, wag: (walk ? 8 : 4) * Math.sin(ph * TAU),
        ear: walk ? 14 + 6 * Math.sin(ph * TAU * 2) : 45 + 10 * Math.sin(ph * TAU * 2),
        mouth: walk ? 'closed' : 'tongue',
    };
}
const gaitPoses = (kind, n) => Object.fromEntries([...Array(n)].map((_, i) => [`${kind}_${i + 1}`, P(gait(kind, i, n))]));

export const POSES = {
    stand_1: P({ wag: 0, mouth: 'tongue' }),
    stand_2: P({ wag: 18, mouth: 'tongue' }),
    stand_3: P({ wag: -14, mouth: 'tongue' }),
    calm_1: P({ wag: 0, mouth: 'closed', tail: 20 }),
    calm_2: P({ wag: 8, mouth: 'closed', tail: 20 }),
    // ⚫ MODIFIÉ (25/10/2026) : pas (8 images) et galop (6 images) calculés, voir gait()
    ...gaitPoses('walk', 8),
    ...gaitPoses('run', 6),
    // ⚫ AJOUTÉ (25/10/2026) : repos : respiration (le corps monte un peu), oreille qui bouge, queue qui balance
    idle_1: P({ hip: [64, 92], neckA: 62, tail: 22, wag: 0 }),
    idle_2: P({ hip: [64, 91.4], neckA: 63, tail: 22, wag: 4 }),
    idle_3: P({ hip: [64, 91], neckA: 63.5, tail: 22, wag: 6, ear: 24 }),
    idle_4: P({ hip: [64, 91.4], neckA: 63, tail: 22, wag: 3 }),
    // ⚫ AJOUTÉ (25/10/2026) : patte avant levée (le chat tape dans la pelote / la plume)
    paw: P({ bodyA: 6, neckA: 58, headA: 4, tail: 50, wag: 6, legs: { fn: [72, 40], ff: [6, 2] } }),
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
    // ⚫ AJOUTÉ (25/10/2026) : besoins (accroupi, queue levée)
    potty: P({ bodyA: -18, hip: [60, 112], neckA: 40, headA: -6, tail: 60, mouth: 'closed', eye: 'half', legs: { hf: [40, -60], hn: [36, -56], ff: [-6, -2], fn: [-2, 0] } }),
};

// Les animaux de ferme ne s'assoient pas : équivalents debout / couché
function adapt(S, name, Pz) {
    if (S.canSit || Pz.mode !== 'sit') return Pz;
    const keep = { eye: Pz.eye, mouth: Pz.mouth, ear: Pz.ear, tear: Pz.tear, wag: Pz.wag };
    if (name === 'sit_cry') return P({ ...POSES.lie, ...keep, neckA: 60, headA: 14 });
    if (name === 'sit_sad') return P({ ...POSES.guilty, ...keep });
    return P({ ...keep, tail: 30, neckA: name === 'sit_love' ? 70 : 62, headA: name === 'sit_love' ? 14 : 2 });
}

/* ---------- rendu ---------- */

// function geometry(sp, name) {
//     const S = SPECIES[sp];
function geometry(sp, name, stage = 2) {   // ⚫ MODIFIÉ (25/10/2026) : selon l'âge
    const S = speciesAt(sp, stage);
    const Pz = adapt(S, name, POSES[name]);
    const cv = canvasOf(sp, stage);
    const lm = S.legMul;
    // la hanche monte ou descend avec la longueur des pattes (mode debout)
    const legLen = 49;
    let hip = [Pz.hip[0] + (cv.ax - BASE_AX) - (S.torso[0] - 1) * 20, Pz.hip[1] + (cv.gy - BASE_GY)];
    if (Pz.mode === 'stand') hip[1] -= legLen * (lm - 1);
    if (Pz.mode !== 'stand') hip[1] += (1 - S.torso[1]) * 8;
    const a = Pz.bodyA;
    const [sx, sy] = S.torso;
    const T = p => add(hip, rot([p[0] * sx, p[1] * sy], a));
    const neckBase = T([56, -12]);
    const headC = add(neckBase, pol(Pz.neckL + S.neck, Pz.neckA));
    return { S, Pz, cv, hip, a, T, neckBase, headC };
}

// export function animalSvg(sp, name) {
export function animalSvg(sp, name, stage = 2) {   // ⚫ MODIFIÉ (25/10/2026) : stage 0 bébé, 1 ado, 2 adulte
    const { S, Pz, cv, hip, a, T, headC } = geometry(sp, name, stage);
    const gy = cv.gy, lm = S.legMul, lw = S.legW;
    const shoulder = T([48, 6]), hipJ = T([-4, 4]), tailBase = T([-24, -8]);

    let far = '', near = '';
    const shadowW = (Pz.mode === 'lie' ? 64 : 52) * S.torso[0];
    const shadow = `<ellipse cx="${f(hip[0] + 30 * S.torso[0])}" cy="${gy + 1}" rx="${f(shadowW)}" ry="6" fill="#0c2006" opacity="0.45" filter="url(#dshadow)"/>`;

    if (Pz.mode === 'stand') {
        const L = Pz.legs;
        far = leg(S, add(hipJ, [-2, 0]), L.hf[0], L.hf[1], 24 * lm, 25 * lm, 10 * lw, 5.8 * lw, 4 * lw, true)
            + leg(S, add(shoulder, [-3, 0]), L.ff[0], L.ff[1], 22 * lm, 24 * lm, 9 * lw, 5.4 * lw, 4 * lw, true);
        near = (S.wool ? '' : `<ellipse cx="${f(hipJ[0] + 2)}" cy="${f(hipJ[1] + 2)}" rx="${f(15 * S.torso[1])}" ry="${f(19 * S.torso[1])}" fill="url(#furBody)" transform="rotate(${f(-a - 15)} ${f(hipJ[0] + 2)} ${f(hipJ[1] + 2)})"/>`)
            + leg(S, hipJ, L.hn[0], L.hn[1], 24 * lm, 25 * lm, 11 * lw, 6.2 * lw, 4.2 * lw, false)
            + leg(S, shoulder, L.fn[0], L.fn[1], 22 * lm, 24 * lm, 10 * lw, 5.6 * lw, 4.2 * lw, false);
    } else if (Pz.mode === 'sit') {
        far = foldedHind(S, add(hipJ, [-3, 0]), true, gy) + leg(S, add(shoulder, [-3, 0]), 4, 0, 18, gy - shoulder[1] - 22, 8 * lw, 5 * lw, 3.6 * lw, true);
        near = foldedHind(S, hipJ, false, gy) + leg(S, shoulder, 8, 2, 18, gy - shoulder[1] - 22, 9 * lw, 5.2 * lw, 3.8 * lw, false);
    } else {   // couché
        far = foldedHind(S, add(hipJ, [-3, 0]), true, gy) + leg(S, add(shoulder, [-2, 2]), 70, 88, 14 * lm, 22 * lm, 8 * lw, 5 * lw, 3.6 * lw, true);
        near = foldedHind(S, hipJ, false, gy) + leg(S, add(shoulder, [6, 4]), 74, 90, 14 * lm, 24 * lm, 9 * lw, 5.2 * lw, 3.8 * lw, false);
    }
    // const back = tail(S, tailBase, Pz.tail, Pz.wag, Pz.mode === 'stand' ? 38 : 30);
    const back = sc(S.tailK, tailBase, tail(S, tailBase, Pz.tail, Pz.wag, Pz.mode === 'stand' ? 38 : 30));   // ⚫ 25/10/2026 : queue courte du petit

    // cou
    const nb1 = T([42, -20]), nb2 = T([68, 6]);
    const neckW = S.nose === 'horse' ? 1.25 : 1;
    const neck = `<path d="M${f(nb1[0])} ${f(nb1[1])} Q${f(headC[0] - 14 * neckW)} ${f(headC[1] - 4)} ${f(headC[0] - 6)} ${f(headC[1] - 12)} L${f(headC[0] + 10)} ${f(headC[1] + 6)} Q${f(headC[0] + 2)} ${f(headC[1] + 18 * neckW)} ${f(nb2[0])} ${f(nb2[1])} Z" fill="${S.wool ? 'url(#wool)' : 'url(#furBody)'}"/>`;
    // crinière (cheval) le long du haut du cou
    let mane = '';
    if (S.mane) {    // bande de crins le long du dessus du cou, mèches qui retombent du côté visible
        const top = t => [lerp(nb1[0], headC[0] - 6, t), lerp(nb1[1], headC[1] - 12, t) - Math.sin(t * Math.PI) * 4];
        let band = '';
        for (let i = 0; i <= 10; i++) { const q = top(i / 10); band += (i ? 'L' : 'M') + f(q[0]) + ' ' + f(q[1]) + ' '; }
        mane = `<path d="${band}" stroke="${S.mane}" stroke-width="6" fill="none" stroke-linecap="round" stroke-linejoin="round"/>`;
        for (let i = 1; i < 9; i++) {
            const q = top(i / 9), dn = rot([3, 7], Pz.neckA - 62);
            mane += `<path d="M${f(q[0])} ${f(q[1])} q${f(dn[0] * 0.5)} ${f(dn[1] * 0.6)} ${f(dn[0])} ${f(dn[1])}" stroke="#3a2414" stroke-width="2.4" fill="none" stroke-linecap="round"/>`;
        }
    }
    // collier (chien, chat)
    let collar = '';
    if (S.collar) {
        const c1 = add(headC, rot([-12, 6], Pz.headA * 0.5 + Pz.neckA - 70)), c2 = add(headC, rot([8, 16], Pz.headA * 0.5 + Pz.neckA - 70));
        const tag = add([(c1[0] + c2[0]) / 2, (c1[1] + c2[1]) / 2], [2, 5]);
        collar = `<path d="M${f(c1[0])} ${f(c1[1])} L${f(c2[0])} ${f(c2[1])}" stroke="${S.collar}" stroke-width="${S.bell ? 3.5 : 5}" stroke-linecap="round"/>`
            + `<circle cx="${f(tag[0])}" cy="${f(tag[1])}" r="${S.bell ? 3.6 : 3.2}" fill="#ffd24a" stroke="#b8860b" stroke-width="0.8"/>`
            + (S.bell ? `<path d="M${f(tag[0] - 2)} ${f(tag[1] + 1)} h4" stroke="#8a6a0a" stroke-width="0.8"/>` : '');
    }

    const body = back + far + torso(S, hip, a) + near + neck + mane + collar + head(S, headC, Pz.headA, Pz);
    return `<svg xmlns="http://www.w3.org/2000/svg" width="${cv.W}" height="${cv.H}" viewBox="0 0 ${cv.W} ${cv.H}">${defs(S)}
      ${shadow}
      <g ${S.furTex ? 'filter="url(#fur)"' : ''} transform="translate(${cv.ax} ${gy}) scale(${S.scale}) translate(${-cv.ax} ${-gy})">${body}</g>
    </svg>`;
}

// Position de la bouche par rapport à l'ancrage (pour porter un objet, manger, boire)
// export function mouthOf(sp, name) {
export function mouthOf(sp, name, stage = 2) {   // ⚫ MODIFIÉ (25/10/2026) : selon l'âge
    const { S, Pz, cv, headC } = geometry(sp, name, stage);
    const m = add(headC, rot([S.head.mouth[1][0] + 6, S.head.mouth[1][1]], Pz.headA));
    // mise à l'échelle autour de l'ancrage
    return [Math.round((m[0] - cv.ax) * S.scale), Math.round((m[1] - cv.gy) * S.scale)];
}

// ⚫ AJOUTÉ (25/10/2026) : arrière-train (base de la queue) par rapport à l'ancrage, pour la pose "besoins" :
// le firmware place l'animal pour que ses besoins tombent pile sur le coin gravier
// export function rearOf(sp, name = 'potty') {
export function rearOf(sp, name = 'potty', stage = 2) {   // ⚫ MODIFIÉ (25/10/2026) : selon l'âge
    const { S, cv, T } = geometry(sp, name, stage);
    const r = T([-26, 10]);
    return [Math.round((r[0] - cv.ax) * S.scale), Math.round((cv.gy - cv.gy) * S.scale)];
}
