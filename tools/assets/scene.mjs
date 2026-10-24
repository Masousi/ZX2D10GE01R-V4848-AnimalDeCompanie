// ⚫ AJOUTÉ (25/10/2026) : décor de la cour (480x480, écran rond) : ciel, collines, arbres, palissade,
// pelouse texturée, piste de course en sable, niche, gamelles. Version jour et version nuit.
// Positions reprises par le firmware (main/pet_scene.h) : niche, gamelles, piste.

export const SPOTS = {
    kennelDoor: [100, 296],   // pied du chien couché devant la niche
    food: [186, 378],         // centre de la gamelle de croquettes (⚫ 25/10/2026 : recentrée, le chien mange à sa gauche)
    water: [352, 372],        // centre de la gamelle d'eau
    track: [318, 252, 112, 26], // piste : centre x, y, demi-axes
};

// Petit générateur pseudo-aléatoire déterministe (même image à chaque génération)
function rng(seed) {
    let s = seed >>> 0;
    return () => ((s = (s * 1664525 + 1013904223) >>> 0) / 4294967296);
}

function grassTufts(r, n, y0, y1, night) {
    let out = '';
    for (let i = 0; i < n; i++) {
        const y = y0 + (y1 - y0) * Math.pow(r(), 0.8);
        const x = r() * 480;
        const depth = (y - y0) / (y1 - y0);            // 0 au fond, 1 devant
        const h = 4 + depth * 9 + r() * 4;
        const lean = (r() - 0.5) * 5;
        const g = night ? `hsl(${120 + r() * 20},35%,${12 + r() * 10}%)` : `hsl(${85 + r() * 30},${45 + r() * 20}%,${28 + r() * 22}%)`;
        out += `<path d="M${x.toFixed(1)} ${y.toFixed(1)} q${(lean / 2).toFixed(1)} ${(-h / 2).toFixed(1)} ${lean.toFixed(1)} ${(-h).toFixed(1)}" stroke="${g}" stroke-width="${(0.8 + depth * 1.2).toFixed(2)}" fill="none" stroke-linecap="round"/>`;
    }
    return out;
}

function flowers(r, n, night) {
    let out = '';
    const cols = ['#ffffff', '#ffe45c', '#ff9ec7', '#c9a8ff'];
    for (let i = 0; i < n; i++) {
        // en bordure : le long de la palissade et sur les côtés
        const side = r();
        let x, y;
        if (side < 0.5) { x = r() * 480; y = 218 + r() * 14; } else { x = side < 0.75 ? 10 + r() * 60 : 410 + r() * 60; y = 230 + r() * 150; }
        const c = cols[Math.floor(r() * cols.length)];
        const s = 1.4 + (y - 210) / 120;
        const op = night ? 0.35 : 0.95;
        out += `<g opacity="${op}"><circle cx="${x.toFixed(1)}" cy="${(y + s * 2).toFixed(1)}" r="${(s * 0.5).toFixed(2)}" fill="#3d7a25"/>`;
        for (let k = 0; k < 5; k++) {
            const a = k / 5 * Math.PI * 2;
            out += `<circle cx="${(x + Math.cos(a) * s).toFixed(1)}" cy="${(y + Math.sin(a) * s).toFixed(1)}" r="${(s * 0.75).toFixed(2)}" fill="${c}"/>`;
        }
        out += `<circle cx="${x.toFixed(1)}" cy="${y.toFixed(1)}" r="${(s * 0.55).toFixed(2)}" fill="#f2a900"/></g>`;
    }
    return out;
}

function stars(r, n) {
    let out = '';
    for (let i = 0; i < n; i++) {
        const x = r() * 480, y = r() * 150, s = 0.4 + r() * 1.1;
        out += `<circle cx="${x.toFixed(1)}" cy="${y.toFixed(1)}" r="${s.toFixed(2)}" fill="#fff" opacity="${(0.4 + r() * 0.6).toFixed(2)}"/>`;
    }
    return out;
}

function fence(night) {
    // palissade en planches à bouts pointus, de y=150 (pointes) à y=222 (pied)
    let planks = '';
    for (let x = -6, i = 0; x < 486; x += 22, i++) {
        const w = 19, top = 152 + ((i * 7) % 5);
        planks += `<path d="M${x} 224 L${x} ${top + 8} L${x + w / 2} ${top} L${x + w} ${top + 8} L${x + w} 224 Z" fill="url(#plank)" stroke="#6b4426" stroke-width="0.8"/>`;
        // veinage du bois
        planks += `<path d="M${x + 5} ${top + 14} q2 20 0 40 M${x + 13} ${top + 18} q-2 16 0 34" stroke="#8d5d35" stroke-width="0.6" fill="none" opacity="0.55"/>`;
        // clous
        planks += `<circle cx="${x + w / 2}" cy="172" r="0.9" fill="#4a3020"/><circle cx="${x + w / 2}" cy="206" r="0.9" fill="#4a3020"/>`;
    }
    const rails = `<rect x="0" y="168" width="480" height="8" fill="url(#rail)"/><rect x="0" y="202" width="480" height="8" fill="url(#rail)"/>`;
    return `<g>${planks}${rails}</g>`
        + `<rect x="0" y="222" width="480" height="14" fill="url(#fenceShadow)"/>`
        + (night ? '' : '');
}

function kennel() {
    // niche en 3/4 : façade (x 42..150), côté droit en fuite, toit rouge à deux pans
    return `
    <ellipse cx="112" cy="300" rx="88" ry="16" fill="#1d3a12" opacity="0.45" filter="url(#soft6)"/>
    <!-- côté droit (en fuite) -->
    <path d="M150 228 L186 214 L186 282 L150 298 Z" fill="url(#woodSide)"/>
    <g stroke="#7a4a24" stroke-width="0.8" opacity="0.6">
      <path d="M150 246 L186 233 M150 264 L186 250 M150 282 L186 267"/>
    </g>
    <!-- façade -->
    <path d="M42 228 L96 186 L150 228 L150 298 L42 298 Z" fill="url(#woodFront)"/>
    <g stroke="#7a4a24" stroke-width="0.9" opacity="0.6">
      <path d="M42 246 H150 M42 264 H150 M42 282 H150 M60 214 H132"/>
    </g>
    <!-- porte en arc, intérieur sombre -->
    <path d="M70 298 L70 262 Q96 230 122 262 L122 298 Z" fill="url(#door)"/>
    <path d="M70 298 L70 262 Q96 230 122 262 L122 298" fill="none" stroke="#5a3418" stroke-width="3"/>
    <!-- plaque en forme d'os -->
    <g transform="translate(96 230)">
      <path d="M-14 -3 h28 a4 4 0 1 1 3 5 a4 4 0 1 1 -3 5 h-28 a4 4 0 1 1 -3 -5 a4 4 0 1 1 3 -5 z" fill="#f4ecd8" stroke="#a88f68" stroke-width="0.8" transform="translate(0 -2)"/>
    </g>
    <!-- toit -->
    <path d="M30 236 L96 176 L106 170 L198 206 L190 220 L106 186 L40 244 Z" fill="url(#roofSide)"/>
    <path d="M30 236 L96 176 L104 184 L40 244 Z" fill="url(#roofFront)"/>
    <path d="M96 176 L106 170 L198 206 L190 214 Z" fill="#b8322a"/>
    <g stroke="#7e1d18" stroke-width="0.9" opacity="0.7" fill="none">
      <path d="M112 182 L190 212 M108 190 L186 220"/>
      <path d="M118 175 l-4 10 M134 181 l-4 10 M150 187 l-4 10 M166 193 l-4 10 M182 199 l-4 10"/>
    </g>
    <path d="M30 236 L96 176 L106 170" fill="none" stroke="#e8655a" stroke-width="1.6" opacity="0.8"/>`;
}

