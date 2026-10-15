/*
 * Releve de la memoire disponible : quelles sources existent vraiment sur
 * la machine, et est-il utile de separer ST-RAM et memoire alternative ?
 *
 * Aucune allocation : Malloc(-1) et Mxalloc(-1, mode) ne font que rendre la
 * taille du plus grand bloc libre.
 */
#include <mint/osbind.h>
#include <mint/cookie.h>
#include "tlog.h"

extern long sbar_cookie_get(void);
extern long sbar_cookie_tag;
extern long sbar_cookie_val;

static short get_cookie(long tag, long *val)
{
	sbar_cookie_tag = tag;
	if (Supexec(sbar_cookie_get) != 0L)
		return 0;
	*val = sbar_cookie_val;
	return 1;
}

/* variables systeme, lisibles en superviseur seulement */
static long v_phystop, v_ramtop, v_ramvalid, v_osversion;

static long read_sysvars(void)
{
	v_phystop   = *(long *)0x42eL;
	v_ramtop    = *(long *)0x5a4L;
	v_ramvalid  = *(long *)0x5a8L;
	v_osversion = (long)*(unsigned short *)(*(long *)0x4f2L + 2L);
	return 0;
}

/* kb : convertir aussi en Ko (faux pour un cookie ou une signature) */
static void line(const char *label, long v, short kb)
{
	tlog("\r\n");
	tlog(label);
	tlog(" = ");
	tlog_dec(v);
	if (kb && v > 1024L) {
		tlog("  (");
		tlog_dec(v >> 10);
		tlog(" Ko)");
	}
}

/* rend l'adresse obtenue, dit si elle est en memoire alternative, libere */
static void try_alloc(const char *label, long a)
{
	tlog("\r\n");
	tlog(label);
	tlog(" -> ");
	if (a <= 0L) {
		tlog("echec");
		return;
	}
	tlog_dec(a);
	tlog(a >= v_phystop ? "   ALTERNATIVE" : "   ST-RAM");
	Mfree((void *)a);
}

int main(void)
{
	long val;

	tlog_open("C:\\SBARMEM.TXT");
	tlog("--- StatsBar : memoire disponible ---");

	Supexec(read_sysvars);
	tlog("\r\nTOS ");
	tlog_dec(v_osversion >> 8);
	tlog(".");
	if ((v_osversion & 0xff) < 10)
		tlog("0");
	tlog_dec(v_osversion & 0xff);

	if (get_cookie(C__MCH, &val))
		line("_MCH     ", val, 0);
	if (get_cookie(C__FRB, &val))
		line("_FRB     ", val, 0);  /* tampon de transfert, si memoire alt. */

	tlog("\r\n");
	line("phystop  ", v_phystop, 1);   /* haut de la ST-RAM       */
	line("ramtop   ", v_ramtop, 1);    /* haut de la memoire alt. */
	line("ramvalid ", v_ramvalid, 0);  /* 324517139 si ramtop est valide */

	tlog("\r\n");
	line("Malloc(-1)      ", Malloc(-1L), 1);

	/*
	 * Mxalloc n'existe qu'a partir du TOS 1.04 ; avant, l'appel n'est pas
	 * implemente et la reponse est imprevisible.
	 */
	if (v_osversion >= 0x0104) {
		line("Mxalloc(-1,0) ST ", Mxalloc(-1L, 0), 1);
		line("Mxalloc(-1,1) alt", Mxalloc(-1L, 1), 1);
		line("Mxalloc(-1,2) les deux", Mxalloc(-1L, 2), 1);
	} else {
		tlog("\r\nMxalloc : absent de ce TOS");
	}

	/*
	 * Allocation reelle de 64 Ko, rendue aussitot. L'ADRESSE tranche ce
	 * que Mxalloc(-1, mode) laisse ambigu : au-dessus de phystop, c'est
	 * de la memoire alternative, meme si le mode 1 repond 0.
	 */
	tlog("\r\n");
	tlog("\r\n-- allocation de 64 Ko, adresse obtenue --");
	tlog("\r\n   (au-dessus de ");
	tlog_dec(v_phystop);
	tlog(" = memoire alternative)");
	try_alloc("Malloc         ", Malloc(65536L));
	if (v_osversion >= 0x0104) {
		try_alloc("Mxalloc mode 0 ", Mxalloc(65536L, 0));
		try_alloc("Mxalloc mode 1 ", Mxalloc(65536L, 1));
		try_alloc("Mxalloc mode 2 ", Mxalloc(65536L, 2));
		try_alloc("Mxalloc mode 3 ", Mxalloc(65536L, 3));
	}

	tlog("\r\n--- fin ---\r\n");
	tlog_pause();
	tlog_close();
	return 0;
}
