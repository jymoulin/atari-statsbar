/*
 * MENUTST.PRG - erases StatsBar's corner the way Thing does under XaAES.
 *
 * Under XaAES the menu bar is redrawn by its owner whenever the menu
 * changes (application switch, menu_bar() call), across the whole width,
 * StatsBar's corner included, and nobody else is told. This program
 * installs a menu bar, then swaps between two menu trees every 3 s: each
 * swap is a full redraw of the bar. Every swap is written on the host
 * console by NatFeats with the 200 Hz clock, as is every repair by a
 * SBAR_PROF build of StatsBar: the difference is the time the corner
 * stayed blank. Hatari only (--natfeats on). Never in ToTest/.
 */
#include <mint/osbind.h>
#include "statsbar.h"

extern long prof_puts(void);
extern char *prof_str;

#define NOBJ 14

static OBJECT tree[2][NOBJ];
static char line[64];

static long rd_hz200(void)
{
	return *(volatile long *)0x4baL;
}

static short add(OBJECT *t, short *n, short parent, short type,
                 short x, short y, short w, short h, long spec)
{
	short me = (*n)++;
	OBJECT *o = &t[me];

	o->ob_next = parent;
	o->ob_head = o->ob_tail = -1;
	o->ob_type = type;
	o->ob_flags = 0;
	o->ob_state = 0;
	o->ob_spec.index = spec;
	o->ob_x = x;
	o->ob_y = y;
	o->ob_width = w;
	o->ob_height = h;
	if (parent >= 0) {
		if (t[parent].ob_head < 0)
			t[parent].ob_head = me;
		else
			t[t[parent].ob_tail].ob_next = me;
		t[parent].ob_tail = me;
	}
	return me;
}

/* the classic RCS menu: bar, one title, its drop-down with 6 slots */
static void build(OBJECT *t, const char *title, short scr_w, short scr_h,
                  short cw, short ch)
{
	static const char *items[8] = {
		"  About MENUTST  ", "-----------------",
		"  1", "  2", "  3", "  4", "  5", "  6"
	};
	short n = 0, root, bar, act, scr, box, i;

	root = add(t, &n, -1, G_IBOX, 0, 0, scr_w, scr_h, 0L);
	bar  = add(t, &n, root, G_BOX, 0, 0, scr_w, (short)(ch + 3), 0x1100L);
	act  = add(t, &n, bar, G_IBOX, 2, 0, (short)(8 * cw), (short)(ch + 3), 0L);
	add(t, &n, act, G_TITLE, 0, 0, (short)(8 * cw), (short)(ch + 3),
	    (long)title);
	scr  = add(t, &n, root, G_IBOX, 0, (short)(ch + 3), scr_w,
	           (short)(9 * ch), 0L);
	box  = add(t, &n, scr, G_BOX, 2, 0, (short)(17 * cw), (short)(8 * ch),
	           0xFF1100L);
	for (i = 0; i < 8; i++)
		add(t, &n, box, G_STRING, 0, (short)(i * ch), (short)(17 * cw), ch,
		    (long)items[i]);
	t[box + 2].ob_state = OS_DISABLED;
	t[n - 1].ob_flags = OF_LASTOB;
}

static void say(const char *what, long n)
{
	char *p = s_cat(line, "MENUTST ");

	p = s_cat(p, what);
	p = u_dec32(p, n);
	p = s_cat(p, " hz ");
	p = u_dec32(p, Supexec(rd_hz200));
	p = s_cat(p, "\n");
	*p = '\0';
	prof_str = line;
	Supexec(prof_puts);
}

int main(void)
{
	short msg[8], d, cw, ch, k = 0;
	short work_in[11], work_out[57], vh, i;
	long n = 0;

	if (appl_init() < 0)
		return 1;
	vh = graf_handle(&cw, &ch, &d, &d);
	for (i = 0; i < 10; i++)
		work_in[i] = 1;
	work_in[10] = 2;
	v_opnvwk(work_in, &vh, work_out);
	if (vh > 0)
		v_clsvwk(vh);
	build(tree[0], " Desk ", (short)(work_out[0] + 1), (short)(work_out[1] + 1),
	      cw, ch);
	build(tree[1], " Desk ", (short)(work_out[0] + 1), (short)(work_out[1] + 1),
	      cw, ch);
	graf_mouse(ARROW, 0L);
	menu_bar(tree[0], 1);

	for (;;) {
		short ev = evnt_multi(MU_MESAG | MU_TIMER, 0, 0, 0,
		                      0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		                      msg, 3000UL, &d, &d, &d, &d, &d, &d);

		if ((ev & MU_MESAG) && msg[0] == MN_SELECTED)
			menu_tnormal(tree[k], msg[3], 1);
		if (ev & MU_TIMER) {
			k ^= 1;
			say("swap ", ++n);
			menu_bar(tree[k], 1);
		}
	}
	return 0;
}