function bowl(cx, cy, body, rim, inner, extra) {
    // ⚫ 25/10/2026 : gamelles agrandies (rayon 34) avec reflet sur le bord
    const R = 34, r2 = 28;
    return `
    <ellipse cx="${cx}" cy="${cy + 12}" rx="${R + 8}" ry="9" fill="#14300c" opacity="0.5" filter="url(#soft3)"/>
    <path d="M${cx - R} ${cy - 4} L${cx - R + 7} ${cy + 11} Q${cx} ${cy + 19} ${cx + R - 7} ${cy + 11} L${cx + R} ${cy - 4} Z" fill="url(#${body})"/>
    <ellipse cx="${cx}" cy="${cy - 4}" rx="${R}" ry="9.5" fill="url(#${rim})"/>
    <ellipse cx="${cx}" cy="${cy - 3.5}" rx="${r2}" ry="7.2" fill="${inner}"/>
    <path d="M${cx - R + 4} ${cy - 7} Q${cx - 12} ${cy - 13} ${cx + 6} ${cy - 13}" stroke="#fff" stroke-width="1.4" fill="none" opacity="0.55"/>
    ${extra}`;
}

function track() {
    const [cx, cy, rx, ry] = SPOTS.track;
    return `
    <ellipse cx="${cx}" cy="${cy}" rx="${rx + 6}" ry="${ry + 5}" fill="#4f8a2c" opacity="0.6" filter="url(#soft3)"/>
    <ellipse cx="${cx}" cy="${cy}" rx="${rx}" ry="${ry}" fill="url(#sand)"/>
    <ellipse cx="${cx}" cy="${cy}" rx="${rx}" ry="${ry}" fill="#000" opacity="0.18" filter="url(#sandNoise)"/>
    <ellipse cx="${cx}" cy="${cy}" rx="${rx - 30}" ry="${ry - 11}" fill="url(#grassIsland)"/>
    <ellipse cx="${cx}" cy="${cy}" rx="${rx}" ry="${ry}" fill="none" stroke="#b08850" stroke-width="1.2" opacity="0.6"/>
    ${cone(cx - rx + 8, cy + 6)}${cone(cx + rx - 10, cy + 4)}`;
}

function cone(x, y) {
    return `<ellipse cx="${x}" cy="${y + 1}" rx="9" ry="3" fill="#2a3a14" opacity="0.4"/>
    <path d="M${x - 8} ${y} L${x - 2} ${y - 20} L${x + 2} ${y - 20} L${x + 8} ${y} Z" fill="url(#cone)"/>
    <path d="M${x - 5.6} ${y - 7} L${x + 5.6} ${y - 7} L${x + 4.4} ${y - 11} L${x - 4.4} ${y - 11} Z" fill="#fff" opacity="0.9"/>
    <rect x="${x - 10}" y="${y - 1}" width="20" height="3" rx="1" fill="#e2550f"/>`;
}

function trees(night) {
    // rideau d'arbres derrière la palissade
    const r = rng(7);
    let out = '';
    const xs = [10, 55, 95, 150, 205, 260, 300, 350, 400, 445, 480];
    for (const x of xs) {
        const s = 26 + r() * 18, y = 132 - r() * 16;
        out += `<rect x="${x - 3}" y="${y}" width="6" height="40" fill="${night ? '#1b1d28' : '#5b4128'}"/>`;
        for (let k = 0; k < 4; k++) {
            const dx = (r() - 0.5) * s, dy = (r() - 0.6) * s * 0.8;
            out += `<circle cx="${(x + dx).toFixed(1)}" cy="${(y + dy).toFixed(1)}" r="${(s * (0.55 + r() * 0.3)).toFixed(1)}" fill="url(#tree)"/>`;
        }
    }
    return out;
}

