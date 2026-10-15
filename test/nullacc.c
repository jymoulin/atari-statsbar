/*
 * Accessoire temoin pour CPUTST : meme boucle evnt_multi que StatsBar,
 * reveil toutes les 200 ms, et rien d'autre. La difference de travail
 * mesuree entre ce temoin et StatsBar est le cout propre de StatsBar.
 */
#include <gem.h>

int main(void)
{
	short msg[8], d;

	appl_init();
	menu_register(gl_apid, "  Temoin");
	for (;;)
		evnt_multi(MU_MESAG | MU_TIMER, 0, 0, 0,
		           0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		           msg, 200UL, &d, &d, &d, &d, &d, &d);
	return 0;
}
