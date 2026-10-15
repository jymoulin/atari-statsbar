| ---------------------------------------------------------------------
| Outils du profilage sous Hatari (make proftest). Jamais dans ToTest/.
|
| prof_puts : ecrit prof_str sur la console de l'hote par NatFeats
| (NF_STDERR), a appeler par Supexec. Hatari doit tourner avec
| --natfeats on.
| prof_usage : Prusage() de MiNT, prof_r[0] = temps noyau, prof_r[1] =
| temps utilisateur, en ms.
| ---------------------------------------------------------------------
	.text
	.globl	prof_puts
prof_puts:
	pea	.Lname
	bsr.s	.Lnf_id			| nf_id("NF_STDERR")
	addq.l	#4,%sp
	tst.l	%d0
	beq.s	.Lout
	move.l	prof_str,-(%sp)
	move.l	%d0,-(%sp)
	bsr.s	.Lnf_call		| nf_call(id, str)
	addq.l	#8,%sp
.Lout:
	rts

| les arguments se lisent a 4(sp), derriere une adresse de retour :
| on passe donc par un vrai appel de sous-programme
.Lnf_id:
	.word	0x7300
	rts
.Lnf_call:
	.word	0x7301
	rts

	.globl	prof_usage
prof_usage:
	move.l	%d2,-(%sp)
	move.l	%a2,-(%sp)
	pea	prof_r
	move.w	#0x11e,-(%sp)		| Prusage
	trap	#1
	addq.l	#6,%sp
	move.l	(%sp)+,%a2
	move.l	(%sp)+,%d2
	rts

.Lname:	.asciz	"NF_STDERR"
	.even

	.bss
	.globl	prof_r
prof_r:	.space	32
	.globl	prof_str
prof_str: .space 4
