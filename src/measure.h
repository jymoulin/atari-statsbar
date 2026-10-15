/*
 * StatsBar - mesure de la frequence CPU, partagee par l'accessoire et
 * par SPEEDTST pour qu'ils ne puissent pas diverger.
 */
#ifndef MEASURE_H
#define MEASURE_H

#define MEAS_RUNS 3

/* detail d'une mesure, pour les traces de SPEEDTST */
typedef struct {
	long n;                 /* iterations par boucle                   */
	long tops[MEAS_RUNS];   /* ecart de chaque passe, en tops de 200 Hz */
	long best;              /* ecart median retenu                      */
	long ticks;             /* duree totale de la mesure                */
} MEASINFO;

/* frequence en DIXIEMES de MHz ; 0 si la mesure est impossible */
long measure_mhz10(long cpu, MEASINFO *info);

#endif
