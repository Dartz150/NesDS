@---------------------------------------------------------------------------------
	#include "equates.h"
@---------------------------------------------------------------------------------
	.global mapper73init
	latch = mapperData
	irqen = mapperData+4
	counter = mapperData+8
@---------------------------------------------------------------------------------
.section .text,"ax"
@---------------------------------------------------------------------------------
mapper73init:	@Konami Salamander (J)...
@---------------------------------------------------------------------------------
	.word write8000,writeA000,writeC000,writeE000

	adr r0,hook
	str_ r0,scanlineHook

	bx lr
@---------------------------------------------------------------------------------
write8000:
@---------------------------------------------------------------------------------
	ldr_ r2,latch
	and r0,r0,#0xF
	tst addy,#0x1000
	bne write9000
	bic r2,r2,#0xF0000
	orr r0,r2,r0,lsl#16
	str_ r0,latch
	bx lr
write9000:
	bic r2,r2,#0xF00000
	orr r0,r2,r0,lsl#20
	str_ r0,latch
	bx lr
@---------------------------------------------------------------------------------
writeA000:
@---------------------------------------------------------------------------------
	ldr_ r2,latch
	and r0,r0,#0xF
	tst addy,#0x1000
	bne writeB000
	bic r2,r2,#0xF000000
	orr r0,r2,r0,lsl#24
	str_ r0,latch
	bx lr
writeB000:
	bic r2,r2,#0xF0000000
	orr r0,r2,r0,lsl#28
	str_ r0,latch
	bx lr
@---------------------------------------------------------------------------------
writeC000:
@---------------------------------------------------------------------------------
	tst addy,#0x1000
	bne writeD000
	strb_ r0,irqen
	tst r0,#2			;@ Timer enabled?
	ldrne_ r0,latch
	strne_ r0,counter
	mov r0, #0
	b m6502SetIRQPin
writeD000:				;@ irqAck
	ldrb_ r0,irqen
	bic r0,r0,#2		;@ Disable Timer
	orr r0,r0,r0,lsl#1	;@ Move repeat bit to Enable bit
	strb_ r0,irqen
	mov r0, #0
	b m6502SetIRQPin
@---------------------------------------------------------------------------------
writeE000:
@---------------------------------------------------------------------------------
	tst addy,#0x1000
	bne map89AB_
	bx lr
@---------------------------------------------------------------------------------
hook:
@---------------------------------------------------------------------------------
	ldrb_ r0,irqen
	tst r0,#2			;@ Timer active?
	bxeq lr
	ldr_ r2,counter
	ldr r1,=0x71aaab	;@ 113.66667 (Cycles per scanline)
	tst r0,#4			;@ 8 bit timer?
	bne timer8bit

	adds r2,r2,r1
	bcc h0
takeIrq:
	ldr_ r0,latch
	str_ r0,counter
	mov r0, #1
	b m6502SetIRQPin
timer8bit:
	mov r2,r2,ror#24
	adds r2,r2,r1,lsl#8
	mov r2,r2,ror#8
	bcs takeIrq
h0:
	str_ r2,counter
	bx lr
@---------------------------------------------------------------------------------
