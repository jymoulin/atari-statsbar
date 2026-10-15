#!/bin/sh
# Fabrique une disquette 1,44 Mo FreeMiNT + XaAES + MintNet + StatsBar,
# pour tester sous Hatari ce qui n'existe que sous MiNT : graphique de
# charge CPU (/kern/uptime) et trafic MintNet.
#
#   sh test/mintdisk.sh [accessoire]      (defaut : /tmp/statsbar/STATSBAR.ACC)
#
# Puis :
#   Hatari --machine ste --memsize 4 --monitor mono --tos tos206fr.img \
#          --disk-a /tmp/statsbar/mint.st --drive-b off --fastfdc on ...
#   (compter ~20 000 trames avant que XaAES soit pret)
#
# Pourquoi une disquette : le pilote FAT de FreeMiNT lit des SECTEURS, et
# ne voit donc pas le disque GEMDOS emule par Hatari (-d). Pourquoi 4 Mo :
# avec 1 Mo, le lancement de l'AES echoue faute de memoire. Pourquoi
# XaAES : avec l'AES du TOS, qui attend en boucle active, le processus idle
# de MiNT ne tourne jamais et la charge CPU vaut 100 % en permanence.
# xaaesst.km est renomme xaaes000.km, nom que le chargeur cherche sur
# 68000 ; moose.adi est indispensable (sans lui : KM_RUN, erreur 25).
# NATFEATS=1 : Hatari lance avec --natfeats on, le chargeur cherche alors
# MINT.PRG au lieu de MINT000.PRG ; on ajoute le noyau sous ce nom.
set -e
ACC="${1:-/tmp/statsbar/STATSBAR.ACC}"
# KERNEL=chemin : un autre mint000.prg (ex. noyau a comptabilite exacte)
F=~/Downloads/freemint-latest-st_ste
X=$F/mint/1-19-098
K=${KERNEL:-$X/mint000.prg}
T=/tmp/statsbar
mkdir -p $T
printf 'GEM=u:/a/mint/1-19-098/xaaes/xaloader.prg\r\n' > $T/mintx.cnf
printf 'accpath = a:\\\r\n' > $T/xaaes.cnf
python3 "$(dirname "$0")/mkfloppy.py" --hd $T/mint.st \
  $F/auto/mint-098.prg=AUTO/MINT-098.PRG \
  $K=MINT/1-19-098/MINT000.PRG \
  $X/inet4.xdd=MINT/1-19-098/INET4.XDD \
  $T/mintx.cnf=MINT/1-19-098/MINT.CNF \
  $X/xaaes/xaloader.prg=MINT/1-19-098/XAAES/XALOADER.PRG \
  $X/xaaes/xaaesst.km=MINT/1-19-098/XAAES/XAAES000.KM \
  $X/xaaes/moose.adi=MINT/1-19-098/XAAES/MOOSE.ADI \
  $X/xaaes/xaaes.rsc=MINT/1-19-098/XAAES/XAAES.RSC \
  $X/xaaes/xaaeswdg.rsc=MINT/1-19-098/XAAES/XAAESWDG.RSC \
  $X/xaaes/xa_xtobj.rsc=MINT/1-19-098/XAAES/XA_XTOBJ.RSC \
  $X/xaaes/xaaes.rsl=MINT/1-19-098/XAAES/XAAES.RSL \
  $T/xaaes.cnf=MINT/1-19-098/XAAES/XAAES.CNF \
  "$ACC"=STATSBAR.ACC \
  ${NATFEATS:+$K=MINT/1-19-098/MINT.PRG}
