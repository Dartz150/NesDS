@---------------------------------------------------------------------------------
	#include "equates.h"
@---------------------------------------------------------------------------------
	.global mapper87init
@---------------------------------------------------------------------------------
.section .text,"ax"
@---------------------------------------------------------------------------------
@ Jaleco J87
@ USed in:
@ Argus (J)
@ City Connection (J)
@ Ninja Jajamaru Kun
mapper87init:
@---------------------------------------------------------------------------------
	.word write87,write87,write87,write87

	adr r1,write87
	str_ r1,m6502WriteTbl+12

	bx lr
@---------------------------------------------------------------------------------
write87:
@---------------------------------------------------------------------------------
	and r0,r0,#3
	movs r0,r0,lsr#1
	orrcs r0,r0,#2
	b chr01234567_
@---------------------------------------------------------------------------------
