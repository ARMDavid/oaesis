	.globl	_link_in
	.globl	_link_remove
	.globl	_set_stack
	.globl	_newmvec
	.globl  _newbvec
	.globl  _newtvec
	.globl _vdicall
	.globl _accstart
	.globl _VsetScreen
	.globl _VsetMode

	.globl	_h_aes_call
	.globl	_oldmvec
	.globl	_oldbvec
	.globl	_Moudev_handler
	.globl	_p_fsel_extern
        .globl _aescall

	.text
	
myxgemdos:
	cmpw		#0xc8,d0
	bne		retaddr

	moveml	d0-d7/a0-a6,sp@-
    movel _p_fsel_extern,a0
    tstw   a0@
    beq    intern

    movel  d1,a0
    movel  a0@,a0
    cmpw   #0x5a,a0@
    beq    ignore
    cmpw   #0x5b,a0@
    beq    ignore

intern:
	movel	d1,sp@-
	jsr		_h_aes_call
	addql		#4,sp
	moveml	sp@+,d0-d7/a0-a6

	rte

ignore:
	moveml	sp@+,d0-d7/a0-a6
	
retaddr:
	jmp		_link_in|

_link_in:
	movel	0x88,retaddr+2|
	movel	#myxgemdos,0x88
	rts|

_link_remove:
	movel	retaddr+2,0x88|
	rts|


_set_stack:
	movel	sp@(4),a0
	movel	sp@,a0@-
	movel	a0,sp
	rts

_newmvec:
	movel sp,newstack+800
	lea newstack+800,sp
	moveml	d0-d2/a0-a2,sp@-
	lea     pc@(mmov+4),a0
	movew	d1,a0@+ 	| pass position
	movew  d0,a0@
	pea   a0@(-6)
	jsr	_Moudev_handler
	addql		#4,sp		
	moveml	sp@+,d0-d2/a0-a2
	movel sp@,sp
	rts

_newbvec:
	movel sp,newstack+800
	lea newstack+800,sp
	moveml	d0-d2/a0-a2,sp@-
	lea     pc@(mbut+6),a0
	movew	d0,a0@ 	| pass buttons
	pea   a0@(-6)
	jsr	_Moudev_handler
	addql		#4,sp		|
	moveml	sp@+,d0-d2/a0-a2
	movel  sp@,sp
	rts

_newtvec:
	movel sp,newstack+800
	lea newstack+800,sp
	moveml	d0-d2/a0-a2,sp@-
	pea   mtim
	jsr	_Moudev_handler
	addql   #4,sp		|
	moveml	sp@+,d0-d2/a0-a2
	movel  sp@,sp
	rts

_vdicall:
	movel sp@(4),d1
	movel #0x73,d0
	trap   #2
	rts
	
_accstart:
	movel sp@(4),a0
	movel a0@(16),a1
	movel a1,a0@(8)
	addl  a0@(12),a1
	movel a1,a0@(16)
	movel a0@(8),a1
	jmp a1@

_VsetScreen:
	link  a6,#0
	movew a6@(18),sp@-
	movew a6@(16),sp@-
	movel a6@(12),sp@-
	movel a6@(8),sp@-
	movew #0x05,sp@-
	trap  #14
	lea   sp@(14),sp
	unlk  a6
	rts
		
_VsetMode:
	link  a6,#0
	movew a6@(8),sp@-
	movew #0x58,sp@-
	trap  #14
	addql #4,sp
	unlk  a6
	rts

_aescall:
	movel sp@(4),d1
	movel #200,d0
	trap   #2
	rts

mmov:
	.word 0,2,0,0
mbut:
	.word 0,1,1,0
mtim:
	.word 0,0,0,20
	
newstack:
	.even
	.comm ,4*201
