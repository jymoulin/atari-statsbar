/*
 * StatsBar - detection de la machine, mesure de la frequence CPU,
 * bascule 8/16 MHz et lecture du temps CPU inoccupe.
 */
#include <mint/osbind.h>
#include <mint/cookie.h>
#include "statsbar.h"
#include "mhz.h"
#include "measure.h"

SYSTEM sys;

static long uptime_fh = -1;     /* u:\kern\uptime, garde ouvert */

CONFIG cfg = {
	1,      /* show_time  : heure hh:mm                       */
	1,      /* show_caps  : verrous clavier                   */
	1,      /* show_mhz   : frequence CPU                     */
	0x08,   /* hot_shift  : Alternate revele les graphiques   */
	1,      /* show_idle  : idle CPU (FreeMiNT uniquement)    */
	1,      /* show_graph : graphique papillon des E/S        */
	5,      /* graph_period : 5 x 200 ms = 1 s par colonne,   */
	        /*              32 colonnes = 32 s d'historique   */
	0,      /* show_rw    : temoins R/W (redondants avec le   */
	        /*              graphique)                        */
	0,      /* show_kb    : volume cumule                     */
	3,      /* rw_hold    : remanence, 3 x 200 ms             */
	0,      /* offset     : colonnes depuis le bord droit     */
	0,      /* boot_fast  : vitesse rapide au demarrage       */
	""      /* font_nm    : "" = police des menus GEM         */
};

/* version du TOS : _sysbase ($4F2) + 2, lisible en superviseur seulement */
static long rd_tos(void)
{
	return (long)*(unsigned short *)(*(long *)0x4f2L + 2L);
}

/* routines superviseur, cf. probe.S */
extern long sbar_probe_speed(void);
extern long sbar_probe_delta(void);
extern long sbar_probe_n;
extern long sbar_probe_mste(void);
extern long sbar_cookie_get(void);
extern long sbar_cookie_tag;
extern long sbar_cookie_val;
extern long sbar_mste_put(void);
extern long sbar_f030_get(void);
extern long sbar_f030_put(void);
extern char sbar_speed_val;

/*
 * Indice rendu par sbar_probe_speed() sur un 68000 a 8 MHz, mesure sous
 * Hatari (--machine ste) : 477, tres stable (+/- 0,2 %).
 *
 * Cet indice n'est PAS une frequence : il mesure la vitesse d'execution
 * reelle, qui depend autant du cache et du CPU que du quartz. Un MegaSTE
 * bascule a 16 MHz + cache donne 1096 (x2,3 et non x2), et un Falcon 030
 * a 16 MHz donne ~3500 (x7,3). On s'en sert donc comme indice de
 * performance relative, jamais pour afficher des MHz.
 */
#define SPEED_REF_8MHZ 477

/*
 * Indice (base 100) par MHz, par famille de CPU.
 *
 * Utilisable seulement quand le cache instruction du CPU est actif en
 * permanence, ce qui est le cas des 68020 et au-dela : la boucle de mesure
 * tient dedans, donc l'indice suit lineairement l'horloge. Verifie sur
 * STE + TwiSTEr en mode 68020 : indice 200 a 8 MHz et 590 a 24 MHz, soit
 * 25,0 et 24,6 par MHz.
 *
 * Inutilisable sur 68000, qui n'a aucun cache interne : le CPU est limite
 * par le bus, et le cache EXTERNE de 16 Ko du MegaSTE (bit 0 de $FFFF8E21,
 * sur la carte mere, pas dans le CPU) fausse tout. Passer de 8 a 16 MHz y
 * fait varier l'indice de 100 a 230 selon l'etat de ce cache, pas d'un
 * facteur 2.
 */
static short idx100_per_mhz(void)
{
	switch (sys.cpu) {
	case 20: return 25;
	default: return 0;      /* inconnu ou non fiable */
	}
}

