	title	'SNDMAIL Send Mail Transient Program'

;***************************************************************
;***************************************************************
;**                                                           **
;**     S N D M A I L   T r a n s i e n t   P r o g r a m     **
;**                                                           **
;***************************************************************
;***************************************************************

;
; Equates
;
BDOS	equ	0005h
buff	equ	0080h

print	equ	9
version	equ	12
sndmsg	equ	66
rcvmsg	equ	67
configtbl equ	69

start:
	lxi	h,0
	dad	sp
	lxi	sp,CCPStack+2
	push	h	; save CCP stack pointer
	mvi	c,version
	call	BDOS	; get version number
	mov	a,h
	ani	0000$0010b
	jz	versionerr ; CP/Net must be loaded
	mvi	c,configtbl
	call	BDOS	; get config table address
	inx	h
	mov	a,m
	sta	Mailmsg+2 ; Mailmsg.sid = configtbl.slaveID
	lxi	d,0	; D = dest ID, E = Mstr ID
	lxi	h,buff
	mov	a,m	; get # chars in the command tail
	ora	a
	jz	sndmsgerr
	mov	c,a	; A = # chars in command tail
	xra	a
scanblnks:
	inx	h
	mov	a,m
	cpi	' '
	jnz	pastblnks ; skip past leading blanks
	dcr	c
	jnz	scanblnks
	jmp	sndmsgerr
pastblnks:
	cpi	'['
	jnz	scanLftParen
scanMstrID:
	inx	h
	dcr	c
	jz	sndmsgerr
	mov	a,m
	cpi	']'
	jz	scanLftParen
	sui	'0'
	cpi	10
	jc	updateMstrID
	adi	('0'-'A'+10) and 0ffh
	cpi	16
	jnc	sndmsgerr
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
scanLftParen:
	mov	a,m
	cpi	'('
	jz	scanDestID
	inx	h
	dcr	c
	jnz	scanLftParen
	jmp	sndmsgerr
scanDestID:
	inx	h
	dcr	c
	jz	sndmsgerr
	mov	a,m
	cpi	')'
	jz	scanLftQuote
	sui	'0'
	cpi	10
	jc	updateDestID
	adi	('0'-'A'+10) and 0ffh
	cpi	16
	jnc	sndmsgerr
updateDestID:
	push	psw
	mov	a,d
	add	a
	add	a
	add	a
	add	a
	mov	d,a	; accum * 16
	pop	psw
	add	d
	mov	d,a
	jmp	scanDestID
scanLftQuote:
	inx	h
	dcr	c
	jz	sndmsgerr
	mov	a,m
	cpi	'"'
	jnz	scanLftQuote
	mov	a,e
	mov	b,d
	lxi	d,Mailmsg+1
	stax	d	; msg.did = [xx]
	inx	d
	inx	d
	mvi	a,sndmsg
	stax	d	; msg.fnc = sndmsg
	inx	d
	inx	d
	mov	a,b
	stax	d	; msg.msg(0) = (xx)
	mvi	b,0	; msg.siz = 0
scanmsg:
	inx	h
	mov	a,m
	cpi	'"'
	jz	dosndmsg
	inx	d
	stax	d
	inr	b
	dcr	c
	jnz	scanmsg
	dcr	b
dosndmsg:
	lxi	h,Mailmsg+4
	mov	m,b	; msg.siz =
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
	lda	Mailmsg+5
	inr	a
	jnz	Exit
	lxi	d,sndmsgfailedmsg
	jmp	printmsg

versionerr:
	lxi	d,versionerrmsg
	jmp	printmsg

networkerr:
	lxi	d,networkerrmsg
	jmp	printmsg

sndmsgerr:
	lxi	d,sndmsgerrmsg
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

sndmsgerrmsg:
	db	'Illegal SNDMAIL command.'
	db	'$'

sndmsgfailedmsg:
	db	'Destination processor not logged in or mailbox full.'
	db	'$'

lclstack:
	dw	0c7c7h,0c7c7h,0c7c7h,0c7c7h,0c7c7h
	dw	0c7c7h,0c7c7h,0c7c7h,0c7c7h,0c7c7h
CCPStack:
	dw	$-$

Mailmsg:
	db	0	; msg.fmt = 0
	db	$-$	; msg.did
	db	$-$	; msg.sid
	db	$-$	; msg.fnc
	db	$-$	; msg.siz
	ds	256	; msg.msg(0) ... msg.msg(255)

	end	start
