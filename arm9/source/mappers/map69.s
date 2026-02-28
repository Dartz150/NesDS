;@----------------------------------------------------------------------------
	#include "equates.h"
;@----------------------------------------------------------------------------
	.global mapper69init

	.struct mapperData
countdown:	.word 0
irqEn:		.byte 0
cmd:		.byte 0
video:		.byte 0		;@ Number of cycles per scanline
audioReg:   .byte 0		;@ Store selected register via $C000
.align
;@----------------------------------------------------------------------------
.section .text,"ax"
;@----------------------------------------------------------------------------
;@ Sunsoft FME-7, 5A & 5B
;@ Used in:
;@ Barcode World
;@ Batman ROTJ
;@ Gimmick
;@ Gremlins (J)
;@ Hebereke
mapper69init:
;@----------------------------------------------------------------------------
	.word write0,write1,rom_W,rom_W			;@ There is a music channel also

    ldr r1, =write2
    str_ r1, m6502WriteTbl+24  @ $C000 - $DFFF Slot
    ldr r1, =write3
    str_ r1, m6502WriteTbl+28  @ $E000 - $FFFF Slot

	mov r1,#-1
	mov r1,r1,lsr#16
	str_ r1,countdown

	ldr_ r1,emuFlags
	tst r1,#PALTIMING
	movne r1,#107			;@ PAL
	moveq r1,#113			;@ NTSC
	strb_ r1,video

	adr r0,hook
	str_ r0,scanlineHook

	bx lr
;@----------------------------------------------------------------------------
write0:		;@ $8000
;@----------------------------------------------------------------------------
	strb_ r0,cmd
	bx lr
;@----------------------------------------------------------------------------
write1:		;@ $A000
;@----------------------------------------------------------------------------
	ldrb_ r1,cmd
	movs r1,r1,lsl#29
	ldrcc r2,=writeCHRTBL
	adrcs r2,commandList
	ldr pc,[r2,r1,lsr#27]
;@----------------------------------------------------------------------------
write2:		;@ $C000-$DFFF Sunsoft 5B Audio Register Select
;@----------------------------------------------------------------------------
	and r0, r0, #0x0F      ;@ only the 4 LSB
    strb_ r0, audioReg
    bx lr
;@----------------------------------------------------------------------------
write3:		;@ $E000-$FFFF Sunsoft 5B Audio Register Value
;@----------------------------------------------------------------------------
	stmfd sp!, {r0-r1, lr}
    ldrb_ r1, audioReg     ;@ r1 = dst reg (0-15)
    ;@ r0 already has the value (0-255)
    
    ;@ Send to the sound write dispatcher
    add r1, r1, #0xC000    
    bl writeAPU            ;@ (val r0, addr r1)
    ldmfd sp!, {r0-r1, lr}
    bx lr
;@----------------------------------------------------------------------------
commandList:	.word mapJinx,map89_,mapAB_,mapCD_,mirrorKonami_,irqEn69,irqA69,irqB69
;@----------------------------------------------------------------------------

irqEn69:
	strb_ r0,irqEn
	mov r0,#0
	b rp2A03SetIRQPin
irqA69:
	strb_ r0,countdown+2
	bx lr
irqB69:
	strb_ r0,countdown+3
	bx lr
;@----------------------------------------------------------------------------
mapJinx:
;@----------------------------------------------------------------------------
	tst r0,#0x40
	ldreq r1,=rom_W			;@ Swap in ROM at $6000-$7FFF.
	ldrne r1,=sram_W		;@ Swap in sram at $6000-$7FFF.
	str_ r1,m6502WriteTbl+12
	beq map67_
	ldr r1,=CART_SRAM-0x6000	;@ sram at $6000.
	str_ r1,m6502MemTbl+12
	bx lr
;@----------------------------------------------------------------------------
hook:
;@----------------------------------------------------------------------------
	ldrb_ r2,irqEn
	tst r2,#0x80			;@ Timer enabled?
	bxeq lr

	ldr_ r0,countdown
	ldrb_ r1,video			;@ Number of cycles per scanline.
	subs r0,r0,r1,lsl#16
	str_ r0,countdown
	bxpl lr

	ands r0,r2,#1			;@ IRQ enabled?
	bne rp2A03SetIRQPin
	bx lr
