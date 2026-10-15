/*
 * Verification du handler hdv_rw sur machine reelle.
 *
 * LECTURE SEULE, et un seul secteur a la fois.
 *
 * La version precedente lisait DEUX secteurs dans un tampon de 1 Ko en
 * supposant des secteurs de 512 octets. Sur une partition Atari un peu
 * grosse, un secteur logique fait souvent 1024, 2048 voire 4096 octets :
 * la lecture debordait alors largement du tampon et ecrasait la memoire,
 * d'où les deux bombes. On interroge donc le BPB d'abord, et on refuse de
 * lire si le secteur ne tient pas dans le tampon.
 *
 * Trace sur la console et dans C:\SBARDSK.TXT, reecrite a chaque ligne pour
 * rester exploitable meme si le programme plante.
 */
#include <mint/osbind.h>
#include "tlog.h"

extern long sbar_rw_install(void);
extern long sbar_rw_remove(void);
extern long sbar_rw_calls;
extern long sbar_rw_rd;
extern long sbar_rw_wr;
extern long sbar_rw_map;

#define BUFSIZE 8192L

static char buf[BUFSIZE];

/* $476 est une variable systeme : lecture en mode superviseur */
static long getvec(void)
{
	return *(long *)0x476L;
}

static void report(const char *tag)
{
	tlog("\r\n");
	tlog(tag);
	tlog(" calls="); tlog_dec(sbar_rw_calls);
	tlog(" rd=");    tlog_dec(sbar_rw_rd);
	tlog(" wr=");    tlog_dec(sbar_rw_wr);
	tlog(" map=");   tlog_dec(sbar_rw_map);
}

int main(void)
{
	_BPB *bpb;
	long r;
	long recsiz = 0;
	short drive = targ_drive(-1);  /* "DISKTST A" pour imposer A: */
	short auto_drv = (drive < 0);

	if (auto_drv)
		drive = 2;

	tlog_open("C:\\SBARDSK.TXT");
	tlog("--- StatsBar disk hook ---");

	/* sans argument : C:, et a defaut A: (le disque GEMDOS de Hatari
	   n'a pas de BPB et court-circuite hdv_rw) */
	bpb = (_BPB *)Getbpb(drive);
	if (!bpb && auto_drv) {
		drive = 0;
		bpb = (_BPB *)Getbpb(drive);
	}

	tlog("\r\nlecteur teste : ");
	tlog_dec((long)drive);
	tlog(" (0=A: 2=C:)");

	tlog("\r\n1. hdv_rw avant = ");
	tlog_dec(Supexec(getvec));

	tlog("\r\n2. Getbpb(");
	tlog_dec((long)drive);
	tlog(") ");
	if (!bpb) {
		tlog("-> NULL, lecteur absent");
		tlog_pause();
		tlog_close();
		return 1;
	}
	recsiz = (long)(unsigned short)bpb->recsiz;
	tlog("-> recsiz="); tlog_dec(recsiz);
	tlog(" clsiz=");    tlog_dec((long)bpb->clsiz);
	tlog(" numcl=");    tlog_dec((long)(unsigned short)bpb->numcl);

	if (recsiz <= 0 || recsiz > BUFSIZE) {
		tlog("\r\n   secteur > tampon, pas de lecture");
		tlog_pause();
		tlog_close();
		return 1;
	}

	tlog("\r\n3. installation du handler");
	Supexec(sbar_rw_install);
	tlog("\r\n   hdv_rw apres = ");
	tlog_dec(Supexec(getvec));
	report("   init  :");

	tlog("\r\n4. Rwabs lecture 1 secteur");
	r = Rwabs(2, buf, 1, 0, drive);
	tlog(" -> "); tlog_dec(r);

	report("   apres :");

	tlog("\r\n5. retrait du handler");
	r = Supexec(sbar_rw_remove);
	tlog(" -> "); tlog_dec(r);
	tlog(" (0=ok)");
	tlog("\r\n   hdv_rw final = ");
	tlog_dec(Supexec(getvec));

	tlog("\r\n--- fin ---\r\n");
	tlog_pause();
	tlog_close();
	return 0;
}
