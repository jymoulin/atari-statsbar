/*
 * StatsBar - point d'entree.
 *
 * Le meme binaire fonctionne en accessoire (STATSBAR.ACC) et en application
 * (STATSBAR.APP). Sous TOS mono-tache, seul l'accessoire a du sens puisqu'une
 * application monopoliserait la machine ; sous MiNT ou MagiC, les deux vont.
 */
#include <mint/osbind.h>
#include <mint/basepage.h>
#include "statsbar.h"

#define TICK_MS     200L   /* periode de rafraichissement           */
#ifdef PROF_NOFORCE
#define FORCE_TICKS 30000
#else
#define FORCE_TICKS 1500   /* repeinte de securite toutes les 5 min */
#endif

static short is_acc;
static short menu_id = -1;

long kb_state = 0;
short mouse_x = 0, mouse_y = 0;    /* 0 : pres de la barre, on masque */
short mouse_b = 0;

#ifdef SBAR_PROF
/*
 * make proftest : toutes les 10 s, le temps CPU consomme par l'accessoire
 * (Prusage de MiNT) et le nombre de reveils, ecrits sur la console de
 * Hatari par NatFeats. PROF_NOIDLE, PROF_NOFORCE, PROF_NOPOLL retirent
 * chacun un poste pour en mesurer le cout.
 */
extern long prof_puts(void);
extern long prof_usage(void);
extern long prof_r[8];
extern char *prof_str;

static long rd_hz200(void)
{
	return *(volatile long *)0x4baL;
}

/* one line on the host console, stamped with the 200 Hz clock */
void prof_event(const char *what)
{
	static char ev[64];
	char *p = s_cat(ev, "SBAR ");

	p = s_cat(p, what);
	p = s_cat(p, " hz ");
	p = u_dec32(p, Supexec(rd_hz200));
	p = s_cat(p, "\n");
	*p = '\0';
	prof_str = ev;
	Supexec(prof_puts);
}

static void prof_tick(short evt)
{
	static long n_loop = 0, n_timer = 0, last = 0, t0 = 0;
	static short k = 0;
	static char line[96];
	long now;
	char *p;

	n_loop++;
	if (evt & MU_TIMER)
		n_timer++;
	if (++k < 50)
		return;
	k = 0;
	prof_usage();
	now = prof_r[0] + prof_r[1];
	if (t0 == 0)
		t0 = Tgettime();
	p = s_cat(line, "SBAR prof: ");
	p = u_dec32(p, now - last);
	p = s_cat(p, " ms CPU (sys ");
	p = u_dec32(p, prof_r[0]);
	p = s_cat(p, " usr ");
	p = u_dec32(p, prof_r[1]);
	p = s_cat(p, ") boucles ");
	p = u_dec32(p, n_loop);
	p = s_cat(p, " timers ");
	p = u_dec32(p, n_timer);
	p = s_cat(p, "\n");
	*p = '\0';
	prof_str = line;
	Supexec(prof_puts);
	last = now;
	n_loop = 0;
	n_timer = 0;
}
#endif

/*
 * Raccourci de vitesse : Ctrl + Alternate + Shift (l'un ou l'autre)
 * bascule entre lent et rapide. Rien que des touches de modification,
 * lues par Kbshift a chaque top : aucun detournement du clavier, et
 * l'application au premier plan ne recoit aucune touche parasite. Il faut
 * tenir la combinaison un top (200 ms) ; elle n'agit qu'une fois par
 * appui. La frequence s'affiche alors 3 s a la place de l'heure.
 */
static void speed_key(void)
{
	static short held = 0;
	long k = kb_state & 0x0f;
	short now = ((k & 0x0c) == 0x0c) && (k & 0x03);
	short cur;

	if (now && !held) {
		cur = sys_speed_get();
		if (cur >= 0) {
			sys_speed_set((short)!cur);
			dsp_flash_mhz();
		}
	}
	held = now;
}

