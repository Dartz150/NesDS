@---------------------------------------------------------------------------------
	#include "equates.h"
@---------------------------------------------------------------------------------
	.global mapper34init
@---------------------------------------------------------------------------------
.section .text,"ax"
@---------------------------------------------------------------------------------
@ BNROM
@ Used in:
@ Deadly Towers
@---------------------------------------------------------------------------------
@ NINA-001
@ Used in:
@ Impossible Mission 2
@---------------------------------------------------------------------------------
mapper34init:
@---------------------------------------------------------------------------------
	.word map89ABCDEF_,map89ABCDEF_,map89ABCDEF_,map89ABCDEF_	@ Deadly Towers

	adr r1,write0
	str_ r1,m6502WriteTbl+12

	mov r0,#0
	b map89ABCDEF_
@---------------------------------------------------------------------------------
write0:			@Impossible Mission 2
@---------------------------------------------------------------------------------
	ldr r1,=0x7fff
	cmp addy,r1		@7FFF
	beq chr4567_
	sub r1,r1,#1
	cmp addy,r1		@7FFE
	beq chr0123_
	sub r1,r1,#1
	cmp addy,r1		@7FFD
	beq map89ABCDEF_
	b sram_W
@---------------------------------------------------------------------------------
