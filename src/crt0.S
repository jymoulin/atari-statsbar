/*
 * StatsBar - demarrage minimal.
 *
 * Remplace la startup de la MiNTLib, qui tirait a elle seule localtime,
 * stdio, malloc et la lecture de /etc/passwd : une cinquantaine de Ko de
 * code jamais execute, mais resident tant que l'accessoire est charge.
 *
 * Deux conventions d'entree, et une seule est documentee :
 *   - programme : la basepage est en 4(sp), la pile est en haut de la TPA ;
 *   - accessoire : l'AES passe la basepage dans A0 et ne prete AUCUNE pile
 *     utilisable. Lire 4(sp) ici provoque une bus error (a7 = 0). C'est
 *     exactement le test que fait la startup de la MiNTLib : A0 non nul et
 *     p_parent nul designent un accessoire.
 *
 * Dans les deux cas on bascule aussitot sur notre propre pile, prise dans
 * le BSS, donc a l'interieur du bloc que le Mshrink conserve.
 *
 * 3 Ko, mesure par gcc -fstack-usage : la chaine d'appels la plus
 * profonde (main -> clic du panneau -> dsp_open -> dsp_update -> binding
 * libgem) atteint environ 1 Ko. Les traps AES, VDI, BIOS et GEMDOS, comme
 * les interruptions, basculent sur la pile superviseur : seuls nos cadres
 * C occupent celle-ci. Elle faisait 8 Ko tant que le v_gtext de libgem,
 * qui reserve 2104 octets par appel, etait utilise (cf. display.c).
 * Le BSS est deja mis a zero par le chargeur GEMDOS.
 */
	.text
	.globl	_start
_start:
	move.l	%a0,%d0
	beq.s	.Lprg			| A0 nul : programme
	tst.l	36(%a0)			| p_parent non nul : programme aussi
	bne.s	.Lprg

	| --- accessoire : l'AES a alloue le bloc, on ne le retaille pas
	move.l	%a0,_base
	lea	sbar_stack_top,%sp
	bra.s	.Lrun

	| --- programme : basepage sur la pile, puis on rend le reste de la TPA
.Lprg:
	move.l	4(%sp),%a0
	move.l	%a0,_base
	lea	sbar_stack_top,%sp	| notre pile est dans le BSS conserve
	move.l	12(%a0),%d0		| p_tlen
	add.l	20(%a0),%d0		| + p_dlen
	add.l	28(%a0),%d0		| + p_blen
	add.l	#0x100,%d0		| + basepage
	move.l	%d0,-(%sp)
	move.l	%a0,-(%sp)
	clr.w	-(%sp)
	move.w	#0x4a,-(%sp)		| Mshrink
	trap	#1
	lea	12(%sp),%sp

.Lrun:
	jsr	main
	move.w	%d0,-(%sp)
	move.w	#0x4c,-(%sp)		| Pterm
	trap	#1

| ---------------------------------------------------------------------
| Les trois seules fonctions de la libc encore utilisees, par nous et par
| libgem. Versions mot a mot, alignees quand les deux pointeurs le sont.
| ---------------------------------------------------------------------
	.globl	memcpy
memcpy:
	move.l	4(%sp),%a1		| dest
	move.l	8(%sp),%a0		| src
	move.l	12(%sp),%d0		| n
	beq.s	.Lmc_done
	move.l	%a0,%d1
	or.l	4(%sp),%d1
	btst	#0,%d1
	bne.s	.Lmc_byte
	move.l	%d0,%d1
	lsr.l	#2,%d1
	beq.s	.Lmc_tail
	subq.l	#1,%d1
.Lmc_long:
	move.l	(%a0)+,(%a1)+
	subq.l	#1,%d1
	bpl.s	.Lmc_long
.Lmc_tail:
	and.l	#3,%d0
	beq.s	.Lmc_done
.Lmc_byte:
	subq.l	#1,%d0
.Lmc_b1:
	move.b	(%a0)+,(%a1)+
	subq.l	#1,%d0
	bpl.s	.Lmc_b1
.Lmc_done:
	move.l	4(%sp),%d0
	rts

	.globl	memset
memset:
	move.l	4(%sp),%a0
	move.l	8(%sp),%d1		| c
	move.l	12(%sp),%d0		| n
	beq.s	.Lms_done
	subq.l	#1,%d0
.Lms_1:
	move.b	%d1,(%a0)+
	subq.l	#1,%d0
	bpl.s	.Lms_1
.Lms_done:
	move.l	4(%sp),%d0
	rts

	.globl	strlen
strlen:
	move.l	4(%sp),%a0
	move.l	%a0,%a1
.Lsl_1:
	tst.b	(%a0)+
	bne.s	.Lsl_1
	move.l	%a0,%d0
	sub.l	%a1,%d0
	subq.l	#1,%d0
	rts

	.bss
	.even
	.globl	_base
_base:
	.skip	4
	.globl	sbar_stack_bot
sbar_stack_bot:
	.skip	3072			| pile : l'AES n'en fournit pas
	.even
	.globl	sbar_stack_top
sbar_stack_top:
