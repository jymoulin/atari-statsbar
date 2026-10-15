/*
 * StatsBar - informations systeme dans le coin haut-droit de la barre de menu
 *
 * Cible : Atari STE (TOS 2.06) et Falcon030, en accessoire ou en application.
 * L'affichage passe par le VDI (pas d'ecriture directe ecran) afin de rester
 * compatible cartes graphiques, true color, MiNT et MagiC.
 */
#ifndef SBAR_H
#define SBAR_H

#include <gem.h>

#define SBAR_IDENT   0x53424152L   /* 'SBAR' */
#define SBAR_VERSION 0x0100

/* --- machines reconnues (cookie _MCH) --- */
enum {
	MACH_ST = 0,
	MACH_STE,
	MACH_MEGASTE,
	MACH_TT,
	MACH_FALCON,
	MACH_OTHER
};

/* --- bascule de vitesse disponible --- */
enum {
	SPD_NONE = 0,
	SPD_MSTE,         /* $FFFF8E21 : MegaSTE, TwiSTEr, Hypercache */
	SPD_F030          /* $FFFF8007 bit 0 : Falcon 030 8/16 MHz    */
};

/* Etat systeme, fige au demarrage (sauf cpu_mhz, remesurable) */
typedef struct {
	short machine;    /* MACH_xxx */
	short spd_ctrl;   /* SPD_xxx : registre de vitesse exploitable */
	short cpu;        /* cookie _CPU : 0, 10, 20, 30, 40, 60 */
	short mint;       /* version FreeMiNT (0 si absent) */
	short multi;      /* 1 si AES multitache (FreeMiNT ou MagiC) */
	short alt_ram;    /* 1 si la machine a de la memoire alternative */
	short tos;        /* version du TOS, 0x0206 pour 2.06 */
	short kern;       /* 1 si /kern/uptime est lisible => idle CPU disponible */
	short idt;        /* cookie _IDT (-1 si absent) */
	short cpu_mhz;    /* frequence nominale courante */
	short mhz_slow;   /* frequence en position lente */
	short mhz_fast;   /* frequence en position rapide */
	long  mhz_reg;    /* registre de vitesse lors de la derniere mesure */
	short speed_idx;  /* indice de vitesse mesure (0 = pas encore mesure) */
} SYSTEM;

/* Ce que l'on affiche */
typedef struct {
	short show_time;    /* heure hh:mm */
	short show_caps;    /* etat des verrous clavier */
	short show_mhz;     /* frequence CPU */
	short hot_shift;    /* masque Kbshift revelant les graphiques d'E/S */
	short show_idle;    /* pourcentage de temps CPU inoccupe (FreeMiNT) */
	short show_graph;   /* graphique papillon des E/S disque */
	short graph_period; /* periodes de timer (200 ms) par colonne */
	short show_rw;      /* temoins R/W d'activite disque */
	short show_kb;      /* volume cumule transfere, en Ko */
	short rw_hold;      /* remanence des temoins, en periodes de timer */
	short offset;       /* decalage en caracteres depuis le bord droit */
	short boot_fast;    /* passer en vitesse rapide au demarrage */
	char  font_nm[8];   /* police choisie, "6x6"... ; "" = celle des menus */
} CONFIG;

/* Activite disque relevee depuis le dernier appel a disk_poll() */
typedef struct {
	long rd_sect;    /* secteurs lus depuis le dernier releve      */
	long wr_sect;    /* secteurs ecrits depuis le dernier releve   */
	long map;        /* bitmap des lecteurs BIOS touches           */
	long calls;      /* nombre total d'appels a hdv_rw             */
	long rd_bytes;   /* octets lus depuis le dernier releve        */
	long wr_bytes;   /* octets ecrits depuis le dernier releve     */
	long total_kb;   /* volume cumule depuis l'installation, en Ko */
} DISKACT;

extern SYSTEM sys;
extern CONFIG cfg;

/*
 * Etat des touches de modification (Kbshift), lu UNE fois par top par la
 * boucle principale : sous MiNT chaque appel systeme coute, et il y en
 * avait trois par top.
 */
extern long kb_state;

