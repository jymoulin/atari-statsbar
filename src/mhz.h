/*
 * StatsBar - conversion de la mesure differentielle en MHz.
 *
 * Partage par l'accessoire ET par SPEEDTST, pour qu'ils ne puissent plus
 * diverger : une copie de cette formule dans l'accessoire, restee en
 * dixiemes de MHz alors qu'elle etait lue comme des MHz, a produit un
 * affichage de 246 MHz sur un 68020 a 24 MHz.
 *
 * Derivation : l'ecart total vaut N * delta cycles, qui a F MHz durent
 * N * delta / (F * 1e6) secondes, soit tops = N * delta / (F * 5000)
 * periodes de 200 Hz. D'ou F = N * delta / (5000 * tops).
 *
 * delta est exprime en CENTIEMES de cycle (delta100), la valeur 68020
 * n'etant pas entiere.
 */
#ifndef MHZ_H
#define MHZ_H

/*
 * Ecart de cycles entre mulu.w %d1,%d0 (d1 = 7) et nop, en centiemes, par
 * famille de CPU (valeur du cookie _CPU).
 *
 * Valeurs etablies avec la methode actuelle (meilleure de trois passes
 * sur _hz_200), qui integre le cout normal des interruptions du systeme.
 * 68000 : 40,0 sur STE reel, 40,8 sous Hatari ; 40,5 tombe juste dans les
 *         deux cas (8,0 / 16,0 MHz).
 * 68020 : 26,4 sur TwiSTEr reel, identique a 8 et a 24 MHz.
 * 68030 : 26,4 aussi, valide sur Falcon reel a 16 MHz sous MagiC
 *         (16,1 et 16,2 MHz bruts sur les passes calmes). Sous Hatari le
 *         68030 mesurait 24 MHz : c'etait l'emulateur, pas la constante.
 */
static inline long mhz_delta100(long cpu)
{
	switch (cpu) {
	case 0:   return 4050L;
	case 20:  return 2640L;
	case 30:
	case 40:
	case 60:  return 2640L;
	default:  return 0L;
	}
}

/* frequence en DIXIEMES de MHz, arrondie */
static inline long mhz_tenths(long n, long delta100, long tops)
{
	if (tops <= 0 || delta100 <= 0)
		return 0L;
	/* F10 = n * delta100 / (50000 * tops) ; n <= 400000 et
	 * delta100 <= 4050 : le produit tient dans un long signe */
	return (n * delta100 + 25000L * tops) / (50000L * tops);
}

/* frequence en MHz entiers, arrondie */
static inline long mhz_whole(long n, long delta100, long tops)
{
	return (mhz_tenths(n, delta100, tops) + 5L) / 10L;
}

/*
 * Classement de la mesure vers une frequence d'horloge nominale.
 *
 * La mesure est faite interruptions autorisees : les gestionnaires
 * d'interruption du systeme (timer, VBL, pilotes residents, ordonnanceur de
 * MagiC ou Geneva) prennent une part fixe du temps CPU, qui la fait
 * SYSTEMATIQUEMENT sous-estimer. Sur la TwiSTEr a 24 MHz : 23,5 sous TOS,
 * 21,8 sous Geneva, 21,1 sous MagiC ; a 8 MHz, de 7,3 a 6,6. Les passes
 * etant stables, ce n'est pas du bruit : aucune statistique ne l'efface.
 *
 * Ce qui interesse l'utilisateur est l'horloge, qui ne prend que quelques
 * valeurs. Puisque le biais ne fait que sous-estimer, on retient la plus
 * petite frequence nominale superieure ou egale a la mesure, avec une
 * tolerance de 7 % au-dessus pour l'erreur d'arrondi du compteur (+/- 1
 * top sur ~20, soit 5 %). Confondre 24 et 16 demanderait un biais de plus
 * de 25 %.
 *
 * Les frequences intermediaires rares (20, 25, 33 MHz) sont omises : plus
 * la table est dense, moins le classement est sur.
 */
static const short mhz_nominal[] = {
	8, 16, 24, 32, 40, 50, 66, 80, 100, 0
};

static inline short mhz_snap(long f10)
{
	long lo = f10 * 93L / 100L;         /* mesure moins 7 % */
	short i;

	if (f10 <= 0)
		return 0;
	for (i = 0; mhz_nominal[i]; i++)
		if ((long)mhz_nominal[i] * 10L >= lo)
			return mhz_nominal[i];
	return (short)((f10 + 5L) / 10L);   /* au-dela de la table */
}

#endif /* MHZ_H */
