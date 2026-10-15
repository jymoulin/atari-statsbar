/*
 * StatsBar - compteurs de trafic reseau.
 *
 * Deux piles possibles, et une seule unite chacune :
 *
 *   STinG (et STinG-LW de jym, sur lwIP) : OCTETS. Chaque port tient
 *   stat_rcv_data / stat_sd_data. On y accede comme la CLI de STinG-LW :
 *   cookie 'STiK' -> DRV_LIST -> get_dftab("MODULE_LAYER") -> table STX
 *   -> query_chains() -> chaine des ports.
 *
 *   MintNet (FreeMiNT) : PAQUETS seulement. Rien dans /kern : il faut une
 *   socket, SIOCGIFCONF pour enumerer les interfaces, SIOCGIFSTATS pour
 *   leurs compteurs. On ignore la boucle locale.
 *
 * STinG est prefere s'il est present : il donne des octets.
 */
#include <mint/osbind.h>
#include <mint/mintbind.h>
#include "statsbar.h"

/* ------------------------------------------------------------------ */
/* STinG                                                              */
/* ------------------------------------------------------------------ */
#define STIK_COOKIE 0x5354694bL          /* 'STiK' */

/* disposition de la trame Pure-C : alignement sur 2 octets */
typedef struct {
	char   magic[10];                    /* "STiKmagic" */
	void  *get_dftab;                    /* DRV_HDR *get_dftab(const char *) */
	void  *etm_exec;
	void  *cfg;
	void  *basepage;
} DRV_LIST;

_Static_assert(__builtin_offsetof(DRV_LIST, get_dftab) == 10,
               "DRV_LIST : get_dftab doit etre a l'offset 10 (trame Pure-C)");

typedef struct {
	const char *module, *author, *version;
	void *set_dgram_ttl, *check_dgram_ttl, *load_routing_table;
	void *set_sysvars;
	void *query_chains;                  /* void (PORT **, DRIVER **, LAYER **) */
} STX_HEAD;

typedef struct port_desc {
	const char *name;
	short  type;
	short  active;
	long   flags;
	long   ip_addr, sub_mask;
	short  mtu, max_mtu;
	long   stat_sd_data;                 /* octets emis   */
	void  *send;
	long   stat_rcv_data;                /* octets recus  */
	void  *receive;
	short  stat_dropped;
	void  *driver;
	struct port_desc *next;
} STPORT;

_Static_assert(__builtin_offsetof(STX_HEAD, query_chains) == 28,
               "STX : query_chains doit etre a l'offset 28");
_Static_assert(__builtin_offsetof(STPORT, stat_sd_data) == 24 &&
               __builtin_offsetof(STPORT, stat_rcv_data) == 32 &&
               __builtin_offsetof(STPORT, next) == 46,
               "PORT : disposition differente de port.h de STinG");

#define L_INTERNAL 0                     /* boucle locale      */
#define L_MASQUE   7                     /* NAT : double compte */

extern long sbar_cdecl3(void *fn, long a, long b, long c);

static STX_HEAD *stx = 0L;

static short sting_init(void)
{
	DRV_LIST *drv;
	long val;

	if (!sys_cookie(STIK_COOKIE, &val) || val == 0L)
		return 0;
	drv = (DRV_LIST *)val;
	if (drv->magic[0] != 'S' || drv->magic[4] != 'm' || !drv->get_dftab)
		return 0;
	stx = (STX_HEAD *)sbar_cdecl3(drv->get_dftab,
	                              (long)"MODULE_LAYER", 0L, 0L);
	if (!stx || !stx->query_chains) {
		stx = 0L;
		return 0;
	}
	return 1;
}

static STPORT *sting_ports(void)
{
	STPORT *p = 0L;

	sbar_cdecl3(stx->query_chains, (long)&p, 0L, 0L);
	return p;
}

static void sting_totals(long *in, long *out)
{
	STPORT *p;

	*in = *out = 0L;
	for (p = sting_ports(); p; p = p->next) {
		if (!p->active || p->type == L_INTERNAL || p->type == L_MASQUE)
			continue;
		*in  += p->stat_rcv_data;
		*out += p->stat_sd_data;
	}
}

/* ------------------------------------------------------------------ */
/* MintNet                                                            */
/* ------------------------------------------------------------------ */
#define IFNAMSIZ     16
#define SIOCGIFCONF  (('S' << 8) | 12)
#define SIOCGIFSTATS (('S' << 8) | 29)
#define MAX_IF       8

/* 36 octets, identique du cote du noyau (inet4/if.h) et de la MiNTLib */
typedef struct {
	char name[IFNAMSIZ];
	union {
		struct {
			unsigned long in_packets, in_errors;
			unsigned long out_packets, out_errors, collisions;
		} stats;
		char pad[20];
	} u;
} IFREQ;

