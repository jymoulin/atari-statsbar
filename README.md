# StatsBar pour Atari

Accessoire GEM qui affiche l'état de la machine dans le coin droit de la
barre de menu : l'équivalent Atari de StatsBar sur Mac. Pour STE et
Falcon030, sous TOS, EmuTOS, MagiC et FreeMiNT.

## Dans la barre

- **Au repos** : l'heure, précédée sous FreeMiNT (avec un AES multitâche
  comme XaAES) d'un graphe de **charge CPU** sur 32 s.
- **Alternate maintenu** : le graphe des **E/S disque** prend la place de
  l'heure (lectures au-dessus de la ligne médiane, écritures en dessous),
  précédé du graphe **réseau** si STinG ou MintNet est présent. Échelle
  logarithmique, en couleur dès que l'écran en a 4.
- **Survol** du coin : un petit panneau avec la **mémoire libre** (ST et
  alternative), la **fréquence** du CPU, un switch de **vitesse** (MegaSTE,
  cartes 16 MHz type TwiSTEr, Falcon 8/16 MHz) et le choix de la **police**.
- **Ctrl + Alternate + Shift** : bascule de vitesse ; la fréquence
  s'affiche 3 s à la place de l'heure.
- **Menu Bureau** : dialogue de configuration (vitesse, rapide au boot,
  police).

## Installation

Copier `STATSBAR.ACC` à la racine du disque de démarrage. Sous MagiC ou
FreeMiNT, le même fichier renommé `STATSBAR.APP` peut être lancé au
démarrage, ce qui libère un emplacement d'accessoire.

Les réglages sont dans `STATSBAR.INF`, à la racine du disque de démarrage,
et se corrigent à la main :

```
FONT=6x6
BOOTFAST=1
```

## Points techniques

- Tout le rendu passe par le VDI : VGA, true color et cartes graphiques.
- ~30 Ko résidents : démarrage minimal (`src/crt0.S`) sans la MiNTLib.
- Activité disque comptée par un handler `hdv_rw` chaîné en XBRA (`SBAR`).
- Charge CPU lue dans `/kern/uptime` de FreeMiNT ; trafic réseau dans les
  compteurs de STinG, ou par ioctl sous MintNet.
- Sobre sous MiNT : seul ce qui change est redessiné. Une application qui
  efface la barre est repérée par un pixel témoin, et le coin est repeint
  sans clignotement.

## Compilation

Chaîne croisée `m68k-atari-mintelf` dans `/opt/cross-mint` :

```
make            # /tmp/statsbar/STATSBAR.ACC
make tools      # outils de test .TOS, en lecture seule
```

Les tests tournent sous Hatari (`test/`) : disquettes FreeMiNT + XaAES pour
STE et Falcon, profilage du coût CPU par NatFeats (`make proftest`).
`README.txt` et `ChangeLog.txt` détaillent les décisions et les mesures.
