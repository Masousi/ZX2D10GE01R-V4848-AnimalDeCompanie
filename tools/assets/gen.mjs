// ⚫ AJOUTÉ (25/10/2026) : génère les images du firmware et main/pet_assets_gen.h.
//   cd tools/assets && npm install && npm run gen      (option : --preview <dossier> pour des PNG de contrôle)
// ⚫ MODIFIÉ (25/10/2026) : plusieurs animaux : toutes les images sont rangées dans UN fichier "pack"
// (assets/pet_assets.bin) écrit dans la partition flash "assets" (cf. partitions_lvgl.csv, CMakeLists.txt) et
// lu directement depuis la flash par le firmware (main/pet_assets.c) ; plus d'EMBED_FILES (main/assets/*.bin
// RETIRÉ). Animaux : une seule orientation (vers la droite), en 256 couleurs ; le firmware décode l'espèce
// choisie et fabrique l'orientation gauche en PSRAM. Décor et accessoires : RGB565 (+ alpha), tramés.
//
// Format du pack (little-endian) :
//   en-tête  : 'PETA' (u32), version (u32), nombre d'images (u32), réservé (u32)
//   entrées  : par image { offset u32, taille u32, largeur u16, hauteur u16, dx i16, dy i16, format u8, 3 x 0 }
//   données  : alignées sur 4 octets
//   formats  : 0 = RGB565, 1 = RGB565 + alpha (3 o/pixel), 2 = 256 couleurs (palette 256 x BGRA puis index)
//              ⚫ AJOUTÉ (25/10/2026) : 3 = 256 couleurs compressé (transparence en plages, voir toIndexedRle)
// ⚫ MODIFIÉ (25/10/2026) : 3 âges par espèce (bébé, adolescent, adulte), dessinés chacun avec ses proportions
import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';
// import { renderSvg, toLvgl, toIndexed } from './render.mjs';
import { renderSvg, toLvgl, toIndexed, toIndexedRle } from './render.mjs';   // ⚫ 25/10/2026
import { sceneSvg, SPOTS } from './scene.mjs';
// import { animalSvg, SPECIES, POSES, canvasOf, mouthOf, rearOf } from './animals.mjs';
import { animalSvg, SPECIES, POSES, STAGES, canvasOf, mouthOf, rearOf } from './animals.mjs';   // ⚫ 25/10/2026
import * as props from './props.mjs';
import * as props2 from './props2.mjs';
import * as props3 from './props3.mjs';   // ⚫ AJOUTÉ (25/10/2026)

const here = path.dirname(fileURLToPath(import.meta.url));
const packPath = path.resolve(here, '../../assets/pet_assets.bin');
const hdr = path.resolve(here, '../../main/pet_assets_gen.h');
const pi = process.argv.indexOf('--preview');
const previewDir = pi > 0 ? process.argv[pi + 1] : null;
if (previewDir) fs.mkdirSync(previewDir, { recursive: true });
fs.mkdirSync(path.dirname(packPath), { recursive: true });
// ⚫ RETIRÉ (25/10/2026) : ancien dossier main/assets (une image = un .bin intégré à l'application)
fs.rmSync(path.resolve(here, '../../main/assets'), { recursive: true, force: true });

const entries = [];   // { name, w, h, ox, oy, cf, data }

// Recadre un sprite sur ses pixels visibles, renvoie le décalage
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

// cf : 0 = RGB565 (fond), 1 = RGB565 + alpha, 2 = 256 couleurs
function emit(name, img, { cf = 1, ax = 0, ay = 0, png } = {}) {
    let x0 = 0, y0 = 0;
    if (cf !== 0) ({ img, x0, y0 } = crop(img));
    // const data = cf === 2 ? toIndexed(img) : toLvgl(img, cf === 1);
    const data = cf === 3 ? toIndexedRle(img) : cf === 2 ? toIndexed(img) : toLvgl(img, cf === 1);   // ⚫ 25/10/2026
    entries.push({ name, w: img.w, h: img.h, ox: x0 - ax, oy: y0 - ay, cf, data });
    if (previewDir && png) fs.writeFileSync(path.join(previewDir, `${name}.png`), png);
}

const t0 = Date.now();

