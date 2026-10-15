/*
 * StatsBar - dialogue de configuration.
 *
 * form_alert ne sait faire que trois boutons et une icone : d'ou l'ancien
 * empilement de sous-dialogues. On construit donc ici un VRAI dialogue GEM,
 * arbre d'objets monte en memoire. Pas de fichier .RSC : l'accessoire reste
 * un seul fichier, et un .RSC absent ou mal place est la premiere cause de
 * plantage des accessoires.
 *
 * Pas de liste deroulante non plus : menu_popup() n'existe qu'a partir de
 * l'AES 3.3 (TOS 4, MagiC, MiNT) et manque sur le TOS 2.06 vise. Les
 * boutons radio sont equivalents ici - trois tailles de police au plus -
 * et montrent tout d'un coup d'oeil.
 *
 * La petite croix en haut a gauche se comporte comme la case de fermeture
 * d'une fenetre : elle ferme sans rien appliquer, exactement comme Annuler.
 * Ses DEUX rectangles sont cliquables : objc_find() rend l'objet le plus
 * profond sous la souris, donc un carre interieur non selectionnable
 * avalait les clics portant au milieu de la case.
 */
#include <mint/osbind.h>
#include "statsbar.h"

#define MAXOBJ     28
#define MAXSPD      2
#define FONT_SLOTS  6

static OBJECT tree[MAXOBJ];
static short  nobj;

/* textes construits a l'ouverture */
static char t_cpu[40];
static char t_idx[40];
static char t_mem[40];
static char t_spd[MAXSPD][12];

/* indices d'objets retenus pour relire l'etat en sortie */
static short o_ok, o_quit, o_boot;
static short o_spd[MAXSPD], n_spd;
static short o_fnt[FONT_SLOTS], n_fnt;

/* metriques de caractere de l'AES */
static short cw, chh, bh;

/* ------------------------------------------------------------------ */
/* Construction de l'arbre                                             */
/* ------------------------------------------------------------------ */
static short add(short parent, short type, unsigned short flags,
                 unsigned short state, short x, short y, short w, short h)
{
	short me = nobj++;
	OBJECT *o = &tree[me];

	o->ob_next  = parent;       /* le dernier enfant renvoie au parent */
	o->ob_head  = -1;
	o->ob_tail  = -1;
	o->ob_type  = type;
	o->ob_flags = flags;
	o->ob_state = state;
	o->ob_spec.index = 0L;
	o->ob_x = x;
	o->ob_y = y;
	o->ob_width  = w;
	o->ob_height = h;

	if (parent >= 0) {
		if (tree[parent].ob_head < 0)
			tree[parent].ob_head = me;
		else
			tree[tree[parent].ob_tail].ob_next = me;
		tree[parent].ob_tail = me;
	}
	return me;
}

/* fond blanc, cadre noir d'epaisseur size (negatif = cadre a l'interieur) */
static void box_spec(short i, short framesize, short interior)
{
	tree[i].ob_spec.obspec.character   = 0;
	tree[i].ob_spec.obspec.framesize   = framesize;
	tree[i].ob_spec.obspec.framecol    = 1;     /* noir */
	tree[i].ob_spec.obspec.textcol     = 1;
	tree[i].ob_spec.obspec.textmode    = 0;
	tree[i].ob_spec.obspec.fillpattern = 7;     /* plein */
	tree[i].ob_spec.obspec.interiorcol = interior;
}

static void txt_spec(short i, const char *s)
{
	tree[i].ob_spec.free_string = (char *)s;
}

