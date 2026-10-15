/*
 * StatsBar - rendu dans le coin haut-droit de la barre de menu.
 *
 * Tout passe par le VDI : c'est un peu plus lent qu'une ecriture directe en
 * memoire ecran facon LED-Panel, mais c'est ce qui permet de fonctionner sur
 * Falcon en VGA, en true color et sur carte graphique. On ne redessine que
 * ce qui a change, donc le cout reel est negligeable.
 *
 * La zone comprend, de gauche a droite : le graphique papillon des E/S
 * disque, puis le texte (verrous, idle, heure ou frequence).
 */
#include <mint/osbind.h>
#include "statsbar.h"

static short vh = 0;
static short scr_w, scr_h;
static short bar_h;                 /* hauteur de la barre de menu (gl_hbox) */
static short chr_w, chr_h;          /* cellule de la police systeme          */
static short cell_w, cell_h;        /* cellule reellement obtenue du VDI     */
static char  last[48] = "";

/* ce qui est actuellement dessine, pour savoir quoi effacer ou deplacer */
static short last_left;             /* bord gauche de toute la zone        */
static short last_lg = -1;          /* graphique a gauche, -1 = aucun      */
static short last_rg = -1;          /* graphique a droite (a la place du texte) */
static short planes = 1;            /* plans de l'ecran, pour les MFDB     */
static short ncolors = 2;           /* couleurs simultanees (work_out[13]) */
static short fcolor = -1;           /* fill colour set on vh, -1 = unknown */
static void graph_colors(void);
static void forget_screen(void);
#ifdef SBAR_PROF
extern void prof_event(const char *what);
#endif

/*
 * Under MiNT every VDI call is a trap, about 0.3 ms on a real Falcon:
 * the fill colour is only set when it actually changes. The cache is
 * dropped whenever someone else may have drawn with our workstation
 * (dsp_vdi_state, after the panel).
 */
static void fill_color(short c)
{
	if (c != fcolor) {
		vsf_color(vh, c);
		fcolor = c;
	}
}

/* drawable height of the bar: its last line is the AES separator */
static short area_height(void)
{
	return (short)((bar_h > 1) ? bar_h - 1 : bar_h);
}

/*
 * Clipping stays on the bar for good: two VDI calls less per update.
 * The hover panel, the only other user of the workstation, draws below
 * the bar and switches it off meanwhile.
 */
void dsp_clip(short on)
{
	short pxy[4];

	if (vh <= 0)
		return;
	pxy[0] = 0;
	pxy[1] = 0;
	pxy[2] = (short)(scr_w - 1);
	pxy[3] = (short)(area_height() - 1);
	vs_clip(vh, on, pxy);
}

/*
 * v_gtext maison. Celui de libgem reserve 2104 octets de pile a CHAQUE
 * appel (un tableau de 1024 mots pour convertir la chaine), ce qui a lui
 * seul imposait une pile de 8 Ko ; nos textes font moins de 48 caracteres.
 * Tableaux statiques : l'accessoire n'est pas reentrant, et ils passent de
 * la pile au BSS pour 150 octets.
 */
#define TXT_MAX 64

extern void sbar_vdi(void *pb);

static short vt_contrl[12];
static short vt_intin[TXT_MAX];
static short vt_ptsin[4];
static short vt_intout[4];
static short vt_ptsout[4];
static short *vt_pb[5] = { vt_contrl, vt_intin, vt_ptsin, vt_intout, vt_ptsout };

void dsp_gtext(short h, short x, short y, const char *str)
{
	short n = 0;

	while (str[n] && n < TXT_MAX) {
		vt_intin[n] = (short)(unsigned char)str[n];
		n++;
	}
	vt_contrl[0] = 8;           /* v_gtext */
	vt_contrl[1] = 1;           /* un point */
	vt_contrl[3] = n;           /* caracteres */
	vt_contrl[5] = 0;
	vt_contrl[6] = h;
	vt_ptsin[0] = x;
	vt_ptsin[1] = y;
	sbar_vdi(vt_pb);
}

static short s_same(const char *a, const char *b)
{
	while (*a && *a == *b) {
		a++;
		b++;
	}
	return (*a == *b);
}

static char *s_copy(char *d, const char *s)
{
	while ((*d = *s) != '\0') {
		d++;
		s++;
	}
	return d;
}

/*
 * Choix de la police.
 *
 * On passe par vst_point() et non vst_height() : vst_height() met la police
 * systeme a l'echelle et rend n'importe quelle taille intermediaire, souvent
 * illisible (en 320x200, vst_height(8) donne une cellule 11x10 et les tres
 * petites valeurs donnent de la bouillie).
 *
 * vst_point() lui-meme ne se limite pas aux tailles gravees : au-dela de la
 * police des menus il rend des versions MISES A L'ECHELLE, plus larges
 * qu'elle. En 640x400 (cellule des menus 8x16) il rend ainsi 12x12, 16x16
 * puis 16x32. Ne juger que la hauteur faisait donc passer 12x12 pour "plus
 * petit" que 8x16, alors que c'est visiblement plus gros. Le critere porte
 * sur les DEUX dimensions : on ne retient que les tailles dont la cellule
 * tient dans celle des menus. Il en reste trois sur un VDI Atari :
 * 6x6, 8x8 et 8x16 (deux seulement en 640x200 : 6x6 et 8x8).
 *
 * La liste est rendue au dialogue de configuration ; la derniere entree est
 * toujours la police des menus GEM.
 */
