;@----------------------------------------------------------------------------
	#include "equates.h"
;@----------------------------------------------------------------------------
	.global mapper11init
	.global mapper66init
	.global mapper140init
;@----------------------------------------------------------------------------
.section .text,"ax"
;@----------------------------------------------------------------------------
;@ Color Dreams
mapper11init:
;@----------------------------------------------------------------------------
	.word write11,write11,write11,write11
	bx lr
;@----------------------------------------------------------------------------
;@ NES-GNROM & NES-MHROM
;@ Used in:
;@ Gumshoe
;@ Super Mario Bros. + Duck Hunt
mapper66init:
;@----------------------------------------------------------------------------
	.word write66,write66,write66,write66

	ldrb_ r0,cartFlags
	orr r0,r0,#MIRROR
	strb_ r0,cartFlags

	bx lr
;@----------------------------------------------------------------------------
;@ Jaleco JF-11 & JF-14
;@ Used in:
;@ Bio Senshi Dan - Increaser Tono Tatakai
mapper140init:
;@----------------------------------------------------------------------------
	.word void,void,void,void

	adr r1,write66
	str_ r1,m6502WriteTbl+12

	ldrb_ r0,cartFlags
	orr r0,r0,#MIRROR
	strb_ r0,cartFlags

	bx lr
;@----------------------------------------------------------------------------
write11:
;@----------------------------------------------------------------------------
	stmfd sp!,{r0,lr}
	bl map89ABCDEF_
	ldmfd sp!,{r0,lr}
	mov r0,r0,lsr#4
	b chr01234567_
;@----------------------------------------------------------------------------
write66:
;@----------------------------------------------------------------------------
	stmfd sp!,{r0,lr}
	bl chr01234567_
	ldmfd sp!,{r0,lr}
	mov r0,r0,lsr#4
	b map89ABCDEF_
;@----------------------------------------------------------------------------