// décor
// for (const night of [false, true]) { … bg_day / bg_night … }
// ⚫ MODIFIÉ (25/10/2026) : 8 fonds (heure réelle x météo réelle) + premier plan de l'abri pour chacun
// (pixels du fond là où sont le toit et les poteaux avant : dessinés devant l'animal quand il est dessous)
const BGS = [['bg_day', 'day', 'clear'], ['bg_day_cloudy', 'day', 'cloudy'], ['bg_day_rain', 'day', 'rain'], ['bg_day_snow', 'day', 'snow'],
             ['bg_dusk', 'dusk', 'clear'], ['bg_night', 'night', 'clear'], ['bg_night_cloudy', 'night', 'cloudy'], ['bg_night_snow', 'night', 'snow']];
const bgRenders = BGS.map(([name, tod, wx]) => {
    const r = renderSvg(sceneSvg({ tod, wx }), 480);
    emit(name, r, { cf: 0, png: r.png });
    return { name, wx, r };
});

// accessoires (ancrage = centre)
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
P('frisbee', props.frisbee(), 34);
P('bone', props.bone(), 34);
// ⚫ AJOUTÉ (25/10/2026)
P('bowl_food', props2.bowl('food', false), 110);
P('bowl_food_tip', props2.bowl('food', true), 110);
P('bowl_water', props2.bowl('water', false), 110);
P('bowl_water_tip', props2.bowl('water', true), 110);
P('hand_1', props2.hand(false), 112);
P('hand_2', props2.hand(true), 112);
P('bird_up', props2.bird('up'), 30);
P('bird_down', props2.bird('down'), 30);
P('bird_stand', props2.bird('stand'), 30);
P('bird_peck', props2.bird('peck'), 30);
P('poop', props2.poop(), 30);
// ⚫ AJOUTÉ (25/10/2026) : jeux propres à chaque espèce, gestes, météo
P('hand_scold', props3.handScold(), 72);
P('hand_point', props3.handPoint(), 84);
for (let i = 0; i < 4; i++) P(`hourglass_${i + 1}`, props3.hourglass(i / 3), 28);
P('yarn', props3.yarn(), 28);
P('wand_1', props3.wand(0), 96);
P('wand_2', props3.wand(1), 96);
P('hurdle', props3.hurdle(), 64);
P('hand_carrot', props3.handCarrot(), 100);
P('carrot_bit', props3.carrotBit(), 22);
P('hay', props3.hay(), 80);
P('mud', props3.mud(), 120);
P('hand_brush_1', props3.handBrush(false), 96);
P('hand_brush_2', props3.handBrush(true), 96);
P('grass', props3.grass(), 48);
P('drop', props3.drop(), 4);
P('splash', props3.splash(), 16);
P('flake', props3.flake(), 10);
P('cloud_dark', props3.cloudDark(4, 140, 46), 140);
P('dirt', props3.dirt(), 56);        // ⚫ AJOUTÉ (25/10/2026) : trou rebouché
P('sprouts', props3.sprouts(), 56);  // ⚫ AJOUTÉ (25/10/2026) : gazon qui repousse
P('heart_crack', props3.heartBroken(false), 44);   // ⚫ AJOUTÉ (25/10/2026) : punition
P('heart_split', props3.heartBroken(true), 44);
P('cone_up', props3.coneUp(), 22);      // ⚫ AJOUTÉ (25/10/2026) : cônes de la piste
P('cone_down', props3.coneDown(), 30);
// premier plan de l'abri, un par fond (ancrage 0,0 : position écran)
for (const { name, wx, r } of bgRenders) {
    const mask = renderSvg(sceneSvg({ shelterOnly: true, wx }), 480);
    const k = Buffer.alloc(r.rgba.length);
    for (let i = 0; i < r.w * r.h; i++) {
        const a = mask.rgba[i * 4 + 3];
        for (let c = 0; c < 3; c++) k[i * 4 + c] = Math.round(r.rgba[i * 4 + c] * a / 255);
        k[i * 4 + 3] = a;
    }
    emit(name.replace('bg_', 'shelter_'), { w: r.w, h: r.h, rgba: k });
}
const propCount = entries.length;

