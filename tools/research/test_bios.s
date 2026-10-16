@ Stand-in GBA BIOS for automated tests of the HLE runtime (never shipped).
@ BIOS HLE services most SWIs natively; what still reaches the BIOS is the IRQ
@ vector and the SWIs HLE leaves to LLE: Halt, IntrWait, VBlankIntrWait and
@ RegisterRamReset. These follow GBATEK's description, minimally.
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

irq:                        @ BIOS IRQ dispatcher: call [0x03FFFFFC]
    stmfd sp!, {r0-r3, r12, lr}
    mov   r0, #0x04000000
    add   lr, pc, #0
    ldr   pc, [r0, #-4]
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
done:
    ldmfd sp!, {r2, r3, r11, r12, lr}
    movs  pc, lr

halt:
    mov   r12, #0x04000000
    mov   r11, #0
    strb  r11, [r12, #0x301]
    b     done

vblank_intr_wait:
    mov   r0, #1
    mov   r1, #1
intr_wait:                  @ r0 = discard old flags, r1 = flags to wait for
    mov   r12, #0x04000000
    cmp   r0, #0
    beq   1f
    ldrh  r11, [r12, #-8]    @ BIOS IF copy at 0x03FFFFF8
    bic   r11, r11, r1
    strh  r11, [r12, #-8]
1:
    msr   cpsr_c, #0x1F     @ System mode, IRQs on, while halted
    mov   r11, #1
    strb  r11, [r12, #0x208] @ IME = 1
    mov   r11, #0
    strb  r11, [r12, #0x301] @ HALT until an IRQ
    ldrh  r11, [r12, #-8]
    ands  r11, r11, r1
    beq   1b
    ldrh  r11, [r12, #-8]
    bic   r11, r11, r1
    strh  r11, [r12, #-8]
    msr   cpsr_c, #0x93     @ back to SVC, IRQs off
    b     done
