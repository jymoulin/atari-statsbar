#!/bin/sh
# Falcon version of mintdisk.sh: a 1.44 MB floppy with the FreeMiNT system
# installed on jym's Falcon (~/Downloads/ToTest/MINT/1-20-cur, kernel with
# exact CPU accounting) + XaAES + StatsBar, to measure the accessory's
# CPU cost under Hatari (make proftest).
#
#   sh test/mintfalc.sh [accessory]     (default: /tmp/statsbar/PROF.ACC)
#
# Then:
#   Hatari --machine falcon --tos tos404fr.img --memsize 14 --mmu on \
#          --monitor vga --disk-a /tmp/statsbar/mintf.st --natfeats on ...
#
# The kernel goes straight into AUTO\ (no loader): with --natfeats on the
# loader would look for MINT030H.PRG. It finds its system folder
# A:\MINT\1-20-cur\ by itself. Minimal mint.cnf and xaaes.cnf: no desktop,
# no TosWin2, only XaAES and the accessory.
set -e
ACC="${1:-/tmp/statsbar/PROF.ACC}"
S=${SYSTEM:-$HOME/Downloads/ToTest/MINT/1-20-cur}
V=$(basename $S)
X=$S/mint/$V
T=/tmp/statsbar
IMG=${IMG:-$T/mintf.st}
mkdir -p $T
printf 'GEM=u:/a/mint/%s/xaaes/xaloader.prg\r\n' $V > $T/mintf.cnf
printf 'accpath = a:\\\r\n' > $T/xaaesf.cnf
# SHELLPRG=path : a program XaAES starts as its desktop (test/menutst.c)
if [ -n "$SHELLPRG" ]; then
  printf 'shell = a:\\%s\r\n' $(basename $SHELLPRG | tr a-z A-Z) >> $T/xaaesf.cnf
fi
python3 "$(dirname "$0")/mkfloppy.py" --hd $IMG \
  $X/mint030.prg=AUTO/MINT030.PRG \
  $T/mintf.cnf=MINT/$V/MINT.CNF \
  $X/xaaes/xaloader.prg=MINT/$V/XAAES/XALOADER.PRG \
  $X/xaaes/xaaes.km=MINT/$V/XAAES/XAAES.KM \
  $X/xaaes/moose.adi=MINT/$V/XAAES/MOOSE.ADI \
  $X/xaaes/xaaes.rsc=MINT/$V/XAAES/XAAES.RSC \
  $X/xaaes/xaaeswdg.rsc=MINT/$V/XAAES/XAAESWDG.RSC \
  $X/xaaes/xa_xtobj.rsc=MINT/$V/XAAES/XA_XTOBJ.RSC \
  $X/xaaes/xaaes.rsl=MINT/$V/XAAES/XAAES.RSL \
  $T/xaaesf.cnf=MINT/$V/XAAES/XAAES.CNF \
  "$ACC"=STATSBAR.ACC ${SHELLPRG:+$SHELLPRG=$(basename "$SHELLPRG" | tr a-z A-Z)}
