@ Port BIOS — a small replacement GBA BIOS written for this port from GBATEK's
@ description (no Nintendo code). It is recompiled into hod.aarch64 so the
@ BIOS code that still runs every frame executes natively.
@
@ BIOS HLE (--bios-hle) services the math, copy and decompression SWIs in
@ C++. What remains for a BIOS is what this file implements:
@   IRQ vector (call [0x03FFFFFC]), SoftReset (0), RegisterRamReset (1),
@   Halt (2), IntrWait (4) and VBlankIntrWait (5).
@ Any other SWI returns at once. Castlevania: Harmony of Dissonance calls
@ nothing else from the BIOS (swi 0/1/5 plus HLE'd 6/8/0xA/0xB/0xE/0x11).
@
@ Built by rocknix/scripts/gen-port-bios.sh; the result is committed in
@ rocknix/bios/generated/ so the normal build needs no ARM assembler.
    .arm
    .text
    .org 0x00
    b   hang                @ reset (HLE boots the cart itself)
    b   hang                @ undefined
    b   swi                 @ SWI
    b   hang                @ prefetch abort
    b   hang                @ data abort
    b   hang                @ reserved
    b   irq                 @ IRQ
    b   hang                @ FIQ
hang:
    b   hang

irq:                        @ IRQ dispatcher: call the handler at [0x03FFFFFC]
    stmfd sp!, {r0-r3, r12, lr}
    mov   r0, #0x04000000
    add   lr, pc, #0
    ldr   pc, [r0, #-4]
irq_return:                 @ handler returns here (own entry in port_bios.toml)
    ldmfd sp!, {r0-r3, r12, lr}
    subs  pc, lr, #4

swi:
    stmfd sp!, {r2, r3, r11, r12, lr}
    ldrb  r12, [lr, #-2]     @ comment byte (Thumb low byte / ARM bits 16-23)
    cmp   r12, #2
    beq   halt
    cmp   r12, #4
    beq   intr_wait
    cmp   r12, #5
    beq   vblank_intr_wait
    cmp   r12, #1
    beq   register_ram_reset
    cmp   r12, #0
    beq   soft_reset
swi_done:
    ldmfd sp!, {r2, r3, r11, r12, lr}
    movs  pc, lr

halt:
    mov   r12, #0x04000000
    mov   r11, #0
    strb  r11, [r12, #0x301] @ HALTCNT
    b     swi_done

vblank_intr_wait:
    mov   r0, #1
    mov   r1, #1
intr_wait:                  @ r0 = discard old flags, r1 = flags to wait for
    mov   r12, #0x04000000
    cmp   r0, #0
    beq   intr_wait_loop
    ldrh  r11, [r12, #-8]    @ BIOS IF copy at 0x03FFFFF8 (= 0x03007FF8)
    bic   r11, r11, r1
    strh  r11, [r12, #-8]
intr_wait_loop:
    msr   cpsr_c, #0x1F     @ System mode, IRQs on, while halted
    mov   r11, #1
    strb  r11, [r12, #0x208] @ IME = 1
    mov   r11, #0
    strb  r11, [r12, #0x301] @ HALT until an IRQ
    ldrh  r11, [r12, #-8]
    ands  r11, r11, r1
    beq   intr_wait_loop
    ldrh  r11, [r12, #-8]
    bic   r11, r11, r1
    strh  r11, [r12, #-8]
    msr   cpsr_c, #0x93     @ back to SVC, IRQs off
    b     swi_done

@ RegisterRamReset: r0 bit 0 EWRAM, 1 IWRAM (but its top 0x200 bytes),
@ 2 palette, 3 VRAM, 4 OAM, 5 SIO, 6 sound, 7 other I/O registers.
@ DISPCNT is always set to forced blank.
register_ram_reset:
    stmfd sp!, {r4-r7, lr}
    mov   r12, #0x04000000
    mov   r11, #0x80
    strh  r11, [r12]
    mov   r4, #0
    mov   r5, #0
    mov   r6, #0
    mov   r7, #0
    tst   r0, #0x01
    beq   rrr_iwram
    mov   r2, #0x02000000
    mov   r3, #0x00040000
    bl    clear
rrr_iwram:
    tst   r0, #0x02
    beq   rrr_palette
    mov   r2, #0x03000000
    mov   r3, #0x7E00
    bl    clear
rrr_palette:
    tst   r0, #0x04
    beq   rrr_vram
    mov   r2, #0x05000000
    mov   r3, #0x400
    bl    clear
rrr_vram:
    tst   r0, #0x08
    beq   rrr_oam
    mov   r2, #0x06000000
    mov   r3, #0x18000
    bl    clear
rrr_oam:
    tst   r0, #0x10
    beq   rrr_sio
    mov   r2, #0x07000000
    mov   r3, #0x400
    bl    clear
rrr_sio:
    tst   r0, #0x20
    beq   rrr_sound
    add   r2, r12, #0x120   @ SIODATA..SIOCNT, SIODATA8
    mov   r3, #0x10
    bl    clear
    add   r2, r12, #0x134
    mov   r11, #0x8000
    strh  r11, [r2]          @ RCNT: general-purpose mode
rrr_sound:
    tst   r0, #0x40
    beq   rrr_regs
    add   r2, r12, #0x60    @ SOUND1CNT_L..SOUNDCNT_X, FIFOs untouched
    mov   r3, #0x30
    bl    clear
rrr_regs:
    tst   r0, #0x80
    beq   rrr_done
    add   r2, r12, #0x04    @ DISPSTAT..BLDY (DISPCNT keeps forced blank)
    mov   r3, #0x50
    bl    clear
    add   r2, r12, #0xB0    @ DMA0..DMA3
    mov   r3, #0x30
    bl    clear
    add   r2, r12, #0x100   @ timers
    mov   r3, #0x10
    bl    clear
    mov   r11, #0x100       @ identity BG2/BG3 affine matrices
    strh  r11, [r12, #0x20]
    strh  r11, [r12, #0x26]
    strh  r11, [r12, #0x30]
    strh  r11, [r12, #0x36]
    add   r2, r12, #0x200   @ IE, IF (write-1-to-clear), WAITCNT, IME
    mov   r11, #0
    strh  r11, [r2]
    sub   r11, r11, #1
    strh  r11, [r2, #2]
    mov   r11, #0
    strh  r11, [r2, #4]
    strh  r11, [r2, #8]
rrr_done:
    ldmfd sp!, {r4-r7, lr}
    b     swi_done

clear:                      @ zero r3 bytes at r2 (multiple of 16), r4-r7 = 0
    stmia r2!, {r4-r7}
    subs  r3, r3, #16
    bgt   clear
    bx    lr

@ SoftReset: clear 0x03007E00-0x03007FFF, reset the IRQ/SVC/System stacks and
@ restart the cart at 0x08000000 (0x02000000 if [0x03007FFA] is non-zero), in
@ System mode with r0-r12 and LR cleared.
soft_reset:
    mov   r12, #0x04000000
    ldrb  r11, [r12, #-6]    @ 0x03007FFA
    mov   r2, #0x03000000
    add   r2, r2, #0x7E00
    mov   r3, #0x200
    mov   r4, #0
    mov   r5, #0
    mov   r6, #0
    mov   r7, #0
    bl    clear
    mov   r0, #0
    msr   cpsr_c, #0xD2     @ IRQ mode
    ldr   sp, =0x03007FA0
    mov   lr, #0
    msr   spsr_cxsf, r0
    msr   cpsr_c, #0xD3     @ SVC mode
    ldr   sp, =0x03007FE0
    mov   lr, #0
    msr   spsr_cxsf, r0
    msr   cpsr_c, #0x1F     @ System mode, IRQs enabled
    ldr   sp, =0x03007F00
    cmp   r11, #0
    moveq r12, #0x08000000
    movne r12, #0x02000000
    mov   r0, #0
    mov   r1, #0
    mov   r2, #0
    mov   r3, #0
    mov   r4, #0
    mov   r5, #0
    mov   r6, #0
    mov   r7, #0
    mov   r8, #0
    mov   r9, #0
    mov   r10, #0
    mov   r11, #0
    mov   lr, #0
    bx    r12
    .ltorg
