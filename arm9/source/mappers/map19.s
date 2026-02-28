;@----------------------------------------------------------------------------
	#include "equates.h"
;@----------------------------------------------------------------------------
	.global mapper19init
	.global mapper210init

	.struct mapperData
counter:	.word 0
reg0:		.byte 0
reg1:		.byte 0
n163_addr:  .byte 0
n163_inc:   .byte 0
.align
n163_ram:   .space 128 @ N163 Internal RAM
.align
;@----------------------------------------------------------------------------
.section .text,"ax"
;@----------------------------------------------------------------------------
;@ Namco 129 & Namco 163
;@ Used in:
;@ Digital Devil Story: Megami Tensei II
;@ Final Lap
;@ Hydlide 3
;@ Star Wars
mapper19init:
;@----------------------------------------------------------------------------
	.word map19_8,map19_A,map19_C,map19_E

	adr r1,write0
	str_ r1,rp2A03MemWrite

	adr r1,map19_r
	str_ r1,rp2A03MemRead

	adr r0,hook
	str_ r0,scanlineHook

	ldr r0,=VRAM_chr		;@ Enable chr write
	ldr r1,=vram_write_tbl
	mov r2,#8
	b filler

;@----------------------------------------------------------------------------
mapper210init:
;@----------------------------------------------------------------------------
	.word map19_8,map19_A,map210_C,map19_E

	bx lr

;@----------------------------------------------------------------------------
write0:
;@----------------------------------------------------------------------------
	cmp addy,#0x4800
	beq n163_write_data		;@ Write N163 Sound Expansion Data
	blo empty_W
    and r1,addy,#0x7800
	cmp r1,#0x5000
	streqb_ r0,counter+2
	moveq r0,#0
	beq rp2A03SetIRQPin

	cmp r1,#0x5800
	streqb_ r0,counter+2
	mov r0,#0
	b rp2A03SetIRQPin
;@----------------------------------------------------------------------------
n163_write_data:			 ;@ Write to $4800 (Data port)
;@----------------------------------------------------------------------------
    stmfd sp!, {r0, lr}
    ldrb_ r1, n163_addr      ;@ Index
    ldr r2, =n163_ram        ;@ Load N163 ram
    strb r0, [r2, r1]
    
    add r1, r1, #0xF800
    
    stmfd sp!, {r1}
    bl writeAPU     		 ;@ Jump to dispatcher in misc.c
    ldmfd sp!, {r1}
    ldmfd sp!, {r0, lr}
    
    ldrb_ r2, n163_inc		 ;@ Auto-increment Write
    tst r2, #0x80
    bxeq lr                  ;@ If there's no auto-inc, exit
    
    sub r1, r1, #0xF800      ;@ Index
    add r1, r1, #1
    and r1, r1, #0x7F
    strb_ r1, n163_addr
    
    bx lr
;@----------------------------------------------------------------------------
map19_r:
;@----------------------------------------------------------------------------
	cmp addy,#0x4800
    beq n163_read_data		 ;@ Read N163 Sound Expansion Data
	blo empty_R
	mov r0, #0

	and r1,addy,#0x7800

	cmp r1,#0x5000
	ldreqb_ r0,counter+2
	bxeq lr

	cmp r1,#0x5800
	ldreqb_ r0,counter+3
	biceq r0, r0, #0x80
	bx lr

;@----------------------------------------------------------------------------
n163_read_data:				 ;@ Read from $4800 (Data port)
;@----------------------------------------------------------------------------
    ldrb_ r1, n163_addr
    ldr r2, =n163_ram
    ldrb r0, [r2, r1]
    
    ldrb_ r2, n163_inc		 ;@ Also handle Read Auto-Increment
    tst r2, #0x80
    bxeq lr
    
    add r1, r1, #1
    and r1, r1, #0x7F
    strb_ r1, n163_addr
    bx lr
;@----------------------------------------------------------------------------
map19_8:
;@----------------------------------------------------------------------------
	cmp r0, #0xE0
	bcc 0f
	ldrb_ r1, reg0
	ands r1, r1, r1
	beq 1f
0:
	and r1,addy,#0x7800
	ldr r2,=writeCHRTBL
	ldr pc,[r2,r1,lsr#9]

;@----------------------------------------------------------------------------
map19_A:
;@----------------------------------------------------------------------------
	cmp r0, #0xE0
	bcc 0f
	ldrb_ r1, reg1
	ands r1, r1, r1
	beq 1f
0:
	and r1,addy,#0x7800
	ldr r2,=writeCHRTBL
	ldr pc,[r2,r1,lsr#9]

1:
	and r1,addy,#0x7800
	and r0, r0, #0x1F
	mov r1, r1, lsr#11
	b chr1k

;@----------------------------------------------------------------------------
map19_C:			;@ Do NameTable RAMROM change, for mirroring.
;@----------------------------------------------------------------------------
	cmp r0, #0xE0
	bxcc lr

	mov r1, addy, lsr#11
	and r0, r0, #1
	and r1, r1, #3
	ldr r2, =CART_VRAM
	add r2, r2, r0, lsl#11
	ldr r0, =vram_map+8*4
	str r2, [r0, r1, lsl#2]
	bx lr
;@----------------------------------------------------------------------------
map19_E:
;@----------------------------------------------------------------------------
	and r1,addy,#0x7800
	cmp r1,#0x6000
	beq map89_
	cmp r1,#0x7000
	beq mapCD_
	cmp r1, #0x7800        ;@ Check range $F800-$FFFF
    beq n163_write_addr	   ;@Write N163 Sound Expansion Data Address
	cmp r1,#0x6000
    beq map89_
	cmp r1,#0x6800
	bxne lr

	and r1, r0, #0x40
	strb_ r1, reg0
	and r1, r0, #0x80
	strb_ r1, reg1
	b mapAB_
;@----------------------------------------------------------------------------
n163_write_addr:	;@ Write to $F800 (Direction port)
;@----------------------------------------------------------------------------
    and r1, r0, #0x7F
    strb_ r1, n163_addr
    and r1, r0, #0x80
    strb_ r1, n163_inc
    bx lr
;@----------------------------------------------------------------------------
map210_C:			;@ Enable WRAM.
;@----------------------------------------------------------------------------
	tst addy,#0x1800
	bxne lr
	tst r0,#1
	;@ Enable WRAM if set
	bx lr
;@----------------------------------------------------------------------------
hook:				;@ Counter is 15bit, enable is top bit.
;@----------------------------------------------------------------------------

	ldr_ r0,counter
	tst r0,#0x80000000			@ Enabled
	bxeq lr
@	adds r0,r0,#0x71aaab		@ 113.66667
	adds r0,r0,#0x720000
	str_ r0,counter
	bxcc lr

	mov r0,#1
	b rp2A03SetIRQPin
