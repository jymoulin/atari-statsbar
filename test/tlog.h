/* Sortie de test : console GEMDOS (visible sous "hatari --conout 2")
   ET fichier, pour pouvoir relire le resultat sur machine reelle. */
#ifndef TLOG_H
#define TLOG_H

void tlog_open(const char *path);
void tlog(const char *s);
void tlog_dec(long v);
void tlog_close(void);
void tlog_pause(void);

/* Lettre de lecteur passee en argument ("DISKTST A"), sinon deflt.
   Lue dans la queue de commande de la basepage : le demarrage minimal
   (src/crt0.S) ne construit pas de argv. */
short targ_drive(short deflt);

#endif