#define FONT_MAX 6

static short font_pt[FONT_MAX];
static short font_cw[FONT_MAX];
static short font_ch[FONT_MAX];
static char  font_nm[FONT_MAX][8];
static short font_n = 0;
static short font_cur = 0;

static void font_label(short i)
{
	char *p = font_nm[i];

	p = u_dec(p, font_cw[i], 0);
	*p++ = 'x';
	p = u_dec(p, font_ch[i], 0);
	*p = '\0';
}

static void pick_font(void)
{
	short pt, cw, ch, cellw, cellh, i;

	vst_font(vh, 1);
	font_n = 0;

	for (pt = 1; pt <= 24 && font_n < FONT_MAX; pt++) {
		vst_point(vh, pt, &cw, &ch, &cellw, &cellh);
		if (cellw <= 0 || cellh <= 0)
			continue;
		if (cellw > chr_w || cellh > chr_h)
			continue;           /* taille mise a l'echelle : plus grosse */
		if (font_n && cellw == font_cw[font_n - 1] &&
		              cellh == font_ch[font_n - 1])
			continue;           /* meme taille qu'au corps precedent */
		font_pt[font_n] = pt;
		font_cw[font_n] = cellw;
		font_ch[font_n] = cellh;
		font_label(font_n);
		font_n++;
	}

	if (font_n == 0) {          /* VDI inattendu : on prend les menus */
		font_pt[0] = 9;
		font_cw[0] = chr_w;
		font_ch[0] = chr_h;
		font_label(0);
		font_n = 1;
	}

	/*
	 * La police est retenue par son NOM ("6x6") et non par son rang : la
	 * liste depend de la resolution. Introuvable ou "" : celle des menus.
	 */
	i = font_n - 1;
	if (cfg.font_nm[0]) {
		short k;

		for (k = 0; k < font_n - 1; k++)
			if (s_same(font_nm[k], cfg.font_nm))
				i = k;
	}
	font_cur = i;
	cell_w = font_cw[i];
	cell_h = font_ch[i];
	vst_point(vh, font_pt[i], &cw, &ch, &cellw, &cellh);
}

short dsp_font_count(void)   { return font_n; }
short dsp_font_menu(void)    { return font_n - 1; }
short dsp_font_current(void) { return font_cur; }

const char *dsp_font_name(short i)
{
	return (i >= 0 && i < font_n) ? font_nm[i] : "";
}

void dsp_font_set(short i)
{
	if (i < 0 || i >= font_n - 1)
		cfg.font_nm[0] = '\0';      /* la police des menus */
	else
		s_copy(cfg.font_nm, font_nm[i]);
	cfg_save();
	dsp_geometry();                 /* relit la police et redessine */
}

/* ------------------------------------------------------------------ */
short dsp_open(void)
{
	short work_in[11];
	short work_out[57];
	short wbox, i;
	short dummy_w, dummy_h;

	vh = graf_handle(&chr_w, &chr_h, &wbox, &bar_h);
	if (vh <= 0)
		return 0;

	for (i = 0; i < 10; i++)
		work_in[i] = 1;
	work_in[10] = 2;                /* coordonnees raster */
	v_opnvwk(work_in, &vh, work_out);
	if (vh <= 0)
		return 0;

	scr_w = work_out[0] + 1;
	scr_h = work_out[1] + 1;
	ncolors = work_out[13];
	fcolor = -1;

	{
		short ext[57];

		vq_extnd(vh, 1, ext);
		planes = ext[4];
		if (planes < 1)
			planes = 1;
	}

	pick_font();
	graph_colors();
	dsp_vdi_state();
	(void)dummy_w;
	(void)dummy_h;
	vsf_interior(vh, FIS_SOLID);
	vsf_perimeter(vh, 0);

	if (cell_w <= 0)
		cell_w = chr_w;
	if (cell_h <= 0)
		cell_h = chr_h;

	last[0] = '\0';
	last_left = scr_w;                  /* rien de dessine */
	last_lg = -1;
	last_rg = -1;
	forget_screen();
	return 1;
}

static void fill(short x0, short x1, short area_h, short color);

void dsp_close(void)
{
	short pxy[4];
	short area_h;

	if (vh <= 0)
		return;

	/*
	 * On rend la barre telle qu'on l'a trouvee. Indispensable pour
	 * l'application, qui se termine : sans cela l'heure reste affichee
	 * jusqu'au prochain redessin du bureau. Le trait de separation de
	 * l'AES, derniere ligne de la barre, n'est pas efface.
	 */
	area_h = (short)(bar_h - 1);
	if (area_h < 1)
		area_h = bar_h;
	if (last_left < scr_w) {
		wind_update(BEG_UPDATE);
		graf_mouse(M_OFF, 0L);
		pxy[0] = 0;
		pxy[1] = 0;
		pxy[2] = (short)(scr_w - 1);
		pxy[3] = (short)(area_h - 1);
		vs_clip(vh, 1, pxy);
		fill(last_left, (short)(scr_w - 1), area_h, 0);
		vs_clip(vh, 0, pxy);
		graf_mouse(M_ON, 0L);
		wind_update(END_UPDATE);
	}

	v_clsvwk(vh);
	vh = 0;
}