int main(void)
{
	short msg[8];
	short evt, tick = 0;
	short ox, oy, obut, okst, okey, obrk;
	short other;
	short running = 1;

	if (appl_init() < 0)
		return 1;

	/* un accessoire est charge par l'AES et n'a pas de basepage parente */
	is_acc = (_base->p_parent == 0L);

	/*
	 * Une seule instance a la fois : l'accessoire et l'application sont le
	 * meme binaire. En lancer un second installerait un deuxieme handler
	 * hdv_rw et superposerait deux affichages.
	 */
	other = appl_find("STATSBAR");
	if (other >= 0 && other != gl_apid) {
		form_alert(1, "[1][StatsBar tourne deja.|"
		              "Configuration : menu Bureau.][OK]");
		appl_exit();
		return 0;
	}

	sys_detect();
	cfg_load();
#ifdef PROF_NOIDLE
	cfg.show_idle = 0;
#endif

#ifdef SBAR_TEST_SPEED
	sys_speed_set(0);       /* make spdtest : simule un demarrage lent */
#endif
	/* option "rapide au boot" : seulement si on n'y est pas deja */
	if (cfg.boot_fast && sys_speed_get() == 0)
		sys_speed_set(1);
#ifdef SBAR_TEST_SPEED
	/* ...et inverse l'option dans le .INF : deux lancements de suite
	   verifient lecture, ecriture et passage en rapide */
	cfg.boot_fast = (short)!cfg.boot_fast;
	cfg_save();
#endif

	if (cfg.show_graph || cfg.show_rw || cfg.show_kb)
		disk_install();

	/* STinG ou MintNet : le graphique reseau n'existe que si l'un est la */
	if (cfg.show_graph)
		net_init();

	if (!dsp_open()) {
		appl_exit();
		return 1;
	}

	pnl_open();

	/*
	 * Un accessoire s'inscrit toujours. Une APPLICATION ne s'inscrit que
	 * sous AES multitache : sous TOS mono-tache elle se termine en
	 * refermant son dialogue, et l'entree du menu Bureau resterait a
	 * pointer sur sa memoire liberee - d'ou une ligne de caracteres
	 * aleatoires dans le menu. menu_unregister() n'existant qu'a partir
	 * de l'AES 3.3, on ne peut pas nettoyer apres coup : on ne s'inscrit
	 * pas.
	 */
	if (is_acc || sys.multi)
		menu_id = menu_register(gl_apid, "  StatsBar");

	/*
	 * Frequence mesuree des le demarrage : environ une demi-seconde,
	 * inapercue pendant le chargement des accessoires. L'appui sur
	 * Alternate est ensuite instantane, alors qu'avant il declenchait la
	 * mesure et figeait brievement la machine.
	 */
	if (cfg.show_mhz)
		sys_mhz();

	/*
	 * En application, l'AES nous laisse la souris en abeille : on remet la
	 * fleche.
	 *
	 * Sous un AES mono-tache, une application n'a pas droit au menu
	 * Bureau et le bureau ne revient qu'a sa fin : le dialogue, avec son
	 * bouton Quitter, est sa SEULE interface. On l'ouvre donc tout de
	 * suite et on s'arrete des qu'il se referme.
	 *
	 * Sous un AES multitache (MagiC, XaAES), l'entree StatsBar apparait
	 * bien dans le menu Bureau - verifie par jym sous MagiC. Le programme
	 * demarre alors en silence, comme un accessoire, ce qui convient a un
	 * lancement au boot ; Quitter reste accessible par le menu.
	 */
	if (!is_acc) {
		graf_mouse(ARROW, 0L);
		if (!sys.multi || menu_id < 0) {
			dlg_open(1);
			running = 0;
		}
	}

	if (running)
		dsp_update(1);

#ifdef SBAR_TEST_DIALOG
	/* make dlgtest : ouvre le dialogue au demarrage, pour le controler
	   sous Hatari sans avoir a cliquer dans le menu Bureau */
	dlg_open(0);
#endif
#ifdef SBAR_TEST_PANEL
	/* make pnltest : affiche le panneau de survol au demarrage, Hatari ne
	   sachant pas amener la souris sur le coin */
	pnl_show_now();
#endif

	while (running) {
		/*
		 * MU_BUTTON seulement quand le panneau est ouvert : c'est alors
		 * qu'on tient BEG_MCTRL, donc que les clics nous parviennent.
		 * Le survol, lui, se detecte par la position de la souris que
		 * rend chaque evnt_multi, et non par MU_M1 - voir l'entete de
		 * src/panel.c.
		 */
		short ev = MU_MESAG | MU_TIMER;

		if (pnl_visible())
			ev |= MU_BUTTON;

		evt = evnt_multi(ev,
		                 1, 1, 1,         /* un clic, bouton gauche */
		                 0, 0, 0, 0, 0,
		                 0, 0, 0, 0, 0,
		                 msg, (unsigned long)TICK_MS,
		                 &ox, &oy, &obut, &okst, &okey, &obrk);

		mouse_x = ox;
		mouse_y = oy;
		mouse_b = obut;
#ifdef SBAR_PROF
		prof_tick(evt);
#endif
		if (evt & MU_BUTTON)
			pnl_click(ox, oy);

		if (evt & MU_MESAG) {
			switch (msg[0]) {
			case AC_OPEN:
				if (msg[4] == menu_id && dlg_open(!is_acc))
					running = 0;
				break;
			case AC_CLOSE:
				break;
			case AP_TERM:
				running = 0;
				break;
			case WM_REDRAW:
				dsp_update(1);
				break;
			}
		}

#ifdef PROF_NULL
		evt &= ~MU_TIMER;       /* reveil a vide : cout de l'AES seul */
#endif
#ifdef PROF_READONLY
		{
			static short n = 0;     /* lecture seule, sans dessin */
			if (++n >= 5) { n = 0; sys_idle_percent(); }
		}
#endif
		if (evt & MU_TIMER) {
#ifndef PROF_NOKB
			kb_state = Kbshift(-1);
#endif
			speed_key();
#ifndef PROF_NOTICK
			dsp_tick();             /* fait avancer l'historique d'E/S */
#endif
#ifndef PROF_NOPOLL
			pnl_poll();             /* suit la souris pour le panneau */
#endif
			/*
			 * Tant que le panneau est affiche on ne redessine pas
			 * la barre : dsp_update() prend wind_update() de son
			 * cote, et l'imbrication n'est pas fiable sur tous les
			 * AES. L'historique, lui, continue d'avancer.
			 */
			if (pnl_visible()) {
				tick = 0;
			} else if (++tick >= FORCE_TICKS) {
				tick = 0;
				/*
				 * Une application qui change sa barre de menu ecrase
				 * notre coin sans qu'on en soit averti. Le pixel
				 * temoin (display.c) le detecte a chaque top ; cette
				 * repeinte sans effacement, invisible, n'est qu'une
				 * securite pour un effacement qui l'aurait epargne.
				 */
				dsp_update(2);
			} else {
#ifndef PROF_NOUPD
				dsp_update(0);
#endif
			}
		}
	}

	pnl_close();
	dsp_close();

	/*
	 * Si un autre programme s'est accroche a hdv_rw apres nous, on ne peut
	 * pas se retirer sans casser sa chaine : dans ce cas on reste en
	 * memoire plutot que de rendre une zone encore referencee.
	 */
	if (disk_remove() != 0) {
		appl_exit();
		Ptermres(0L, 0);
	}

	appl_exit();
	return 0;
}