export function sceneSvg({ night = false } = {}) {
    const r = rng(42);
    const sky = night
        ? `<stop offset="0" stop-color="#070b24"/><stop offset="0.6" stop-color="#16205a"/><stop offset="1" stop-color="#2d3c80"/>`
        : `<stop offset="0" stop-color="#2f7fd6"/><stop offset="0.55" stop-color="#7cc0f5"/><stop offset="1" stop-color="#d6f0ff"/>`;
    const grassTop = night ? '#1f4a2c' : '#86cc52', grassBot = night ? '#0e2616' : '#3c8a26';
    const hill1 = night ? '#18304a' : '#7fb8a0', hill2 = night ? '#14303a' : '#5f9e5a';

    return `<svg xmlns="http://www.w3.org/2000/svg" width="480" height="480" viewBox="0 0 480 480">
  <defs>
    <linearGradient id="sky" x1="0" y1="0" x2="0" y2="1">${sky}</linearGradient>
    <radialGradient id="sunGlow" cx="0.5" cy="0.5" r="0.5">
      <stop offset="0" stop-color="${night ? '#fffbe8' : '#fffbe0'}" stop-opacity="1"/>
      <stop offset="0.25" stop-color="${night ? '#e8ecff' : '#fff2a8'}" stop-opacity="0.9"/>
      <stop offset="1" stop-color="${night ? '#8090ff' : '#ffe680'}" stop-opacity="0"/>
    </radialGradient>
    <linearGradient id="grass" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="${grassTop}"/><stop offset="1" stop-color="${grassBot}"/>
    </linearGradient>
    <linearGradient id="grassIsland" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="${night ? '#1c4429' : '#7cc24a'}"/><stop offset="1" stop-color="${night ? '#183c22' : '#62ad3a'}"/>
    </linearGradient>
    <linearGradient id="tree" x1="0" y1="0" x2="0.3" y2="1">
      <stop offset="0" stop-color="${night ? '#1d3a3a' : '#5aa04a'}"/><stop offset="1" stop-color="${night ? '#0d1f22' : '#2f6b2e'}"/>
    </linearGradient>
    <linearGradient id="plank" x1="0" y1="0" x2="1" y2="0">
      <stop offset="0" stop-color="#b9824c"/><stop offset="0.5" stop-color="#d9a46a"/><stop offset="1" stop-color="#a87240"/>
    </linearGradient>
    <linearGradient id="rail" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#c58d55"/><stop offset="1" stop-color="#7d5130"/>
    </linearGradient>
    <linearGradient id="fenceShadow" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#103008" stop-opacity="0.5"/><stop offset="1" stop-color="#103008" stop-opacity="0"/>
    </linearGradient>
    <linearGradient id="woodFront" x1="0" y1="0" x2="1" y2="1">
      <stop offset="0" stop-color="#e2ad72"/><stop offset="1" stop-color="#b47640"/>
    </linearGradient>
    <linearGradient id="woodSide" x1="0" y1="0" x2="1" y2="0">
      <stop offset="0" stop-color="#9a6234"/><stop offset="1" stop-color="#7a4a24"/>
    </linearGradient>
    <radialGradient id="door" cx="0.5" cy="0.3" r="0.8">
      <stop offset="0" stop-color="#3a2414"/><stop offset="1" stop-color="#120804"/>
    </radialGradient>
    <linearGradient id="roofFront" x1="0" y1="0" x2="1" y2="1">
      <stop offset="0" stop-color="#e2483c"/><stop offset="1" stop-color="#a8261e"/>
    </linearGradient>
    <linearGradient id="roofSide" x1="0" y1="0" x2="1" y2="1">
      <stop offset="0" stop-color="#d83a30"/><stop offset="1" stop-color="#8e1e18"/>
    </linearGradient>
    <linearGradient id="sand" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#ead39a"/><stop offset="1" stop-color="#c9a764"/>
    </linearGradient>
    <linearGradient id="cone" x1="0" y1="0" x2="1" y2="0">
      <stop offset="0" stop-color="#ff8a3a"/><stop offset="0.5" stop-color="#ff6a10"/><stop offset="1" stop-color="#c84a00"/>
    </linearGradient>
    <linearGradient id="redBody" x1="0" y1="0" x2="1" y2="0">
      <stop offset="0" stop-color="#9e1c1c"/><stop offset="0.35" stop-color="#e8443c"/><stop offset="1" stop-color="#8a1414"/>
    </linearGradient>
    <linearGradient id="redRim" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#ff7a6a"/><stop offset="1" stop-color="#c22a24"/>
    </linearGradient>
    <linearGradient id="blueBody" x1="0" y1="0" x2="1" y2="0">
      <stop offset="0" stop-color="#1c4e9e"/><stop offset="0.35" stop-color="#4a90e8"/><stop offset="1" stop-color="#163e86"/>
    </linearGradient>
    <linearGradient id="blueRim" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#9cc8ff"/><stop offset="1" stop-color="#2e6cc8"/>
    </linearGradient>
    <radialGradient id="water" cx="0.4" cy="0.35" r="0.7">
      <stop offset="0" stop-color="#d4f2ff"/><stop offset="0.5" stop-color="#6cc4f0"/><stop offset="1" stop-color="#2a86c8"/>
    </radialGradient>
    <radialGradient id="vignette" cx="0.5" cy="0.5" r="0.5">
      <stop offset="0.72" stop-color="#000" stop-opacity="0"/><stop offset="1" stop-color="#000" stop-opacity="${night ? 0.55 : 0.35}"/>
    </radialGradient>
    <radialGradient id="lampGlow" cx="0.5" cy="0.5" r="0.5">
      <stop offset="0" stop-color="#ffd27a" stop-opacity="0.75"/><stop offset="1" stop-color="#ffb040" stop-opacity="0"/>
    </radialGradient>
    <filter id="soft3" x="-30%" y="-80%" width="160%" height="260%"><feGaussianBlur stdDeviation="3"/></filter>
    <filter id="soft6" x="-30%" y="-80%" width="160%" height="260%"><feGaussianBlur stdDeviation="6"/></filter>
    <filter id="cloudBlur" x="-20%" y="-50%" width="140%" height="200%"><feGaussianBlur stdDeviation="2.2"/></filter>
    <!-- texture de l'herbe : bruit de Perlin, sombre et clair -->
    <filter id="grassNoise" x="0" y="0" width="100%" height="100%">
      <feTurbulence type="fractalNoise" baseFrequency="0.035 0.11" numOctaves="4" seed="3"/>
      <feColorMatrix type="matrix" values="0 0 0 0 0  0 0 0 0 0.06  0 0 0 0 0  0 0 0 -2.2 1.35"/>
    </filter>
    <filter id="grassLight" x="0" y="0" width="100%" height="100%">
      <feTurbulence type="fractalNoise" baseFrequency="0.02 0.06" numOctaves="3" seed="11"/>
      <feColorMatrix type="matrix" values="0 0 0 0 0.85  0 0 0 0 1  0 0 0 0 0.5  0 0 0 2.4 -1.45"/>
    </filter>
    <filter id="sandNoise" x="0" y="0" width="100%" height="100%">
      <feTurbulence type="fractalNoise" baseFrequency="0.25" numOctaves="2" seed="5"/>
      <feColorMatrix type="matrix" values="0 0 0 0 0.3  0 0 0 0 0.2  0 0 0 0 0.05  0 0 0 -2.6 1.6"/>
    </filter>
    <clipPath id="lawnClip"><rect x="0" y="222" width="480" height="258"/></clipPath>
  </defs>

  <rect width="480" height="480" fill="url(#sky)"/>
  ${night ? stars(r, 140) : ''}
  ${night
        ? `<circle cx="372" cy="78" r="70" fill="url(#sunGlow)" opacity="0.55"/><circle cx="372" cy="78" r="20" fill="#f6f2dc"/><circle cx="380" cy="72" r="19" fill="#d9d4bc" opacity="0.35"/><circle cx="365" cy="85" r="4" fill="#cfc8ae" opacity="0.6"/><circle cx="376" cy="70" r="3" fill="#cfc8ae" opacity="0.5"/>`
        : `<circle cx="378" cy="72" r="90" fill="url(#sunGlow)"/><circle cx="378" cy="72" r="24" fill="#fffbe6"/>`}
  ${night ? '' : `<g filter="url(#cloudBlur)" fill="#ffffff" opacity="0.9">
      <ellipse cx="110" cy="82" rx="46" ry="14"/><ellipse cx="88" cy="74" rx="22" ry="14"/><ellipse cx="124" cy="70" rx="26" ry="18"/>
      <ellipse cx="250" cy="112" rx="34" ry="9"/><ellipse cx="262" cy="104" rx="18" ry="11"/>
    </g>`}
  <!-- collines lointaines -->
  <path d="M0 150 Q80 104 170 132 T340 118 T480 130 L480 200 L0 200 Z" fill="${hill1}"/>
  <path d="M0 168 Q100 132 210 156 T420 146 T480 150 L480 200 L0 200 Z" fill="${hill2}"/>
  ${trees(night)}
  ${fence(night)}

  <!-- pelouse -->
  <rect x="0" y="222" width="480" height="258" fill="url(#grass)"/>
  <g clip-path="url(#lawnClip)">
    <g opacity="${night ? 0.04 : 0.09}" fill="#fff">
      <path d="M-40 480 L60 222 L120 222 L40 480 Z"/><path d="M120 480 L180 222 L240 222 L200 480 Z"/>
      <path d="M280 480 L300 222 L360 222 L360 480 Z"/><path d="M440 480 L420 222 L480 222 L520 480 Z"/>
    </g>
    <rect x="0" y="222" width="480" height="258" filter="url(#grassNoise)" opacity="0.55"/>
    <rect x="0" y="222" width="480" height="258" filter="url(#grassLight)" opacity="${night ? 0.1 : 0.35}"/>
    <rect x="0" y="222" width="480" height="20" fill="url(#fenceShadow)"/>
  </g>
  ${grassTufts(r, 1400, 226, 480, night)}
  ${track()}
  ${flowers(r, 34, night)}
  ${kennel()}
  ${bowl(SPOTS.food[0], SPOTS.food[1], 'redBody', 'redRim', '#3a0c0c', `<g transform="translate(${SPOTS.food[0]} ${SPOTS.food[1] + 8})" fill="#ffd9d0" opacity="0.8"><path d="M-7 -1.5 h14 a2 2 0 1 1 1.5 2.5 a2 2 0 1 1 -1.5 2.5 h-14 a2 2 0 1 1 -1.5 -2.5 a2 2 0 1 1 1.5 -2.5 z" transform="translate(0 -1)"/></g>`)}
  ${bowl(SPOTS.water[0], SPOTS.water[1], 'blueBody', 'blueRim', 'url(#water)',
        `<ellipse cx="${SPOTS.water[0] - 9}" cy="${SPOTS.water[1] - 5}" rx="10" ry="2" fill="#fff" opacity="0.8"/><ellipse cx="${SPOTS.water[0] + 8}" cy="${SPOTS.water[1] - 2}" rx="5" ry="1" fill="#fff" opacity="0.5"/>`)}
  ${night ? `
  <!-- nuit : voile bleu + lanterne sur la niche -->
  <rect width="480" height="480" fill="#081030" opacity="0.55"/>
  <circle cx="190" cy="196" r="70" fill="url(#lampGlow)"/>
  <rect x="186" y="186" width="8" height="12" rx="2" fill="#ffe6a0"/><rect x="185" y="183" width="10" height="3" fill="#3a2a1a"/>` : ''}
  <rect width="480" height="480" fill="url(#vignette)"/>
</svg>`;
}
