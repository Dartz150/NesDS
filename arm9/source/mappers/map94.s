;@----------------------------------------------------------------------------
	#include "equates.h"
;@----------------------------------------------------------------------------
	.global mapper94init
;@----------------------------------------------------------------------------
.section .text,"ax"
;@----------------------------------------------------------------------------
;@ HVC-UN1ROM board
;@ Used in:
;@ Senjou no Ookami (Japanese version of Commando)
mapper94init:
;@----------------------------------------------------------------------------
	.word write94,write94,write94,write94
	bx lr
;@----------------------------------------------------------------------------
write94:
;@----------------------------------------------------------------------------
	mov r0,r0,lsr#2
	b map89AB_
;@----------------------------------------------------------------------------