/* ------------------------------------------------------------------ */
/* Ce que le panneau de survol a besoin de savoir du rendu de la barre. */
/* ------------------------------------------------------------------ */
short dsp_vdi(void)    { return vh; }

/*
 * Etat VDI du rendu de la barre. Le panneau de survol dessine avec la
 * police des menus et doit remettre ceci en place en partant, sinon la
 * barre se retrouverait avec la mauvaise police - dsp_update() ne
 * reprogramme que la couleur a chaque passage, pas la police.
 */
void dsp_vdi_state(void)
{
	short d;

	if (vh <= 0)
		return;
	vst_font(vh, 1);
	vst_point(vh, font_pt[font_cur], &d, &d, &d, &d);
	vst_alignment(vh, 0, 5, &d, &d);
	vst_effects(vh, 0);
	vst_rotation(vh, 0);
	vswr_mode(vh, MD_REPLACE);
	fcolor = -1;                /* the panel changed it behind our back */
	dsp_clip(1);
}

/* police des menus, pour le panneau */
void dsp_use_menu_font(void)
{
	short d;

	if (vh <= 0 || font_n <= 0)
		return;
	vst_font(vh, 1);
	vst_point(vh, font_pt[font_n - 1], &d, &d, &d, &d);
	vst_alignment(vh, 0, 5, &d, &d);
	vst_effects(vh, 0);
	vst_rotation(vh, 0);
	vswr_mode(vh, MD_REPLACE);
}
short dsp_planes(void) { return planes; }
short dsp_bar_h(void)  { return bar_h; }
short dsp_scr_w(void)  { return scr_w; }
short dsp_scr_h(void)  { return scr_h; }

/* emprise actuelle dans la barre : la cible du survol */
void dsp_bar_area(short *x, short *w)
{
	*x = last_left;
	*w = (short)(scr_w - last_left);
	if (*w < 1) {
		*x = (short)(scr_w - 1);
		*w = 1;
	}
}

/*
 * A rappeler apres un changement de resolution : l'AES recharge les
 * accessoires dans ce cas, mais une application, elle, survit.
 */
void dsp_geometry(void)
{
	short keep = last_left;

	dsp_close();
	dsp_open();
	/* on garde l'ancien bord gauche, sinon l'ancien rendu reste affiche */
	last_left = keep;
	dsp_update(1);
}

/* ------------------------------------------------------------------ */
/* Graphique papillon des E/S disque                                   */
/*                                                                     */
/* 32 colonnes d'une seconde chacune (graph_period = 5 periodes de     */
/* 200 ms), soit 32 s d'historique. Les lectures montent au-dessus     */
/* d'une ligne mediane, les ecritures descendent en dessous : le sens, */
/* l'intensite et l'historique se lisent d'un coup d'oeil, sans        */
/* couleur.                                                            */
/*                                                                     */
/* La colonne de droite est VIVANTE : elle monte au fil de la seconde  */
/* en cours, a chaque periode de 200 ms. Le graphique ne defile qu'une */
/* fois par seconde, mais reagit immediatement a un acces disque.      */
/*                                                                     */
/* Echelle logarithmique : chaque pixel de hauteur double le debit.    */
/* Les E/S couvrent plusieurs ordres de grandeur (un secteur de        */
/* repertoire contre une copie de plusieurs Mo) : en lineaire on ne    */
/* verrait que les gros transferts.                                    */
/* ------------------------------------------------------------------ */
#define GRAPH_W   32
#define GRAPH_GAP 4          /* pixels entre deux elements de la barre */

/*
 * Trois graphiques, meme mecanique : un historique circulaire de 32
 * colonnes d'une seconde, dont la plus a droite est VIVANTE.
 *   GR_DISK  papillon : lectures au-dessus de la mediane, ecritures dessous
 *   GR_NET   papillon : reception au-dessus, emission dessous
 *   GR_CPU   barre simple montant du bas : charge du processeur. Seulement
 *            sous FreeMiNT AVEC un AES multitache (XaAES...) : avec l'AES
 *            du TOS, qui attend en boucle active, le processus idle de
 *            MiNT ne tourne jamais et la charge vaut 100 % en permanence -
 *            verifie sous Hatari, graphique plein.
 * Tous tournent en permanence, affiches ou non : ouvrir Alternate montre
 * donc un historique deja rempli.
 */
enum { GR_DISK, GR_NET, GR_CPU, GR_N };

typedef struct {
	unsigned char up[GRAPH_W];       /* niveaux 0..8, tampon circulaire  */
	unsigned char dn[GRAPH_W];       /* toujours 0 pour une barre simple */
	short live;                      /* colonne en cours = la plus a droite */
	short active;                    /* colonnes non nulles              */
	short pending;                   /* decalages en attente de rendu    */
	short live_dirty;                /* la colonne vivante a change      */
	long  acc_up, acc_dn;            /* cumul de la seconde en cours     */
	short single;                    /* barre simple depuis le bas       */
	short col_up, col_dn;            /* couleurs VDI, 0 = monochrome     */
	short shown;                     /* on screen: all columns equal to  */
	                                 /* this (up | dn << 8), -1 = no     */
} GRAPH;

static GRAPH gr[GR_N] = {
	{ .live = GRAPH_W - 1, .shown = -1 },
	{ .live = GRAPH_W - 1, .shown = -1 },
	{ .live = GRAPH_W - 1, .shown = -1, .single = 1 }
};

