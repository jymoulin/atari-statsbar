# StatsBar - Makefile
#
# Toolchain : cross-gcc MiNT ELF dans /opt/cross-mint
#
# Tout ce qui est genere (objets, binaires, images de test, captures) va
# dans /tmp/statsbar. Seuls les binaires a tester sur la vraie machine
# sont copies dans ToTest/, par "make totest".

CROSS   := /opt/cross-mint/bin/m68k-atari-mintelf-
CC      := $(CROSS)gcc
SIZE    := $(CROSS)size

BUILD   := /tmp/statsbar
OBJDIR  := $(BUILD)/obj

# Repertoire de test commun a tous les projets : on n'y retire QUE nos
# propres fichiers, les autres projets y deposent les leurs.
TOTEST  := $(HOME)/Downloads/ToTest/statsbar
TODO    := $(HOME)/Downloads/ToTest/todo_statsbar.txt

# -m68000 : le binaire doit tourner sur STE comme sur Falcon
#
# PAS de -mshort : la toolchain ne fournit pas de libc compilee en int 16 bits
# (seuls mshort/libgem.a et mshort/libm.a existent, pas mshort/libc.a). En
# -mshort on poussait donc un int sur 2 octets a des fonctions libc compilees
# pour 4 : memset(p,0,64) recevait une taille aberrante et ecrivait n'importe
# ou. C'est ce qui produisait les deux bombes.
CFLAGS  := -m68000 -Os -fomit-frame-pointer \
           -ffunction-sections -fdata-sections \
           -Wall -Wextra -Isrc -Itest
#
# -nostdlib : la startup de la MiNTLib tirait a elle seule localtime,
# stdio, malloc et la lecture de /etc/passwd, soit 50 Ko de code jamais
# executes mais residents tant que l'accessoire est charge. src/crt0.S la
# remplace (basepage, pile, Mshrink, main) et fournit les trois seules
# fonctions libc utilisees : memcpy, memset, strlen. -lgcc reste
# necessaire pour les divisions et multiplications 32 bits du 68000.
LDFLAGS := -m68000 -nostdlib -Wl,--gc-sections -Wl,-e,_start -s
LIBS    := -lgem -lgcc
HDRS    := src/statsbar.h src/mhz.h src/measure.h

# crt0.o doit rester en tete : c'est lui qui definit le point d'entree.
OBJS    := $(addprefix $(OBJDIR)/, crt0.o main.o display.o dialog.o \
                                   panel.o sysinfo.o config.o disk.o net.o \
                                   measure.o probe.o diskhook.o)

ACC     := $(BUILD)/STATSBAR.ACC

# Outils de test surs pour la vraie machine : lecture seule, et SPEEDTST
# restaure le registre de vitesse en sortie.
TOOLS   := $(BUILD)/SPEEDTST.TOS $(BUILD)/DISKTST.TOS \
           $(BUILD)/FONTTST.TOS $(BUILD)/STRESS.TOS $(BUILD)/MEMTST.TOS \
           $(BUILD)/NETTST.TOS

.PHONY: all tools totest cputst altest dlgtest spdtest proftest pnltest menutst iogen clean test

all: $(ACC)

$(ACC): $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) $(LIBS) -o $@
	@$(SIZE) $@

tools: $(TOOLS)

$(OBJDIR)/%.o: src/%.c $(HDRS) | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: src/%.S | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Les outils se lient comme l'accessoire : crt0.S en tete, pas de libc.
$(OBJDIR)/tlog.o: test/tlog.c test/tlog.h | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/SPEEDTST.TOS: test/speedtst.c $(OBJDIR)/crt0.o $(OBJDIR)/tlog.o $(OBJDIR)/measure.o $(OBJDIR)/probe.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJDIR)/crt0.o $< $(OBJDIR)/tlog.o $(OBJDIR)/measure.o $(OBJDIR)/probe.o $(LIBS) -o $@

$(BUILD)/DISKTST.TOS: test/disktst.c $(OBJDIR)/crt0.o $(OBJDIR)/tlog.o $(OBJDIR)/diskhook.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJDIR)/crt0.o $< $(OBJDIR)/tlog.o $(OBJDIR)/diskhook.o $(LIBS) -o $@

$(BUILD)/STRESS.TOS: test/stress.c $(OBJDIR)/crt0.o $(OBJDIR)/tlog.o $(OBJDIR)/diskhook.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJDIR)/crt0.o $< $(OBJDIR)/tlog.o $(OBJDIR)/diskhook.o $(LIBS) -o $@

$(BUILD)/MEMTST.TOS: test/memtst.c $(OBJDIR)/crt0.o $(OBJDIR)/tlog.o $(OBJDIR)/probe.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJDIR)/crt0.o $< $(OBJDIR)/tlog.o $(OBJDIR)/probe.o $(LIBS) -o $@

$(BUILD)/NETTST.TOS: test/nettst.c $(OBJDIR)/crt0.o $(OBJDIR)/tlog.o $(OBJDIR)/net.o $(OBJDIR)/sysinfo.o $(OBJDIR)/measure.o $(OBJDIR)/probe.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJDIR)/crt0.o $< $(OBJDIR)/tlog.o $(OBJDIR)/net.o $(OBJDIR)/sysinfo.o $(OBJDIR)/measure.o $(OBJDIR)/probe.o $(LIBS) -o $@

$(BUILD)/FONTTST.TOS: test/fonttst.c $(OBJDIR)/crt0.o $(OBJDIR)/tlog.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJDIR)/crt0.o $< $(OBJDIR)/tlog.o $(LIBS) -o $@

