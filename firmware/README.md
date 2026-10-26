<!-- ⚫ AJOUTÉ (24/10/2026) : firmware prêt à flasher, sans avoir à compiler -->
<!-- ⚫ MODIFIÉ (24/10/2026) : firmware "Animal de compagnie" -->
<!-- ⚫ RETIRÉ (24/10/2026) : les 7 firmwares de démo (ZX2D10GE01R_demo_all / widgets / keypad_encoder / benchmark /
     stress / music / led .bin) et leur tableau : plus de démo dans ce projet. -->
# Firmware précompilé

<!-- ⚫ MODIFIÉ (25/10/2026) : l'image contient aussi le pack d'images (partition "assets" à 0x410000) -->
<!-- ⚫ MODIFIÉ (25/10/2026) : partition "assets" désormais à 0x210000 (application 2 Mo) -->
Image complète (bootloader + table de partitions + application + **pack d'images** des animaux, partition
`assets` à 0x210000), à flasher à l'adresse `0x0`.

| Fichier | Contenu | Taille | SHA-256 |
|---|---|---|---|
<!-- ⚫ MODIFIÉ (25/10/2026) : 6 animaux à adopter, page web, QR code -->
<!-- ⚫ MODIFIÉ (25/10/2026) : 10 espèces x 3 âges, météo réelle, IA, sons, commandes web -->
<!-- ⚫ MODIFIÉ (25/10/2026) : + 41 sons réalistes dans le pack -->
| `ZX2D10GE01R_pet.bin` | **Animal de compagnie** : 10 espèces × 3 âges, cour avec abri, heure et météo réelles, IA, sons réalistes, page web (LED à 160/255) | 16358776 o | `c471f487b2c9717d3eded4fe6954dc868529c2647b817bf9fd996f3a431ff989` |

<!-- ⚫ MODIFIÉ (25/10/2026) : nouvelle vérification -->
Vérifié sur la carte (25/10/2026, mêmes sources) : démarrage sans erreur, lignes
`ASSETS: Pack : 1371 images, 13863 Ko`, `WIFI: Connecté à …`, `METEO: … ciel dégagé …`, `IA: Google Gemini : …` puis
`RGB: 26 images/s, rendu 17 ms/image` dans le journal série.

- Cible : ESP32-S3 N16R8 (flash 16 Mo, PSRAM octale 8 Mo), ESP-IDF v5.0.9.

## Flasher

```sh
esptool.py --chip esp32s3 -p COM4 -b 921600 write_flash 0x0 ZX2D10GE01R_pet.bin
```

<!-- ⚫ MODIFIÉ (25/10/2026) : ~4 min (était ≈ 2 min) -->
Remplacer `COM4` par le port de la carte (≈ 4 min).
Ou avec l'outil web : https://espressif.github.io/esptool-js/ (adresse `0x0`).

<!-- ⚫ MODIFIÉ (25/10/2026) : constaté sur la carte, l'ancienne phrase était fausse :
La sauvegarde de l'animal (NVS) n'est pas écrasée par ce flash : pour repartir avec un nouvel animal,
lancer d'abord `esptool.py --chip esp32s3 -p COM4 erase_flash`.
-->
⚠️ Cette image complète **efface la sauvegarde** (NVS, à 0x9000 : animal adopté, WiFi de la maison, position,
réglages de l'IA et du son) : `merge_bin` comble l'espace entre le bootloader et l'application avec du vide. Au
démarrage suivant, l'écran d'adoption s'affiche. Pour mettre à jour en **gardant** l'animal, flasher depuis les
sources : `idf.py -p COM4 flash`.

## Régénérer

1. *(si les images ont changé)* `cd tools/assets && npm install && npm run gen`
2. `idf.py build` (réglages éventuels dans `idf.py menuconfig` → **Animal de compagnie**)
3. Dans `build/` :
   ```sh
   esptool.py --chip esp32s3 merge_bin -o ../firmware/ZX2D10GE01R_pet.bin @flash_args
   ```