_Static_assert(sizeof(IFREQ) == 36, "ifreq : 36 octets comme le noyau");

typedef struct {
	short len;
	void *buf;
} IFCONF;

static short sock = -1;

/*
 * SIOCGIFCONF rend une entree par ADRESSE, pas par interface : sous
 * FreeMiNT, lo0 y figure deux fois. Sans ce tri, une carte portant deux
 * adresses verrait son trafic compte deux fois.
 */
static short seen_before(IFREQ *req, short i)
{
	short j, k;

	for (j = 0; j < i; j++) {
		for (k = 0; k < IFNAMSIZ; k++) {
			if (req[j].name[k] != req[i].name[k])
				break;
			if (req[i].name[k] == '\0')
				return 1;
		}
		if (k == IFNAMSIZ)
			return 1;
	}
	return 0;
}

static short mintnet_init(void)
{
	long fd;

	if (!sys.mint)
		return 0;
	fd = Fsocket(2L, 2L, 0L);              /* AF_INET, SOCK_DGRAM */
	if (fd < 0L)
		return 0;
	sock = (short)fd;
	return 1;
}

static void mintnet_totals(long *in, long *out)
{
	IFREQ req[MAX_IF];
	IFCONF ifc;
	short i, n;

	*in = *out = 0L;
	ifc.len = (short)sizeof(req);
	ifc.buf = req;
	if (Fcntl(sock, (long)&ifc, SIOCGIFCONF) < 0L)
		return;
	n = (short)(ifc.len / (short)sizeof(IFREQ));
	for (i = 0; i < n && i < MAX_IF; i++) {
		if (req[i].name[0] == 'l' && req[i].name[1] == 'o')
			continue;                      /* boucle locale */
		if (seen_before(req, i))
			continue;
		if (Fcntl(sock, (long)&req[i], SIOCGIFSTATS) < 0L)
			continue;
		*in  += (long)req[i].u.stats.in_packets;
		*out += (long)req[i].u.stats.out_packets;
	}
}

static short kind;

/* ------------------------------------------------------------------ */
/* Liste des ports ou interfaces, pour l'outil NETTST                  */
/* ------------------------------------------------------------------ */
void net_list(void (*cb)(const char *name, long in, long out, short counted))
{
	if (kind == NET_STING) {
		STPORT *p;

		for (p = sting_ports(); p; p = p->next)
			cb(p->name ? p->name : "?", p->stat_rcv_data, p->stat_sd_data,
			   (short)(p->active && p->type != L_INTERNAL &&
			           p->type != L_MASQUE));
	} else if (kind == NET_MINTNET) {
		IFREQ req[MAX_IF];
		IFCONF ifc;
		short i, n;

		ifc.len = (short)sizeof(req);
		ifc.buf = req;
		if (Fcntl(sock, (long)&ifc, SIOCGIFCONF) < 0L)
			return;
		n = (short)(ifc.len / (short)sizeof(IFREQ));
		for (i = 0; i < n && i < MAX_IF; i++) {
			req[i].name[IFNAMSIZ - 1] = '\0';
			if (seen_before(req, i))
				continue;
			if (Fcntl(sock, (long)&req[i], SIOCGIFSTATS) < 0L)
				continue;
			cb(req[i].name, (long)req[i].u.stats.in_packets,
			   (long)req[i].u.stats.out_packets,
			   (short)!(req[i].name[0] == 'l' && req[i].name[1] == 'o'));
		}
	}
}

/* ------------------------------------------------------------------ */
static long  last_in, last_out;

short net_init(void)
{
	if (sting_init())
		kind = NET_STING;
	else if (mintnet_init())
		kind = NET_MINTNET;
	else
		kind = NET_NONE;
	if (kind != NET_NONE)
		net_totals(&last_in, &last_out);
	return kind;
}

short net_kind(void) { return kind; }

void net_totals(long *in, long *out)
{
	if (kind == NET_STING)
		sting_totals(in, out);
	else if (kind == NET_MINTNET)
		mintnet_totals(in, out);
	else
		*in = *out = 0L;
}

/*
 * Trafic depuis l'appel precedent. Un compteur qui recule (remise a zero
 * par CTL_GENERIC_CLR_STAT, interface retiree) compte pour zero : on se
 * recale simplement dessus.
 */
void net_poll(long *d_in, long *d_out)
{
	long in, out;

	net_totals(&in, &out);
	*d_in  = (in  >= last_in)  ? in  - last_in  : 0L;
	*d_out = (out >= last_out) ? out - last_out : 0L;
	last_in = in;
	last_out = out;
}
