/*
 * Releve des metriques de police du VDI : pour chaque hauteur demandee,
 * on note ce que le VDI rend reellement. Permet de caler l'affichage sur
 * la police des menus GEM, et d'identifier la plus petite disponible.
 */
#include <mint/osbind.h>
#include <gem.h>
#include "tlog.h"

int main(void)
{
	short work_in[11], work_out[57];
	short vh, i, h;
	short cw, ch, cellw, cellh;
	short gw, gh, bw, bh;

	tlog_open("C:\\SBARFNT.TXT");
	appl_init();
	vh = graf_handle(&gw, &gh, &bw, &bh);

	tlog("\r\n--- metriques police ---\r\n");
	tlog("gl_wchar="); tlog_dec(gw);
	tlog(" gl_hchar="); tlog_dec(gh);
	tlog(" gl_wbox="); tlog_dec(bw);
	tlog(" gl_hbox="); tlog_dec(bh);

	for (i = 0; i < 10; i++)
		work_in[i] = 1;
	work_in[10] = 2;
	v_opnvwk(work_in, &vh, work_out);

	tlog("\r\necran ");
	tlog_dec(work_out[0] + 1); tlog("x"); tlog_dec(work_out[1] + 1);

	vst_font(vh, 1);
	for (h = 4; h <= 20; h++) {
		vst_height(vh, h, &cw, &ch, &cellw, &cellh);
		tlog("\r\nvst_height("); tlog_dec(h);
		tlog(") -> char "); tlog_dec(cw); tlog("x"); tlog_dec(ch);
		tlog("  cell "); tlog_dec(cellw); tlog("x"); tlog_dec(cellh);
	}

	tlog("\r\n-- vst_point --");
	for (h = 1; h <= 20; h++) {
		vst_point(vh, h, &cw, &ch, &cellw, &cellh);
		tlog("\r\nvst_point("); tlog_dec(h);
		tlog(") -> char "); tlog_dec(cw); tlog("x"); tlog_dec(ch);
		tlog("  cell "); tlog_dec(cellw); tlog("x"); tlog_dec(cellh);
	}

	tlog("\r\n--- fin ---\r\n");
	tlog_pause();
	tlog_close();
	v_clsvwk(vh);
	appl_exit();
	return 0;
}
