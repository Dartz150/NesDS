@---------------------------------------------------------------------------------
	#include "equates.h"
@---------------------------------------------------------------------------------
	.global mapper253init
	latch = mapperData+0
	irqen = mapperData+1
	k4irq = mapperData+2
	counter = mapperData+3

	reg0	= mapperData+0
	reg1	= mapperData+1
	reg2	= mapperData+2
	reg3	= mapperData+3
	reg4	= mapperData+4
	reg5	= mapperData+5
	reg6	= mapperData+6
	reg7	= mapperData+7
@---------------------------------------------------------------------------------
.section .text,"ax"
@---------------------------------------------------------------------------------
mapper253init:
@---------------------------------------------------------------------------------
	.word write89, writeAB, writeCD, writeEF
	
	ldr r0, =0x0100
	str_ r0, reg0
	ldr r0, =0x0302
	str_ r0, reg4 + 4
	ldr r0, =0x0504
	str_ r0, reg0 + 8
	ldr r0, =0x0706
	str_ r0, reg0 + 12

	stmfd sp!, {lr}
	bl Konami_Init

	mov r0, #0
	bl chr01234567_

	ldr r0,=VRAM_chr				@enable chr write
	ldr r1,=vram_write_tbl
	mov r2,#8
	bl filler

	adr r0, frameHook
	str_ r0, newFrameHook

	ldmfd sp!, {pc}

@--------------
write89:
	ldr r1, =0x8010
	cmp addy, r1
	beq map89_
	ldr r1, =0x9400
	cmp addy, r1
	bxne lr
	and r0, r0, #3
	tst r0, #2
	beq 0f
	tst r0, #1
	b mirror1_
0:
	tst r0, #1
	b mirror2V_

@---------------------------------------------------------------------------------
writeAB:
	ldr r1, =0xa010
	cmp r1, addy
	beq mapAB_
	tst addy, #0x1000
	bxeq lr

writeppu:
	mov r2, addy, lsr#12
	sub r2, r2, #0xb
	mov r2, r2, lsl#1
	tst addy, #0x8
	addne r2, r2, #1
	adrl_ r1, reg0
	tst addy, #0x4
	add addy, r1, r2
	ldrb r1, [addy]
	andeq r1, #0xF0
	andeq r0, #0xF
	orreq r0, r0, r1
	andne r1, #0xF
	orrne r0, r1, r0, lsl#4
	strb r0, [addy]
	mov r1, r2
	b chr1k

@---------------------------------------------------------------------------------
writeCD:	
	b writeppu
@---------------------------------------------------------------------------------
writeEF:	
	tst addy, #0x1000	@addy=0xF***
	beq writeppu

	and r1, addy, #0xc
	ldr pc, [pc, r1]
	mov r0, r0
	.word KoLatchLo, KoLatchHi, KoIRQEnable, KoIRQack

@------------------------
frameHook:
	mov r0,#-1
	ldr r1,=agb_obj_map
	str r0,[r1],#4
	str r0,[r1],#4
	str r0,[r1],#4
	str r0,[r1],#4

	mov r0,#-1		@code from resetCHR
	ldr r1,=agb_bg_map
	mov r2,#16 * 2
	b filler
