// ⚫ AJOUTÉ (25/10/2026) : rendu SVG -> pixels (resvg) et conversion au format LVGL 8.3 (couleurs 16 bits) :
// - fond opaque : LV_IMG_CF_TRUE_COLOR (RGB565 little-endian, 2 octets/pixel) ;
// - sprite : LV_IMG_CF_TRUE_COLOR_ALPHA (RGB565 + alpha, 3 octets/pixel).
// Tramage Floyd-Steinberg vers RGB565 : pas de bandes dans les dégradés (ciel, pelouse).
import { Resvg } from '@resvg/resvg-js';

export function renderSvg(svg, width) {
    const r = new Resvg(svg, { fitTo: { mode: 'width', value: width }, font: { loadSystemFonts: true } });
    const img = r.render();
    return { w: img.width, h: img.height, rgba: Buffer.from(img.pixels), png: img.asPng() };
}

// Miroir horizontal (chien tourné vers la gauche)
export function mirror({ w, h, rgba }) {
    const out = Buffer.alloc(rgba.length);
    for (let y = 0; y < h; y++) {
        for (let x = 0; x < w; x++) {
            rgba.copy(out, (y * w + (w - 1 - x)) * 4, (y * w + x) * 4, (y * w + x) * 4 + 4);
        }
    }
    return { w, h, rgba: out };
}

export function toLvgl({ w, h, rgba }, withAlpha) {
    const bpp = withAlpha ? 3 : 2;
    const out = Buffer.alloc(w * h * bpp);
    // erreurs de tramage (r, g, b) ligne courante / suivante
    let cur = new Float32Array((w + 2) * 3), nxt = new Float32Array((w + 2) * 3);
    for (let y = 0; y < h; y++) {
        for (let x = 0; x < w; x++) {
            const i = (y * w + x) * 4;
            let a = rgba[i + 3];
            // resvg rend en alpha prémultiplié : on revient aux couleurs réelles
            const un = a > 0 ? 255 / a : 0;
            const c = [0, 1, 2].map(k => (withAlpha ? rgba[i + k] * un : rgba[i + k]) + cur[(x + 1) * 3 + k]);
            const r5 = Math.max(0, Math.min(31, Math.round(c[0] * 31 / 255)));
            const g6 = Math.max(0, Math.min(63, Math.round(c[1] * 63 / 255)));
            const b5 = Math.max(0, Math.min(31, Math.round(c[2] * 31 / 255)));
            if (a > 8 || !withAlpha) {
                const q = [r5 * 255 / 31, g6 * 255 / 63, b5 * 255 / 31];
                for (let k = 0; k < 3; k++) {
                    const e = c[k] - q[k];
                    cur[(x + 2) * 3 + k] += e * 7 / 16;
                    nxt[(x) * 3 + k] += e * 3 / 16;
                    nxt[(x + 1) * 3 + k] += e * 5 / 16;
                    nxt[(x + 2) * 3 + k] += e * 1 / 16;
                }
            }
            const v = (r5 << 11) | (g6 << 5) | b5;
            const o = (y * w + x) * bpp;
            out[o] = v & 0xFF;
            out[o + 1] = v >> 8;
            if (withAlpha) out[o + 2] = a;
        }
        [cur, nxt] = [nxt, cur];
        nxt.fill(0);
    }
    return out;
}

// ⚫ AJOUTÉ (25/10/2026) : image en 256 couleurs (palette ARGB8888, entrée 0 = transparent) par coupe médiane
// sur (R, G, B, A) : 3 fois moins de place que RGB565 + alpha. Format écrit : palette 256 x 4 octets
// (B, G, R, A, ordre de lv_color32_t) puis 1 index par pixel. Le firmware la décode au démarrage.
// ⚫ AJOUTÉ (25/10/2026) : 256 couleurs compressé (format 3) : palette, puis suite de blocs :
//   octet b >= 0x80 : (b & 0x7F) + 1 pixels transparents ; b < 0x80 : b + 1 index qui suivent tels quels.
// La moitié d'une image d'animal est transparente : ~2 fois moins de place, et décodage aussi rapide.
export function toIndexedRle(img) {
    const raw = toIndexed(img);
    const pal = raw.subarray(0, 1024), d = raw.subarray(1024);
    const out = [];
    let i = 0;
    while (i < d.length) {
        let j = i;
        if (d[i] === 0) {
            while (j < d.length && d[j] === 0 && j - i < 128) j++;
            out.push(0x80 | (j - i - 1));
        } else {
            while (j < d.length && d[j] !== 0 && j - i < 128) j++;
            out.push(j - i - 1);
            for (let k = i; k < j; k++) out.push(d[k]);
        }
        i = j;
    }
    return Buffer.concat([pal, Buffer.from(out)]);
}

export function toIndexed({ w, h, rgba }) {
    const px = [];
    const idx = new Uint8Array(w * h);
    for (let i = 0; i < w * h; i++) {
        const a = rgba[i * 4 + 3];
        if (a < 6) continue;
        const un = 255 / a;
        px.push([Math.min(255, rgba[i * 4] * un), Math.min(255, rgba[i * 4 + 1] * un), Math.min(255, rgba[i * 4 + 2] * un), a, i]);
    }
    // coupe médiane : on coupe toujours la boîte la plus étendue (alpha pondéré, il compte pour les bords)
    let boxes = [px];
    const W = [1, 1.2, 0.8, 1.6];
    const range = b => {
        let best = -1, ch = 0;
        for (let c = 0; c < 4; c++) {
            let lo = 1e9, hi = -1e9;
            for (const p of b) { if (p[c] < lo) lo = p[c]; if (p[c] > hi) hi = p[c]; }
            const r = (hi - lo) * W[c] * Math.sqrt(b.length);
            if (r > best) { best = r; ch = c; }
        }
        return [best, ch];
    };
    while (boxes.length < 255) {
        let bi = -1, bestR = 0, bestC = 0;
        boxes.forEach((b, i) => { if (b.length > 1) { const [r, c] = range(b); if (r > bestR) { bestR = r; bi = i; bestC = c; } } });
        if (bi < 0) break;
        const b = boxes[bi].sort((p, q) => p[bestC] - q[bestC]);
        const m = b.length >> 1;
        boxes.splice(bi, 1, b.slice(0, m), b.slice(m));
    }
    const pal = Buffer.alloc(256 * 4);   // entrée 0 : transparente
    boxes.forEach((b, i) => {
        const s = [0, 0, 0, 0];
        for (const p of b) for (let c = 0; c < 4; c++) s[c] += p[c];
        const n = b.length || 1, k = (i + 1) * 4;
        pal[k] = Math.round(s[2] / n); pal[k + 1] = Math.round(s[1] / n); pal[k + 2] = Math.round(s[0] / n); pal[k + 3] = Math.round(s[3] / n);
        for (const p of b) idx[p[4]] = i + 1;
    });
    return Buffer.concat([pal, Buffer.from(idx)]);
}
