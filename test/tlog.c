#include <mint/osbind.h>
#include <mint/basepage.h>
#include "tlog.h"

/*
 * Le fichier est referme puis rouvert en ajout apres chaque ecriture : en
 * cas de plantage, la trace reste lisible jusqu'au dernier point atteint.
 * C'est lent, mais c'est un outil de diagnostic.
 */
static long fh = -1;
static const char *fname = 0;

void tlog_open(const char *path)
{
	fname = path;
	fh = Fcreate(path, 0);
}

void tlog(const char *s)
{
	long n = 0;

	Cconws(s);
	if (fh >= 0) {
		while (s[n])
			n++;
		Fwrite((short)fh, n, s);
		Fclose((short)fh);
		fh = Fopen(fname, 1);
		if (fh >= 0)
			Fseek(0L, (short)fh, 2);
	}
}

void tlog_dec(long v)
{
	char tmp[12], out[14];
	short n = 0, i = 0;

	if (v < 0) {
		out[i++] = '-';
		v = -v;
	}
	do {
		tmp[n++] = (char)('0' + (short)(v % 10L));
		v /= 10L;
	} while (v);
	while (n)
		out[i++] = tmp[--n];
	out[i] = '\0';
	tlog(out);
}

void tlog_close(void)
{
	if (fh >= 0) {
		Fclose((short)fh);
		fh = -1;
	}
}

void tlog_pause(void)
{
	tlog("\r\nUne touche pour terminer.\r\n");
	Cconin();
}

short targ_drive(short deflt)
{
	const char *cmd = _base->p_cmdlin;
	short len = (short)(unsigned char)cmd[0];
	short i;
	char c;

	for (i = 1; i <= len && cmd[i] == ' '; i++)
		;
	if (i > len)
		return deflt;
	c = cmd[i];
	if (c >= 'a' && c <= 'z')
		c = (char)(c - 'a' + 'A');
	if (c < 'A' || c > 'P')
		return deflt;
	return (short)(c - 'A');
}
