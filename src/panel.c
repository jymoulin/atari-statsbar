/*
 * StatsBar - petit panneau affiche quand la souris s'attarde sur le coin.
 *
 * C'est le seul element qui sort de la barre de menu : il se dessine sur le
 * bureau, qui appartient a d'autres. On sauvegarde donc le fond avant de
 * dessiner et on le restitue en partant (vro_cpyfm vers un tampon hors
 * ecran), plutot que de compter sur les messages de redessin - certaines
 * applications les traitent mal, et sous TOS mono-tache le bureau ne
 * redessine pas spontanement.
 *
 * Pas menu_popup() : il n'existe qu'a partir de l'AES 3.3, absent du
 * TOS 2.06 vise. Pas form_do() non plus pour les boutons : il est modal et
 * monopoliserait l'AES. On teste donc les clics a la main, ce qui est de
 * toute facon trivial sur quatre zones.
 *
 * L'ecran est reserve (wind_update) tant que le panneau est affiche, comme
 * le fait un menu deroulant. Le repli automatique au bout de PNL_LIFE tops
 * garantit qu'on ne bloque jamais durablement une autre application.
 *
 * DETECTION : on lit la position de la souris a chaque top plutot que
 * d'utiliser l'evenement de rectangle MU_M1 d'evnt_multi. Le
 * rectangle transmis etait pourtant exact - verifie par journalisation sur
 * la machine reelle : x=564 y=0 w=76 h=19, pile sur le graphique et
 * l'heure - mais l'AES declenchait une bonne largeur plus a gauche, dans
 * une zone vide de la barre. Cause non elucidee ; une position lue ne
 * laisse aucune place a l'interpretation. D'abord par graf_mkstate, puis
 * par la position que rend deja evnt_multi a chaque retour (mouse_x,
 * mouse_y) : graf_mkstate coutait 1 ms par top sous XaAES sur un STE.
 */
#include <mint/osbind.h>
#include "statsbar.h"

#define PNL_DWELL  2      /* tops de 200 ms avant ouverture : ~400 ms     */
#define PNL_LIFE   50     /* repli automatique au bout de ~10 s           */
#define PNL_ZONES  8      /* zones cliquables : vitesses + tailles        */

/* action d'une zone cliquable */
enum { Z_SPEED, Z_FONT };

typedef struct {
	short x, y, w, h;     /* en coordonnees ecran */
	short what;           /* Z_SPEED ou Z_FONT    */
	short arg;
} ZONE;

static short shown = 0;
static short dwell = PNL_DWELL;   /* decompte avant ouverture            */
static short life  = 0;
static short px, py, pw, ph;      /* rectangle du panneau, a l'ecran     */
static short cw, chh;             /* cellule de la police des menus      */

static ZONE  zone[PNL_ZONES];
static short nzone;

/* tampon de sauvegarde du fond */
static void *savebuf = 0L;
static long  savelen = 0L;
static MFDB  scr_mfdb, buf_mfdb;

/* textes du panneau */
static char l_mem[48];
static char l_cpu[24];

/* ------------------------------------------------------------------ */
static void geometry(void)
{
	short dummy;

	graf_handle(&cw, &chh, &dummy, &dummy);
	if (cw <= 0)  cw = 8;
	if (chh <= 0) chh = 8;

	/* 30 caracteres de large, trois lignes, marges d'un demi-caractere */
	pw = (short)(30 * cw);
	ph = (short)(3 * (chh + 2) + 6);
	px = (short)(dsp_scr_w() - pw);
	py = dsp_bar_h();
	if (px < 0)
		px = 0;
	if (py + ph > dsp_scr_h())
		ph = (short)(dsp_scr_h() - py);
}

/* ------------------------------------------------------------------ */
void pnl_open(void)
{
	short wdwidth;

	pnl_close();
	geometry();

	wdwidth = (short)((pw + 15) / 16);
	savelen = (long)wdwidth * 2L * (long)ph * (long)dsp_planes();
	savebuf = (void *)Malloc(savelen);
	if ((long)savebuf <= 0L) {
		savebuf = 0L;             /* sans tampon, pas de panneau */
		savelen = 0L;
		return;
	}

	scr_mfdb.fd_addr = 0L;        /* 0 = ecran */

	buf_mfdb.fd_addr    = savebuf;
	buf_mfdb.fd_w       = (short)(wdwidth * 16);
	buf_mfdb.fd_h       = ph;
	buf_mfdb.fd_wdwidth = wdwidth;
	buf_mfdb.fd_stand   = 0;      /* format du peripherique */
	buf_mfdb.fd_nplanes = dsp_planes();
	buf_mfdb.fd_r1 = buf_mfdb.fd_r2 = buf_mfdb.fd_r3 = 0;
}

void pnl_close(void)
{
	if (shown)
		pnl_leave();
	if (savebuf) {
		Mfree(savebuf);
		savebuf = 0L;
		savelen = 0L;
	}
}