static short g_ticks = 0;            /* periodes ecoulees dans la colonne */

static short rd_hold = 0;            /* remanence des temoins R/W */
static short wr_hold = 0;
static long  shown_kb = 0;
static short flash = 0;              /* tops restants d'affichage des MHz */

#define FLASH_TICKS 15               /* 3 s */

void dsp_flash_mhz(void)
{
	flash = FLASH_TICKS;
}

/*
 * Niveau sur 8 d'un volume cumule sur une colonne. On le ramene a un
 * debit par seconde, pour que l'echelle ne depende pas de graph_period :
 * 1 jusqu'a "base" par seconde, puis +1 a chaque doublement, plafond a 8
 * a 128 fois la base.
 */
static unsigned char rate_level(long amount, long base)
{
	long rate, v;
	unsigned char lvl;

	if (amount <= 0)
		return 0;
	rate = amount * 5L / (long)cfg.graph_period;
	v = (rate + base - 1L) / base;
	lvl = 1;
	while (v > 1 && lvl < 8) {
		v >>= 1;
		lvl++;
	}
	return lvl;
}

/*
 * Disque : 1 jusqu'a 8 Ko/s, plafond a 1 Mo/s. Une disquette (~25 Ko/s)
 * monte vers 3, un disque dur qui copie atteint le haut. Sur la partition
 * C: de la machine de test, un seul secteur fait 8 Ko et vaut donc 1.
 */
static unsigned char disk_level(long bytes) { return rate_level(bytes, 8192L); }

/*
 * Reseau : STinG compte en octets - 1 jusqu'a 256 o/s, plafond a 32 Ko/s,
 * soit un ping visible et un transfert a 80 Ko/s au sommet. MintNet ne
 * compte que des paquets - 1 par seconde, plafond a 128 par seconde.
 */
static unsigned char net_level(long amount)
{
	return rate_level(amount, net_kind() == NET_STING ? 256L : 1L);
}

/* charge CPU en pour cent, sur 8 ; toute charge non nulle se voit */
static unsigned char cpu_level(short pct)
{
	if (pct <= 0)
		return 0;
	return (unsigned char)((pct * 8 + 99) / 100);
}

/* remplace une case de l'historique en tenant le compte des non nulles */
static void gr_store(GRAPH *g, short idx, unsigned char u, unsigned char d)
{
	if (g->up[idx] || g->dn[idx])
		g->active--;
	g->up[idx] = u;
	g->dn[idx] = d;
	if (u || d)
		g->active++;
}

/* met a jour la colonne vivante */
static void gr_live(GRAPH *g, unsigned char u, unsigned char d)
{
	if (u != g->up[g->live] || d != g->dn[g->live]) {
		gr_store(g, g->live, u, d);
		g->live_dirty = 1;
	}
}

/* ouvre une nouvelle colonne a droite, en ecrasant la plus ancienne */
static void gr_next(GRAPH *g)
{
	short before = g->active;

	g->acc_up = 0;
	g->acc_dn = 0;
	g->live = (short)((g->live + 1) % GRAPH_W);
	gr_store(g, g->live, 0, 0);
	/* un graphique plat qui reste plat n'a pas besoin d'etre redessine */
	if ((before || g->active) && g->pending < GRAPH_W)
		g->pending++;
}

/*
 * Releve periodique, a appeler UNE fois par top de timer : il fait avancer
 * les historiques. Les redessins supplementaires (message de redraw,
 * sortie de dialogue) passent par dsp_update() seul et ne les decalent pas.
 */
void dsp_tick(void)
{
	DISKACT act;
	GRAPH *g;
	long d_in, d_out;
	short idle;

	if (flash > 0)
		flash--;
	if (!(cfg.show_graph || cfg.show_rw || cfg.show_kb))
		return;
	disk_poll(&act);
	shown_kb = act.total_kb;

	if (act.rd_sect > 0)
		rd_hold = cfg.rw_hold;
	else if (rd_hold > 0)
		rd_hold--;
	if (act.wr_sect > 0)
		wr_hold = cfg.rw_hold;
	else if (wr_hold > 0)
		wr_hold--;

	if (!cfg.show_graph)
		return;

	/* disque et reseau : la colonne vivante grandit au fil de la seconde */
	g = &gr[GR_DISK];
	g->acc_up += act.rd_bytes;
	g->acc_dn += act.wr_bytes;
	gr_live(g, disk_level(g->acc_up), disk_level(g->acc_dn));

	/*
	 * Reseau : STinG se lit dans sa memoire, pour rien, a chaque top.
	 * MintNet demande des ioctl sur un socket, par interface : 3 ms par
	 * top sur un STE, soit 1,5 % du processeur (make proftest). On ne
	 * l'interroge donc qu'une fois par colonne, juste avant de la clore.
	 */
	if (net_kind() == NET_STING ||
	    (net_kind() == NET_MINTNET && g_ticks + 1 >= cfg.graph_period)) {
		net_poll(&d_in, &d_out);
		g = &gr[GR_NET];
		g->acc_up += d_in;
		g->acc_dn += d_out;
		gr_live(g, net_level(g->acc_up), net_level(g->acc_dn));
	}

	if (++g_ticks < cfg.graph_period)
		return;
	g_ticks = 0;

	gr_next(&gr[GR_DISK]);
	gr_next(&gr[GR_NET]);

	/*
	 * CPU : un echantillon par seconde, qui porte sur la seconde ecoulee.
	 * On ouvre la colonne d'abord et on l'y ecrit ensuite, pour que la
	 * colonne de droite montre toujours la derniere mesure plutot qu'une
	 * seconde encore vide. Lire /kern/uptime une fois par seconde, et non
	 * a chaque top comme avant.
	 */
	if (sys.kern && sys.multi && cfg.show_idle) {
		idle = sys_idle_percent();
		gr_next(&gr[GR_CPU]);
		if (idle >= 0)
			gr_live(&gr[GR_CPU], cpu_level((short)(100 - idle)), 0);
	}
}

