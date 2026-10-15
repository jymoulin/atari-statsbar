#!/bin/sh
# Lance StatsBar (ou un autre programme) dans Hatari.
#
#   sh test/run-hatari.sh [machine] [programme]
#
# machine   : st | ste | megaste | tt | falcon   (defaut : ste)
# programme : chemin Atari a autostarter, ex. C:\SPEEDTST.PRG
#
# Tout se passe dans /tmp/statsbar (binaires, disque C:, captures) :
# rien de temporaire dans le depot. Le dossier /tmp/statsbar/hd est monte
# en disque C: ; l'accessoire qui s'y trouve est charge au boot par le TOS.

HATARI="$HOME/Downloads/hatari-2.6.1_macos/Hatari.app/Contents/MacOS/Hatari"
TOS="${TOS:-$HOME/Downloads/hatari-2.6.1_macos/tos206fr.img}"
BUILD=/tmp/statsbar
HD="$BUILD/hd"

MACHINE="${1:-ste}"
AUTOPRG="$2"
TIMEOUT="${TIMEOUT:-25}"

mkdir -p "$HD"
# seul l'accessoire livre est installe ; VERIFY.ACC (make verify) ne doit
# jamais s'y trouver, sinon deux accessoires se chargent
rm -f "$HD"/*.ACC
cp -f "$BUILD"/STATSBAR.ACC "$BUILD"/*.TOS "$HD"/ 2>/dev/null

set -- --machine "$MACHINE" \
       ${CPULEVEL:+--cpulevel "$CPULEVEL"} \
       ${CPUCLOCK:+--cpuclock "$CPUCLOCK"} \
       ${FLOPPY:+--disk-a "$FLOPPY"} \
       --tos "$TOS" \
       -d "$HD" \
       --conout 2 \
       --sound off \
       --fast-boot on \
       --fast-forward ${FF:-on} \
       --screenshot-dir "$BUILD"

[ -n "$AUTOPRG" ] && set -- "$@" --auto "$AUTOPRG"

# SHOT=<vbl> : capture l'ecran apres <vbl> trames puis quitte l'emulateur,
# ce qui permet de verifier le rendu sans interaction.
if [ -n "$SHOT" ]; then
	printf 'screenshot %s\nquit\n' "$BUILD/shot.png" > "$BUILD/shot.ini"
	printf 'b VBL > %s :once :quiet :file %s\n' "$SHOT" "$BUILD/shot.ini" \
		> "$BUILD/dbg.ini"
	set -- "$@" --parse "$BUILD/dbg.ini"
fi

# alarm() coupe l'emulateur au bout de TIMEOUT secondes, sans sleep bloquant
exec perl -e "alarm $TIMEOUT; exec @ARGV" "$HATARI" "$@"
