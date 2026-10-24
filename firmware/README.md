<!-- ⚫ AJOUTÉ (24/10/2026) : firmware prêt à flasher, sans avoir à compiler -->
<!-- ⚫ MODIFIÉ (24/10/2026) : firmware "Animal de compagnie" -->
<!-- ⚫ RETIRÉ (24/10/2026) : les 7 firmwares de démo (ZX2D10GE01R_demo_all / widgets / keypad_encoder / benchmark /
     stress / music / led .bin) et leur tableau : plus de démo dans ce projet. -->
# Firmware précompilé

Image complète (bootloader + table de partitions + application), à flasher à l'adresse `0x0`.

| Fichier | Contenu | Taille | SHA-256 |
|---|---|---|---|
<!-- ⚫ MODIFIÉ (25/10/2026) : chien dans la cour (images), soif, écran sans décalage, direct_mode -->
| `ZX2D10GE01R_pet.bin` | **Chien dans sa cour** (Kiko, temps réel, LED à 160/255) | 4 150 912 o | `ba227338d54190e421ba16966be71ffde75e49f3b505d17c9da1549cf5574afc` |

Vérifié sur la carte (25/10/2026) : flash (hash vérifié), démarrage unique sans erreur, lignes
`PET_UI: Chien prêt (Kiko), 63 images` puis `RGB: 25 images/s, rendu 28 ms/image` dans le journal série.

- Cible : ESP32-S3 N16R8 (flash 16 Mo, PSRAM octale 8 Mo), ESP-IDF v5.0.9.

## Flasher

```sh
esptool.py --chip esp32s3 -p COM4 -b 460800 write_flash 0x0 ZX2D10GE01R_pet.bin
```

Remplacer `COM4` par le port de la carte.
Ou avec l'outil web : https://espressif.github.io/esptool-js/ (adresse `0x0`).

La sauvegarde de l'animal (NVS) n'est pas écrasée par ce flash : pour repartir avec un nouvel animal,
lancer d'abord `esptool.py --chip esp32s3 -p COM4 erase_flash`.

## Régénérer

1. *(si les images ont changé)* `cd tools/assets && npm install && npm run gen`
2. `idf.py build` (réglages éventuels dans `idf.py menuconfig` → **Animal de compagnie**)
3. Dans `build/` :
   ```sh
   esptool.py --chip esp32s3 merge_bin -o ../firmware/ZX2D10GE01R_pet.bin @flash_args
   ```