/* ------------------------------------------------------------------ */
/* Composition du dialogue                                             */
/* ------------------------------------------------------------------ */
static void build(short with_quit)
{
	short root, grp, i, y, w, h;
	short bw, gap, lbl;
	char *p;

	nobj = 0;
	n_spd = 0;
	n_fnt = 0;

	graf_handle(&cw, &chh, &i, &bh);
	if (cw <= 0) cw = 8;
	if (chh <= 0) chh = 8;
	bh = chh + 4;                      /* hauteur d'un bouton */

	gap = cw;
	lbl = 9 * cw;                      /* colonne des intitules */
	/*
	 * 38 caracteres : le dialogue doit tenir meme en 320x200, ou l'ecran
	 * n'en fait que 40. Toutes les lignes sont calibrees la-dessus.
	 */
	w   = 38 * cw;

	/* --- textes ------------------------------------------------- */
	p = s_cat(t_cpu, "CPU : ");
	p = u_dec(p, sys_mhz(), 0);
	p = s_cat(p, " MHz");
	*p = '\0';

	if (sys.speed_idx == 0)
		sys_measure_speed();
	p = s_cat(t_idx, "Indice : ");
	p = u_dec(p, sys.speed_idx, 0);
	p = s_cat(p, "   (100 = STE a 8 MHz)");
	*p = '\0';

	{
		long st, alt;

		sys_mem(&st, &alt);
		p = s_cat(t_mem, "Libre  : ");
		p = u_mem(p, st);
		p = s_cat(p, " ST");
		if (sys.alt_ram) {
			p = s_cat(p, "  +  ");
			p = u_mem(p, alt);
			p = s_cat(p, " rapide");
		}
		*p = '\0';
	}

	/* --- racine ------------------------------------------------- */
	root = add(-1, G_BOX, OF_NONE, OS_OUTLINED | OS_SHADOWED,
	           0, 0, w, 0);
	box_spec(root, 2, 0);

	y = chh / 2;

	/* --- case de fermeture, facon fenetre ----------------------- */
	grp = add(root, G_BOX, OF_SELECTABLE | OF_EXIT, OS_NORMAL,
	          cw / 2, y, chh, chh);
	box_spec(grp, -1, 0);
	i = add(grp, G_BOX, OF_SELECTABLE | OF_EXIT, OS_NORMAL,
	        chh / 4, chh / 4, chh - chh / 2, chh - chh / 2);
	box_spec(i, 0, 1);                 /* petit carre noir, cliquable aussi */

	i = add(root, G_STRING, OF_NONE, OS_NORMAL,
	        cw / 2 + chh + gap, y, 11 * cw, chh);
	txt_spec(i, "StatsBar");

	y += chh + chh / 2;

	/* --- informations ------------------------------------------- */
	i = add(root, G_STRING, OF_NONE, OS_NORMAL, gap, y, 20 * cw, chh);
	txt_spec(i, t_cpu);
	y += chh + chh / 4;
	i = add(root, G_STRING, OF_NONE, OS_NORMAL, gap, y, 36 * cw, chh);
	txt_spec(i, t_idx);
	y += chh + chh / 4;
	i = add(root, G_STRING, OF_NONE, OS_NORMAL, gap, y, 36 * cw, chh);
	txt_spec(i, t_mem);
	y += chh + chh / 2;

	/* --- vitesse ------------------------------------------------ */
	i = add(root, G_STRING, OF_NONE, OS_NORMAL, gap, y + (bh - chh) / 2,
	        8 * cw, chh);
	txt_spec(i, "Vitesse");

	o_boot = -1;
	if (sys.spd_ctrl != SPD_NONE) {
		short cur = sys_speed_get();

		bw  = 8 * cw;
		grp = add(root, G_IBOX, OF_NONE, OS_NORMAL,
		          lbl, y, 2 * bw + gap, bh);
		box_spec(grp, 0, 0);
		tree[grp].ob_spec.obspec.fillpattern = 0;   /* transparent */

		for (i = 0; i < 2; i++) {
			short mhz = i ? sys.mhz_fast : sys.mhz_slow;

			p = u_dec(t_spd[i], mhz, 0);
			p = s_cat(p, " MHz");
			*p = '\0';
			o_spd[i] = add(grp, G_BUTTON,
			               OF_SELECTABLE | OF_RBUTTON, OS_NORMAL,
			               i * (bw + gap), 0, bw, bh);
			txt_spec(o_spd[i], t_spd[i]);
		}
		n_spd = 2;
		if (cur < 0)
			cur = 0;
		tree[o_spd[cur]].ob_state |= OS_SELECTED;

		/* case a cocher : bouton bascule, ni radio ni sortie */
		y += bh + chh / 4 + 2;
		o_boot = add(root, G_BUTTON, OF_SELECTABLE,
		             cfg.boot_fast ? OS_SELECTED : OS_NORMAL,
		             lbl, y, 2 * bw + gap, bh);
		txt_spec(o_boot, "rapide au boot");
	} else {
		i = add(root, G_STRING, OF_NONE, OS_NORMAL,
		        lbl, y + (bh - chh) / 2, 18 * cw, chh);
		txt_spec(i, "pas de bascule ici");
	}
	y += bh + chh / 2 + 2;

	/* --- police ------------------------------------------------- */
	i = add(root, G_STRING, OF_NONE, OS_NORMAL, gap, y + (bh - chh) / 2,
	        8 * cw, chh);
	txt_spec(i, "Police");

	n_fnt = dsp_font_count();
	if (n_fnt > FONT_SLOTS)
		n_fnt = FONT_SLOTS;
	bw  = 6 * cw;
	grp = add(root, G_IBOX, OF_NONE, OS_NORMAL,
	          lbl, y, n_fnt * (bw + gap), bh);
	box_spec(grp, 0, 0);
	tree[grp].ob_spec.obspec.fillpattern = 0;

	for (i = 0; i < n_fnt; i++) {
		o_fnt[i] = add(grp, G_BUTTON, OF_SELECTABLE | OF_RBUTTON,
		               OS_NORMAL, i * (bw + gap), 0, bw, bh);
		txt_spec(o_fnt[i], dsp_font_name(i));
	}
	if (n_fnt) {
		short cur = dsp_font_current();

		if (cur < 0 || cur >= n_fnt)
			cur = n_fnt - 1;
		tree[o_fnt[cur]].ob_state |= OS_SELECTED;
	}
	y += bh + chh / 4 + 2;

	/* la derniere de la liste est celle des menus GEM : on le dit */
	i = add(root, G_STRING, OF_NONE, OS_NORMAL, gap, y, 34 * cw, chh);
	txt_spec(i, "la plus grande est celle des menus");

	y += chh + chh / 2;

	/* --- boutons de sortie, ranges de droite a gauche ------------ */
	bw = 9 * cw;
	i = (short)(w - gap - bw);
	o_ok = add(root, G_BUTTON, OF_SELECTABLE | OF_EXIT | OF_DEFAULT,
	           OS_NORMAL, i, y, bw, bh);
	txt_spec(o_ok, "OK");

	i -= bw + gap;
	grp = add(root, G_BUTTON, OF_SELECTABLE | OF_EXIT, OS_NORMAL,
	          i, y, bw, bh);
	txt_spec(grp, "Annuler");

	/*
	 * En application seulement : rien d'autre ne permet de se terminer,
	 * le bureau ne revient que quand le programme rend la main.
	 */
	o_quit = -1;
	if (with_quit) {
		i -= bw + gap;
		o_quit = add(root, G_BUTTON, OF_SELECTABLE | OF_EXIT, OS_NORMAL,
		             i, y, bw, bh);
		txt_spec(o_quit, "Quitter");
	}

	h = y + bh + chh / 2;
	tree[root].ob_height = h;
	tree[nobj - 1].ob_flags |= OF_LASTOB;

}

