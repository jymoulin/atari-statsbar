/*
 * Cout CPU de l'accessoire : une application GEM compte ses iterations de
 * travail pendant 10 s, en rendant la main a l'AES a chaque tour. Sous TOS
 * mono-tache c'est a ce moment-la que les accessoires tournent. On compare
 * le compte obtenu avec StatsBar et avec l'accessoire TEMOIN NULLACC.ACC,
 * qui fait la meme boucle evnt_multi sans rien d'autre : l'ecart est ce que
 * StatsBar consomme. Ne jamais comparer a "aucun accessoire" : l'AES
 * emprunte alors un chemin d'attente plus lent et le resultat est faux.
 *
 * On travaille EN CONTINU et on ne rend la main qu'une fois toutes les
 * 20 ms : compter des tours bornes par evnt_timer() mesurerait la cadence
 * de l'AES (50 par seconde, quoi qu'il arrive), pas le CPU disponible.
 * Ce qu'on compte, c'est donc le travail accompli entre deux rendus.
 */
#include <mint/osbind.h>
#include <gem.h>
#include "tlog.h"

#define SECONDS 10L

static long rd_hz200(void)
{
	return *(volatile long *)0x4baL;
}

static volatile long sink;

int main(void)
{
	long t0, t1, n = 0;
	short i;

	appl_init();
	tlog_open("C:\\SBARCPU.TXT");
	tlog("--- StatsBar : cout CPU ---");

	/* laisser le bureau et les accessoires finir de s'installer */
	evnt_timer(2000L);

	t0 = Supexec(rd_hz200);
	t1 = t0;
	{
		long slot = t0;

		do {
			for (i = 0; i < 20; i++)
				sink += i;
			n++;
			if ((n & 15) != 0)
				continue;                 /* horloge lue 1 tour sur 16 */
			t1 = Supexec(rd_hz200);
			if (t1 - slot >= 4) {         /* 20 ms ecoulees */
				evnt_timer(0L);           /* les accessoires tournent */
				slot = Supexec(rd_hz200);
			}
		} while (t1 - t0 < SECONDS * 200L);
	}

	tlog("\r\ntours en ");
	tlog_dec(SECONDS);
	tlog(" s = ");
	tlog_dec(n);
	tlog("\r\n--- fin ---\r\n");
	tlog_close();
	appl_exit();
	return 0;
}