short pnl_visible(void) { return shown; }

void pnl_rect(short *x, short *y, short *w, short *h)
{
	*x = px;
	*y = py;
	*w = pw;
	*h = ph;
}

/* ------------------------------------------------------------------ */
/* Sauvegarde et restitution du fond                                   */
/* ------------------------------------------------------------------ */
static void save_back(short restore)
{
	short vh = dsp_vdi();
	short pxy[8];

	if (!savebuf || vh <= 0)
		return;

	if (restore) {
		pxy[0] = 0;        pxy[1] = 0;
		pxy[2] = (short)(pw - 1);
		pxy[3] = (short)(ph - 1);
		pxy[4] = px;       pxy[5] = py;
		pxy[6] = (short)(px + pw - 1);
		pxy[7] = (short)(py + ph - 1);
		vro_cpyfm(vh, S_ONLY, pxy, &buf_mfdb, &scr_mfdb);
	} else {
		pxy[0] = px;       pxy[1] = py;
		pxy[2] = (short)(px + pw - 1);
		pxy[3] = (short)(py + ph - 1);
		pxy[4] = 0;        pxy[5] = 0;
		pxy[6] = (short)(pw - 1);
		pxy[7] = (short)(ph - 1);
		vro_cpyfm(vh, S_ONLY, pxy, &scr_mfdb, &buf_mfdb);
	}
}

/* ------------------------------------------------------------------ */
/* Dessin                                                             */
/* ------------------------------------------------------------------ */
/*
 * Rectangle plein encadre de noir. Le perimetre de v_bar est trace dans
 * la couleur de remplissage, donc invisible sur un fond blanc : le cadre
 * se dessine separement.
 */
static void box(short x, short y, short w, short h, short black)
{
	short vh = dsp_vdi();
	short pxy[10];

	pxy[0] = x;
	pxy[1] = y;
	pxy[2] = (short)(x + w - 1);
	pxy[3] = (short)(y + h - 1);
	vsf_interior(vh, FIS_SOLID);
	vsf_color(vh, black ? 1 : 0);
	vsf_perimeter(vh, 0);
	v_bar(vh, pxy);

	vsl_color(vh, 1);
	vsl_width(vh, 1);
	pxy[0] = x;                 pxy[1] = y;
	pxy[2] = (short)(x + w - 1); pxy[3] = y;
	pxy[4] = (short)(x + w - 1); pxy[5] = (short)(y + h - 1);
	pxy[6] = x;                 pxy[7] = (short)(y + h - 1);
	pxy[8] = x;                 pxy[9] = y;
	v_pline(vh, 5, pxy);
}

/* une zone cliquable, texte encadre, inversee si elle est active */
static void button(short x, short y, const char *txt, short on,
                   short what, short arg)
{
	short vh = dsp_vdi();
	short n, w;

	for (n = 0; txt[n]; n++)
		;
	w = (short)(n * cw + cw);

	box(x, y, w, (short)(chh + 2), on);
	/*
	 * MD_TRANSPARENT : en MD_REPLACE le VDI repeint toute la cellule de
	 * caractere, ce qui effacerait le fond noir d'un bouton actif et
	 * rendrait son texte blanc invisible.
	 */
	vswr_mode(vh, MD_TRANS);
	vst_color(vh, on ? 0 : 1);
	dsp_gtext(vh, (short)(x + cw / 2), (short)(y + 1), txt);
	vst_color(vh, 1);
	vswr_mode(vh, MD_REPLACE);

	if (nzone < PNL_ZONES) {
		zone[nzone].x = x;
		zone[nzone].y = y;
		zone[nzone].w = w;
		zone[nzone].h = (short)(chh + 2);
		zone[nzone].what = what;
		zone[nzone].arg = arg;
		nzone++;
	}
}

static short btn_width(const char *txt)
{
	short n;

	for (n = 0; txt[n]; n++)
		;
	return (short)(n * cw + cw);
}

static void compose(void)
{
	long st, alt;
	char *p;

	sys_mem(&st, &alt);
	p = s_cat(l_mem, "Libre  ");
	p = u_mem(p, st);
	p = s_cat(p, " ST");
	if (sys.alt_ram) {
		p = s_cat(p, " + ");
		p = u_mem(p, alt);
		p = s_cat(p, " rap.");
	}
	*p = '\0';

	p = s_cat(l_cpu, "CPU    ");
	p = u_dec(p, sys_mhz(), 0);
	p = s_cat(p, " MHz");
	*p = '\0';
}