/* geometrie verticale, trait de separation exclu */
static short g_mid, g_half, g_base;

static void graph_geometry(short area_h)
{
	g_half = (short)((area_h - 1) / 2);
	if (g_half < 1)
		g_half = 1;
	g_mid = (short)(area_h - 1 - g_half);
	g_base = (short)(area_h - 1);          /* ligne de base d'une barre simple */
}

/*
 * Couleurs, des que l'ecran en a au moins 4 (ST moyenne, Falcon, TT...).
 * Indices VDI standard : 2 rouge, 3 vert, 4 bleu, 7 magenta. Les lectures
 * et la reception en vert/bleu, les ecritures et l'emission en rouge/
 * magenta ; la ligne de base reste noire. En 4 couleurs, il n'y a que le
 * rouge et le vert. Appele a chaque ouverture : la resolution a pu changer.
 */
static void graph_colors(void)
{
	short rich = (short)(ncolors >= 16 || ncolors <= 0);   /* 0 : true color */
	short color = (short)(ncolors >= 4 || ncolors <= 0);

	gr[GR_DISK].col_up = color ? 3 : 0;
	gr[GR_DISK].col_dn = color ? 2 : 0;
	gr[GR_NET].col_up  = color ? (rich ? 4 : 3) : 0;
	gr[GR_NET].col_dn  = color ? (rich ? 7 : 2) : 0;
	gr[GR_CPU].col_up  = color ? (rich ? 4 : 3) : 0;
	gr[GR_CPU].col_dn  = 0;
}

/* niveau sur 8 ramene a une hauteur disponible */
static short scale(unsigned char lvl, short room)
{
	return (short)((lvl * room + 7) / 8);
}

/*
 * Papillon : une barre noire de la hauteur montante au-dessus de la
 * mediane jusqu'a la hauteur descendante en dessous ; un seul rectangle.
 * Barre simple : du bas jusqu'a la hauteur. Dans les deux cas une colonne
 * vide se reduit a un pixel, ce qui trace la ligne de base.
 */
static void graph_bar(GRAPH *g, short x0, short x1,
                      unsigned char u, unsigned char d)
{
	short pxy[4];

	pxy[0] = x0;
	pxy[2] = x1;
	if (g->col_up) {
		/* en couleur : un rectangle par sens, base noire d'un pixel */
		short base = g->single ? g_base : g_mid;
		short hu = scale(u, g->single ? g_base : g_half);
		short hd = g->single ? 0 : scale(d, g_half);

		pxy[1] = base;
		pxy[3] = base;
		fill_color(1);
		v_bar(vh, pxy);
		if (hu > 0) {
			pxy[1] = (short)(base - hu);
			pxy[3] = (short)(base - 1);
			fill_color(g->col_up);
			v_bar(vh, pxy);
		}
		if (hd > 0) {
			pxy[1] = (short)(base + 1);
			pxy[3] = (short)(base + hd);
			fill_color(g->col_dn);
			v_bar(vh, pxy);
		}
		return;
	}
	if (g->single) {
		pxy[1] = (short)(g_base - scale(u, g_base));
		pxy[3] = g_base;
	} else {
		pxy[1] = (short)(g_mid - scale(u, g_half));
		pxy[3] = (short)(g_mid + scale(d, g_half));
	}
	v_bar(vh, pxy);
}

/* rendu complet, sur fond deja efface ; les colonnes identiques voisines
 * sont fusionnees en un seul rectangle */
static void graph_full(GRAPH *g, short gx)
{
	short i, idx, start = 0;
	unsigned char u0, d0;

	fill_color(1);
	idx = (short)((g->live + 1) % GRAPH_W);   /* la plus ancienne */
	u0 = g->up[idx];
	d0 = g->dn[idx];
	for (i = 1; i <= GRAPH_W; i++) {
		idx = (short)((g->live + 1 + i) % GRAPH_W);
		if (i == GRAPH_W || g->up[idx] != u0 || g->dn[idx] != d0) {
			graph_bar(g, (short)(gx + start), (short)(gx + i - 1), u0, d0);
			if (i < GRAPH_W) {
				start = i;
				u0 = g->up[idx];
				d0 = g->dn[idx];
			}
		}
	}
}

/* redessine une seule colonne : fond blanc puis barre */
static void graph_column(GRAPH *g, short gx, short area_h, short col, short idx)
{
	short pxy[4];
	short x = (short)(gx + col);

	pxy[0] = x;
	pxy[1] = 0;
	pxy[2] = x;
	pxy[3] = (short)(area_h - 1);
	fill_color(0);
	v_bar(vh, pxy);
	fill_color(1);
	graph_bar(g, x, x, g->up[idx], g->dn[idx]);
}

