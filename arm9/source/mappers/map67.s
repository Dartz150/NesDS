@---------------------------------------------------------------------------------
	#include "equates.h"
@---------------------------------------------------------------------------------
	.global mapper67init
	.global map67_IRQ_Hook
	countdown = mapperData+0
	irqen = mapperData+4
	suntoggle = mapperData+5
@---------------------------------------------------------------------------------
.section .text,"ax"
@---------------------------------------------------------------------------------
@ Sunsoft-3
@ Used in:
@ Fantasy Zone II (J)
@ Mito Koumon II - Sekai Manyuu Ki
@ Vs. Platoon
mapper67init:
@---------------------------------------------------------------------------------
	.word write0,write1,write2,write3

	adr r0,map67_IRQ_Hook
	str_ r0,scanlineHook

	bx lr
@---------------------------------------------------------------------------------
write0:		@8000,8800,9800
@---------------------------------------------------------------------------------
	tst addy,#0x0800
	bxeq lr
	tst addy,#0x1000
	beq chr01_
	b   chr23_
@---------------------------------------------------------------------------------
write1:		@A800-B800
@---------------------------------------------------------------------------------
	tst addy,#0x0800
	bxeq lr
	tst addy,#0x1000
	beq chr45_
	b   chr67_
@---------------------------------------------------------------------------------
write2:		@C800,D800
@---------------------------------------------------------------------------------
	tst addy,#0x1000
	movne r1,#0
	strneb_ r1,suntoggle
	strneb_ r0,irqen
	bxne lr

	ldrb_ r1,suntoggle
	eors r1,r1,#1
	strb_ r1,suntoggle
	strneb_ r0,countdown+1
	streqb_ r0,countdown
	bx lr
@---------------------------------------------------------------------------------
write3:		@E800,F800
@---------------------------------------------------------------------------------
	tst addy,#0x0800
	bxeq lr
	tst addy,#0x1000
	bne map89AB_
	b mirrorKonami_
@---------------------------------------------------------------------------------
map67_IRQ_Hook:
@---------------------------------------------------------------------------------
	ldrb_ r1,irqen
	cmp r1,#0
	bxeq lr

	ldr_ r0,countdown
	subs r0,r0,#113
	str_ r0,countdown
	bxpl lr

	mov r1,#0
	strb_ r1,irqen
	mov r0,r0,lsr#16
	str_ r0,countdown
	mov r0,#1
	b rp2A03SetIRQPin
@---------------------------------------------------------------------------------
