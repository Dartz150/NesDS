@---------------------------------------------------------------------------------
	#include "equates.h"
@---------------------------------------------------------------------------------
	.global mapper69init
	countdown = mapperData+0
	irqen = mapperData+4
	cmd = mapperData+5
	video = mapperData+6		@ number of cycles per scanline
@---------------------------------------------------------------------------------
.section .text,"ax"
@---------------------------------------------------------------------------------
@ Sunsoft FME-7, 5A and 5B, mapper 69
@ Batman ROTJ
@ Gimmick
@ Hebereke
@ Gremlins 2 (J)
@ Barcode World
mapper69init:
@---------------------------------------------------------------------------------
	.word write0,write1,void,void			@There is a music channel also

	mov r1,#-1
	mov r1,r1,lsr#16
	str_ r1,countdown

	ldr_ r1,emuFlags
	tst r1,#PALTIMING
	movne r1,#107				@PAL
	moveq r1,#113				@NTSC
	strb_ r1,video

	adr r0,hook
	str_ r0,scanlineHook

	bx lr
@---------------------------------------------------------------------------------
write0:		@$8000
@---------------------------------------------------------------------------------
	strb_ r0,cmd
	bx lr
@---------------------------------------------------------------------------------
write1:		@$A000
@---------------------------------------------------------------------------------
	ldrb_ r1,cmd
	tst r1,#0x08
	and r1,r1,#7
	ldreq r2,=writeCHRTBL
	adrne r2,commandlist
	ldr pc,[r2,r1,lsl#2]

irqen69:
	strb_ r0,irqen
	mov r0,#0
	b m6502SetIRQPin
irqA69:
	strb_ r0,countdown
	bx lr
irqB69:
	strb_ r0,countdown+1
	bx lr
@---------------------------------------------------------------------------------
mapJinx:
@---------------------------------------------------------------------------------
	tst r0,#0x40
	ldreq r1,=empty_W		@Swap in ROM at $6000-$7FFF.
	ldrne r1,=sram_W		@Swap in sram at $6000-$7FFF.
	str_ r1,m6502WriteTbl+12
	beq map67_
	ldr r1,=NES_RAM-0x5800		@sram at $6000.
	str_ r1,m6502MemTbl+12
	bx lr
@---------------------------------------------------------------------------------
hook:
@---------------------------------------------------------------------------------
	ldrb_ r2,irqen
	tst r2,#0x80			@ Timer enable?
	bxeq lr

	ldr_ r0,countdown
	ldrb_ r1,video			@ Number of cycles per scanline.
	subs r0,r0,r1
	str_ r0,countdown
	bxpl lr

	mov r0,r0,lsr#16
	str_ r0,countdown

	ands r0,r2,#0x01			@ IRQ enable?
	bne m6502SetIRQPin
	bx lr
@---------------------------------------------------------------------------------
commandlist:	.word mapJinx,map89_,mapAB_,mapCD_,mirrorKonami_,irqen69,irqA69,irqB69
@---------------------------------------------------------------------------------
