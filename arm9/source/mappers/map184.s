;@----------------------------------------------------------------------------
	#include "equates.h"
;@----------------------------------------------------------------------------
	.global mapper184init
;@----------------------------------------------------------------------------
.section .text,"ax"
;@----------------------------------------------------------------------------
;@ Sunsoft-1
mapper184init:
;@----------------------------------------------------------------------------
	.word void,void,void,void

	adr r0,write184
	str_ r0,m6502WriteTbl+12
	bx lr
;@----------------------------------------------------------------------------
write184:
;@----------------------------------------------------------------------------
	stmfd sp!,{r0,lr}
	and r0,r0,#7
	bl chr0123_
	ldmfd sp!,{r0,lr}
	mov r0,r0,lsr#4
	orr r0,r0,#8
	b chr4567_
;@----------------------------------------------------------------------------