// animaux : espèce par espèce, toutes les poses dans le même ordre
const speciesIds = Object.keys(SPECIES), poseIds = Object.keys(POSES);
const animalBase = entries.length;
// for (const sp of speciesIds) {
//     const cv = canvasOf(sp);
//     for (const pose of poseIds) {
//         const r = renderSvg(animalSvg(sp, pose), cv.W);
//         emit(`${sp}_${pose}`, r, { cf: 2, ax: cv.ax, ay: cv.gy, png: r.png });
//     }
//     console.log(`${sp} : ${poseIds.length} poses`);
// }
// ⚫ MODIFIÉ (25/10/2026) : âge par âge, puis espèce par espèce ; compressé (format 3)
// ⚫ AJOUTÉ (25/10/2026) : --keep-animals : reprend les images d'animaux du pack existant (seul le décor change :
// quelques secondes au lieu de ~12 min)
const keep = process.argv.includes('--keep-animals');
let old = null;
if (keep) {
    const b = fs.readFileSync(packPath), n = b.readUInt32LE(8), nA = STAGES.length * speciesIds.length * poseIds.length;
    old = [];
    for (let i = n - nA; i < n; i++) {
        const o = 16 + i * 20, off = b.readUInt32LE(o), size = b.readUInt32LE(o + 4);
        old.push({ w: b.readUInt16LE(o + 8), h: b.readUInt16LE(o + 10), ox: b.readInt16LE(o + 12), oy: b.readInt16LE(o + 14),
                   cf: b.readUInt8(o + 16), data: Buffer.from(b.subarray(off, off + size)) });
    }
}
STAGES.forEach((st, stage) => {
    if (old) {
        for (const sp of speciesIds) for (const pose of poseIds) entries.push({ name: `${sp}_${st}_${pose}`, ...old.shift() });
        return;
    }
    for (const sp of speciesIds) {
        const cv = canvasOf(sp, stage);
        for (const pose of poseIds) {
            const r = renderSvg(animalSvg(sp, pose, stage), cv.W);
            emit(`${sp}_${st}_${pose}`, r, { cf: 3, ax: cv.ax, ay: cv.gy, png: r.png });
        }
    }
    console.log(`${st} : ${speciesIds.length} espèces x ${poseIds.length} poses`);
});

// pack
const head = Buffer.alloc(16 + entries.length * 20);
head.write('PETA', 0, 'ascii');
head.writeUInt32LE(1, 4);
head.writeUInt32LE(entries.length, 8);
let off = head.length;
const chunks = [head];
entries.forEach((e, i) => {
    const pad = (4 - (off % 4)) % 4;
    if (pad) { chunks.push(Buffer.alloc(pad)); off += pad; }
    const o = 16 + i * 20;
    head.writeUInt32LE(off, o);
    head.writeUInt32LE(e.data.length, o + 4);
    head.writeUInt16LE(e.w, o + 8);
    head.writeUInt16LE(e.h, o + 10);
    head.writeInt16LE(e.ox, o + 12);
    head.writeInt16LE(e.oy, o + 14);
    head.writeUInt8(e.cf, o + 16);
    chunks.push(e.data);
    off += e.data.length;
});
fs.writeFileSync(packPath, Buffer.concat(chunks));

