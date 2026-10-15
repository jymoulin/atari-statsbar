/*
 * StatsBar - activite disque.
 *
 * Le comptage est fait par le handler assembleur (diskhook.S) insere dans
 * hdv_rw. Ici on ne fait que consommer ses compteurs depuis la boucle
 * d'evenements GEM : rien ne tourne en interruption.
 *
 * Limite connue, la meme que LED-Panel : les acces qui ne passent pas par
 * hdv_rw echappent au comptage (appels XHDI directs, et sous MiNT les
 * systemes de fichiers non-DOS comme ext2 ou minix).
 */
#include <mint/osbind.h>
#include <mint/ostruct.h>
#include "statsbar.h"

extern long sbar_rw_install(void);
extern long sbar_rw_remove(void);
extern long sbar_rw_calls;
extern long sbar_rw_rd;
extern long sbar_rw_wr;
extern long sbar_rw_map;
extern long sbar_rw_rd_drv[32];
extern long sbar_rw_wr_drv[32];

static short installed = 0;
static long  seen_rd = 0;
static long  seen_wr = 0;
static long  seen_rd_drv[32];
static long  seen_wr_drv[32];
static long  total_kb = 0;
static long  total_rest = 0;        /* octets pas encore comptes en Ko */

/*
 * Taille de secteur par lecteur, en octets. Elle n'a rien d'universel :
 * 512 sur disquette, mais une partition Atari un peu grosse monte a 1024,
 * 2048, 4096 voire 8192 octets par secteur logique. Sans cette valeur, le
 * volume transfere est faux d'un facteur 16.
 *
 * On interroge le BPB une seule fois par lecteur, et seulement quand ce
 * lecteur a montre de l'activite : le media est donc present, et on evite
 * de declencher un acces disque sur un lecteur vide.
 */
static short recsiz[32];

static short drive_recsiz(short drv)
{
	_BPB *bpb;

	if (drv < 0 || drv > 31)
		return 512;
	if (recsiz[drv])
		return recsiz[drv];

	bpb = (_BPB *)Getbpb(drv);
	if (bpb && bpb->recsiz > 0)
		recsiz[drv] = bpb->recsiz;
	else
		recsiz[drv] = 512;      /* on ne redemandera pas */
	return recsiz[drv];
}

short disk_install(void)
{
	short i;

	if (installed)
		return 1;
	Supexec(sbar_rw_install);
	installed = 1;
	seen_rd = sbar_rw_rd;
	seen_wr = sbar_rw_wr;
	for (i = 0; i < 32; i++) {
		seen_rd_drv[i] = sbar_rw_rd_drv[i];
		seen_wr_drv[i] = sbar_rw_wr_drv[i];
		recsiz[i] = 0;
	}
	total_kb = 0;
	total_rest = 0;
	return 1;
}

/*
 * Rend 0 si le handler a pu etre retire. Sinon un programme s'est installe
 * apres nous : il ne faut alors surtout pas liberer notre memoire.
 */
short disk_remove(void)
{
	long r;

	if (!installed)
		return 0;
	r = Supexec(sbar_rw_remove);
	if (r == 0)
		installed = 0;
	return (short)r;
}

/*
 * Consomme l'activite accumulee depuis le dernier appel. Les compteurs du
 * handler ne sont jamais remis a zero : on travaille par difference, donc
 * pas de course avec l'interruption.
 *
 * Le volume est calcule par lecteur, avec la taille de secteur de chacun,
 * et cumule au fil de l'eau plutot que recalcule depuis les compteurs
 * bruts : avec des secteurs de 8 Ko, un total en octets deborderait un
 * long en quelques centaines de Mo.
 */
void disk_poll(DISKACT *a)
{
	long rd = sbar_rw_rd;
	long wr = sbar_rw_wr;
	long map, d_rd, d_wr, siz;
	short i;

	a->rd_sect = rd - seen_rd;
	a->wr_sect = wr - seen_wr;
	seen_rd = rd;
	seen_wr = wr;

	map = sbar_rw_map;
	sbar_rw_map = 0;
	a->map = map;
	a->calls = sbar_rw_calls;
	a->rd_bytes = 0;
	a->wr_bytes = 0;

	for (i = 0; i < 32; i++) {
		d_rd = sbar_rw_rd_drv[i] - seen_rd_drv[i];
		d_wr = sbar_rw_wr_drv[i] - seen_wr_drv[i];
		if (d_rd <= 0 && d_wr <= 0)
			continue;
		seen_rd_drv[i] += d_rd;
		seen_wr_drv[i] += d_wr;
		siz = (long)drive_recsiz(i);
		a->rd_bytes += d_rd * siz;
		a->wr_bytes += d_wr * siz;
	}

	total_rest += a->rd_bytes + a->wr_bytes;
	total_kb += total_rest >> 10;
	total_rest &= 1023L;
	a->total_kb = total_kb;
}
