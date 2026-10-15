/*
 * Stress du handler hdv_rw hors de tout contexte GEM : on installe, on
 * enchaine un grand nombre de lectures, on verifie les compteurs, on retire.
 */
#include <mint/osbind.h>
#include <mint/ostruct.h>
#include "tlog.h"

extern long sbar_rw_install(void);
extern long sbar_rw_remove(void);
extern long sbar_rw_calls;
extern long sbar_rw_rd;
extern long sbar_rw_map;
extern long sbar_rw_rd_drv[32];

static char buf[8192];

int main(void)
{
	_BPB *bpb;
	long r;
	long recsiz;
	short i;
	short drive = targ_drive(-1);  /* "STRESS A" pour imposer A: */
	short auto_drv = (drive < 0);

	tlog_open("C:\\SBARSTR.TXT");
	tlog("--- stress hdv_rw ---");
	if (auto_drv)
		drive = 2;

	bpb = (_BPB *)Getbpb(drive);
	/*
	 * Sans argument on prend C:, et a defaut A: - c'est le cas sous
	 * Hatari, dont le disque GEMDOS emule n'a pas de BPB (et court-circuite
	 * de toute facon hdv_rw : seule la disquette teste vraiment le handler).
	 */
	if (!bpb && auto_drv) {
		drive = 0;
		tlog("\r\npas de BPB sur C:, on essaie A: ");
		bpb = (_BPB *)Getbpb(drive);
	}
	if (!bpb) {
		tlog("\r\nGetbpb -> NULL, lecteur absent, arret");
		tlog_pause(); tlog_close();
		return 1;
	}
	tlog("\r\nlecteur teste : "); tlog_dec((long)drive);
	tlog(" (0=A: 2=C:)");
	recsiz = (long)(unsigned short)bpb->recsiz;
	tlog("  recsiz="); tlog_dec(recsiz);
	if (recsiz > (long)sizeof(buf)) {
		tlog("\r\nsecteur plus grand que le tampon, arret");
		tlog_pause(); tlog_close();
		return 1;
	}

	Supexec(sbar_rw_install);

	for (i = 0; i < 300; i++) {
		r = Rwabs(2, buf, 1, (short)(i & 15), drive);
		if (r != 0) {
			tlog("\r\nerreur a l'iteration ");
			tlog_dec((long)i);
			tlog(" -> "); tlog_dec(r);
			break;
		}
	}

	tlog("\r\niterations="); tlog_dec((long)i);
	tlog("\r\ncalls="); tlog_dec(sbar_rw_calls);
	tlog(" rd="); tlog_dec(sbar_rw_rd);
	tlog(" rd_drv="); tlog_dec(sbar_rw_rd_drv[drive]);
	tlog(" map="); tlog_dec(sbar_rw_map);

	r = Supexec(sbar_rw_remove);
	tlog("\r\nremove -> "); tlog_dec(r);
	tlog("\r\n--- fin ---\r\n");
	tlog_pause();
	tlog_close();
	return 0;
}
