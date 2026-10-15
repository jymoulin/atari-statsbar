/*
 * StatsBar - reglages sauvegardes.
 *
 * Un petit fichier texte a la racine du disque de demarrage, la ou le TOS
 * charge les accessoires : X:\STATSBAR.INF, X etant _bootdev ($446). Il
 * se lit et se corrige a la main :
 *
 *     FONT=6x6
 *     BOOTFAST=1
 *
 * FONT absent ou vide : la police des menus. Une ligne inconnue est
 * ignoree. Pas d'auto-modification du binaire : un .ACC reecrit par
 * lui-meme ne survit pas a une copie ou a un antivirus.
 *
 * Le fichier n'est reecrit que si un reglage sauvegarde a change : jamais
 * d'ecriture disque pour un simple changement de vitesse.
 */
#include <mint/osbind.h>
#include "statsbar.h"

#define INF_MAX 128

static char inf_path[] = "C:\\STATSBAR.INF";
static char inf_buf[INF_MAX];

/* derniers reglages lus ou ecrits, pour ne pas reecrire pour rien */
static short saved_boot = -1;
static char  saved_font[8];

static long rd_bootdev(void)
{
	return (long)*(volatile short *)0x446L;
}

static void set_path(void)
{
	short d = (short)Supexec(rd_bootdev);

	if (d >= 0 && d < 26)
		inf_path[0] = (char)('A' + d);
}

static short key_is(const char *p, const char *key)
{
	while (*key)
		if (*p++ != *key++)
			return 0;
	return 1;
}

static void remember(void)
{
	short i;

	saved_boot = cfg.boot_fast;
	for (i = 0; i < 7 && cfg.font_nm[i]; i++)
		saved_font[i] = cfg.font_nm[i];
	saved_font[i] = '\0';
}

void cfg_load(void)
{
	long fh, n;
	char *p, *end;
	short i;

	set_path();
	fh = Fopen(inf_path, 0);
	if (fh < 0) {
		remember();
		return;
	}
	n = Fread((short)fh, INF_MAX - 1, inf_buf);
	Fclose((short)fh);
	if (n < 0)
		n = 0;
	inf_buf[n] = '\0';

	p = inf_buf;
	end = inf_buf + n;
	while (p < end) {
		if (key_is(p, "FONT=")) {
			p += 5;
			for (i = 0; i < 7 && *p > ' '; i++)
				cfg.font_nm[i] = *p++;
			cfg.font_nm[i] = '\0';
		} else if (key_is(p, "BOOTFAST=")) {
			cfg.boot_fast = (p[9] == '1');
		}
		while (p < end && *p != '\n')      /* ligne suivante */
			p++;
		p++;
	}
	remember();
}

void cfg_save(void)
{
	long fh;
	char *p;
	short i, same = (cfg.boot_fast == saved_boot);

	for (i = 0; same && i < 8; i++) {
		if (cfg.font_nm[i] != saved_font[i])
			same = 0;
		if (!cfg.font_nm[i])
			break;
	}
	if (same)
		return;

	p = s_cat(inf_buf, "FONT=");
	p = s_cat(p, cfg.font_nm);
	p = s_cat(p, "\r\nBOOTFAST=");
	*p++ = (char)(cfg.boot_fast ? '1' : '0');
	p = s_cat(p, "\r\n");

	fh = Fcreate(inf_path, 0);
	if (fh < 0)
		return;                    /* disque protege : tant pis */
	Fwrite((short)fh, p - inf_buf, inf_buf);
	Fclose((short)fh);
	remember();
}
