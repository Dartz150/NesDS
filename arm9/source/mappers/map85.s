;@----------------------------------------------------------------------------
	#include "equates.h"
;@----------------------------------------------------------------------------
	.global mapper85init
;@----------------------------------------------------------------------------
.section .text,"ax"
;@----------------------------------------------------------------------------
;@ Konami VRC7
;@ Used in:
;@ Tiny Toon Adventure 2 (J)...
;@ Lagrange Point, requires CHRRAM swappability  =)
mapper85init:
;@----------------------------------------------------------------------------
	.word write85,write85,write85,write85
	b Konami_Init
VRC7:
	tst addy,#0x20          ;@ $9010 (A5=0) || $9030 (A5=1)
    beq vrc7_write_addr     ;@ Call hooks in misc.c
    b vrc7_write_data
;@----------------------------------------------------------------------------
write85:
;@----------------------------------------------------------------------------
	mov r1,addy,lsr#11
	and r1,r1,#0xE
	tst addy,#0x18
	orrne r1,r1,#1

	ldr pc,[pc,r1,lsl#2]
	nop
tbl85:	.word map89_,mapAB_,mapCD_,VRC7,chr0_,chr1_,chr2_,chr3_,chr4_,chr5_,chr6_,chr7_,mirrorKonami_,KoLatch,KoIRQEnable,KoIRQack
;@----------------------------------------------------------------------------
