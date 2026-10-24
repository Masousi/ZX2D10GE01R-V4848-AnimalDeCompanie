# ZX2D10GE01R : Kiko, chien virtuel dans sa cour
<!-- ⚫ MODIFIÉ (25/10/2026) : titre (était « ZX2D10GE01R : Animal de compagnie ») -->
ZX2D10GE01R base on https://github.com/espressif/esp-idf/tree/release/v5.0

<!-- ⚫ MODIFIÉ (24/10/2026) : le projet devient un firmware "Animal de compagnie" (type Tamagotchi) -->
<!-- ⚫ RETIRÉ (24/10/2026) : sections sur les démos LVGL (menu principal, Widgets, Keypad and encoder, Benchmark,
     Stress, Music, Animations LED, chenillard, options "ZX2D10 Demo", firmwares par démo) : plus de démo dans ce projet. -->
> **Origine :** fork de [wireless-tag-com/ZX2D10GE01R-V4848](https://github.com/wireless-tag-com/ZX2D10GE01R-V4848)
> (projet d'origine du fabricant de la carte). Cette version remplace les démos LVGL par un **animal de compagnie
> virtuel** façon Tamagotchi, piloté à l'encodeur, dont l'état est aussi affiché sur l'anneau de LED.
>
> <!-- ⚫ AJOUTÉ (25/10/2026) -->
> L'animal est un **chien qui vit dans sa cour** : niche pour dormir, piste pour courir, gamelle de croquettes,
> gamelle d'eau, cycle jour / nuit. Le décor et le chien (26 poses, dans les deux sens) sont de vraies images,
> générées par `tools/assets` (voir plus bas).

<!-- ⚫ AJOUTÉ (25/10/2026) : aperçu du décor et du chien (rendu des images du firmware, sans l'interface) -->
| Le jour : il mange, il joue | La nuit : il dort devant sa niche |
|---|---|
| ![Cour de jour](docs/apercu_jour.png) | ![Cour de nuit](docs/apercu_nuit.png) |

*Aperçus générés à partir des images du firmware (les jauges, la bulle et le sélecteur d'actions s'affichent par-dessus).*

## Matériel / environnement

- ESP32-S3 N16R8 : flash 16 Mo, PSRAM **octale** 8 Mo, écran rond RGB 480×480 (frame buffer en PSRAM, `fb_in_psram`) + encodeur MT8901.
- Encodeur : rotation sur GPIO 5/6 (MT8901), bouton-poussoir sur GPIO 3 (actif bas), anneau de **13 LED WS2812** sur GPIO 4.
- ESP-IDF **v5.0.9**.

## Jouer

| Geste | Effet |
|---|---|
| **Tourner** l'encodeur | parcourir les actions (liste circulaire en bas de l'écran) |
| **Appuyer** | réaliser l'action sélectionnée |
| Appuyer pendant que l'animal **dort** | le réveiller (grognon s'il n'a pas assez dormi) |

### Les actions

| Action | Effet |
|---|---|
| **Manger** | va à sa gamelle remplie de croquettes : satiété +30 (refuse s'il n'a plus faim) |
| **Boire** <!-- ⚫ AJOUTÉ (25/10/2026) --> | lape à la gamelle d'eau : hydratation +35 (refuse s'il n'a pas soif) |
| **Dormir** | va se coucher devant sa niche, la nuit tombe ; recharge son énergie (refuse s'il n'est pas fatigué) ; se réveille seul à 100 % |
| **Courir** <!-- ⚫ MODIFIÉ (25/10/2026) : était « Jouer » --> | court deux tours de piste derrière la balle puis saute de joie : bonheur +20, mais fatigue, donne faim et soif |
| **Balle** <!-- ⚫ AJOUTÉ (25/10/2026) --> | la balle est lancée au fond de la cour : il court la chercher et la rapporte dans la gueule (bonheur +15, discipline +3) |
| **Frisbee** <!-- ⚫ AJOUTÉ (25/10/2026) --> | le frisbee plane : il court dessous, saute pour l'attraper au vol et le rapporte (bonheur +22, fatigue davantage) |
| **Os à ronger** <!-- ⚫ AJOUTÉ (25/10/2026) --> | jeu calme, possible même fatigué : couché, il ronge son os (bonheur +12) ; console un chagrin, détourne d'une bêtise |
| **Caresser** | bonheur +10 ; pendant une bêtise, ça l'encourage (discipline −6) |
| **Gronder** | corrige une bêtise (discipline +15) ; **sans raison**, il pleure |
| **Punir** | l'envoie à la niche, corrige fortement une bêtise (discipline +30, petit chagrin) ; **sans raison**, il pleure et perd de la santé |
| **Réconforter** | sèche ses larmes (bonheur +18) ; console aussi quand il s'ennuie |

### L'écran
<!-- ⚫ MODIFIÉ (25/10/2026) : chien dans sa cour -->

- **La cour** : palissade, arbres, pelouse, niche rouge, piste en sable avec plots, gamelles rouge (croquettes) et
  bleue (eau). Le jour : soleil, nuages qui passent, papillon. La nuit (quand le chien dort) : lune, étoiles,
  lanterne sur la niche, lucioles.
- **Le chien** contourne sa niche par l'avant, et passe derrière elle (elle le masque) quand il court au bout de la
  piste. <!-- ⚫ AJOUTÉ (25/10/2026) -->
- **Le chien** se déplace dans la cour selon son humeur : il attend devant sa gamelle quand il a faim ou soif,
  creuse un trou dans la pelouse quand il fait une bêtise (mottes de terre), se couche quand il est fatigué ou
  malade, pleure assis, se balade en remuant la queue quand il est heureux, dort devant la niche (« Zzz »).
- **Bord** : 6 jauges en arc — à gauche Faim (orange), Soif (bleu), Énergie (jaune) ; à droite Bonheur (rose),
  Discipline (violet), Santé (vert). L'icône d'une jauge clignote quand elle passe sous 20 %.
- **Haut** : nom et âge (**Bébé** < 1 h, **Enfant** < 6 h, puis **Adulte**) ; bulle de dialogue quand il parle.
- **Bas** : le sélecteur d'actions (une action à la fois, tourner pour changer).

### Règles (temps réel, accélérable)

<!-- ⚫ MODIFIÉ (25/10/2026) : + soif -->
- Éveillé, la satiété se vide en ~1 h, l'hydratation en ~45 min, l'énergie en ~1 h 30, le bonheur en ~45 min
  (deux fois plus vite s'il a faim, soif ou est épuisé), la discipline en ~3 h. Endormi, l'énergie remonte en
  ~10 min ; la faim et la soif progressent deux fois moins vite.
- Énergie à 0 : il s'endort d'épuisement. Satiété, hydratation ou bonheur à 0 : la santé baisse ; elle remonte
  quand tout va bien. Jouer donne faim et soif.
- Il fait des **bêtises** au hasard, d'autant plus souvent que sa discipline est basse : il faut le gronder ou le punir.
- S'il pleure et que personne ne le console pendant 2 min, il boude (bonheur −10).
- L'état est **sauvegardé en flash** (NVS) toutes les 5 min et quand il s'endort : on le retrouve après un
  redémarrage (une coupure de courant fait perdre au plus 5 min de jeu). <!-- ⚫ MODIFIÉ (25/10/2026) : était
  « après chaque action » : chaque écriture flash perturbe brièvement l'écran -->

### L'anneau de LED

| Indication | Signification |
|---|---|
| Nombre de LED allumées | **bien-être global** (moyenne des jauges et de la santé) |
| Vert, vague douce | heureux |
| Blanc bleuté, respiration | va bien |
| Orange clignotant | a faim |
| Cyan clignotant <!-- ⚫ AJOUTÉ (25/10/2026) --> | a soif |
| Jaune pâle, respiration lente | fatigué |
| Bleu, respiration lente | s'ennuie / triste |
| Bleu clair, gouttes | pleure |
| Rouge / magenta alterné rapide | fait une bêtise |
| Vert acide, double battement | malade |
| Indigo très lent | dort |

Effets courts à chaque action : arc-en-ciel (joie), battements roses (câlin), comète orange (repas), vaguelettes bleues (boit),
extinction indigo (s'endort), lever de soleil (réveil), double clignotement ambre (refus), balayage violet
(bêtise corrigée), flash rouge puis bleu (gronder / punir sans raison).

## Options du projet (`menuconfig` → **Animal de compagnie**)

| Option | Défaut | Rôle |
|---|---|---|
| *Nom de l'animal* | Kiko | nom affiché et utilisé dans les messages |
| *Accélération du temps* | 1 | multiplie la vitesse du jeu (ex. 30 : une heure passe en 2 min) — pratique pour tester |
| *Sauvegarde en flash toutes les N secondes* | 300 <!-- ⚫ 25/10/2026 : était 60 --> | sauvegarde périodique (en plus de celle à l'endormissement) |
| *Anneau de LED → Nombre de LED* | 13 | nombre de LED de l'anneau |
| *Anneau de LED → Sens inversé* | ❌ | inverse le sens de la jauge |
| *Anneau de LED → Luminosité max* | 160 | luminosité maximale (l'anneau reste allumé en permanence) |

Pour repartir avec un **nouvel animal** : `idf.py -p COM4 erase-flash` puis reflasher (efface la sauvegarde NVS).
<!-- ⚫ AJOUTÉ (25/10/2026) -->
Flasher l'image complète `firmware/ZX2D10GE01R_pet.bin` à `0x0` efface aussi la sauvegarde ; `idf.py flash` la conserve.

## Compiler et flasher

```sh
idf.py -p COM4 build flash monitor
```

Dans VS Code, sans terminal : `Ctrl+Shift+P` → *Tasks: Run Task* → **ESP: Build + Flash + Monitor**
(tâches dans `.vscode/tasks.json`, fichier local non versionné), ou les boutons Build / Flash / Monitor
de l'extension ESP-IDF. Quitter le moniteur : `Ctrl+]`.

Journal série attendu au démarrage :

```
esp_psram: Found 8MB PSRAM device
PET: Nouvel animal : Kiko            (ou "PET: Animal rechargé (âge … s)")
PET_UI: Chien prêt (Kiko), 67 images
```

Chaque action est tracée : `PET: Action <n> -> <message> (faim …, soif …, énergie …, joie …, discipline …, santé …)`.

<!-- ⚫ AJOUTÉ (25/10/2026) -->
Toutes les 10 s : `RGB: <n> images/s, rendu <t> ms/image` (≈ 20 à 27 images/s selon l'animation).

Firmware prêt à flasher : voir [`firmware/`](firmware/README.md). Avec les images, l'application fait ~4 Mo :
compter ~50 s de flash à 921 600 bauds (`idf.py -p COM4 -b 921600 flash`).

### Dépannage

| Symptôme | Cause probable | Solution |
|---|---|---|
| Écran noir + `no mem for frame buffer` + redémarrages | PSRAM désactivée (`sdkconfig` régénéré) | supprimer `sdkconfig`, puis `idf.py build` (reprend `sdkconfig.defaults`) |
| `SPI Flash Size : 2MB` dans le journal | flash mal configurée | *Serial flasher config → Flash size* = 16 MB |
| L'image se décale vers la droite après une action <!-- ⚫ AJOUTÉ (25/10/2026) --> | l'écriture en flash (sauvegarde) coupe l'accès PSRAM du DMA écran | `CONFIG_LCD_RGB_RESTART_IN_VSYNC` + `CONFIG_LCD_RGB_ISR_IRAM_SAFE` (`sdkconfig.defaults`, corrigé) |
| Des éléments disparaissent une image sur deux pendant que le chien mange ou joue <!-- ⚫ AJOUTÉ (25/10/2026) --> | bascule de frame buffer : signal de fin de trame périmé, LVGL dessinait dans le buffer encore affiché | signal vidé après la bascule, attente d'une fin de trame postérieure (`main/screen.c`, corrigé) |
| L'image bouge quand on valide une action <!-- ⚫ AJOUTÉ (25/10/2026) --> | une écriture flash suspend le remplissage des bounce buffers (ils ne peuvent pas lire la PSRAM pendant l'écriture) | bounce buffers de 30 lignes, lignes de cache de 64 octets (`CONFIG_ESP32S3_DATA_CACHE_LINE_64B`), plus de sauvegarde à l'action : toutes les 5 min et à l'endormissement (`main/pet.c`) |
| Le chien traverse sa niche <!-- ⚫ AJOUTÉ (25/10/2026) --> | décor peint d'un seul bloc, chien dessiné par-dessus, trajets en ligne droite | contournement par le coin avant-droit + niche redessinée devant lui quand il est derrière (`kennel_d` / `kennel_n`) (corrigé) |
| L'image se décale un instant quand le chien bouge (la tête, les pattes) <!-- ⚫ AJOUTÉ (25/10/2026) --> | le DMA de l'écran lit la PSRAM en même temps que le rendu et la lecture des images : débit insuffisant | bounce buffers en RAM interne (`main/screen.c`), interruption de l'écran sur le cœur 0 (`screen_init()` dans `app_main`) (corrigé) |
| Animation saccadée (< 10 images/s) <!-- ⚫ AJOUTÉ (25/10/2026) --> | tout l'écran redessiné à chaque image (`full_refresh`) ou bounce buffers actifs | `direct_mode` + synchro des 2 buffers en fin de trame dans `main/screen.c` (corrigé) |
| Écran noir, ou image incomplète dès qu'une animation démarre | les buffers LVGL sont les frame buffers du panneau : sans `full_refresh` ni synchro, l'autre buffer n'est pas à jour | `direct_mode` + recopie des zones modifiées dans l'autre buffer après le VSYNC, dans `main/screen.c` (corrigé ; ⚫ 25/10/2026 : remplace `full_refresh = 1`) |
| L'encodeur ne fait rien | bouton déclaré actif haut : LVGL le croit appuyé et ignore la rotation | `button_attch(BTN_PIN, 0, 10)` dans `main/screen.c` (corrigé) |
| La jauge de LED tourne à l'envers | sens de l'anneau | *Anneau de LED → Sens inversé* |
| La carte redémarre quand l'anneau s'illumine | alimentation USB insuffisante | port/chargeur USB plus puissant, ou *Luminosité max* plus bas |
| Accents ou icônes remplacés par des carrés | caractère hors des polices générées (`main/font_*.c`) | régénérer la police avec la commande indiquée en tête du fichier, plage élargie |
| Projet copié / renommé : erreurs CMake sur un autre chemin | `build/` contient le chemin de l'ancien dossier | supprimer `build/` et recompiler |

## Compiler depuis VS Code (extension ESP-IDF)

- Version d'ESP-IDF : **v5.0.9** (*ESP-IDF: Select Current ESP-IDF Version*), cible **esp32s3**
  (*ESP-IDF: Set Espressif Device Target*). ⚠️ Changer la cible régénère `sdkconfig` : `sdkconfig.defaults`
  remet les valeurs indispensables.
- Port **COM4** (CH343), méthode de flash **UART**.
- Dans un espace de travail multi-dossiers : *ESP-IDF: Pick a Workspace Folder* → ce projet, avant Build/Flash.
- **Un seul build à la fois** : deux builds simultanés (extension + terminal) sur le même dossier `build/`
  provoquent des erreurs `ninja: failed recompaction: Permission denied` ou des `.obj` introuvables.
- Le moniteur série occupe COM4 : le fermer (`Ctrl+]`) avant un flash lancé ailleurs.

## Organisation du code

| Fichier | Rôle |
|---|---|
| `main/main.c` | initialisation des périphériques (NVS, écran + LVGL + encodeur, anneau de LED), tâche LVGL |
| `main/screen.c` | panneau RGB 480×480 (`direct_mode`, 2 frame buffers synchronisés en fin de trame, bounce buffers, mesure d'images/s), encodeur LVGL (rotation + bouton) |
| `main/pet.c`, `main/pet.h` | modèle de l'animal : jauges, humeur, actions, évolution, sauvegarde NVS |
| `main/pet_ui.c`, `main/pet_ui.h` | écran : la cour, le chien et ses déplacements (scripts), jauges, bulle, sélecteur d'actions |
| `main/pet_assets.c`, `main/pet_assets.h`, `main/pet_assets_gen.h` <!-- ⚫ AJOUTÉ (25/10/2026) --> | images intégrées (`main/assets/*.bin`) et leurs descripteurs LVGL |
| `tools/assets/` <!-- ⚫ AJOUTÉ (25/10/2026) --> | générateur des images : décor (`scene.mjs`), chien articulé (`dog.mjs`), accessoires (`props.mjs`), rendu SVG → RGB565 tramé (`render.mjs`) |
| `main/pet_led.c`, `main/pet_led.h` | anneau de LED : jauge de bien-être, couleur d'humeur, effets d'action |
| `main/font_fr_20.c`, `main/font_fr_28.c` | Montserrat avec accents + icônes FontAwesome (générées par `lv_font_conv`) |
| `main/font_pet_56.c` | grandes icônes des effets flottants (cœurs, étoiles, Zzz, !, ?) |
| `main/Kconfig.projbuild` | menu **Animal de compagnie** |
| `components/mt8901` | lecture de la rotation (compteur d'impulsions PCNT, pilote `driver/pulse_cnt.h`) |
| `components/button` | bouton-poussoir (anti-rebond) |
| `components/ws2812` | pilote des LED WS2812 via le périphérique RMT |
| `components/lvgl` | LVGL 8.3.3 |
| `sdkconfig.defaults` | valeurs indispensables réappliquées si `sdkconfig` est régénéré |
| `docs/` <!-- ⚫ AJOUTÉ (25/10/2026) --> | aperçus du décor (jour / nuit) utilisés par ce README |

## ⚠️ Configuration indispensable (`sdkconfig.defaults`)

L'écran a besoin de la **PSRAM** pour son frame buffer. Sans elle, le firmware plante au démarrage
(`lcd_panel.rgb: no mem for frame buffer` puis Guru Meditation) : **écran noir**.

`idf.py set-target` (ou *ESP-IDF: Set Espressif Device Target* dans VS Code) régénère `sdkconfig`.
Les valeurs indispensables (PSRAM octale 80 MHz, flash 16 Mo, `partitions_lvgl.csv` avec une application de 8 Mo,
CPU 240 MHz, `LV_MEM_CUSTOM`, polices, et ⚫ 25/10/2026 `CONFIG_LCD_RGB_RESTART_IN_VSYNC` /
`CONFIG_LCD_RGB_ISR_IRAM_SAFE` contre le décalage de l'image) sont donc reprises dans `sdkconfig.defaults`
et réappliquées automatiquement.

<!-- ⚫ AJOUTÉ (25/10/2026) -->
## Modifier les images (`tools/assets`)

Le décor, le chien et les accessoires sont décrits en SVG par du code (Node.js), rendus par
[resvg](https://github.com/RazrFalcon/resvg) puis convertis au format LVGL 16 bits avec tramage
(pas de bandes dans les dégradés) :

```sh
cd tools/assets
npm install
npm run gen          # écrit main/assets/*.bin et main/pet_assets_gen.h (~4 min)
```

Ensuite `idf.py build`. Une pose du chien = une entrée de `POSES` dans `dog.mjs` (angles du corps, du cou, de
la tête, des pattes, de la queue, oreilles, yeux, bouche) ; les positions de la niche, des gamelles et de la
piste sont dans `SPOTS` (`scene.mjs`) et reprises automatiquement par le firmware.
