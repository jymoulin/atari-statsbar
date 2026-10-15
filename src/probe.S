| ---------------------------------------------------------------------------
| StatsBar - routines bas niveau, executees en mode superviseur via Supexec()
|
| Supexec() n'accepte aucun argument et recupere le resultat dans d0 : les
| valeurs d'entree passent donc par les variables globales declarees ici.
| ---------------------------------------------------------------------------

	.text

| ---------------------------------------------------------------------------
| long sbar_probe_speed(void)
|
| Mesure la vitesse EFFECTIVE du CPU : compte les passages d'une boucle
| calibree pendant 20 tops du compteur 200 Hz ($4BA), soit 100 ms.
|
| La boucle interne ne touche que des registres, son cout est donc domine par
| le chargement des instructions. Elle devient ~2x plus rapide des que le
| cache est actif (MegaSTE a 16 MHz, ou 68030 du Falcon), ce qui est bien la
| vitesse utile a afficher et pas la frequence nominale du quartz.
| Meme principe que 000CLOCK.TOS de C. Zietz.
| ---------------------------------------------------------------------------
	.globl	sbar_probe_speed
sbar_probe_speed:
	move.l	%d2,-(%sp)
	moveq	#0,%d0			| nombre de passages
	move.l	0x4ba,%d1
.Lsync:
	cmp.l	0x4ba,%d1		| se caler sur un front du timer
	beq.s	.Lsync
	move.l	0x4ba,%d1
	add.l	#20,%d1			| 20 tops de 5 ms = 100 ms
.Louter:
	moveq	#100,%d2
.Linner:
	subq.w	#1,%d2
	bne.s	.Linner
	addq.l	#1,%d0
	cmp.l	0x4ba,%d1
	bne.s	.Louter
	move.l	(%sp)+,%d2
	rts

| ---------------------------------------------------------------------------
| long sbar_probe_delta(void)
|
| Mesure DIFFERENTIELLE de la frequence, d'apres la methode de C. Zietz
| (000CLOCK.TOS). On chronometre deux boucles identiques a une instruction
| pres, de meme taille (2 octets) donc de meme motif de lecture memoire :
|
|   boucle A : nop           + subq.l + bne
|   boucle B : mulu.w %d1,%d0 + subq.l + bne
|
| La difference des deux durees elimine tout ce qui depend du bus et ne
| laisse que le cout CPU de mulu.w moins celui de nop. C'est ce qui rend la
| mesure juste meme quand le CPU est limite par la memoire : sur MegaSTE,
| passer a 16 MHz sans le cache externe ne change pas la vitesse de la
| boucle A, limitee par le bus, mais divise bien par deux celle de mulu.w,
| qui est une operation purement interne au CPU.
|
| d1 = 7 (trois bits a 1) fixe le cout de mulu.w sur 68000 : 38 + 2*3 = 44
| cycles, contre 4 pour nop, soit 40 cycles d'ecart par tour.
|
| Rend l'ecart en tops de 200 Hz. Le nombre d'iterations est dans
| sbar_probe_n.
| ---------------------------------------------------------------------------
	.globl	sbar_probe_delta
sbar_probe_delta:
	movem.l	%d2-%d5,-(%sp)
	moveq	#7,%d1
	moveq	#0,%d0

	move.l	sbar_probe_n,%d2
	move.l	0x4ba,%d3		| T0
.LloopA:
	nop
	subq.l	#1,%d2
	bne.s	.LloopA

	move.l	sbar_probe_n,%d2
	move.l	0x4ba,%d4		| T1
.LloopB:
	mulu.w	%d1,%d0
	subq.l	#1,%d2
	bne.s	.LloopB
	move.l	0x4ba,%d5		| T2

	sub.l	%d4,%d5			| T2 - T1   (duree de B)
	add.l	%d3,%d5
	sub.l	%d4,%d5			| - (T1 - T0) (duree de A)
	move.l	%d5,%d0
	movem.l	(%sp)+,%d2-%d5
	rts

| ---------------------------------------------------------------------------
| long sbar_probe_mste(void)
|
| Lit le registre cache/vitesse $FFFF8E21 en se protegeant d'une bus error,
| ce qui permet de detecter une carte 16 MHz (TwiSTEr, Hypercache) sur une
| machine que le cookie _MCH annonce comme un simple ST ou STE.
| Rend l'octet lu (0..255), ou -1 si le registre n'existe pas.
| ---------------------------------------------------------------------------
	.globl	sbar_probe_mste
sbar_probe_mste:
	move.l	%d2,-(%sp)
	move.l	%a2,-(%sp)
	move.l	%sp,%d2			| SP a restaurer si la lecture echoue
	move.l	8,%a2			| sauver le vecteur bus error
	move.l	#.Lbuserr,8
	nop
	moveq	#0,%d0
	move.b	0xffff8e21,%d0
	bra.s	.Ldone
.Lbuserr:
	move.l	%d2,%sp			| jeter la trame d'exception
	moveq	#-1,%d0