# Binaires a tester sur la vraie machine dans ~/Downloads/ToTest/statsbar/,
# qui est a nous seuls et peut donc etre vide avant copie. La liste des
# tests reste a la racine de ToTest/, comme celles des autres projets.
totest: $(ACC) $(TOOLS)
	@mkdir -p $(TOTEST)
	rm -f $(TOTEST)/*
	cp $(ACC) $(TOOLS) $(TOTEST)/
	cp TODO_TESTS.txt $(TODO)
	@ls -l $(TOTEST)

# Generateur d'E/S pour voir le graphique sous Hatari. ECRIT sur A: :
# reserve a l'emulateur, ne va jamais dans ToTest/.
iogen: | $(OBJDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) src/crt0.S test/iogen.c $(LIBS) -o $(BUILD)/IOGEN.PRG

# Banc de mesure du cout CPU (application GEM). Jamais dans ToTest/.
# NULLACC.ACC est le temoin : meme boucle evnt_multi, aucun travail.
cputst: $(OBJDIR)/crt0.o $(OBJDIR)/tlog.o | $(OBJDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJDIR)/crt0.o test/cputst.c \
		$(OBJDIR)/tlog.o $(LIBS) -o $(BUILD)/CPUTST.PRG
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJDIR)/crt0.o test/nullacc.c \
		$(LIBS) -o $(BUILD)/NULLACC.ACC

# Variante de verification : la barre est figee dans l'etat "Alternate
# maintenu", Hatari ne sachant pas tenir une touche. Jamais dans ToTest/.
altest: | $(OBJDIR)
	$(CC) $(CFLAGS) -DSBAR_FORCE_IOKEY $(LDFLAGS) \
		src/crt0.S src/main.c src/display.c src/dialog.c src/panel.c \
		src/sysinfo.c src/config.c src/disk.c src/net.c src/measure.c src/probe.S \
		src/diskhook.S $(LIBS) -o $(BUILD)/ALTEST.ACC

# Variante de verification : le dialogue de configuration s'ouvre au
# demarrage, pour le controler sous Hatari sans cliquer. Jamais dans ToTest/.
dlgtest: | $(OBJDIR)
	$(CC) $(CFLAGS) -DSBAR_TEST_DIALOG $(LDFLAGS) \
		src/crt0.S src/main.c src/display.c src/dialog.c src/panel.c \
		src/sysinfo.c src/config.c src/disk.c src/net.c src/measure.c src/probe.S \
		src/diskhook.S $(LIBS) \
		-o $(BUILD)/DLGTEST.ACC

# Variante de verification de la bascule de vitesse et du .INF : demarre
# en lent, applique l'option "rapide au boot", inverse l'option dans
# C:\STATSBAR.INF puis ouvre le dialogue. Jamais dans ToTest/.
spdtest: | $(OBJDIR)
	$(CC) $(CFLAGS) -DSBAR_TEST_SPEED -DSBAR_TEST_DIALOG $(LDFLAGS) \
		src/crt0.S src/main.c src/display.c src/dialog.c src/panel.c \
		src/sysinfo.c src/config.c src/disk.c src/net.c src/measure.c src/probe.S \
		src/diskhook.S $(LIBS) \
		-o $(BUILD)/SPDTEST.ACC

# Profilage sous Hatari + MiNT + XaAES (--natfeats on) : l'accessoire
# ecrit toutes les 10 s son temps CPU sur la console de l'hote.
# PROF="-DPROF_NOIDLE ..." retire des postes. Jamais dans ToTest/.
proftest: | $(OBJDIR)
	$(CC) $(CFLAGS) -DSBAR_PROF $(PROF) $(LDFLAGS) \
		src/crt0.S src/main.c src/display.c src/dialog.c src/panel.c \
		src/sysinfo.c src/config.c src/disk.c src/net.c src/measure.c src/probe.S \
		src/diskhook.S test/prof.S $(LIBS) \
		-o $(BUILD)/PROF$(PROFTAG).ACC

# Programme qui fait redessiner la barre par XaAES toutes les 3 s, comme
# Thing en changeant d'application, pour chronometrer la reparation par
# un PROF.ACC (SHELLPRG=... sh test/mintfalc.sh). Jamais dans ToTest/.
menutst: $(OBJDIR)/crt0.o $(OBJDIR)/sysinfo.o $(OBJDIR)/measure.o $(OBJDIR)/probe.o | $(OBJDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJDIR)/crt0.o test/menutst.c \
		$(OBJDIR)/sysinfo.o $(OBJDIR)/measure.o $(OBJDIR)/probe.o \
		test/prof.S $(LIBS) -o $(BUILD)/MENUTST.PRG

# Variante de verification : le panneau de survol s'affiche au demarrage,
# Hatari ne sachant pas amener la souris sur le coin. Jamais dans ToTest/.
pnltest: | $(OBJDIR)
	$(CC) $(CFLAGS) -DSBAR_TEST_PANEL $(LDFLAGS) \
		src/crt0.S src/main.c src/display.c src/dialog.c src/panel.c \
		src/sysinfo.c src/config.c src/disk.c src/net.c src/measure.c src/probe.S \
		src/diskhook.S $(LIBS) -o $(BUILD)/PNLTEST.ACC

$(OBJDIR):
	@mkdir -p $(OBJDIR)

clean:
	rm -rf $(BUILD)

test: all
	@sh test/run-hatari.sh
