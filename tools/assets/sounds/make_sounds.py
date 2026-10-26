# ⚫ AJOUTÉ (25/10/2026) : fabrique les sons réalistes de l'animal (enregistrements libres de droits) :
#   python -I make_sounds.py <ffmpeg.exe> [dossier_cache]
# - télécharge chaque enregistrement source (BigSoundBank : CC0 ; Wikimedia Commons : licence indiquée), ou le
#   reprend du dossier cache ;
# - le découpe à partir du premier son, le limite à quelques secondes, normalise le volume, fondu de fin ;
# - écrit des MP3 mono 22 kHz / 48 kbit/s dans ce dossier (gen.mjs les ajoute au pack d'images) et CREDITS.md.
import os, subprocess, sys, urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
UA = {'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/130.0 Safari/537.36'}
BSB = 'https://bigsoundbank.com/UPLOAD/mp3/{}.mp3'
BSB_PAGE = 'https://bigsoundbank.com/sound-{}.html'
COMMONS = {
    'Mudchute_pig_1.ogg': ('https://upload.wikimedia.org/wikipedia/commons/7/73/Mudchute_pig_1.ogg', 'Secretlondon', 'CC BY-SA 3.0',
                           'https://commons.wikimedia.org/wiki/File:Mudchute_pig_1.ogg'),
    'Mudchute_pig_2.ogg': ('https://upload.wikimedia.org/wikipedia/commons/4/4a/Mudchute_pig_2.ogg', 'Secretlondon', 'CC BY-SA 3.0',
                           'https://commons.wikimedia.org/wiki/File:Mudchute_pig_2.ogg'),
}

# nom du son : (source, début en s ou None = premier son détecté, durée max en s, description)
CLIPS = {
    # chien
    # ⚫ MODIFIÉ (25/10/2026) : extraits choisis d'après le spectrogramme (début précis) : "le premier son" des
    # enregistrements donnait parfois un gémissement ou un hurlement au lieu d'un aboiement
    # 'dog_bark1':    ('bsb:2954', None, 1.2, 'Barking dog #2'),
    # 'dog_bark2':    ('bsb:2955', None, 1.4, 'Barking dog #3'),
    # 'dog_bark3':    ('bsb:0916', None, 1.0, 'Barking dog #1'),
    # 'dog_small':    ('bsb:0612', None, 1.4, 'Small dog barking'),
    # 'dog_whine':    ('bsb:1544', None, 2.2, 'Dogs barking and crying #1'),
    # 'dog_howl':     ('bsb:2450', None, 2.5, 'Dog singing #1'),
    # 'dog_growl':    ('bsb:1060', None, 1.8, '2 small dogs bark and growl'),
    # 'dog_pant':     ('bsb:1547', None, 2.0, 'Dogs breathing'),
    'dog_bark1':    ('bsb:2954', 0.0, 0.9, 'Barking dog #2 (double aboiement)'),
    'dog_bark2':    ('bsb:2954', 2.35, 0.9, 'Barking dog #2 (double aboiement)'),
    'dog_bark3':    ('bsb:2954', 4.95, 0.6, 'Barking dog #2 (aboiement)'),
    'dog_bark4':    ('bsb:0112', 0.75, 1.3, 'Barking dog inside'),
    'dog_old':      ('bsb:2352', 0.2, 0.8, 'Old dog barking #1 (aboiement grave)'),
    'dog_alarm':    ('bsb:2353', 0.0, 2.2, 'Old dog barking #2 (aboiements en rafale)'),
    'dog_small':    ('bsb:0612', 0.1, 0.8, 'Small dog barking (jappement)'),
    'dog_small2':   ('bsb:0612', 1.6, 0.85, 'Small dog barking (jappement)'),
    'dog_small3':   ('bsb:0612', 3.25, 0.8, 'Small dog barking (jappement)'),
    'dog_whine':    ('bsb:1544', 1.05, 2.3, 'Dogs barking and crying #1 (gémissements)'),
    'dog_howl':     ('bsb:2451', 0.0, 2.3, 'Dog singing #2 (hurlement)'),
    'dog_pant':     ('bsb:1547', None, 2.0, 'Dogs breathing'),
    # chat
    'cat_meow1':    ('bsb:1890', None, 1.4, 'Meow cat #2'),
    # 'cat_meow2':    ('bsb:1889', None, 1.4, 'Meow cat #1'),   (⚫ 25/10/2026 : deux clics, pas un miaulement)
    'cat_meow2':    ('bsb:1891', 0.0, 1.6, 'Meow cat #3'),
    'cat_meow3':    ('bsb:1889', 1.45, 1.4, 'Meow cat #1 (miaulement après 1,45 s)'),
    'cat_soft':     ('bsb:0494', None, 1.0, 'Little meow of a cat #1'),
    'cat_purr':     ('bsb:1010', 1.0, 2.6, 'Cat purring #3'),
    'cat_growl':    ('bsb:1885', None, 1.6, 'Growling cat #1'),
    'cat_complain': ('bsb:0658', None, 1.8, 'Cat complaining'),
    'cat_kitten':   ('bsb:0390', None, 1.2, 'Mewing kitten 3 weeks'),
    # mouton, chèvre
    'sheep_1':      ('bsb:2343', None, 1.6, 'Sheep #1'),
    'sheep_2':      ('bsb:2344', None, 1.6, 'Sheep #2'),
    'sheep_3':      ('bsb:2345', None, 1.6, 'Sheep #3'),
    'goat_1':       ('bsb:0279', None, 1.6, 'Bleating goat #1'),
    'goat_2':       ('bsb:1380', None, 1.6, 'Goat #1'),
    'goat_young':   ('bsb:1378', None, 1.4, 'Young goat #1'),
    # cheval, âne, vache
    'horse_neigh':  ('bsb:0863', None, 2.2, 'Horse neighing #3'),
    'horse_call':   ('bsb:1212', None, 1.4, 'Little neighing call #1'),
    'horse_snort':  ('bsb:1543', None, 1.2, 'Breath horse #4'),
    'horse_cough':  ('bsb:0860', None, 1.6, 'Horse coughing'),
    'donkey_1':     ('bsb:1551', None, 3.2, 'Donkey, braying #3'),
    'donkey_2':     ('bsb:1549', None, 3.2, 'Donkey, braying #1'),
    'cow_1':        ('bsb:0546', None, 2.4, 'Cow moos #1'),
    'cow_2':        ('bsb:2382', None, 2.4, 'Cow moos #2'),
    'cow_3':        ('bsb:2383', None, 2.4, 'Cow moos #3'),
    # lapin, cochon
    'rabbit_1':     ('bsb:1376', 2.0, 2.0, 'Bunny hutch'),
    'pig_1':        ('commons:Mudchute_pig_1.ogg', None, 1.6, 'Mudchute pig 1'),
    'pig_2':        ('commons:Mudchute_pig_2.ogg', None, 1.6, 'Mudchute pig 2'),
    # bruits d'action
    'act_crunch':   ('bsb:0407', None, 1.8, 'Eat a rusk'),
    'act_graze':    ('bsb:0986', 1.0, 2.4, 'Horse eating grass #1'),
    'act_carrot':   ('bsb:1846', None, 2.0, 'Horse eats carrot #2'),
    'act_snore':    ('bsb:0170', None, 2.6, 'Man snoring'),
    'act_yawn':     ('bsb:1406', None, 1.6, 'Yawn #1'),
    'act_cough':    ('bsb:0207', None, 1.0, 'Cough average'),
    'act_dig':      ('bsb:1305', None, 1.0, 'Dig with a shovel'),
    'act_sniff':    ('bsb:0784', None, 1.2, 'Sniff'),
}