/* ------------------------------------------------------------------ */
short dlg_open(short with_quit)
{
	short x, y, w, h, ret, i;

	build(with_quit);

	form_center(tree, &x, &y, &w, &h);
	wind_update(BEG_UPDATE);
	wind_update(BEG_MCTRL);
	graf_mouse(ARROW, 0L);
	form_dial(FMD_START,  0, 0, 0, 0, x, y, w, h);
	objc_draw(tree, ROOT, MAX_DEPTH, x, y, w, h);

	ret = form_do(tree, 0) & 0x7fff;

	form_dial(FMD_FINISH, 0, 0, 0, 0, x, y, w, h);
	wind_update(END_MCTRL);
	wind_update(END_UPDATE);

	if (ret >= 0 && ret < nobj)
		tree[ret].ob_state &= ~OS_SELECTED;

	/* la croix se comporte exactement comme Annuler */
	if (ret == o_ok) {
		short fnt = dsp_font_current();

		for (i = 0; i < n_spd; i++)
			if (tree[o_spd[i]].ob_state & OS_SELECTED)
				if (sys_speed_get() != i)
					sys_speed_set(i);
		if (o_boot >= 0)
			cfg.boot_fast = (tree[o_boot].ob_state & OS_SELECTED) ? 1 : 0;
		for (i = 0; i < n_fnt; i++)
			if (tree[o_fnt[i]].ob_state & OS_SELECTED)
				fnt = i;
		dsp_font_set(fnt);       /* sauve et redessine */
	} else {
		dsp_update(1);
	}

	return (short)(ret == o_quit && o_quit >= 0);
}
