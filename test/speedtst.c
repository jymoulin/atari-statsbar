/*
 * Frequence CPU et registre cache/horloge $FFFF8E21.
 *
 * La frequence passe par measure_mhz10(), la MEME fonction que
 * l'accessoire (src/measure.c) : ce test valide donc directement ce que
 * l'accessoire affiche. Chaque ligne donne l'ecart des trois passes : un
 * ecart nettement plus grand que les autres est une passe perturbee par
 * des interruptions, que la mesure ecarte.
 *
 * Une serie de mesures repetees sert a verifier la stabilite : bouger la
 * souris et taper au clavier PENDANT cette serie ne doit plus faire varier
 * le resultat.
 *
 * Le registre est restaure dans son etat initial avant de rendre la main.
 * Trace sur la console et dans C:\SBARSPD.TXT
 */
#include <mint/osbind.h>
#include <mint/cookie.h>
#include "tlog.h"
#include "mhz.h"
#include "measure.h"

/* meme lecteur de cookie jar que l'accessoire : pas de libc (cf. crt0.S) */
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

extern long sbar_probe_speed(void);
extern long sbar_probe_mste(void);
extern long sbar_mste_put(void);
extern char sbar_speed_val;

static long cpu_type = 0;

static void put_mhz10(long f10)
{
	tlog_dec(f10 / 10);
	tlog(",");
	tlog_dec(f10 % 10);
	tlog(" MHz");
}

/* une mesure, avec son detail : ecart de chaque passe et meilleur */
static long one(const char *label)
{
	MEASINFO mi;
	long f10 = measure_mhz10(cpu_type, &mi);
	short r;

	tlog("\r\n");
	tlog(label);
	tlog(" ");
	put_mhz10(f10);
	tlog(" -> ");
	tlog_dec(mhz_snap(f10));
	tlog("  n=");
	tlog_dec(mi.n);
	tlog(" tops=");
	for (r = 0; r < MEAS_RUNS; r++) {
		if (r)
			tlog("/");
		tlog_dec(mi.tops[r]);
	}
	tlog(" best=");
	tlog_dec(mi.best);
	tlog(" t=");
	tlog_dec(mi.ticks * 5);
	tlog("ms");
	return f10;
}

static void set_reg(short v)
{
	sbar_speed_val = (char)v;
	Supexec(sbar_mste_put);
}

int main(void)
{
	long val, saved, f, fmin, fmax;
	short i;

	tlog_open("C:\\SBARSPD.TXT");
	tlog("--- StatsBar speed probe ---");

	tlog("\r\n_MCH = ");
	if (get_cookie(C__MCH, &val))
		tlog_dec(val);
	else
		tlog("absent");

	tlog("\r\n_CPU = ");
	if (get_cookie(C__CPU, &val)) {
		tlog_dec(val);
		cpu_type = val;
	} else
		tlog("absent");
	tlog("  delta100 = ");
	tlog_dec(mhz_delta100(cpu_type));

	saved = Supexec(sbar_probe_mste);
	tlog("\r\n8E21 = ");
	tlog_dec(saved);
	tlog("  (-1=absent b0=cache b1=rapide)");

	tlog("\r\nindice = ");
	tlog_dec(Supexec(sbar_probe_speed));

	/* --- stabilite dans l'etat courant --- */
	tlog("\r\n-- stabilite : BOUGER LA SOURIS maintenant --");
	fmin = fmax = 0;
	for (i = 0; i < 6; i++) {
		f = one("  mesure");
		if (i == 0 || f < fmin)
			fmin = f;
		if (i == 0 || f > fmax)
			fmax = f;
	}
	tlog("\r\n  ecart min-max = ");
	put_mhz10(fmax - fmin);

	/* --- les quatre etats du registre --- */
	if (saved >= 0) {
		tlog("\r\n-- etats du registre --");
		set_reg(0x00); one("lent  sans cache  ");
		set_reg(0x01); one("lent  avec cache  ");
		set_reg(0x02); one("rapide sans cache ");
		set_reg(0x03); one("rapide avec cache ");

		/* on remet la machine exactement comme on l'a trouvee */
		set_reg((short)saved);
		tlog("\r\n8E21 restaure a ");
		tlog_dec(Supexec(sbar_probe_mste));
	}

	tlog("\r\n--- fin ---\r\n");
	tlog_pause();
	tlog_close();
	return 0;
}