def fetch(src, cache):
    kind, key = src.split(':', 1)
    name = f'bsb_{key}.mp3' if kind == 'bsb' else f'commons_{key}'
    path = os.path.join(cache, name)
    if not os.path.exists(path) or os.path.getsize(path) == 0:
        url = BSB.format(key) if kind == 'bsb' else COMMONS[key][0]
        with open(path, 'wb') as f:
            f.write(urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=60).read())
    return path


def main():
    ffmpeg = sys.argv[1]
    cache = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, '_src')
    os.makedirs(cache, exist_ok=True)
    credits = []
    for name, (src, start, dur, desc) in CLIPS.items():
        inp = fetch(src, cache)
        af = []
        if start is None:   # coupe le silence du début (premier son)
            af.append('silenceremove=start_periods=1:start_threshold=-38dB:start_silence=0.03')
        af += [f'atrim=0:{dur}', 'asetpts=PTS-STARTPTS', f'afade=t=in:d=0.02', f'afade=t=out:st={max(0.0, dur - 0.25):.2f}:d=0.25',
               'loudnorm=I=-16:TP=-1.5:LRA=11', 'aresample=22050']
        cmd = [ffmpeg, '-hide_banner', '-loglevel', 'error', '-y']
        if start is not None: cmd += ['-ss', str(start)]
        cmd += ['-i', inp, '-af', ','.join(af), '-ac', '1', '-ar', '22050', '-b:a', '48k', '-codec:a', 'libmp3lame',
                os.path.join(HERE, name + '.mp3')]
        subprocess.run(cmd, check=True)
        kind, key = src.split(':', 1)
        if kind == 'bsb':
            credits.append(f'| `{name}.mp3` | {desc} | Joseph Sardin, BigSoundBank.com (son n° {key}) | CC0 1.0 |')
        else:
            url, artist, lic, page = COMMONS[key]
            credits.append(f'| `{name}.mp3` | [{desc}]({page}) | {artist}, Wikimedia Commons | {lic} |')
        print(name, os.path.getsize(os.path.join(HERE, name + '.mp3')), 'o')
    with open(os.path.join(HERE, 'CREDITS.md'), 'w', encoding='utf-8') as f:
        f.write('<!-- ⚫ AJOUTÉ (25/10/2026) : fichier généré par make_sounds.py -->\n')
        f.write('# Sons de l\'animal : sources et licences\n\n')
        f.write('Extraits découpés, normalisés et recompressés (MP3 mono 22 kHz) d\'enregistrements libres de droits.\n')
        f.write('Les extraits tirés de fichiers CC BY-SA sont diffusés sous la même licence (CC BY-SA 3.0).\n\n')
        f.write('| Fichier | Enregistrement d\'origine | Auteur | Licence |\n|---|---|---|---|\n')
        f.write('\n'.join(credits) + '\n\n')
        f.write('- BigSoundBank : https://bigsoundbank.com (licence : https://bigsoundbank.com/licence.html, CC0 « Free and royalty-free »)\n')
        f.write('- CC BY-SA 3.0 : https://creativecommons.org/licenses/by-sa/3.0/\n')


if __name__ == '__main__':
    main()
