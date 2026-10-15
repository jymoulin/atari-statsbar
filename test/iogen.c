/*
 * IOGEN - generateur d'E/S disque pour visualiser le graphique sous Hatari.
 *
 * RESERVE A L'EMULATEUR, sur une image disquette jetable : il ECRIT sur le
 * lecteur A:. Ne jamais le lancer sur une vraie machine.
 *
 * C'est une application GEM : entre deux salves il rend la main a l'AES
 * (evnt_timer), sans quoi l'accessoire ne recevrait aucun evenement et le
 * graphique resterait fige. Il enchaine des phases de lecture et
 * d'ecriture d'intensites differentes, en boucle.
 */
#include <mint/osbind.h>
#include <gem.h>

static char buf[16 * 512];

/* rwflag : 2 = lecture, 3 = ecriture, sans test de changement de media */
static void burst(short rwflag, short count, short ticks)
{
	short i;

	for (i = 0; i < ticks; i++) {
		if (count > 0)
			Rwabs(rwflag, buf, count, 20, 0);
		evnt_timer(200L);
	}
}

int main(void)
{
	short loop;

	appl_init();
	/* lire d'abord la zone que l'on reecrira, pour ne rien inventer */
	Rwabs(2, buf, 16, 20, 0);

	for (loop = 0; loop < 30; loop++) {
		burst(2, 1, 3);      /* lectures legeres              */
		burst(2, 16, 4);     /* lectures soutenues (8 Ko)     */
		burst(0, 0, 2);      /* pause                         */
		burst(3, 4, 3);      /* ecritures (2 Ko)              */
		burst(3, 16, 2);     /* ecritures soutenues           */
		burst(0, 0, 3);      /* pause                         */
	}
	appl_exit();
	return 0;
}
