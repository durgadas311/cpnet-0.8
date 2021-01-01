	title	'RCVMAIL Receive Mail Transient Program'

;***************************************************************
;***************************************************************
;**                                                           **
;**     R C V M A I L   T r a n s i e n t   P r o g r a m     **
;**                                                           **
;***************************************************************
;***************************************************************

;
; Equates
;
BDOS	equ	0005h
buff	equ	0080h

conout	equ	2
print	equ	9
version	equ	12
sndmsg	equ	66
rcvmsg	equ	67
configtbl equ	69

LF	equ	0ah
CR	equ	0dh

start:
	lxi	h,0
	dad	sp
	lxi	sp,CCPStack+2
	push	h	; save CCP stack pointer
	mvi	c,version
	call	BDOS	; get version number
	mov	a,h
	ani	0000$0010b
	jz	versionerr
restart:
	mvi	c,configtbl
	call	BDOS	; get config table address
	inx	h
	mov	a,m
	sta	Mailmsg+2 ; Mailmsg.sid = configtbl.slaveID
	lxi	d,0	; D = dest ID, E = Mstr ID
	lxi	h,buff
	mov	a,m	; get # chars in the command tail
	ora	a
	jz	dorcvmsg
	mov	c,a	; A = # chars in command tail
	xra	a
scanblnks:
	inx	h
	mov	a,m
	cpi	' '
	jnz	pastblnks ; skip past leading blanks
	dcr	c
	jnz	scanblnks
	jmp	rcvmsgerr
pastblnks:
	cpi	'['
	jnz	rcvmsgerr
scanMstrID:
	inx	h
	dcr	c
	jz	rcvmsgerr
	mov	a,m
	cpi	']'
	jz	dorcvmsg
	sui	'0'
	cpi	10
	jc	updateMstrID
	adi	('0'-'A'+10) and 0ffh
	cpi	16
	jnc	rcvmsgerr
updateMstrID:
	push	psw
	mov	a,e
	add	a
	add	a
	add	a
	add	a
	mov	e,a	; accum * 16
	pop	psw
	add	e
	mov	e,a
	jmp	scanMstrID

dorcvmsg:
	mov	a,e
	lxi	d,Mailmsg+1
	stax	d	; msg.did = [xx]
	inx	d
	inx	d
	mvi	a,rcvmsg
	stax	d	; msg.fnc = rcvmsg
	inx	d
	xra	a
	stax	d
	mvi	c,sndmsg
	lxi	d,Mailmsg
	call	BDOS	; send message to network
	inr	a
	jz	networkerr
	mvi	c,rcvmsg
	lxi	d,Mailmsg
	call	BDOS	; receive message from network
	inr	a
	jz	networkerr
	lda	Mailmsg+4
	ora	a
	jnz	displaymsg
	lda	Mailmsg+5
	inr	a
	jz	rcvmsgfailed
displaymsg:
	lxi	h,FirstPass
	inr	m
	jz	noCRLF
	mvi	c,conout
	mvi	e,CR
	call	BDOS
	mvi	c,conout
	mvi	e,LF
	call	BDOS
noCRLF:
	lxi	h,Mailmsg
	mvi	m,'['
	inx	h
	xchg
	lda	Mailmsg+5
	lxi	h,HexASCIItbl
	push	psw
	push	h
	ani	0f0h
	rrc
	rrc
	rrc
	rrc
	mov	c,a
	mvi	b,0
	dad	b
	mov	a,m
	stax	d
	inx	d
	pop	h
	pop	psw
	ani	0fh
	mov	c,a
	dad	b
	mov	a,m
	stax	d
	xchg
	inx	h
	mvi	m,']'
	inx	h
	mov	e,m
	mvi	m,' '
	inx	h
	mvi	m,'"'
	inx	h
	mvi	d,0
	dad	d
	mvi	m,'"'
	inx	h
	mvi	m,'$'
	mvi	c,print
	lxi	d,Mailmsg
	call	BDOS
	jmp	restart

rcvmsgfailed:
	lda	FirstPass
	inr	a
	jnz	Exit
	lxi	d,rcvmsgfailedmsg
	jmp	printmsg

versionerr:
	lxi	d,versionerrmsg
	jmp	printmsg

networkerr:
	lxi	d,networkerrmsg
	jmp	printmsg

rcvmsgerr:
	lxi	d,rcvmsgerrmsg
printmsg:
	mvi	c,print
	call	BDOS
;	jmp	Exit

Exit:
	pop	h
	sphl		; restore CCP stack pointer
	ret

;
; Local Data Segment
;
versionerrmsg:
	db	'CP/Net is not loaded.'
	db	'$'

networkerrmsg:
	db	'Network access failed.'
	db	'$'

rcvmsgerrmsg:
	db	'Illegal RCVMAIL command.'
	db	'$'

rcvmsgfailedmsg:
	db	'Mailbox empty or not logged in to master.'
	db	'$'

lclstack:
	dw	0c7c7h,0c7c7h,0c7c7h,0c7c7h,0c7c7h
	dw	0c7c7h,0c7c7h,0c7c7h,0c7c7h,0c7c7h
CCPStack:
	dw	$-$

HexASCIItbl:
	db	'0123456789ABCDEF'

Mailmsg:
	db	0	; msg.fmt = 0
	db	$-$	; msg.did
	db	$-$	; msg.sid
	db	$-$	; msg.fnc
	db	$-$	; msg.siz
	ds	256	; msg.msg(0) ... msg.msg(255)

FirstPass:
	db	0ffh

	end	start
