/*
 * StatsBar - mesure de la frequence CPU.
 *
 * Methode differentielle de C. Zietz (000CLOCK.TOS) : deux boucles ne
 * differant que par nop et mulu.w, de meme taille, donc de meme motif
 * d'acces au bus. La difference de leurs durees, chronometree sur _hz_200,
 * ne garde que du temps CPU pur.
 *
 * On garde la MEDIANE de trois passes :
 *   - une passe perturbee par des interruptions (clavier, souris,
 *     ordonnanceur de MagiC) a un ecart gonfle ; la mediane l'ecarte. Une
 *     passe isolee pouvait tomber de 24 a 17 MHz.
 *   - le plus petit ecart, essaye d'abord, est biaise : avec une
 *     resolution de 5 ms, chaque passe varie de +/- 1 top, et le minimum
 *     retient toujours la plus favorable. Sur MegaSTE a 16 MHz il donnait
 *     16,9, arrondi a 17. La mediane n'a pas ce biais.
 *
 * Tout se fait interruptions AUTORISEES, donc sans aucun risque de geler la
 * machine ou de perdre des octets clavier. Une variante en tranches
 * masquees chronometrees sur le Timer C du MFP a ete essayee puis retiree :
 * juste sous Hatari, elle s'est revelee fausse sur la TwiSTEr reelle, ou
 * deux lectures du compteur a 300 us d'intervalle rendaient la meme valeur
 * (voir ChangeLog, 2026-09-21).
 */
#include <mint/osbind.h>
#include "mhz.h"
#include "measure.h"

extern long sbar_probe_delta(void);
extern long sbar_probe_n;

/* Ecart vise par passe, en tops de 5 ms : 20 tops donnent une resolution
 * de 5 %, ce qu'il faut pour que le classement vers une frequence nominale
 * (mhz_snap) separe surement 16 et 24 MHz malgre le biais du systeme. */
#define TARGET_TOPS  20L

/* _hz_200, lu en superviseur */
static long rd_hz200(void)
{
	return *(volatile long *)0x4baL;
}

/*
 * Nombre d'iterations de depart : de quoi obtenir ~21 tops d'ecart a
 * 8 MHz, la vitesse la plus basse envisageable. Un CPU plus rapide
 * rend un ecart plus court et fait grossir n.
 *   68000  : 40 cycles d'ecart par tour -> 21000 tours = 21 tops a 8 MHz
 *   68020+ : 26,4 cycles                -> 32000 tours = 21 tops a 8 MHz
 */
static long start_iters(long cpu)
{
	return (cpu == 0) ? 21000L : 32000L;
}

/*
 * Plafond de n. Il ne sert qu'en cas d'anomalie (_hz_200 fige) : sans lui,
 * n grossirait indefiniment. Il borne la duree d'une passe, au pire, a
 * ~1 s sur un 68000 a 8 MHz et ~2 s sur un 68020 a 8 MHz. Interruptions
 * autorisees : ce serait une attente, jamais un gel.
 */
static long max_iters(long cpu)
{
	return (cpu == 0) ? 100000L : 400000L;
}

long measure_mhz10(long cpu, MEASINFO *info)
{
	long d100 = mhz_delta100(cpu);
	long n, t, best, f, t0, a, b, c;
	short r;

	if (info) {
		info->n = info->best = info->ticks = 0;
		for (r = 0; r < MEAS_RUNS; r++)
			info->tops[r] = 0;
	}
	if (d100 <= 0)
		return 0;

	t0 = Supexec(rd_hz200);

	/* premiere passe, en grossissant n jusqu'a un ecart suffisant */
	n = start_iters(cpu);
	for (;;) {
		sbar_probe_n = n;
		t = Supexec(sbar_probe_delta);
		if (t >= TARGET_TOPS || n >= max_iters(cpu))
			break;
		/*
		 * On vise ~22 tops d'un coup, proportionnellement a l'ecart
		 * obtenu. Un facteur entier arrondi par exces depassait la cible
		 * (x3 pour 10 tops donnait 32 tops, soit 1,6 s de mesure).
		 */
		f = n;
		n = (t > 0) ? n * 22L / t : n * 8L;
		if (n < f * 2L)
			n = f * 2L;
		if (n > f * 8L)
			n = f * 8L;
		if (n > max_iters(cpu))
			n = max_iters(cpu);
	}
	a = t;

	/* deux passes de plus au meme n, puis mediane des trois */
	b = Supexec(sbar_probe_delta);
	c = Supexec(sbar_probe_delta);
	if (info) {
		info->tops[0] = a;
		info->tops[1] = b;
		info->tops[2] = c;
	}
	if ((a <= b && b <= c) || (c <= b && b <= a))
		best = b;
	else if ((b <= a && a <= c) || (c <= a && a <= b))
		best = a;
	else
		best = c;

	if (info) {
		info->n = n;
		info->best = best;
		info->ticks = Supexec(rd_hz200) - t0;
	}
	if (best <= 0)
		return 0;               /* _hz_200 fige : mesure impossible */
	return mhz_tenths(n, d100, best);
}
