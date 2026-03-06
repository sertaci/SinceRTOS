/* src/vectors.S - sinceRTOS The 80MHz PendSV Architecture (WITH WINDOW HANDLERS) */
.section .vectors, "ax"
.global _vecbase_start
.balign 256

_vecbase_start:

/* --- XTENSA STANDART WINDOW HANDLERS --- */
.org 0x000
_WindowOverflow4:
    s32e    a0, a5, -16
    s32e    a1, a5, -12
    s32e    a2, a5, -8
    s32e    a3, a5, -4
    rfwo

.org 0x040
_WindowUnderflow4:
    l32e    a0, a5, -16
    l32e    a1, a5, -12
    l32e    a2, a5, -8
    l32e    a3, a5, -4
    rfwu

.org 0x080
_WindowOverflow8:
    s32e    a0, a9, -16
    l32e    a0, a1, -12
    s32e    a2, a9, -8
    s32e    a3, a9, -4
    s32e    a4, a0, -32
    s32e    a5, a0, -28
    s32e    a6, a0, -24
    s32e    a7, a0, -20
    s32e    a1, a0, -12
    s32e    a0, a9, -12
    rfwo

.org 0x0C0
_WindowUnderflow8:
    l32e    a0, a9, -16
    l32e    a1, a9, -12
    l32e    a2, a9, -8
    l32e    a3, a9, -4
    l32e    a4, a1, -32
    l32e    a5, a1, -28
    l32e    a6, a1, -24
    l32e    a7, a1, -20
    rfwu

.org 0x100
_WindowOverflow12:
    s32e    a0, a13, -16
    l32e    a0, a1, -12
    s32e    a2, a13, -8
    s32e    a3, a13, -4
    s32e    a4, a0, -48
    s32e    a5, a0, -44
    s32e    a6, a0, -40
    s32e    a7, a0, -36
    s32e    a8, a0, -32
    s32e    a9, a0, -28
    s32e    a10, a0, -24
    s32e    a11, a0, -20
    s32e    a1, a0, -12
    s32e    a0, a13, -12
    rfwo

.org 0x140
_WindowUnderflow12:
    l32e    a0, a13, -16
    l32e    a1, a13, -12
    l32e    a2, a13, -8
    l32e    a3, a13, -4
    l32e    a4, a1, -48
    l32e    a5, a1, -44
    l32e    a6, a1, -40
    l32e    a7, a1, -36
    l32e    a8, a1, -32
    l32e    a9, a1, -28
    l32e    a10, a1, -24
    l32e    a11, a1, -20
    rfwu

.org 0x300
    j level1_isr_asm
.org 0x340
    j level1_isr_asm

.section .text
.balign 4

.global level1_isr_asm
level1_isr_asm:
    /* --- Context Save --- */
    wsr.excsave1 a0
    addi a1, a1, -80
    s32i a2, a1, 16
    s32i a3, a1, 20
    rsr a2, excsave1
    s32i a2, a1, 8
    addi a2, a1, 80
    s32i a2, a1, 12
    s32i a4, a1, 24
    s32i a5, a1, 28
    s32i a6, a1, 32
    s32i a7, a1, 36
    s32i a8, a1, 40
    s32i a9, a1, 44
    s32i a10, a1, 48
    s32i a11, a1, 52
    s32i a12, a1, 56
    s32i a13, a1, 60
    s32i a14, a1, 64
    s32i a15, a1, 68
    rsr a2, epc1
    s32i a2, a1, 0
    rsr a2, ps
    s32i a2, a1, 4

    /* Write SP to the current task's TCB */
    movi a2, current_task
    l32i a3, a2, 0
    movi a4, task_list
    slli a5, a3, 5
    add a5, a4, a5
    s32i a1, a5, 0

    /* --- (Demux) --- */
    rsr a2, interrupt

check_timer:
    movi a3, (1 << 6)   /* INT 6 (Timer) */
    and a4, a2, a3
    beqz a4, check_soft

timer_handle:
update_tick:
    movi a5, system_ticks
    l32i a6, a5, 0
    addi a6, a6, 1
    s32i a6, a5, 0

    /* Set up the timer */
    rsr a5, ccompare0
    j _skip_timer_val
    .align 4
    _timer_val: .word 80000

_skip_timer_val:
    l32r a6, _timer_val
    add a5, a5, a6
    wsr a5, ccompare0
    isync

    /* Catch-Up Control */
    rsr a7, ccount
    sub a7, a7, a5
    bgez a7, update_tick

    /* Wake up the sleepers */
    movi a5, system_ticks
    l32i a6, a5, 0
    movi a7, task_list
    movi a8, 0
    movi a9, 6

wake_loop:
    beq a8, a9, end_wake_loop
    slli a10, a8, 5
    add a10, a7, a10
    l32i a11, a10, 4   /* state */
    movi a12, 1
    bne a11, a12, wake_next 
    l32i a11, a10, 8   /* wake_tick */
    bltu a6, a11, wake_next 
    movi a12, 0
    s32i a12, a10, 4

wake_next:
    addi a8, a8, 1
    j wake_loop

end_wake_loop:
    movi a5, (1 << 7)
    wsr a5, intset
    isync

    rsr a2, interrupt

check_soft:
    movi a3, (1 << 7)   /* INT 7 (Software) */
    and a4, a2, a3
    beqz a4, isr_exit

soft_handle:
    movi a3, (1 << 7)
    wsr a3, intclear

    /* --- 3. Round-Robin + Priority --- */
    movi a2, current_
    l32i a11, a2, 0
    movi a4, -1
    mov a8, a11
    movi a9, 6
    movi a10, task_list
    movi a3, 0

find_loop:
    beq a3, a9, end_find_loop
    add a5, a11, a3
    addi a5, a5, 1
    blt a5, a9, no_wrap
    sub a5, a5, a9

no_wrap:
    slli a12, a5, 5
    add a12, a10, a12
    l32i a6, a12, 4
    bnez a6, next_iter
    l32i a7, a12, 16
    bge a4, a7, next_iter
    mov a4, a7
    mov a8, a5

next_iter:
    addi a3, a3, 1
    j find_loop

end_find_loop:
    s32i a8, a2, 0

isr_exit:
    /* (Context Restore) --- */
    movi a2, current_task
    l32i a3, a2, 0
    movi a4, task_list
    slli a5, a3, 5
    add a5, a4, a5
    l32i a1, a5, 0

    l32i a2, a1, 0
    wsr a2, epc1
    l32i a2, a1, 4
    wsr a2, ps
    l32i a4, a1, 24
    l32i a5, a1, 28
    l32i a6, a1, 32
    l32i a7, a1, 36
    l32i a8, a1, 40
    l32i a9, a1, 44
    l32i a10, a1, 48
    l32i a11, a1, 52
    l32i a12, a1, 56
    l32i a13, a1, 60
    l32i a14, a1, 64
    l32i a15, a1, 68
    l32i a0, a1, 8
    l32i a2, a1, 16
    l32i a3, a1, 20
    addi a1, a1, 80
    rsync
    rfe