.Ldone:
	move.l	%a2,8			| remettre le vecteur en place
	move.l	(%sp)+,%a2
	move.l	(%sp)+,%d2
	rts

| ---------------------------------------------------------------------------
| long sbar_mste_put(void)
|
| Ecrit sbar_speed_val dans $FFFF8E21 (bit 0 = cache, bit 1 = 16 MHz).
| A n'appeler QUE si sbar_probe_mste() a confirme la presence du registre.
| ---------------------------------------------------------------------------
	.globl	sbar_mste_put
sbar_mste_put:
	move.b	sbar_speed_val,%d0
	move.b	%d0,0xffff8e21
	rts

| ---------------------------------------------------------------------------
| long sbar_f030_get(void) / long sbar_f030_put(void)
|
| Registre de controle du bus du Falcon, $FFFF8007 : bit 0 = horloge CPU
| (1 = 16 MHz), bit 2 = horloge du blitter, bit 5 = emulation du bus STE.
| On ne touche qu'au bit 0, les autres sont recopies tels quels.
| sbar_f030_put y place le bit 0 de sbar_speed_val. Falcon uniquement.
| ---------------------------------------------------------------------------
	.globl	sbar_f030_get
sbar_f030_get:
	moveq	#0,%d0
	move.b	0xffff8007,%d0
	rts

	.globl	sbar_f030_put
sbar_f030_put:
	move.b	0xffff8007,%d0
	and.b	#0xfe,%d0
	move.b	sbar_speed_val,%d1
	and.b	#1,%d1
	or.b	%d1,%d0
	move.b	%d0,0xffff8007
	rts

	.data
	.globl	sbar_speed_val
sbar_speed_val:
	.byte	0
	.even
	.globl	sbar_probe_n
sbar_probe_n:
	.long	50000


| ---------------------------------------------------------------------
| sbar_cookie_get : recherche dans le cookie jar, a appeler via Supexec.
|
| Remplace le Getcookie() de la MiNTLib, qui n'existe qu'au prix de la
| libc entiere. Entree : sbar_cookie_tag. Sortie : d0 = 0 si trouve, et
| la valeur dans sbar_cookie_val.
| ---------------------------------------------------------------------
	.globl	sbar_cookie_get
sbar_cookie_get:
	move.l	0x5a0,%a0		| _p_cookies
	move.l	%a0,%d0
	beq.s	.Lck_none
	move.l	sbar_cookie_tag,%d1
.Lck_loop:
	move.l	(%a0)+,%d0		| tag
	beq.s	.Lck_none		| fin de table
	cmp.l	%d1,%d0
	beq.s	.Lck_hit
	addq.l	#4,%a0
	bra.s	.Lck_loop
.Lck_hit:
	move.l	(%a0),sbar_cookie_val
	moveq	#0,%d0
	rts
.Lck_none:
	moveq	#-1,%d0
	rts

	.bss
	.even
	.globl	sbar_cookie_tag
sbar_cookie_tag:
	.skip	4
	.globl	sbar_cookie_val
sbar_cookie_val:
	.skip	4

| ---------------------------------------------------------------------
| sbar_vdi : appel VDI brut, pour les bindings maison (cf. display.c).
| Entree : 4(sp) = adresse du VDIPB. Le VDI peut detruire d0-d2/a0-a2 ;
| d2 et a2 sont a preserver pour gcc.
| ---------------------------------------------------------------------
	.text
	.globl	sbar_vdi
sbar_vdi:
	movem.l	%d2/%a2,-(%sp)
	move.l	12(%sp),%d1		| VDIPB
	move.w	#0x73,%d0		| VDI
	trap	#2
	movem.l	(%sp)+,%d2/%a2
	rts

| ---------------------------------------------------------------------
| long sbar_cdecl3(void *fn, long a, long b, long c)
|
| Appelle une fonction "cdecl" de STinG avec trois arguments longs sur la
| pile, et rend le resultat POINTEUR lu dans a0.
|   - STinG d'origine (Pure-C) rend les pointeurs dans a0 seulement, et
|     considere d2/a2 comme detruits, alors que gcc les croit preserves ;
|   - la version gcc de jym (STinG-LW) les rend dans a0 ET d0.
| Lire a0 et tout sauvegarder convient donc aux deux. Une fonction qui
| attend moins d'arguments ignore simplement les suivants (cdecl : c'est
| l'appelant qui depile).
| ---------------------------------------------------------------------
	.globl	sbar_cdecl3
sbar_cdecl3:
	movem.l	%d2-%d7/%a2-%a6,-(%sp)	| 44 octets : fn a 48(sp), a 52, b 56, c 60
	move.l	60(%sp),-(%sp)		| c
	move.l	60(%sp),-(%sp)		| b
	move.l	60(%sp),-(%sp)		| a
	move.l	60(%sp),%a1		| fn
	sub.l	%a0,%a0			| a0 a zero si la fonction n'y met rien
	jsr	(%a1)
	lea	12(%sp),%sp
	move.l	%a0,%d0
	movem.l	(%sp)+,%d2-%d7/%a2-%a6
	rts
