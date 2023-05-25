@---------------------------------------------------------------------------------
	#include "equates.h"
@---------------------------------------------------------------------------------
.section .text,"ax"
@---------------------------------------------------------------------------------
	.global mapper118init

	irq_latch	= mapperData+0
	irq_enable	= mapperData+1
	irq_reload	= mapperData+2
	irq_counter	= mapperData+3

	reg0 = mapperData+4
	reg1 = mapperData+5
	reg2 = mapperData+6
	reg3 = mapperData+7
	reg4 = mapperData+8
	reg5 = mapperData+9
	reg6 = mapperData+10
	reg7 = mapperData+11

	chr01 = mapperData+12
	chr23 = mapperData+13
	chr4  = mapperData+14
	chr5  = mapperData+15
	chr6  = mapperData+16
	chr7  = mapperData+17

	prg0  = mapperData+18
	prg1  = mapperData+19

@---------------------------------------------------------------------------------
@ TKSROM & TLSROM boards with a MMC3
@ Games:
@ Armadillo
@ Pro Sport Hockey
mapper118init:
@---------------------------------------------------------------------------------
	.word write0, empty_W, mmc3CounterW, mmc3IrqEnableW
	b mapper4init+4*4

@-------------------------------------------------------------------
setbank_cpu:
@-------------------------------------------------------------------
	stmfd sp!, {lr}
	ldrb_ r0, prg1
	bl mapAB_
	ldrb_ r0, reg0
	tst r0, #0x40
	beq sbc1
	
	mov r0, #-2
	bl map89_
	ldrb_ r0, prg0
	bl mapCD_
	ldmfd sp!, {lr}
	mov r0, #-1
	b mapEF_

sbc1:
	ldrb_ r0, prg0
	bl map89_
	ldmfd sp!, {lr}
	mov r0, #-1
	b mapCDEF_

@-------------------------------------------------------------------
setbank_ppu:
@-------------------------------------------------------------------
	stmfd sp!, {lr}

	ldrb_ r0, reg0
	tst r0, #0x80
	beq 0f
	
	mov r1, #0
	ldrb_ r0, chr4
	bl chr1k
	mov r1, #1
	ldrb_ r0, chr5
	bl chr1k
	mov r1, #2
	ldrb_ r0, chr6
	bl chr1k
	mov r1, #3
	ldrb_ r0, chr7
	bl chr1k
	mov r1, #4
	ldrb_ r0, chr01
	bl chr1k
	mov r1, #5
	ldrb_ r0, chr01
	add r0, r0, #1
	bl chr1k
	mov r1, #6
	ldrb_ r0, chr23
	bl chr1k
	mov r1, #7
	ldrb_ r0, chr23
	add r0, r0, #1
	bl chr1k
	ldmfd sp!, {pc}
	
0:
	mov r1, #0
	ldrb_ r0, chr01
	bl chr1k
	mov r1, #1
	ldrb_ r0, chr01
	add r0, r0, #1
	bl chr1k
	mov r1, #2
	ldrb_ r0, chr23
	bl chr1k
	mov r1, #3
	ldrb_ r0, chr23
	add r0, r0, #1
	bl chr1k
	mov r1, #4
	ldrb_ r0, chr4
	bl chr1k
	mov r1, #5
	ldrb_ r0, chr5
	bl chr1k
	mov r1, #6
	ldrb_ r0, chr6
	bl chr1k
	mov r1, #7
	ldrb_ r0, chr7
	bl chr1k
	ldmfd sp!, {pc}

@------------------------------------
write0:
@------------------------------------
	stmfd sp!, {lr}
	tst addy, #1
	bne w8001

	strb_ r0, reg0
	bl setbank_cpu
	bl setbank_ppu
	ldmfd sp!, {pc}

w8001:
	strb_ r0, reg1
	ldrb_ r1, reg0
	tst r1, #0x80
	and r1, r1, #7
	beq 0f
	cmp r1, #2
	bne 1f
	adr lr, 1f
	tst r0, #0x80
	b mirror1H_	
0:
	cmp r1, #0
	bne 1f
	tst r0,#0x8
	bl mirror1H_
1:
	ldrb_ r1, reg0
	ldrb_ r0, reg1
	and r1, r1, #7
	cmp r1, #2
	andcc r0, r0, #0xfe
	adrl_ r2, chr01
	strb r0, [r2, r1]
	ldmfd sp!, {lr}
	cmp r1, #6
	bcc setbank_ppu
	b setbank_cpu
