// ⚫ AJOUTÉ (25/10/2026) : génère les images du firmware (main/assets/*.bin) et main/pet_assets_gen.h.
//   cd tools/assets && npm install && npm run gen      (option : --preview <dossier> pour des PNG de contrôle)
// Les .bin sont intégrés au firmware par main/CMakeLists.txt (EMBED_FILES) ; pet_assets_gen.h les décrit
// (taille, format LVGL, décalage par rapport au point d'ancrage du sprite).
import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';
import { renderSvg, mirror, toLvgl } from './render.mjs';
import { sceneSvg, SPOTS } from './scene.mjs';
import { dogSvg, POSES, ANCHOR_X, GROUND_Y, mouthOf } from './dog.mjs';
import * as props from './props.mjs';

const here = path.dirname(fileURLToPath(import.meta.url));
const outDir = path.resolve(here, '../../main/assets');
const hdr = path.resolve(here, '../../main/pet_assets_gen.h');
const pi = process.argv.indexOf('--preview');
const previewDir = pi > 0 ? process.argv[pi + 1] : null;

fs.rmSync(outDir, { recursive: true, force: true });
fs.mkdirSync(outDir, { recursive: true });
if (previewDir) fs.mkdirSync(previewDir, { recursive: true });

const entries = [];

// Recadre un sprite sur ses pixels visibles (gain de place), renvoie le décalage
function crop(img) {
    const { w, h, rgba } = img;
    let x0 = w, y0 = h, x1 = -1, y1 = -1;
    for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) {
        if (rgba[(y * w + x) * 4 + 3] > 4) { x0 = Math.min(x0, x); x1 = Math.max(x1, x); y0 = Math.min(y0, y); y1 = Math.max(y1, y); }
    }
    if (x1 < 0) return { img, x0: 0, y0: 0 };
    const cw = x1 - x0 + 1, ch = y1 - y0 + 1, out = Buffer.alloc(cw * ch * 4);
    for (let y = 0; y < ch; y++) rgba.copy(out, y * cw * 4, ((y + y0) * w + x0) * 4, ((y + y0) * w + x1 + 1) * 4);
    return { img: { w: cw, h: ch, rgba: out }, x0, y0 };
}

function emit(name, img, { alpha = true, ax = 0, ay = 0, doCrop = true, png } = {}) {
    let x0 = 0, y0 = 0;
    if (doCrop && alpha) ({ img, x0, y0 } = crop(img));
    fs.writeFileSync(path.join(outDir, `${name}.bin`), toLvgl(img, alpha));
    entries.push({ name, w: img.w, h: img.h, ox: x0 - ax, oy: y0 - ay, alpha });
    if (previewDir && png) fs.writeFileSync(path.join(previewDir, `${name}.png`), png);
}

const t0 = Date.now();
for (const night of [false, true]) {
    const r = renderSvg(sceneSvg({ night }), 480);
    emit(night ? 'bg_night' : 'bg_day', r, { alpha: false, doCrop: false, png: r.png });
}

for (const pose of Object.keys(POSES)) {
    const r = renderSvg(dogSvg(pose), 200);
    emit(`dog_${pose}_r`, r, { ax: ANCHOR_X, ay: GROUND_Y, png: r.png });
    // tourné vers la gauche : même ancrage (canevas symétrique autour de ANCHOR_X)
    emit(`dog_${pose}_l`, mirror(r), { ax: r.w - ANCHOR_X, ay: GROUND_Y });
}

const P = (name, s, w) => { const r = renderSvg(s, w); emit(name, r, { ax: Math.round(r.w / 2), ay: Math.round(r.h / 2), png: r.png }); };
P('kibble', props.kibble(), 46);
P('ball', props.ball(), 18);
P('cloud_1', props.cloud(3, 130, 44), 130);
P('cloud_2', props.cloud(17, 90, 32), 90);
P('fly_1', props.butterfly(true), 22);
P('fly_2', props.butterfly(false), 22);
P('firefly', props.firefly(), 16);
P('hole', props.hole(), 56);
P('clod', props.clod(), 8);

// En-tête C (X-macro) : nom, largeur, hauteur, décalage x/y du coin haut-gauche par rapport à l'ancrage, alpha
const mouth = mouthOf('eat_1');
let h = `// ⚫ AJOUTÉ (25/10/2026) : FICHIER GÉNÉRÉ par tools/assets/gen.mjs : ne pas modifier à la main.
// Images intégrées depuis main/assets/*.bin (format LVGL 8.3, couleurs 16 bits, tramées).
#pragma once

// X(nom, largeur, hauteur, décalage x, décalage y, alpha)
#define PET_ASSETS(X) \\
`;
for (const e of entries) h += `    X(${e.name}, ${e.w}, ${e.h}, ${e.ox}, ${e.oy}, ${e.alpha ? 1 : 0}) \\\n`;
h += `
// Positions du décor (pixels écran) et bouche du chien qui mange (par rapport à son ancrage, tourné à droite)
#define SCENE_KENNEL_X   ${SPOTS.kennelDoor[0]}
#define SCENE_KENNEL_Y   ${SPOTS.kennelDoor[1]}
#define SCENE_FOOD_X     ${SPOTS.food[0]}
#define SCENE_FOOD_Y     ${SPOTS.food[1]}
#define SCENE_WATER_X    ${SPOTS.water[0]}
#define SCENE_WATER_Y    ${SPOTS.water[1]}
#define SCENE_TRACK_X    ${SPOTS.track[0]}
#define SCENE_TRACK_Y    ${SPOTS.track[1]}
#define SCENE_TRACK_RX   ${SPOTS.track[2]}
#define SCENE_TRACK_RY   ${SPOTS.track[3]}
#define DOG_MOUTH_DX     ${mouth[0]}
#define DOG_MOUTH_DY     ${mouth[1]}
`;
fs.writeFileSync(hdr, h);
const total = entries.reduce((s, e) => s + e.w * e.h * (e.alpha ? 3 : 2), 0);
console.log(`${entries.length} images, ${(total / 1024 / 1024).toFixed(2)} Mo, ${((Date.now() - t0) / 1000).toFixed(1)} s`);
