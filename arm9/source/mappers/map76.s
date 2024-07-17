@---------------------------------------------------------------------------------
	#include "equates.h"
@---------------------------------------------------------------------------------
	.global mapper76init
	cmd = mapperData
@---------------------------------------------------------------------------------
.section .text,"ax"
@---------------------------------------------------------------------------------
@ Namcot 108 on NAMCOT-3446 board
@ Used in:
@ Megami Tensei: Digital Devil Story
@ Also see mapper 88, 206
mapper76init:
@---------------------------------------------------------------------------------
	.word write0,write1,void,void

	bx lr
@---------------------------------------------------------------------------------
write0:		@$8000-8001
@---------------------------------------------------------------------------------
	tst addy,#1
	streqb_ r0,cmd
	bxeq lr
w8001:
	ldrb_ r1,cmd
	and r1,r1,#7
	ldr pc,[pc,r1,lsl#2]
	nop
commandlist:	.word void,void,chr01_,chr23_,chr45_,chr67_,map89_,mapAB_
@---------------------------------------------------------------------------------
write1:		@$A000 not used?
@---------------------------------------------------------------------------------
	tst addy,#1
	bxne lr
	tst r0,#1
	b mirror2V_
@---------------------------------------------------------------------------------
