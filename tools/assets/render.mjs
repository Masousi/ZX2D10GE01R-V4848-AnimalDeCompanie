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