/* ------------------------------------------------------------------ */
/* Conversion entier -> decimal, complete a gauche par des zeros.      */
/* ------------------------------------------------------------------ */
char *u_dec(char *p, short val, short digits)
{
	char tmp[8];
	short n = 0;

	if (val < 0)
		val = 0;
	do {
		tmp[n++] = (char)('0' + (val % 10));
		val /= 10;
	} while (val);
	while (n < digits)
		tmp[n++] = '0';
	while (n)
		*p++ = tmp[--n];
	return p;
}

/*
 * Taille memoire en cinq caracteres au plus : "898K" en dessous de 1 Mo,
 * puis "3.8M". Au-dela de 100 Mo on laisse tomber la decimale.
 */
char *u_mem(char *p, long bytes)
{
	long kb = bytes >> 10;
	long mb, tenth;

	if (kb < 1024L) {
		p = u_dec(p, (short)kb, 0);
		*p++ = 'K';
		return p;
	}
	mb    = kb >> 10;
	tenth = ((kb & 1023L) * 10L) >> 10;
	p = u_dec(p, (short)mb, 0);
	if (mb < 100L) {
		*p++ = '.';
		p = u_dec(p, (short)tenth, 0);
	}
	*p++ = 'M';
	return p;
}

char *s_cat(char *d, const char *s)
{
	while ((*d = *s) != '\0') {
		d++;
		s++;
	}
	return d;
}

char *u_dec32(char *p, long val)
{
	char tmp[12];
	short n = 0;

	if (val < 0)
		val = 0;
	do {
		tmp[n++] = (char)('0' + (short)(val % 10L));
		val /= 10L;
	} while (val);
	while (n)
		*p++ = tmp[--n];
	return p;
}

/* ------------------------------------------------------------------ */
/* Detection de la machine et des services disponibles                 */
/* ------------------------------------------------------------------ */
/* Cookie jar. Getcookie() de la MiNTLib ferait entrer toute la libc :  */
/* on parcourt la table nous-memes (probe.S), sous Supexec.            */
/* ------------------------------------------------------------------ */
short sys_cookie(long tag, long *val)
{
	sbar_cookie_tag = tag;
	if (Supexec(sbar_cookie_get) != 0L)
		return 0;
	*val = sbar_cookie_val;
	return 1;
}