/*
 * Position de la souris rendue par le dernier evnt_multi : l'AES la
 * fournit a chaque retour, timer compris. Sert au survol (plus besoin de
 * graf_mkstate, 1 ms par top sous XaAES sur un STE) et a ne masquer le
 * pointeur pendant un dessin que s'il est pres de la barre.
 */
extern short mouse_x, mouse_y;
extern short mouse_b;              /* buttons held, same source */

/* --- sysinfo.c --- */
void  sys_detect(void);
short sys_measure_speed(void);     /* indice brut, ~477 sur un 68000 a 8 MHz */
short sys_nominal_mhz(void);       /* mesure la frequence courante */
short sys_mhz(void);               /* idem, mesuree une fois puis conservee */
short sys_speed_get(void);         /* -1 = pas de bascule, 0 = lent, 1 = rapide */
void  sys_speed_set(short fast);
short sys_idle_percent(void);      /* -1 si indisponible */
short sys_cookie(long tag, long *val);    /* 1 si le cookie existe */
/* Plus grand bloc libre, en octets. alt vaut 0 sans memoire alternative. */
void  sys_mem(long *st, long *alt);

/* --- config.c : reglages sauvegardes dans X:\STATSBAR.INF --- */
void  cfg_load(void);
void  cfg_save(void);              /* n'ecrit que si quelque chose a change */

/* --- net.c : trafic reseau --- */
enum { NET_NONE = 0, NET_STING, NET_MINTNET };
short net_init(void);                   /* detecte la pile, NET_xxx      */
short net_kind(void);
void  net_totals(long *in, long *out);  /* compteurs cumules             */
void  net_poll(long *d_in, long *d_out);/* trafic depuis l'appel d'avant */
/* STinG compte en OCTETS, MintNet en PAQUETS */
void  net_list(void (*cb)(const char *name, long in, long out, short counted));

/* --- disk.c --- */
short disk_install(void);
short disk_remove(void);
void  disk_poll(DISKACT *a);

/* --- display.c --- */
short dsp_open(void);
void  dsp_close(void);
void  dsp_geometry(void);
void  dsp_tick(void);
void  dsp_update(short force);
short dsp_io_key(void);         /* la touche des graphiques est-elle tenue ? */

/* Tailles de police reellement disponibles, relevees par dsp_open().    */
/* La derniere de la liste est celle des menus GEM.                      */
short       dsp_font_count(void);
short       dsp_font_menu(void);      /* indice de la police des menus   */
short       dsp_font_current(void);   /* indice reellement utilise       */
const char *dsp_font_name(short i);   /* "6x6", "8x8", "8x16"...         */
void        dsp_font_set(short i);    /* choisit, sauve et redessine     */
void        dsp_flash_mhz(void);      /* montre la frequence 3 s         */

/* Ce que le panneau de survol a besoin de savoir du rendu de la barre.  */
short dsp_vdi(void);
void  dsp_gtext(short h, short x, short y, const char *str);  /* sans pile */
void  dsp_vdi_state(void);      /* remet l'etat VDI du rendu de la barre */
void  dsp_clip(short on);       /* clipping on the bar, or none          */
void  dsp_use_menu_font(void);  /* police des menus, pour le panneau     */
short dsp_planes(void);
short dsp_bar_h(void);
short dsp_scr_w(void);
short dsp_scr_h(void);
void  dsp_bar_area(short *x, short *w);   /* emprise actuelle dans la barre */

/* --- panel.c : petit panneau affiche au survol de la barre --- */
void  pnl_open(void);          /* alloue le tampon de fond            */
void  pnl_close(void);
short pnl_visible(void);
void  pnl_leave(void);         /* referme et restitue le fond         */
void  pnl_poll(void);          /* suit la souris, un appel par top     */
short pnl_click(short mx, short my);   /* 1 si un reglage a change     */
#ifdef SBAR_TEST_PANEL
void  pnl_show_now(void);
#endif

/* --- dialog.c --- */
/* with_quit : ajoute un bouton Quitter (application seulement).        */
/* Rend 1 si l'utilisateur a demande a quitter.                         */
short dlg_open(short with_quit);

/* --- util --- */
char *u_dec(char *p, short val, short digits);
char *u_dec32(char *p, long val);
char *u_mem(char *p, long bytes);  /* "898K", "3.8M", "14.0M" */
char *s_cat(char *d, const char *s);

#endif /* SBAR_H */
