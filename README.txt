STATSBAR - etat du projet
=========================
Derniere mise a jour : 2026-10-15
Cibles : Atari STE (TOS 2.06) et Falcon030. Accessoire GEM.
Ex-ClockPanel (CLOCKPNL.ACC), renomme le 2026-10-15 : c'est l'equivalent
Atari de StatsBar sur Mac (~/Documents/Dev/MacOS/statsbar), et il fait
bien plus que l'horloge. Le ChangeLog garde les anciens noms jusqu'a
cette date ; CPNL*.TXT a la racine sont des traces prises sur la vraie
machine avec les anciens outils, gardees hors du depot (.gitignore).
Historique detaille des decisions et mesures : ChangeLog.txt.
Depot : github.com/jymoulin/atari-statsbar (branche main). Le depot git
est dans ~/Downloads/github/atari-statsbar (git init --separate-git-dir,
comme sting/) : ici, .git n'est qu'un fichier qui y renvoie.
README.md = la presentation affichee par GitHub, courte ; ce README.txt
reste l'etat detaille du projet.

OBJECTIF
--------
Afficher dans le coin de la barre de menu ce qui sert vraiment, en restant
compatible Falcon / VGA / true color (Clocky, LED-Panel et MenuInfo en
reference).

PERIMETRE ET ETAT
-----------------
1. Heure hh:mm                                   valide sur STE reel
2. CapsLock (seul verrou existant sur Atari)     valide sur STE reel
3. Activite disque : graphique papillon R/W      valide sur STE reel
   (hook hdv_rw valide sous EmuTOS, MagiC et TOS 2.06 ;
    affiche tant qu'Alternate est maintenu)
4. Frequence CPU + bascule de vitesse            valide sur STE reel
   (68020 : 8 / 24 MHz, sous EmuTOS, MagiC et TOS 2.06 ;
    affichee dans le panneau de survol et le dialogue)
5. Graphique de charge CPU (FreeMiNT + AES multitache)  valide sous Hatari
   (FreeMiNT + XaAES) ; masque sous l'AES du TOS, ou il vaudrait 100 %
6. Memoire libre (ST et alternative separees)    dialogue et panneau
7. Panneau de survol, avec switchs vitesse et police   valide sur STE reel
8. Bascule Alternate : heure <-> graphique disque     valide sur STE reel
9. Graphique reseau, sur Alternate (STinG / MintNet)   valide sur STE reel
10. Couleurs des graphiques (ecran >= 4 couleurs)  valide sur Falcon reel
11. Bascule 8/16 MHz du Falcon ($FFFF8007 bit 0)   valide sous Hatari
12. Raccourci Ctrl+Alt+Shift : bascule de vitesse   a tester sur machine
13. Reglages sauves dans X:\STATSBAR.INF (disque de boot) : police et
    option "rapide au boot"                         valide sous Hatari

ARCHITECTURE
------------
Un binaire : STATSBAR.ACC (STATSBAR.APP = meme fichier, pour MiNT/MagiC).
Rendu par le VDI, jamais par ecriture directe ecran. Seul le handler disque
tourne hors du GEM.

  src/statsbar.h      structures, config, prototypes
  src/crt0.S      [asm] demarrage minimal, a la place de la MiNTLib
  src/main.c      boucle d'evenements, mode accessoire / application
  src/dialog.c    dialogue de configuration (arbre d'objets, sans .RSC)
  src/panel.c     panneau de survol, dessine sur le bureau
  src/display.c   composition et rendu dans la barre de menu
  src/sysinfo.c   detection machine, frequence, idle CPU
  src/disk.c      exploitation des compteurs disque
  src/net.c       trafic reseau : STinG (octets) ou MintNet (paquets)
  src/mhz.h       formule de frequence, partagee avec SPEEDTST
  src/probe.S     [asm] mesures de vitesse, sonde $FFFF8E21
  src/diskhook.S  [asm] handler hdv_rw, chainage XBRA

POINTS TECHNIQUES A NE PAS OUBLIER
----------------------------------
- Lien en -nostdlib : src/crt0.S remplace la startup de la MiNTLib, qui
  faisait entrer a elle seule localtime, stdio, malloc et la lecture de
  /etc/passwd, soit 50 Ko residents jamais executes. Ne restent que
  libgem et libgcc. crt0.S fournit memcpy, memset et strlen ; le cookie
  jar se parcourt dans probe.S (sbar_cookie_get) et non par Getcookie().
  Consequence : pas de argv (lire la queue de commande en basepage+0x80),
  pas de stdio, pas de malloc. crt0.o doit rester en tete du lien.
- Empreinte : ~26 Ko residents, cout CPU indiscernable d'un accessoire
  vide (banc CPUTST contre NULLACC, resolution ~1 %). Pile de 3 Ko,
  dimensionnee par gcc -fstack-usage. NE PAS utiliser le v_gtext de
  libgem : il reserve 2104 octets de pile par appel ; dsp_gtext() le
  remplace.
- Un accessoire n'est PAS lance comme un programme : l'AES passe la
  basepage dans A0 et ne prete AUCUNE pile (lire 4(sp) = bus error). Un
  programme a sa basepage en 4(sp). crt0.S teste A0 non nul + p_parent
  nul, puis prend dans les deux cas sa propre pile de 3 Ko dans le BSS.
- NE PAS compiler en -mshort : la toolchain n'a pas de mshort/libc.a. On
  lierait une libc en int 32 bits a du code en int 16 bits, et tout appel
  libc passant un int decalerait ses arguments (memset hors memoire).
- hdv_rw : 4(sp).w rwflag, 10(sp).w nombre de SECTEURS, 14(sp).w lecteur.
  La taille de secteur varie : 512 sur disquette, 8192 sur la partition C:
  de la machine de test. Comptage par lecteur, taille lue via Getbpb.
- Frequence (src/measure.c, partage avec SPEEDTST) : methode
  differentielle de C. Zietz (000CLOCK.TOS). Deux boucles ne differant que
  par nop / mulu.w, de meme taille : la difference de leurs durees sur
  _hz_200 elimine le bus. MEDIANE de trois passes (ecarte une passe
  perturbee, sans le biais du minimum), interruptions autorisees.
  La charge d'interruption du systeme sous-estime la mesure selon
  l'environnement (machine de test a 24 MHz : -3 a +2 % sous TOS 2.06,
  -12 a -3 % sous EmuTOS, -12 a -11 % sous MagiC) : le resultat est donc
  CLASSE vers une frequence nominale (mhz_snap : 8, 16, 24, 32, 40, 50,
  66, 80, 100). Marge avant de basculer d'un cran trop bas : -28 % a
  24 MHz, mais seulement -14 % a 40 MHz et -11 % a 80 MHz. Au-dela de
  32 MHz il faudra donc un reglage explicite ou une table par carte.
  ~0,7 s, donc mesuree au demarrage de l'accessoire ; cache invalide si
  $FFFF8E21 change. Constante par famille = ecart de cycles en centiemes
  (mhz.h) : 68000 4050, 68020 2640, 68030 2640 (valide sur Falcon reel).
- NE PAS reintroduire de chronometrage sur le compteur du Timer C du MFP :
  juste sous Hatari, il est faux sur la TwiSTEr reelle (lectures figees en
  boucle serree, interruptions masquees), et une variante a gele Geneva.
- $FFFF8E21 : bit 1 = horloge rapide, bit 0 = cache EXTERNE du MegaSTE
  (16 Ko sur la carte mere ; le 68000 n'a aucun cache interne). Le
  registre ne dit pas combien de MHz et peut etre inerte : on mesure, on
  ne le croit pas.
- Police : vst_point() et non vst_height(), qui met a l'echelle et rend des
  tailles illisibles. Mais vst_point() lui-meme met a l'echelle AU-DELA de
  la police des menus, et ces tailles-la sont plus LARGES (12x12, 16x16,
  16x32 en 640x400). On ne retient donc que les cellules qui tiennent dans
  celle des menus, sur les DEUX dimensions : 6x6, 8x8, 8x16 en 640x400 ;
  6x6, 8x8 en 640x200. La derniere de la liste est celle des menus.
- Un accessoire n'a pas a se terminer, une APPLICATION si : sous AES
  mono-tache elle n'a pas droit au menu Bureau, donc son dialogue est sa
  seule interface. En .APP sous AES mono-tache, le dialogue s'ouvre au
  demarrage avec un bouton Quitter, et le programme se termine en le
  refermant. Sous AES multitache (MagiC, XaAES), l'entree du menu Bureau
  existe (verifie sous MagiC) : demarrage silencieux, comme un accessoire.
  Une application sous AES mono-tache NE DOIT PAS appeler menu_register()
  puisqu'elle va se terminer : l'entree survivrait a sa memoire et le menu
  Bureau afficherait des caracteres aleatoires. menu_unregister() n'existe
  qu'a partir de l'AES 3.3, donc pas de rattrapage possible.
- dsp_close() efface notre emprise dans la barre : une application qui se
  termine doit la rendre telle qu'elle l'a trouvee.
- Le champ heure / frequence a une largeur fixe (TIME_CELLS) : le bloc
  etant cale a droite, "8MHz", plus court que "00:00", decalait sinon
  tout le graphique a chaque appui sur Alternate.
- Dialogue : arbre d'objets monte en memoire (src/dialog.c), pas de .RSC.
  objc_find() rend l'objet le plus PROFOND : tout enfant pose sur une
  zone cliquable doit donc etre cliquable lui aussi, sinon il avale les
  clics (cas de la case de fermeture et de son carre noir).
  Pas de menu_popup() non plus : absent de l'AES 2.06. Largeur calibree
  sur 38 caracteres pour tenir en 320x200.
- La derniere ligne de la barre de menu (trait de separation) n'est jamais
  effacee.
- Graphique E/S : 32 colonnes d'une seconde (32 s), colonne de droite
  vivante, lectures au-dessus de la mediane, ecritures en dessous, echelle
  log2 en debit par seconde (plafond 1 Mo/s). Rendu incremental par
  vro_cpyfm + une colonne ; rien n'est dessine tant qu'il reste plat.
  dsp_tick() fait avancer l'historique une fois par top ; dsp_update() ne
  fait que dessiner.

REFERENCES EXTERIEURES (hors depot)
-----------------------------------
- 000CLOCK.TOS de C. Zietz, mesure de frequence sur 68000, dont vient la
  methode differentielle de src/probe.S :
  ~/Downloads/Atari/Programs/68000/OUTILS/TWISTER/SHOW.MHZ/
- CENTBNCH.APP de Centek, la reference 68030 (constante validee depuis
  sur le Falcon reel, on n'en a plus besoin) :
  ~/Downloads/Atari/Programs/68030/OUTILS/BENCH/

MACHINE DE TEST (STE de jym, carte TwiSTEr)
-------------------------------------------
- Mode 68000 : 8 / 16 MHz. Mode 68020 : 8 / 24 MHz (boost x3).
- Sous TOS nu le registre vaut 0 au demarrage : 8 MHz.
- Partition C: : secteurs de 8192 octets. Ecran 640x400.

COMPILATION ET TEST
-------------------
Tout le genere va dans /tmp/statsbar ; le depot ne contient que les
sources et la doc. Les binaires a tester partent dans ~/Downloads/ToTest/.

  make              /tmp/statsbar/STATSBAR.ACC
  make tools        SPEEDTST, DISKTST, FONTTST, STRESS, MEMTST (.TOS)
  make totest       copie l'accessoire et les outils dans ToTest/statsbar/,
                    et TODO_TESTS.txt en todo_statsbar.txt a la racine
                    de ~/Downloads/ToTest/ (partage avec d'autres projets)
  make altest       ALTEST.ACC : barre figee "Alternate tenu" (Hatari)
  make dlgtest      DLGTEST.ACC : dialogue ouvert au demarrage (Hatari)
  make pnltest      PNLTEST.ACC : panneau de survol affiche (Hatari)
  make cputst       CPUTST.PRG + NULLACC.ACC : banc du cout CPU, a
                    comparer au temoin, jamais sans accessoire
  sh test/mintdisk.sh [acc]  disquette 1,44 Mo FreeMiNT + XaAES + MintNet
                    (FreeMiNT ne voit pas le disque GEMDOS de Hatari)
  sh test/mintfalc.sh [acc]  idem pour Falcon, avec le systeme 1-20-cur de
                    ToTest/MINT (noyau a comptabilite exacte) ; avec
                    make proftest et Hatari --natfeats on --mmu on
  make menutst      MENUTST.PRG : echange sa barre de menu toutes les 3 s
                    (Hatari, SHELLPRG= avec mintfalc.sh)
  make iogen        IOGEN.PRG : salves d'E/S, ECRIT sur A: (Hatari)
  make deadtest     SPEEDDEAD.TOS : compteur inerte simule (Hatari)
  Ces cinq dernieres variantes ne vont jamais dans ToTest/.

  sh test/run-hatari.sh [st|ste|megaste|tt|falcon] [programme]
      SHOT=<n> capture apres n trames ; FLOPPY, TOS, FF, TIMEOUT,
      CPULEVEL, CPUCLOCK surchargeables.
  python3 test/mkfloppy.py image.st fichier...   image FAT12 amorcable

  Outils de test en .TOS (pas .PRG : sortie illisible en mode GEM). Chacun
  attend une touche et ecrit aussi sa trace dans C:\SBAR*.TXT, reecrite
  ligne a ligne pour survivre a un plantage. DISKTST et STRESS prennent C:
  par defaut, se rabattent sur A: si C: n'a pas de BPB, et acceptent une
  lettre en argument ("STRESS A"). Ils sont en lecture seule.

  Limites de Hatari : son disque GEMDOS (-d) court-circuite hdv_rw (tester
  sur image disquette), et son 68020/68030 n'est pas fidele en
  temporisation (tout calibrage 020+ se fait sur machine reelle : un
  Falcon emule annonce a 16 MHz se mesure a 24).
  Pour de la TT-RAM sur Falcon : --ttram 16 --addr24 off, l'adressage
  24 bits l'interdisant.

RESTE A FAIRE
-------------
- Valider le mode 68000 sur la TwiSTEr (8 / 16 MHz) ; le mode 68020
  (8 / 24 MHz) est valide sous EmuTOS, MagiC et TOS 2.06, mesure comme
  hook disque et metriques de police (logs EMU/, TOS/ et MGC/).
- Tester l'idle CPU sous FreeMiNT, et decider de sa presentation.
- PRESENTATION VISEE (arretee avec jym le 2026-09-27) :

    normal     [graphique CPU]  hh:mm
    Alt tenu   [graphique reseau]  [graphique disque]
    survol     petit panneau : memoire libre, frequence, et un switch
               de vitesse cliquable

  Alt ECHANGE le contenu des deux emplacements de la barre, comme il
  remplace deja l'heure par la frequence. Une seule rangee, on ne sort
  jamais de la barre. Chaque graphique n'apparait que si sa source
  existe : charge CPU sous FreeMiNT seulement, reseau avec STinG ou
  MintNet seulement, disque partout. Sur un ST/STE sans FreeMiNT ni pile
  reseau, la barre ne montre donc que l'heure, et le graphique disque
  n'apparait que sur Alt.

  Etat : TOUT EST ECRIT ET VALIDE sur le STE et le Falcon. Graphique CPU
  vu sous FreeMiNT + XaAES sur le Falcon reel (jym). Reste a tester sur
  machine : .APP silencieux sous MagiC, bascule Falcon, raccourci, .INF,
  et le survol depuis l'optimisation (position prise dans evnt_multi).

  Cout CPU sous MiNT + XaAES : mesurer avec make proftest (voir
  ChangeLog 2026-10-10 soir et 2026-10-12). Banc Falcon :
  test/mintfalc.sh (systeme 1-20-cur de jym). Au repos, Hatari Falcon :
  155 ms / 10 s, dont 105 pour le seul reveil (evnt_multi XaAES, 5 fois
  par seconde) : c'est le plancher tant qu'on se reveille toutes les
  200 ms. Le code C pese moins de 5 ms / 10 s : l'assembleur n'apporte
  rien, tout est dans les appels systeme (~0,3 ms le trap sur Falcon).

  Contraintes relevees, a ne pas redecouvrir :
  * BEG_MCTRL en plus de BEG_UPDATE tant que le panneau est affiche :
    sans lui, un accessoire qui n'est pas au premier plan ne recoit aucun
    evenement bouton et les zones seraient inertes.
  * Le texte des boutons actifs se trace en MD_TRANS ; en MD_REPLACE le
    VDI repeint toute la cellule et efface le fond noir du bouton.
  * Le panneau dessine avec la police des MENUS : dsp_use_menu_font()
    avant, dsp_vdi_state() apres pour rendre son etat a la barre.
  * Le clipping de la station VDI reste pose EN PERMANENCE sur la barre
    (dsp_clip) : le panneau le coupe (dsp_clip(0)) avant de sauver ou de
    restituer le fond, et dsp_vdi_state() le remet. Idem pour la couleur
    de remplissage, en cache (fill_color) : dsp_vdi_state() l'oublie.
  * Pixel temoin : ne JAMAIS le relire apres un defilement (vro_cpyfm
    decale aussi une zone effacee, qui deviendrait la reference). Lu a
    chaque top 3 s apres une activite de l'utilisateur, chaque seconde
    sinon, et avant tout defilement ; saute seulement si le pointeur le
    recouvre. Sous XaAES la barre est redessinee par le proprietaire du
    menu sans aucune notification : l'ecran est la seule source.
  * Un graphique uniforme deja affiche n'est pas redessine (GRAPH.shown) :
    tout ce qui modifie l'ecran sans passer par dsp_update() doit appeler
    forget_screen() (fait par dsp_open).
  * Seul le PANNEAU DE SURVOL sort de la barre : c'est le seul mecanisme
    nouveau a ecrire (sauvegarde et restitution du fond par vro_cpyfm,
    puisqu'on dessine sur le bureau, qui appartient a d'autres).
    Ne rien envisager sur deux rangees DANS la barre : gl_hbox vaut 19
    pixels en 640x400 mais 11 en 640x200 et 320x200.
  * Detection du survol : graf_mkstate() a chaque top, PAS l'evenement de
    rectangle MU_M1. Le rectangle transmis etait pourtant exact (verifie
    par journalisation sur la machine reelle) et le binding de gemlib
    aussi (verifie au desassemblage), mais l'AES declenchait une bonne
    largeur plus a gauche. Cause non elucidee ; graf_mkstate ne laisse
    aucune place a l'interpretation. ~400 ms d'attente avant ouverture
    pour ne pas reagir a un simple passage. Pas menu_popup() non plus,
    absent avant l'AES 3.3.
  * Le switch du panneau se gere a la main (MU_BUTTON + test de zone), pas
    par form_do(), qui est modal et monopoliserait l'AES.
  * Alt n'a PAS besoin de temporisation : l'echange sur place est discret,
    rien ne clignote sur un Alt+lettre.
  * L'historique des graphiques doit continuer a tourner MEME CACHE,
    sinon Alt ouvrirait sur un graphique vide a remplir en 32 s. Seul le
    dessin est saute. En pratique l'accessoire devient plus leger qu'avant
    sur une machine nue : plus de defilement chaque seconde.

- ACCESSOIRE ou PROGRAMME : question tranchee le 2026-09-27. On reste un
  accessoire. Un PRG du dossier AUTO s'execute AVANT le demarrage de
  l'AES : appl_init() y echoue, donc ni VDI ni barre de menu. Et sous TOS
  mono-tache une seule application tourne a la fois, le bureau ne revenant
  qu'a sa fin. L'accessoire est donc le SEUL moyen d'etre resident et
  d'utiliser GEM sous TOS 2.06 ou EmuTOS.
  Sous MagiC et MiNT en revanche, le meme binaire en .APP peut etre lance
  au demarrage et vivre en tache de fond : la, un emplacement d'accessoire
  est libere, sans une ligne de code de plus.
  Ne pas confondre les deux limites : les 6 ACCESSOIRES CHARGES (un .ACC
  la consomme qu'il s'inscrive au menu ou non) et les LIGNES DU MENU
  BUREAU (rendues en n'appelant pas menu_register). Supprimer l'entree de
  menu ne libere donc pas d'emplacement de chargement.
  L'entree du menu Bureau reste en place tant que le survol n'est pas
  valide sur les quatre systemes : c'est la seule porte d'entree de
  secours si MU_M1 se comporte mal quelque part. Une option la supprimera
  ensuite.
- Le panneau de survol portera aussi le switch de POLICE, pas seulement
  celui de vitesse : meme machinerie, cout negligeable.
- Memoire libre : FAIT (sys_mem, u_mem, sys.alt_ram), affiche dans le
  dialogue, reste a porter dans le panneau de survol. Plus grand bloc
  libre, pas un pourcentage, et DEUX valeurs des que la machine a de la
  memoire alternative : sur les trois systemes de la machine de test,
  Malloc(-1) rend le bloc de RAM RAPIDE (6,3 a 7,9 Mo) alors que seuls
  3,6 a 3,9 Mo de ST-RAM sont utilisables. DETECTION PAR
  Mxalloc(-1, 1) > 0, JAMAIS par ramtop ($5A4), qui reste a 0 quand la
  memoire est declaree par Maddalt() - ce que fait le pilote TwiSTEr.
  Mxalloc n'existe qu'a partir du TOS 1.04, d'ou sys.tos.
- Charge CPU : FreeMiNT uniquement (/kern/uptime). Echantillonner une fois
  par seconde et garder le descripteur ouvert (Fseek au lieu de rouvrir) :
  la version ecrite rouvre le fichier cinq fois par seconde pour rien.
- Reseau : brique la moins sure. Compteurs dans les structures PORT de
  STinG via son cookie, ou ioctl SIOCGIFSTATS sur une socket MintNet.
  Deux implementations, aucune verifiable sans la machine : faire un
  NETTST.TOS de reconnaissance AVANT d'ecrire l'affichage, comme MEMTST,
  qui a evite une detection fausse par ramtop.
- Ordre de travail retenu : panneau de survol, puis rangee Alt, puis
  charge CPU, puis reseau.
- Reglage explicite des frequences pour les cartes inconnues (a mettre
  dans le .INF si besoin).