/*
 * Rendu incremental : on decale le graphique de n pixels vers la gauche
 * par une copie ecran a ecran, puis on ne dessine que les n nouvelles
 * colonnes. Une copie et deux rectangles par periode au lieu de redessiner
 * les 32 colonnes. Le redessin complet periodique rattrape le cas ou une
 * application aurait ecrase la barre de menu entre-temps.
 */
static void graph_scroll(GRAPH *g, short gx, short area_h, short n)
{
	MFDB scr;
	short pxy[8];
	short k;

	scr.fd_addr = 0L;                   /* 0 = l'ecran */

	pxy[0] = (short)(gx + n);           /* source */
	pxy[1] = 0;
	pxy[2] = (short)(gx + GRAPH_W - 1);
	pxy[3] = (short)(area_h - 1);
	pxy[4] = gx;                        /* destination */
	pxy[5] = 0;
	pxy[6] = (short)(gx + GRAPH_W - 1 - n);
	pxy[7] = (short)(area_h - 1);
	vro_cpyfm(vh, S_ONLY, pxy, &scr, &scr);

	for (k = 0; k < n; k++)
		graph_column(g, gx, area_h, (short)(GRAPH_W - n + k),
		             (short)((g->live - (n - 1 - k) + GRAPH_W) % GRAPH_W));
}

/* redessin incremental d'un graphique affiche, puis remise a zero */
static void graph_refresh(GRAPH *g, short gx, short area_h)
{
	if (g->pending >= GRAPH_W) {
		fill(gx, (short)(gx + GRAPH_W - 1), area_h, 0);
		graph_full(g, gx);
	} else if (g->pending > 0) {
		/* le decalage redessine aussi la colonne vivante */
		graph_scroll(g, gx, area_h, g->pending);
	} else if (g->live_dirty) {
		graph_column(g, gx, area_h, GRAPH_W - 1, g->live);
	}
}

static short graph_changed(GRAPH *g)
{
	return (short)(g->pending > 0 || g->live_dirty);
}

static void graph_done(GRAPH *g)
{
	g->pending = 0;
	g->live_dirty = 0;
}

/* the value of every column when they are all equal, -1 otherwise */
static short graph_uniform(GRAPH *g)
{
	short i;
	unsigned char u = g->up[0], d = g->dn[0];

	for (i = 1; i < GRAPH_W; i++)
		if (g->up[i] != u || g->dn[i] != d)
			return -1;
	return (short)(u | (d << 8));
}

/*
 * A graph whose 32 columns are all equal, and already shown that way,
 * looks exactly the same after a scroll: nothing to draw. That is the
 * CPU graph of an idle machine - any load under 13 % is level 1 - which
 * otherwise cost a scroll and a column every second: a third of the
 * accessory's CPU time under MiNT + XaAES (make proftest, Falcon).
 */
static void graph_settle(GRAPH *g)
{
	if (graph_changed(g) && g->shown >= 0 && graph_uniform(g) == g->shown)
		graph_done(g);
}

/* ------------------------------------------------------------------ */
/* Composition du texte                                                */
/* ------------------------------------------------------------------ */
#define TIME_CELLS 5        /* "hh:mm" : largeur du champ heure/frequence */

/*
 * La combinaison qui revele les graphiques d'entrees/sorties est-elle
 * maintenue ? C'est Alternate par defaut (cfg.hot_shift), et Alternate
 * SEUL : Ctrl+Alt+Shift est le raccourci de la vitesse, qui doit montrer
 * la frequence et non les graphiques.
 */
short dsp_io_key(void)
{
	long shift = kb_state;

	return (cfg.hot_shift != 0 &&
	        (shift & 0x0f) == cfg.hot_shift);
}

static void compose(char *buf, short slot_taken)
{
	char *p = buf;
	long shift = kb_state;
	static unsigned short t;
	static short t_age = 0;

	/* le graphique occupe la place du texte : rien a composer */
	if (slot_taken) {
		*p = '\0';
		return;
	}

	if (cfg.show_rw) {
		*p++ = rd_hold ? 'R' : ' ';
		*p++ = wr_hold ? 'W' : ' ';
		*p++ = ' ';
	}
	if (cfg.show_kb) {
		p = u_dec32(p, shown_kb);
		*p++ = 'K';
		*p++ = ' ';
	}

	/* verrous et modificateurs clavier */
	if (cfg.show_caps && (shift & 0x10))
		p = s_copy(p, "CAPS ");


	/* apres le raccourci de vitesse : la frequence a la place de l'heure */
	if (flash > 0) {
		short mhz = sys_mhz();

		if (mhz < 10)
			*p++ = ' ';
		p = u_dec(p, mhz, 0);
		p = s_copy(p, "MHz");
	} else if (cfg.show_time) {
		/* l'heure ne se relit qu'une fois par seconde (5 tops) */
		if (t_age <= 0 || t == 0) {
			t = (unsigned short)Tgettime();
			t_age = 5;
		}
		t_age--;
		p = u_dec(p, (short)((t >> 11) & 0x1f), 2);
		*p++ = ':';
		p = u_dec(p, (short)((t >> 5) & 0x3f), 2);
	}

	*p = '\0';
}

