// ⚫ AJOUTÉ (25/10/2026) : petits éléments animés par le firmware par-dessus le décor :
// croquettes (gamelle pleine), balle, nuages, papillon (2 images), luciole, trou creusé, motte de terre.
const svg = (w, h, body, defs = '') =>
    `<svg xmlns="http://www.w3.org/2000/svg" width="${w}" height="${h}" viewBox="0 0 ${w} ${h}"><defs>${defs}</defs>${body}</svg>`;

function rng(seed) {
    let s = seed >>> 0;
    return () => ((s = (s * 1664525 + 1013904223) >>> 0) / 4294967296);
}

export function kibble() {
    // tas de croquettes qui dépasse du bord de la gamelle (46 x 18)
    const r = rng(9);
    let k = '';
    for (let i = 0; i < 34; i++) {
        const x = 5 + r() * 36, y = 9 + Math.abs(x - 23) * 0.18 + r() * 6 - (1 - Math.abs(x - 23) / 23) * 5;
        const c = ['#8a4a1c', '#a65e26', '#6e3812', '#b8743a'][Math.floor(r() * 4)];
        k += `<ellipse cx="${x.toFixed(1)}" cy="${y.toFixed(1)}" rx="${(2.4 + r()).toFixed(1)}" ry="${(1.7 + r() * 0.6).toFixed(1)}" fill="${c}" transform="rotate(${(r() * 180).toFixed(0)} ${x.toFixed(1)} ${y.toFixed(1)})"/>`
            + `<circle cx="${(x - 0.6).toFixed(1)}" cy="${(y - 0.6).toFixed(1)}" r="0.6" fill="#e8b07a" opacity="0.6"/>`;
    }
    return svg(46, 18, k);
}

export function ball() {
    return svg(18, 18, `<circle cx="9" cy="9" r="8" fill="url(#bg)"/><path d="M2.5 6 Q9 10 15.5 6 M2.5 12 Q9 8 15.5 12" stroke="#fff" stroke-width="1.3" fill="none" opacity="0.9"/>`,
        `<radialGradient id="bg" cx="0.35" cy="0.3" r="0.75"><stop offset="0" stop-color="#f4ff8a"/><stop offset="0.6" stop-color="#c8e020"/><stop offset="1" stop-color="#7a9a08"/></radialGradient>`);
}

export function cloud(seed, w, h) {
    const r = rng(seed);
    let c = '';
    for (let i = 0; i < 7; i++) {
        const x = w * (0.2 + 0.6 * r()), y = h * (0.45 + 0.25 * r()), rx = w * (0.14 + 0.12 * r()), ry = h * (0.28 + 0.2 * r());
        c += `<ellipse cx="${x.toFixed(1)}" cy="${y.toFixed(1)}" rx="${rx.toFixed(1)}" ry="${ry.toFixed(1)}"/>`;
    }
    return svg(w, h, `<g fill="#fff" filter="url(#b)" opacity="0.92">${c}<ellipse cx="${w / 2}" cy="${h * 0.7}" rx="${w * 0.42}" ry="${h * 0.18}"/></g>`
        + `<g fill="#c8dcf0" opacity="0.5" filter="url(#b)"><ellipse cx="${w / 2}" cy="${h * 0.8}" rx="${w * 0.36}" ry="${h * 0.1}"/></g>`,
        `<filter id="b" x="-10%" y="-20%" width="120%" height="140%"><feGaussianBlur stdDeviation="2"/></filter>`);
}

export function butterfly(open) {
    const s = open ? 1 : 0.35;   // ailes ouvertes / repliées (vue de dessus)
    const wing = (dir) => `<g transform="translate(11 9) scale(${dir * s} 1)">
        <path d="M0 0 C4 -9 11 -8 9 -1 C8 2 3 2 0 0 Z" fill="url(#w1)" stroke="#5a2a00" stroke-width="0.5"/>
        <path d="M0 0 C5 2 8 7 4 8 C1 8 0 4 0 0 Z" fill="url(#w2)" stroke="#5a2a00" stroke-width="0.5"/>
        <circle cx="6" cy="-3.5" r="1.1" fill="#fff" opacity="0.9"/></g>`;
    return svg(22, 18, wing(1) + wing(-1) + `<ellipse cx="11" cy="9.5" rx="0.9" ry="4" fill="#2a1a10"/>`,
        `<linearGradient id="w1" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#ffb020"/><stop offset="1" stop-color="#e85a00"/></linearGradient>
         <linearGradient id="w2" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#ffd060"/><stop offset="1" stop-color="#f08a10"/></linearGradient>`);
}