// en-tête C
let h = `// ⚫ AJOUTÉ (25/10/2026) : FICHIER GÉNÉRÉ par tools/assets/gen.mjs : ne pas modifier à la main.
// Décrit le pack d'images assets/pet_assets.bin (partition flash "assets", lu par main/pet_assets.c).
#pragma once

#define PET_PACK_MAGIC     0x41544550u   // 'PETA'
#define PET_PACK_COUNT     ${entries.length}
#define PET_PACK_SIZE      ${off}

// Décor et accessoires : X(nom, index dans le pack)
#define PET_ASSETS(X) \\
`;
for (let i = 0; i < propCount; i++) h += `    X(${entries[i].name}, ${i}) \\\n`;
h += `
// Espèces : X(id, nom affiché, cri)
#define PET_SPECIES(X) \\
${speciesIds.map(s => `    X(${s}, "${SPECIES[s].label}", "${SPECIES[s].sound}") \\`).join('\n')}

#define PET_SPECIES_COUNT  ${speciesIds.length}

// Poses (même ordre pour toutes les espèces) : image = PET_ANIMAL_BASE + espèce * PET_POSE_COUNT + pose
// ⚫ MODIFIÉ (25/10/2026) : image = PET_ANIMAL_BASE + (âge * PET_SPECIES_COUNT + espèce) * PET_POSE_COUNT + pose
#define PET_POSES(X) \\
${poseIds.map(p => `    X(${p}) \\`).join('\n')}

#define PET_POSE_COUNT     ${poseIds.length}
#define PET_ANIMAL_BASE    ${animalBase}
#define PET_AGE_COUNT      ${STAGES.length}   // ⚫ AJOUTÉ (25/10/2026) : 0 bébé, 1 adolescent, 2 adulte

// Gueule de l'animal par espèce et par pose, par rapport à son ancrage (tourné vers la droite)
// ⚫ MODIFIÉ (25/10/2026) : par âge
static const short PET_MOUTH[PET_AGE_COUNT][PET_SPECIES_COUNT][PET_POSE_COUNT][2] = {
${STAGES.map((_, st) => '  {\n' + speciesIds.map(sp => '    { ' + poseIds.map(p => { const m = mouthOf(sp, p, st); return `{ ${m[0]}, ${m[1]} }`; }).join(', ') + ' },').join('\n') + '\n  },').join('\n')}
};

// ⚫ AJOUTÉ (25/10/2026) : arrière-train (pose "besoins") par rapport à l'ancrage, tourné vers la droite
// static const short PET_REAR[PET_SPECIES_COUNT] = { ${speciesIds.map(sp => rearOf(sp)[0]).join(', ')} };
static const short PET_REAR[PET_AGE_COUNT][PET_SPECIES_COUNT] = {   // ⚫ MODIFIÉ (25/10/2026) : par âge
${STAGES.map((_, st) => '    { ' + speciesIds.map(sp => rearOf(sp, 'potty', st)[0]).join(', ') + ' },').join('\n')}
};

// Positions du décor (pixels écran)
#define SCENE_BED_X      ${SPOTS.bed[0]}
#define SCENE_BED_Y      ${SPOTS.bed[1]}
#define SCENE_TOILET_X   ${SPOTS.toilet[0]}
#define SCENE_TOILET_Y   ${SPOTS.toilet[1]}
#define SCENE_FOOD_X     ${SPOTS.food[0]}
#define SCENE_FOOD_Y     ${SPOTS.food[1]}
#define SCENE_WATER_X    ${SPOTS.water[0]}
#define SCENE_WATER_Y    ${SPOTS.water[1]}
#define SCENE_TRACK_X    ${SPOTS.track[0]}
#define SCENE_TRACK_Y    ${SPOTS.track[1]}
#define SCENE_TRACK_RX   ${SPOTS.track[2]}
#define SCENE_TRACK_RY   ${SPOTS.track[3]}
#define SCENE_MESS_X0    ${SPOTS.mess[0]}
#define SCENE_MESS_Y0    ${SPOTS.mess[1]}
#define SCENE_MESS_X1    ${SPOTS.mess[2]}
#define SCENE_MESS_Y1    ${SPOTS.mess[3]}
#define SCENE_CONE0_X    ${SPOTS.cones[0][0]}   // ⚫ AJOUTÉ (25/10/2026) : base des cônes
#define SCENE_CONE0_Y    ${SPOTS.cones[0][1]}
#define SCENE_CONE1_X    ${SPOTS.cones[1][0]}
#define SCENE_CONE1_Y    ${SPOTS.cones[1][1]}
#define SCENE_SHELTER_X0 ${SPOTS.shelter[0]}
#define SCENE_SHELTER_Y0 ${SPOTS.shelter[1]}
#define SCENE_SHELTER_X1 ${SPOTS.shelter[2]}
#define SCENE_SHELTER_Y1 ${SPOTS.shelter[3]}
`;
fs.writeFileSync(hdr, h);
console.log(`${entries.length} images, pack ${(off / 1024 / 1024).toFixed(2)} Mo, ${((Date.now() - t0) / 1000).toFixed(1)} s`);