/* ------------------------------------------------------------------ */
static void fill(short x0, short x1, short area_h, short color)
{
	short pxy[4];

	if (x1 < x0)
		return;
	pxy[0] = x0;
	pxy[1] = 0;
	pxy[2] = x1;
	pxy[3] = (short)(area_h - 1);
	fill_color(color);
	v_bar(vh, pxy);
}

/*
 * Pixel temoin. Une application qui redessine sa barre de menu efface
 * notre coin sans prevenir. Plutot que de tout redessiner a intervalle
 * fixe (un flash visible, et une bosse sur le graphique CPU), on retient
 * un pixel NOIR qu'on vient de dessiner et on le relit a chaque top : un
 * seul appel VDI. S'il a change, on repeint. Le temoin est la ligne de
 * base du graphique affiche, toujours noire, ou a defaut un point du ':'
 * de l'heure, cherche une fois par changement de texte.
 */
static short sent_ok = 0, sent_x, sent_y, sent_pel, sent_idx;

static void sent_graph(short g, short gx, short reread)
{
	short y = gr[g].single ? g_base : g_mid;

	/* the same black baseline pixel as last time: no need to read it
	   again, except after a full redraw (the palette may have changed) */
	if (!reread && sent_ok && sent_x == gx && sent_y == y)
		return;
	sent_x = gx;
	sent_y = y;
	v_get_pixel(vh, sent_x, sent_y, &sent_pel, &sent_idx);
	sent_ok = 1;
}

/* the screen is no longer what we drew: nothing may be skipped */
static void forget_screen(void)
{
	short i;

	sent_ok = 0;
	for (i = 0; i < GR_N; i++)
		gr[i].shown = -1;
}

static void sent_text(const char *buf, short tx, short y)
{
	short k, cx, yy, ref_pel, ref_idx, pel, idx;

	sent_ok = 0;
	for (k = 0; buf[k] && buf[k] != ':'; k++)
		;
	if (!buf[k] || y <= 0)
		return;
	/* reference du fond : la ligne 0, effacee et jamais ecrite */
	cx = (short)(tx + k * cell_w + cell_w / 2);
	v_get_pixel(vh, cx, 0, &ref_pel, &ref_idx);
	for (yy = y; yy < y + cell_h; yy++) {
		v_get_pixel(vh, cx, yy, &pel, &idx);
		if (pel != ref_pel || idx != ref_idx) {
			sent_x = cx;
			sent_y = yy;
			sent_pel = pel;
			sent_idx = idx;
			sent_ok = 1;
			return;
		}
	}
}

/*
 * How often the sentinel is read. The bar gets redrawn behind our back
 * when the user switches applications or uses a menu (Thing under
 * XaAES): right after some mouse or keyboard activity. For 3 s after
 * any, it is read at every tick, so the corner comes back within 200 ms;
 * otherwise once per second. Each read is a VDI call, ~0.4 ms under MiNT
 * (make proftest: reading at every tick costs 10 % more, idle).
 */
#define SENT_ACTIVE 15               /* ticks of attention after input */

static short sent_period(void)
{
	static short attention = 0, last_x = -1, last_y = -1;
	static long last_kb = 0;

	if (mouse_x != last_x || mouse_y != last_y || mouse_b ||
	    kb_state != last_kb) {
		last_x = mouse_x;
		last_y = mouse_y;
		last_kb = kb_state;
		attention = SENT_ACTIVE;
	} else if (attention > 0) {
		attention--;
	}
#ifdef PROF_ATTENTION
	attention = SENT_ACTIVE;   /* make proftest: Hatari cannot move the mouse */
#endif
	return (short)(attention > 0 ? 1 : 5);
}

/*
 * The pointer is drawn into screen memory: over the sentinel, its colour
 * would be read and the bar repainted for nothing at every check. Its
 * sprite is 16 x 16 around any hot spot; the margin is doubled for the
 * motion since evnt_multi gave the position.
 */
static short sent_covered(void)
{
	short dx = (short)(mouse_x - sent_x), dy = (short)(mouse_y - sent_y);

	return (short)(dx > -32 && dx < 32 && dy > -32 && dy < 32);
}

/* now: about to scroll a graph - never shift an erased corner */
static short sent_lost(short now)
{
	static short every = 0;
	short pel, idx, period = sent_period();

	if (!sent_ok || sent_covered())
		return 0;
	if (!now && ++every < period)
		return 0;
	every = 0;
	v_get_pixel(vh, sent_x, sent_y, &pel, &idx);
	return (short)(pel != sent_pel || idx != sent_idx);
}

/* repeint un graphique colonne par colonne, sans effacer d'abord */
static void graph_repaint(GRAPH *g, short gx, short area_h)
{
	short i;

	for (i = 0; i < GRAPH_W; i++)
		graph_column(g, gx, area_h, i, (short)((g->live + 1 + i) % GRAPH_W));
}

/*
 * force : 0 = seulement ce qui a change, 1 = tout effacer et redessiner,
 * 2 = repeindre par-dessus sans effacer (pas de flash), pour reparer la
 * barre ou verifier periodiquement.
 */