static void draw(void)
{
	short vh = dsp_vdi();
	short x, y, i, dummy;

	nzone = 0;
	compose();

	dsp_use_menu_font();
	(void)dummy;

	/* fond blanc encadre de noir */
	box(px, py, pw, ph, 0);

	x = (short)(px + cw / 2);
	y = (short)(py + 2);

	vst_color(vh, 1);
	dsp_gtext(vh, x, y, l_mem);
	y += chh + 2;

	dsp_gtext(vh, x, y, l_cpu);
	if (sys.spd_ctrl != SPD_NONE) {
		short bx = (short)(px + pw - cw);
		char  t0[12], t1[12];
		char *p;
		short cur = sys_speed_get();

		p = u_dec(t0, sys.mhz_slow, 0); *p = '\0';
		p = u_dec(t1, sys.mhz_fast, 0); *p = '\0';
		bx -= btn_width(t1);
		button(bx, (short)(y - 1), t1, cur == 1, Z_SPEED, 1);
		bx -= btn_width(t0) + 2;
		button(bx, (short)(y - 1), t0, cur == 0, Z_SPEED, 0);
	}
	y += chh + 2;

	dsp_gtext(vh, x, y, "Police");
	{
		short bx = (short)(px + pw - cw);
		short n = dsp_font_count();
		short cur = dsp_font_current();

		for (i = (short)(n - 1); i >= 0; i--) {
			bx -= btn_width(dsp_font_name(i));
			button(bx, (short)(y - 1), dsp_font_name(i),
			       i == cur, Z_FONT, i);
			bx -= 2;
		}
	}
}

/* ------------------------------------------------------------------ */
/* Apparition et disparition                                          */
/* ------------------------------------------------------------------ */
static void show(void)
{
	if (shown || !savebuf || dsp_vdi() <= 0)
		return;

	geometry();
	/*
	 * BEG_MCTRL en plus de BEG_UPDATE : sans lui, un accessoire qui n'est
	 * pas au premier plan ne recoit aucun evenement bouton, et les zones
	 * du panneau seraient inertes. C'est ce que fait un menu deroulant.
	 */
	wind_update(BEG_UPDATE);
	wind_update(BEG_MCTRL);
	graf_mouse(M_OFF, 0L);
	dsp_clip(0);              /* the bar keeps its clipping on: not here */
	save_back(0);
	draw();
	dsp_vdi_state();          /* back to the bar's font and clipping */
	graf_mouse(M_ON, 0L);
	shown = 1;
	life = PNL_LIFE;
}

void pnl_leave(void)
{
	dwell = PNL_DWELL;
	if (!shown)
		return;
	graf_mouse(M_OFF, 0L);
	dsp_clip(0);
	save_back(1);
	dsp_clip(1);
	graf_mouse(M_ON, 0L);
	wind_update(END_MCTRL);
	wind_update(END_UPDATE);
	shown = 0;
}

/* la souris est-elle sur notre coin de barre ? */
static short in_bar(short mx, short my)
{
	short bx, bw;

	dsp_bar_area(&bx, &bw);
	return (my >= 0 && my < dsp_bar_h() && mx >= bx && mx < bx + bw);
}

static short in_panel(short mx, short my)
{
	return (mx >= px && mx < px + pw && my >= py && my < py + ph);
}

/*
 * Appele une fois par top. La temporisation evite d'ouvrir le panneau au
 * simple passage de la souris vers les menus ; le decompte de vie garantit
 * qu'on ne garde pas l'ecran reserve indefiniment.
 */
void pnl_poll(void)
{
	short mx = mouse_x, my = mouse_y;

#ifdef SBAR_TEST_PANEL
	if (shown)
		return;          /* ouverture forcee : on ne referme jamais */
#endif
	if (shown) {
		/* on reste tant qu'on est sur la barre OU sur le panneau */
		if ((!in_bar(mx, my) && !in_panel(mx, my)) || --life <= 0)
			pnl_leave();
		return;
	}

	if (in_bar(mx, my)) {
		if (dwell > 0 && --dwell == 0)
			show();
	} else {
		dwell = PNL_DWELL;
	}
}

/* ------------------------------------------------------------------ */
#ifdef SBAR_TEST_PANEL
/* make pnltest : ouverture forcee, Hatari ne sait pas deplacer la souris */
void pnl_show_now(void) { show(); life = 32767; }
#endif

short pnl_click(short mx, short my)
{
	short i;

	if (!shown)
		return 0;

	for (i = 0; i < nzone; i++) {
		if (mx >= zone[i].x && mx < zone[i].x + zone[i].w &&
		    my >= zone[i].y && my < zone[i].y + zone[i].h) {
			short what = zone[i].what;
			short arg  = zone[i].arg;

			/*
			 * On referme AVANT d'appliquer : changer la police
			 * reouvre la station VDI, ce qui invaliderait le fond
			 * sauvegarde.
			 */
			pnl_leave();
			if (what == Z_SPEED) {
				if (sys_speed_get() != arg)
					sys_speed_set(arg);
			} else {
				dsp_font_set(arg);
			}
			return 1;
		}
	}
	pnl_leave();
	return 0;
}
