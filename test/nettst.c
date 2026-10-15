/*
 * Reconnaissance du trafic reseau, avant de l'afficher : quelle pile est
 * trouvee, quels ports ou interfaces, quels compteurs, et le trafic
 * mesure seconde par seconde pendant 15 s. Lecture seule.
 * Trace dans C:\SBARNET.TXT.
 */
#include <mint/osbind.h>
#include "statsbar.h"
#include <mint/basepage.h>
#include "tlog.h"

static void show(const char *name, long in, long out, short counted)
{
	tlog("\r\n  ");
	tlog(name);
	tlog("  recus=");
	tlog_dec(in);
	tlog("  emis=");
	tlog_dec(out);
	tlog(counted ? "" : "   (ignore)");
}

static long rd_hz200(void) { return *(volatile long *)0x4baL; }

/* "NETTST Q" : pas d'attente de touche a la fin (lancement par mint.cnf) */
static short quiet(void)
{
	const char *c = _base->p_cmdlin;
	short i;

	for (i = 1; i <= (unsigned char)c[0]; i++)
		if (c[i] == 'Q' || c[i] == 'q')
			return 1;
	return 0;
}

static void finish(void)
{
	tlog("\r\n--- fin ---\r\n");
	if (!quiet())
		tlog_pause();
	tlog_close();
}

int main(void)
{
	short k, s;
	long t, d_in, d_out;

	tlog_open("C:\\SBARNET.TXT");
	tlog("--- StatsBar : trafic reseau ---");
	sys_detect();
	tlog("\r\nMiNT = ");
	tlog_dec((long)sys.mint);

	k = net_init();
	tlog("\r\npile  = ");
	tlog(k == NET_STING ? "STinG (octets)" :
	     k == NET_MINTNET ? "MintNet (paquets)" : "aucune");
	if (k == NET_NONE) {
		finish();
		return 0;
	}

	tlog("\r\nports / interfaces :");
	net_list(show);

	tlog("\r\n\r\n-- trafic par seconde, 15 s : lance un transfert maintenant --");
	for (s = 1; s <= 15; s++) {
		t = Supexec(rd_hz200);
		while (Supexec(rd_hz200) - t < 200L)
			;
		net_poll(&d_in, &d_out);
		tlog("\r\n  ");
		tlog_dec((long)s);
		tlog(" s : recus ");
		tlog_dec(d_in);
		tlog("  emis ");
		tlog_dec(d_out);
	}
	finish();
	return 0;
}