void sys_detect(void)
{
	long val;
	long probe;

	sys.machine   = MACH_ST;
	sys.cpu       = 0;
	sys.mint      = 0;
	sys.multi     = 0;
	sys.alt_ram   = 0;
	sys.tos       = (short)Supexec(rd_tos);
	sys.kern      = 0;
	sys.idt       = -1;
	sys.spd_ctrl  = SPD_NONE;

	if (sys_cookie(C__MCH, &val)) {
		switch (val) {
		case 0x00000000L: sys.machine = MACH_ST;      break;
		case 0x00010000L: sys.machine = MACH_STE;     break;
		case 0x00010010L: sys.machine = MACH_MEGASTE; break;
		case 0x00020000L: sys.machine = MACH_TT;      break;
		case 0x00030000L: sys.machine = MACH_FALCON;  break;
		default:          sys.machine = MACH_OTHER;   break;
		}
	}

	if (sys_cookie(C__CPU, &val))
		sys.cpu = (short)val;
	if (sys_cookie(C__IDT, &val))
		sys.idt = (short)val;
	if (sys_cookie(C_MiNT, &val))
		sys.mint = (short)val;
	/*
	 * AES multitache ? Le test canonique est aes_global[1], le nombre
	 * d'applications simultanees : 1 pour un AES mono-tache, -1 pour un
	 * AES multitache. Le cookie MiNT ne suffit PAS : FreeMiNT peut tourner
	 * avec l'AES du TOS, toujours mono-tache. Seul MagiC est reconnu par
	 * son cookie en plus, par prudence. Doit etre appele apres appl_init :
	 * sans lui (outils .TOS), aes_global est a zero et on conclut "non".
	 */
	sys.multi = (aes_global[1] == -1) || sys_cookie(C_MagX, &val) ||
	            sys_cookie(C_MgMx, &val) || sys_cookie(C_MgPC, &val);

	/*
	 * Memoire alternative. NE PAS se fier a ramtop ($5A4) : il reste a 0
	 * quand la memoire est declaree par Maddalt(), ce que fait le pilote
	 * de la carte TwiSTEr. Seul Mxalloc mode 1 dit la verite, verifie sous
	 * EmuTOS, MagiC et TOS 2.06. Mxalloc n'existe qu'a partir du TOS 1.04.
	 */
	if (sys.tos >= 0x0104)
		sys.alt_ram = (Mxalloc(-1L, 1) > 0L);

	/*
	 * Registre cache/vitesse $FFFF8E21 : present sur MegaSTE, mais aussi
	 * sur un ST/STE equipe d'une carte 16 MHz (TwiSTEr, Hypercache), que
	 * le cookie _MCH n'annonce pas. On teste donc reellement le registre,
	 * protege d'une bus error, sur les seules machines ou il peut exister.
	 */
	if (sys.machine == MACH_ST || sys.machine == MACH_STE ||
	    sys.machine == MACH_MEGASTE) {
		probe = Supexec(sbar_probe_mste);
		if (probe >= 0)
			sys.spd_ctrl = SPD_MSTE;
	}

	/*
	 * Falcon : bit 0 de $FFFF8007, horloge du 68030 a 8 ou 16 MHz. Pas
	 * sur un Falcon accelere (_CPU 40/60 : CT60...), ou ce bit ne regle
	 * plus le processeur principal.
	 */
	if (sys.machine == MACH_FALCON && sys.cpu == 30)
		sys.spd_ctrl = SPD_F030;

	/*
	 * Frequences associees aux deux etats du bit 1 de $FFFF8E21.
	 *
	 * Le registre ne porte qu'UN bit : il dit "rapide" ou "lent", jamais
	 * combien de MHz. La valeur depend de la carte, pas du registre :
	 *   MegaSTE et cartes 16 MHz a 68000 : 8 / 16
	 *   TwiSTEr en mode 68020            : 8 / 24
	 * On choisit donc d'apres le type de CPU. Impossible de faire mieux
	 * sans connaitre la carte ; ces valeurs restent surchargeables.
	 *
	 * Mesurer ne remplace pas cette table : sur MegaSTE, passer a 16 MHz
	 * sans le cache ne gagne rien (477 -> 480), le 68000 restant bloque
	 * sur un bus a 8 MHz. L'indice ne suit donc pas l'horloge.
	 */
	sys.mhz_slow = 8;
	switch (sys.cpu) {
	case 20:  sys.mhz_fast = 24; break;   /* TwiSTEr 68020 */
	case 30:
	case 40:
	case 60:  sys.mhz_fast = 32; break;
	default:  sys.mhz_fast = 16; break;   /* MegaSTE, Hypercache */
	}
	if (sys.spd_ctrl == SPD_F030)
		sys.mhz_fast = 16;

	/*
	 * FreeMiNT expose le temps inoccupe dans /kern/uptime. Le fichier
	 * reste OUVERT : kernfs regenere son contenu a chaque lecture, et
	 * l'ouvrir a chaque fois coutait 27 ms sur un STE (resolution du
	 * chemin u:\kern\...), soit 2,7 % du processeur pour une lecture par
	 * seconde - mesure sous Hatari, make proftest.
	 */
	if (sys.mint) {
		uptime_fh = Fopen("u:\\kern\\uptime", 0);
		if (uptime_fh >= 0)
			sys.kern = 1;
	}

	/* mesures faites a part, voir sys_mhz() */
	sys.speed_idx = 0;
	sys.cpu_mhz = 0;
	sys.mhz_reg = -1;
}

/*
 * Frequence courante, mesuree au premier appel puis conservee.
 *
 * L'accessoire l'appelle une fois au demarrage (la mesure prend environ
 * une demi-seconde, qui passe inapercue au chargement des accessoires), si
 * bien que l'appui sur Alternate est ensuite instantane.
 *
 * Le cache est invalide si le registre de vitesse a change depuis la
 * mesure ($FFFF8E21, ou $FFFF8007 sur Falcon) : un autre
 * programme a pu changer la vitesse, comme le SPEED.PRG livre avec la
 * TwiSTEr (Ctrl+Alt+/- du pave numerique). Relire le registre ne coute
 * qu'un Supexec.
 */
