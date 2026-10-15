| ---------------------------------------------------------------------------
| StatsBar - interception du vecteur BIOS hdv_rw ($476)
|
| Le vecteur recoit ses parametres sur la pile, exactement comme Rwabs() :
|
|    4(sp).w  rwflag   bit 0 = ecriture, bit 3 = acces physique
|    6(sp).l  buffer
|   10(sp).w  count    nombre de SECTEURS transferes
|   12(sp).w  recno    (-1 => le numero de secteur est dans lrecno)
|   14(sp).w  dev      lecteur BIOS, 0 = A:
|   16(sp).l  lrecno
|
| Le handler doit rester minuscule : il tourne dans le contexte de n'importe
| quel processus, souvent interruptions masquees. Il se contente donc
| d'incrementer des compteurs, l'affichage est fait plus tard par le GEM.
| ---------------------------------------------------------------------------

	.equ	hdv_rw, 0x476

	.text

| ---------------------------------------------------------------------------
| long sbar_rw_install(void) - via Supexec()
| ---------------------------------------------------------------------------
	.globl	sbar_rw_install
sbar_rw_install:
	move.w	%sr,%d1
	ori.w	#0x0700,%sr
	move.l	hdv_rw,%d0
	move.l	%d0,sbar_rw_old
	move.l	#sbar_rw_hook,hdv_rw
	move.w	%d1,%sr
	moveq	#0,%d0
	rts

| ---------------------------------------------------------------------------
| long sbar_rw_remove(void) - via Supexec()
|
| Rend 0 si le handler a pu etre retire, -1 s'il est pris en sandwich par un
| programme installe apres nous et qui n'a pas de structure XBRA exploitable.
| Dans ce cas l'appelant doit rester en memoire.
| ---------------------------------------------------------------------------
	.globl	sbar_rw_remove
sbar_rw_remove:
	move.l	%a0,-(%sp)
	move.l	%a1,-(%sp)
	move.w	%sr,%d1
	ori.w	#0x0700,%sr

	lea	hdv_rw,%a0		| a0 = ou se trouve le pointeur a corriger
	move.l	(%a0),%d0
.Lscan:
	cmp.l	#sbar_rw_hook,%d0	| est-ce notre handler ?
	beq.s	.Lunlink
	tst.l	%d0
	beq.s	.Lfail
	move.l	%d0,%a1
	cmp.l	#0x58425241,-12(%a1)	| 'XBRA' douze octets avant l'entree ?
	bne.s	.Lfail
	lea	-4(%a1),%a0		| le maillon suivant est en -4
	move.l	(%a0),%d0
	bra.s	.Lscan

.Lunlink:
	move.l	sbar_rw_old,(%a0)	| on se retire de la chaine
	moveq	#0,%d0
	bra.s	.Ldone
.Lfail:
	moveq	#-1,%d0
.Ldone:
	move.w	%d1,%sr
	move.l	(%sp)+,%a1
	move.l	(%sp)+,%a0
	rts

| ---------------------------------------------------------------------------
| Le handler lui-meme, precede de son en-tete XBRA.
| ---------------------------------------------------------------------------
	.ascii	"XBRA"
	.ascii	"SBAR"
	.globl	sbar_rw_old
sbar_rw_old:
	.long	0

sbar_rw_hook:
	movem.l	%d0-%d2/%a0,-(%sp)	| la pile a recule de 16 octets
	addq.l	#1,sbar_rw_calls

	moveq	#0,%d1
	move.w	10+16(%sp),%d1		| count : nombre de SECTEURS
	beq.s	.Lout			| rien a compter

	moveq	#0,%d0
	move.w	14+16(%sp),%d0		| dev : lecteur BIOS
	cmp.w	#31,%d0
	bhi.s	.Lout			| hors table (test non signe)

	move.l	sbar_rw_map,%d2
	bset	%d0,%d2			| bset sur registre : modulo 32
	move.l	%d2,sbar_rw_map

	lsl.w	#2,%d0			| index en longs
	move.w	4+16(%sp),%d2		| rwflag
	btst	#0,%d2
	bne.s	.Lwrite

	lea	sbar_rw_rd_drv,%a0
	add.l	%d1,(%a0,%d0.w)
	add.l	%d1,sbar_rw_rd
	bra.s	.Lout
.Lwrite:
	lea	sbar_rw_wr_drv,%a0
	add.l	%d1,(%a0,%d0.w)
	add.l	%d1,sbar_rw_wr

.Lout:
	movem.l	(%sp)+,%d0-%d2/%a0
	move.l	sbar_rw_old,-(%sp)	| chainage vers le handler precedent
	rts

| ---------------------------------------------------------------------------
| Compteurs, lus par le C. Ils ne sont jamais remis a zero par le handler.
| ---------------------------------------------------------------------------
	.data
	.globl	sbar_rw_calls
	.globl	sbar_rw_rd
	.globl	sbar_rw_wr
	.globl	sbar_rw_map
sbar_rw_calls:
	.long	0
sbar_rw_rd:
	.long	0
sbar_rw_wr:
	.long	0
sbar_rw_map:
	.long	0

| Secteurs par lecteur. Indispensable parce que la taille de secteur varie
| d'un lecteur a l'autre : 512 octets sur disquette, mais 1024, 2048, 4096
| voire 8192 sur une partition Atari un peu grosse. Sans ce detail par
| lecteur, impossible de convertir les secteurs en Ko.
	.globl	sbar_rw_rd_drv
	.globl	sbar_rw_wr_drv
sbar_rw_rd_drv:
	.space	32*4,0
sbar_rw_wr_drv:
	.space	32*4,0