void dsp_update(short force)
{
	char buf[48];
	short n, tw, right, tx, left, y, area_h, i;
	short layout, text_changed, any_graph;
	short io_key, lg, rg, lgx, rgx, hide;

	if (vh <= 0)
		return;

	/*
	 * Deux etats de la barre. Au repos : l'heure. Tant qu'Alternate est
	 * maintenu : les graphiques d'entrees/sorties prennent la place, le
	 * disque a l'emplacement de l'heure. La frequence, elle, n'est plus
	 * ici : elle est dans le panneau de survol et dans le dialogue.
	 */
#ifdef SBAR_FORCE_IOKEY
	io_key = cfg.show_graph;   /* make altest : etat "touche tenue" fige */
#else
	io_key = dsp_io_key() && cfg.show_graph;
#endif

	/*
	 * Deux emplacements. Au repos : [charge CPU] heure. Alternate tenu :
	 * [reseau] [disque]. Chaque graphique n'apparait que si sa source
	 * existe : CPU sous FreeMiNT, reseau avec STinG ou MintNet.
	 */
	lg = -1;
	rg = -1;
	if (io_key) {
		rg = GR_DISK;
		if (net_kind() != NET_NONE)
			lg = GR_NET;
	} else if (cfg.show_graph && cfg.show_idle && sys.kern && sys.multi) {
		lg = GR_CPU;
	}

	compose(buf, (short)(rg >= 0));

	/*
	 * Largeur = nombre de caracteres x pas de la cellule. On n'utilise pas
	 * vqt_extent() : il rend l'encombrement des glyphes, alors que v_gtext
	 * avance d'une cellule par caractere.
	 */
	for (n = 0; buf[n]; n++)
		;
	tw = (rg >= 0) ? (short)GRAPH_W : (short)(n * cell_w);

	/*
	 * La derniere ligne de la barre de menu porte le trait de separation
	 * dessine par l'AES : on ne l'efface jamais.
	 */
	area_h = area_height();
	graph_geometry(area_h);

	right = (short)(scr_w - cfg.offset * cell_w);
	tx = (short)(right - tw);
	rgx = (rg >= 0) ? tx : -1;
	lgx = -1;
	if (lg >= 0) {
		lgx = (short)(tx - GRAPH_GAP - GRAPH_W);
		if (lgx < 0) {                  /* pas la place : on s'en passe */
			lgx = -1;
			lg = -1;
		}
	}
	left = (lgx >= 0) ? lgx : tx;
	if (left < 0)
		left = 0;

	layout = (left != last_left) || (lg != last_lg) || (rg != last_rg);
	for (i = 0; i < GR_N; i++) {
		if (i != lg && i != rg)
			gr[i].shown = -1;       /* not on screen */
		else if (!force && !layout)
			graph_settle(&gr[i]);
	}
	any_graph = (lg >= 0 && graph_changed(&gr[lg])) ||
	            (rg >= 0 && graph_changed(&gr[rg]));
	if (!force && !layout && sent_lost(any_graph)) {
		force = 2;                  /* notre coin a ete efface */
#ifdef SBAR_PROF
		prof_event("repair");
#endif
	}
	text_changed = force || layout || !s_same(buf, last);
	if (!text_changed && !any_graph)
		return;

	y = (short)((area_h - cell_h) / 2);
	if (y < 0)
		y = 0;

	/*
	 * Le pointeur n'est masque que s'il est pres de la barre (hauteur du
	 * pointeur comprise) : deux appels AES de moins par dessin le reste
	 * du temps.
	 */
	hide = (short)(mouse_y < bar_h + 32);

	/* never draw outside the menu bar: the clipping stays on it for
	   good, see dsp_clip() */
	wind_update(BEG_UPDATE);
	if (hide)
		graf_mouse(M_OFF, 0L);

	if (force == 2 && !layout) {
		/* repeindre sans effacer : le texte en mode remplacement peint
		 * aussi son fond, les colonnes du graphique aussi */
		if (lg >= 0)
			graph_repaint(&gr[lg], lgx, area_h);
		if (rg >= 0)
			graph_repaint(&gr[rg], rgx, area_h);
		text_changed = (rg < 0);
	} else if (force || layout) {
		/* tout redessiner, en effacant aussi l'ancienne emprise */
		fill((left < last_left) ? left : last_left, (short)(right - 1),
		     area_h, 0);
		if (lg >= 0)
			graph_full(&gr[lg], lgx);
		if (rg >= 0)
			graph_full(&gr[rg], rgx);
		text_changed = (rg < 0);
	} else {
		if (lg >= 0)
			graph_refresh(&gr[lg], lgx, area_h);
		if (rg >= 0)
			graph_refresh(&gr[rg], rgx, area_h);
		if (text_changed && rg < 0)
			fill(tx, (short)(right - 1), area_h, 0);
	}

	if (text_changed && rg < 0) {
		vst_color(vh, 1);
		dsp_gtext(vh, tx, y, buf);
	}

	/* nouveau temoin, tant que l'ecran est a nous */
	if (lg >= 0)
		sent_graph(lg, lgx, (short)(force || layout));
	else if (rg >= 0)
		sent_graph(rg, rgx, (short)(force || layout));
	else if (text_changed)
		sent_text(buf, tx, y);

	if (hide)
		graf_mouse(M_ON, 0L);
	wind_update(END_UPDATE);

	if (lg >= 0) {
		graph_done(&gr[lg]);
		gr[lg].shown = graph_uniform(&gr[lg]);
	}
	if (rg >= 0) {
		graph_done(&gr[rg]);
		gr[rg].shown = graph_uniform(&gr[rg]);
	}
	s_copy(last, buf);
	last_left = left;
	last_lg = lg;
	last_rg = rg;
}
