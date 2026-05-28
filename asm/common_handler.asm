%define IRQ_BASE 0x20

section .text

extern _ZN16InterruptManager15handleInterruptEhj
extern _ZN16InterruptManager15handleExceptionEhj

;----------------------------------------
; Internal macro: save registers, call C++
; handler, restore, and iret.
;
; Stack on entry (top to bottom):
;   [error_code, eip, cs, eflags, (user_esp, user_ss)]
;
; The error_code is at the correct CPUState.error offset
; (+44 from the register-save base).  The vector is NOT on
; the stack at entry; it is passed as an immediate argument.
;----------------------------------------
%macro ExcHandlerBody 2
    ; Save Segment Registers
    push gs
    push fs
    push es
    push ds

    ; Save General Purpose Registers
    push ebp
    push edi
    push esi
    push edx
    push ecx
    push ebx
    push eax

    ; Stack now:
    ;   [eax, ebx, ecx, edx, esi, edi, ebp, ds, es, fs, gs, error, eip, cs, eflags, ...]
    ;          ^ CPUState starts here
    ; CPUState.error is at offset +44 from eax -- correct!

    ; Call C++ Handler
    push esp                   ; Pass pointer to CPUState (Stack Top)
    push dword %1              ; Pass vector number (immediate, not on CPUState stack)
    call %2                    ; Call the C++ handler function
    mov esp, eax               ; Switch Stack (if Schedule() returned a new one)

    ; Restore General Purpose Registers
    pop eax
    pop ebx
    pop ecx
    pop edx
    pop esi
    pop edi
    pop ebp

    ; Restore Segment Registers
    pop ds
    pop es
    pop fs
    pop gs

    ; Cleanup and Return -- remove error code (4 bytes)
    add esp, 4
    iret
%endmacro

;----------------------------------------
; Macro for exceptions without error code
;----------------------------------------
%macro HandleExceptionWithoutError 1
global _ZN16InterruptManager19HandleException%1Ev
_ZN16InterruptManager19HandleException%1Ev:
    push dword 0                     ; Push dummy error code (CPUState.error)
    ExcHandlerBody %1, _ZN16InterruptManager15handleExceptionEhj
%endmacro

;----------------------------------------
; Macro for exceptions with error code
;----------------------------------------
%macro HandleExceptionWithError 1
global _ZN16InterruptManager19HandleException%1Ev
_ZN16InterruptManager19HandleException%1Ev:
    ; Error code already on stack from CPU (CPUState.error)
    ExcHandlerBody %1, _ZN16InterruptManager15handleExceptionEhj
%endmacro

;----------------------------------------
; Macro for Interrupt Requests
;----------------------------------------
%macro HandleInterruptRequest 1
global _ZN16InterruptManager26HandleInterruptRequest%1Ev
_ZN16InterruptManager26HandleInterruptRequest%1Ev:
    push dword 0                     ; Push dummy error code (CPUState.error)
    ExcHandlerBody %1 + IRQ_BASE, _ZN16InterruptManager15handleInterruptEhj
%endmacro


;---------------------
; Exceptions
;---------------------
HandleExceptionWithoutError 0x00   ; Divide Error
HandleExceptionWithoutError 0x01   ; Debug
HandleExceptionWithoutError 0x02   ; NMI
HandleExceptionWithoutError 0x03   ; Breakpoint
HandleExceptionWithoutError 0x04   ; Overflow
HandleExceptionWithoutError 0x05   ; BOUND Range Exceeded
HandleExceptionWithoutError 0x06   ; Invalid Opcode
HandleExceptionWithoutError 0x07   ; Device Not Available
HandleExceptionWithError    0x08   ; Double Fault
HandleExceptionWithoutError 0x09   ; Coprocessor Segment Overrun
HandleExceptionWithError    0x0A   ; Invalid TSS
HandleExceptionWithError    0x0B   ; Segment Not Present
HandleExceptionWithError    0x0C   ; Stack-Segment Fault
HandleExceptionWithError    0x0D   ; General Protection Fault
HandleExceptionWithError    0x0E   ; Page Fault
HandleExceptionWithoutError 0x0F   ; Reserved
HandleExceptionWithoutError 0x10   ; x87 FPU Error
HandleExceptionWithError    0x11   ; Alignment Check
HandleExceptionWithoutError 0x12   ; Machine Check
HandleExceptionWithoutError 0x13   ; SIMD Floating-Point Exception

;---------------------
; Interrupt Requests
;---------------------
HandleInterruptRequest 0x00
HandleInterruptRequest 0x01
HandleInterruptRequest 0x02
HandleInterruptRequest 0x03
HandleInterruptRequest 0x04
HandleInterruptRequest 0x05
HandleInterruptRequest 0x06
HandleInterruptRequest 0x07
HandleInterruptRequest 0x08
HandleInterruptRequest 0x09
HandleInterruptRequest 0x0A
HandleInterruptRequest 0x0B
HandleInterruptRequest 0x0C
HandleInterruptRequest 0x0D
HandleInterruptRequest 0x0E
HandleInterruptRequest 0x0F
HandleInterruptRequest 0x31

HandleInterruptRequest 0x80
HandleInterruptRequest 0x81


global _ZN16InterruptManager22IgnoreInterruptRequestEv
_ZN16InterruptManager22IgnoreInterruptRequestEv:
    iret