short sys_mhz(void)
{
	long reg = -1;

	if (sys.spd_ctrl != SPD_NONE) {
		reg = Supexec(sys.spd_ctrl == SPD_F030 ? sbar_f030_get
		                                       : sbar_probe_mste);
		if (reg != sys.mhz_reg)
			sys.cpu_mhz = 0;
	}
	if (sys.cpu_mhz == 0) {
		sys.cpu_mhz = sys_nominal_mhz();
		sys.mhz_reg = reg;
	}
	return sys.cpu_mhz;
}

/* ------------------------------------------------------------------ */
/* Indice de vitesse mesure (100 ms)                                   */
/* ------------------------------------------------------------------ */
short sys_measure_speed(void)
{
	long loops = Supexec(sbar_probe_speed);

	/* ramene a une base 100 = 68000 a 8 MHz */
	sys.speed_idx = (short)((loops * 100L + (SPEED_REF_8MHZ / 2)) / SPEED_REF_8MHZ);
	return sys.speed_idx;
}

/* ------------------------------------------------------------------ */
/* Frequence nominale                                                  */
/*                                                                     */
/* On la deduit du registre $FFFF8E21 quand il existe (c'est exact et  */
/* instantane) et du type de machine sinon. Mesurer la frequence d'un  */
/* 68030 par une boucle d'instructions ne marche pas : le rapport      */
/* cycles/instruction n'a rien a voir avec celui d'un 68000.           */
/* ------------------------------------------------------------------ */
/*
 * Frequence par mesure differentielle (methode 000CLOCK de C. Zietz).
 *
 * On chronometre deux boucles qui ne different que par une instruction de
 * meme taille : la soustraction elimine tout ce qui depend du bus et ne
 * laisse que du temps CPU pur. C'est ce qui permet de voir les 16 MHz d'un
 * MegaSTE meme avec son cache externe eteint, la ou une simple boucle ne
 * mesure rien (480 contre 477) parce qu'elle est limitee par la memoire.
 *
 * Le nombre d'iterations est ajuste pour garder au moins 20 tops de
 * 200 Hz, sinon la resolution serait trop grossiere sur machine rapide.
 */
static short measure_mhz(void)
{
	/* frequence nominale la plus plausible, voir mhz_snap() */
	return mhz_snap(measure_mhz10((long)sys.cpu, 0));
}

short sys_nominal_mhz(void)
{
	short per = idx100_per_mhz();
	short fast;
	short m;

	/* la mesure differentielle prime : elle ne depend ni du registre ni
	 * de la vitesse du bus */
	m = measure_mhz();
	if (m > 0)
		return m;

	/*
	 * Quand la famille de CPU le permet, on MESURE la frequence courante
	 * au lieu de la deduire du registre. C'est le seul moyen fiable :
	 * sur STE + TwiSTEr en mode 68000, $FFFF8E21 est totalement inerte
	 * (477 dans les quatre combinaisons) alors que le bit 1 reflete bien
	 * la position du switch materiel. Se fier au registre afficherait
	 * 16 MHz sur une machine qui tourne a 8.
	 */
	if (per > 0) {
		if (sys.speed_idx == 0)
			sys_measure_speed();
		return (short)((sys.speed_idx + per / 2) / per);
	}

	/* MegaSTE : machine connue, le registre y est fiable */
	if (sys.spd_ctrl == SPD_MSTE && sys.machine == MACH_MEGASTE) {
		fast = sys_speed_get();
		if (fast >= 0)
			return fast ? sys.mhz_fast : sys.mhz_slow;
	}

	switch (sys.machine) {
	case MACH_TT:     return 32;
	case MACH_FALCON: return 16;
	default:          return 8;
	}
}