export function firefly() {
    return svg(16, 16, `<circle cx="8" cy="8" r="8" fill="url(#g)"/><circle cx="8" cy="8" r="1.6" fill="#fbffd0"/>`,
        `<radialGradient id="g" cx="0.5" cy="0.5" r="0.5"><stop offset="0" stop-color="#eaff80" stop-opacity="0.9"/><stop offset="0.35" stop-color="#c8f040" stop-opacity="0.35"/><stop offset="1" stop-color="#a0e020" stop-opacity="0"/></radialGradient>`);
}

export function hole() {
    // trou creusé dans la pelouse + terre retournée (56 x 22)
    const r = rng(4);
    let clods = '';
    for (let i = 0; i < 16; i++) {
        const a = r() * Math.PI * 2, d = 18 + r() * 8;
        const x = 28 + Math.cos(a) * d, y = 11 + Math.sin(a) * d * 0.33;
        clods += `<ellipse cx="${x.toFixed(1)}" cy="${y.toFixed(1)}" rx="${(1.6 + r() * 2).toFixed(1)}" ry="${(1.2 + r()).toFixed(1)}" fill="${r() < 0.5 ? '#6a4224' : '#87573a'}"/>`;
    }
    return svg(56, 22, `<ellipse cx="28" cy="11" rx="24" ry="8.5" fill="#7a4e2c"/>${clods}<ellipse cx="28" cy="11.5" rx="15" ry="5" fill="url(#h)"/>`,
        `<radialGradient id="h" cx="0.5" cy="0.4" r="0.6"><stop offset="0" stop-color="#1e1008"/><stop offset="1" stop-color="#4a2c16"/></radialGradient>`);
}

export function clod() {
    return svg(8, 7, `<ellipse cx="4" cy="3.6" rx="3.4" ry="2.8" fill="#6e4426"/><circle cx="3" cy="2.6" r="0.9" fill="#9a6a44"/>`);
}

// ⚫ AJOUTÉ (25/10/2026) : accessoires des nouveaux jeux (frisbee, os à ronger)
export function frisbee() {
    return svg(34, 14, `<ellipse cx="17" cy="8" rx="16" ry="5.5" fill="#a8121e"/><ellipse cx="17" cy="6.5" rx="16" ry="5.5" fill="url(#f)"/>`
        + `<ellipse cx="17" cy="6.5" rx="9" ry="2.6" fill="none" stroke="#ffffff" stroke-width="0.9" opacity="0.6"/>`
        + `<ellipse cx="12" cy="4.8" rx="5" ry="1.2" fill="#fff" opacity="0.55"/>`,
        `<radialGradient id="f" cx="0.4" cy="0.35" r="0.7"><stop offset="0" stop-color="#ff7a5a"/><stop offset="1" stop-color="#d81e2a"/></radialGradient>`);
}

export function bone() {
    return svg(34, 14, `<g transform="translate(17 7)"><path d="M-11 -2.6 h22 a4 4 0 1 1 3.2 4.6 a4 4 0 1 1 -3.2 4.6 h-22 a4 4 0 1 1 -3.2 -4.6 a4 4 0 1 1 3.2 -4.6 z" transform="translate(0 -2)" fill="url(#b)" stroke="#b8a27a" stroke-width="0.8"/></g>`
        + `<path d="M8 4.6 h17" stroke="#fff" stroke-width="1" opacity="0.7" stroke-linecap="round"/>`,
        `<linearGradient id="b" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#fffaf0"/><stop offset="1" stop-color="#dccbaa"/></linearGradient>`);
}