/* ------------------------------------------------------------------ */
/* Bascule de vitesse : MegaSTE et cartes compatibles, ou Falcon       */
/* ------------------------------------------------------------------ */
short sys_speed_get(void)
{
	long v;

	if (sys.spd_ctrl == SPD_F030)
		return (short)(Supexec(sbar_f030_get) & 0x01);
	if (sys.spd_ctrl != SPD_MSTE)
		return -1;
	v = Supexec(sbar_probe_mste);
	if (v < 0)
		return -1;
	return (v & 0x02) ? 1 : 0;   /* bit 1 = 16 MHz */
}

void sys_speed_set(short fast)
{
	if (sys.spd_ctrl == SPD_NONE)
		return;
	if (sys.spd_ctrl == SPD_F030) {
		sbar_speed_val = fast ? 0x01 : 0x00;
		Supexec(sbar_f030_put);
	} else {
		/* bit 0 = cache, bit 1 = 16 MHz : les deux ensemble, comme le
		 * fait le SPEED.PRG livre avec le TwiSTEr et le panneau du
		 * MegaSTE. */
		sbar_speed_val = fast ? 0x03 : 0x00;
		Supexec(sbar_mste_put);
	}
	sys.speed_idx = 0;              /* les anciennes mesures sont caduques */
	sys.cpu_mhz = 0;
}

/* ------------------------------------------------------------------ */
/* Temps CPU inoccupe, via /kern/uptime de FreeMiNT                    */
/*                                                                     */
/* Le fichier contient "<uptime> <idle>", en secondes avec 3 decimales.*/
/* On travaille en centiemes de seconde pour ne pas deborder.          */
/* ------------------------------------------------------------------ */
static long parse_cs(const char **pp)
{
	const char *p = *pp;
	long sec = 0;
	long frac = 0;
	short n;

	while (*p == ' ')
		p++;
	while (*p >= '0' && *p <= '9')
		sec = sec * 10 + (*p++ - '0');
	if (*p == '.') {
		p++;
		for (n = 0; n < 2 && *p >= '0' && *p <= '9'; n++)
			frac = frac * 10 + (*p++ - '0');
		while (*p >= '0' && *p <= '9')
			p++;
	}
	*pp = p;
	return sec * 100L + frac;
}

/*
 * Plus grand bloc libre, et non memoire libre totale : c'est ce qui
 * repond a "est-ce que ce programme va tenir". Les deux se confondent
 * sous TOS nu, la fragmentation les separe sous MiNT et MagiC.
 *
 * Deux valeurs, parce qu'un chiffre unique tromperait : sur la machine de
 * test, Malloc(-1) rend le bloc de RAM RAPIDE (6,3 Mo) alors que seuls
 * 3,9 Mo de ST-RAM sont utilisables par un programme qui l'exige.
 */
void sys_mem(long *st, long *alt)
{
	long v;

	if (sys.tos >= 0x0104) {
		v = Mxalloc(-1L, 0);
		*st = (v > 0L) ? v : 0L;
		*alt = sys.alt_ram ? Mxalloc(-1L, 1) : 0L;
		if (*alt < 0L)
			*alt = 0L;
	} else {
		v = Malloc(-1L);
		*st = (v > 0L) ? v : 0L;
		*alt = 0L;
	}
}

short sys_idle_percent(void)
{
	static long last_up = 0;
	static long last_idle = 0;
	char buf[64];
	const char *p = buf;
	long fh, len, up, idle, dup, didle;

	if (!sys.kern)
		return -1;

	fh = uptime_fh;
	if (Fseek(0L, (short)fh, 0) != 0)
		return -1;
	len = Fread((short)fh, (long)sizeof(buf) - 1, buf);
	if (len <= 0)
		return -1;
	buf[len] = '\0';

	up   = parse_cs(&p);
	idle = parse_cs(&p);

	dup   = up - last_up;
	didle = idle - last_idle;
	last_up   = up;
	last_idle = idle;

	if (dup <= 0 || didle < 0)
		return -1;          /* premier appel, ou compteurs incoherents */
	if (didle > dup)
		didle = dup;
	return (short)((didle * 100L) / dup);
}
