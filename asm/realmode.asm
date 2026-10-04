;
; MIT License
;
; Copyright (c) 2025 Malaka Gunawardana
;
; Permission is hereby granted, free of charge, to any person obtaining a copy
; of this software and associated documentation files (the "Software"), to deal
; in the Software without restriction, including without limitation the rights
; to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
; copies of the Software, and to permit persons to whom the Software is
; furnished to do so, subject to the following conditions:
;
; The above copyright notice and this permission notice shall be included in all
; copies or substantial portions of the Software.
;
; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
; IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
; FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
; AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
; LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
; OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
; SOFTWARE.
;

; ---------------------------------------------------------------------------- ;
;  Layout:                                                                     ;
;    * The [bits 16] block (realmode_stub_start..realmode_stub_end) is copied  ;
;      to STUB_BASE in low memory at runtime. All absolute references are      ;
;      anchored with (label - realmode_stub_start) + STUB_BASE; NASM folds     ;
;      these to constants, so the copy is position-correct without relocs.     ;
;    * bios_int10 is 32-bit protected-mode glue that performs the copy, fills  ;
;      the io register block, drops to the 16-bit code, and resumes the        ;
;      caller once the BIOS call returns.                                      ;
;                                                                              ;
;  Flow of one call:                                                           ;
;    bios_int10 -> jmp far 0x30:STUB_BASE+rm16_entry   (16-bit pmode)          ;
;              -> clear CR0.PG+PE -> jmp 0x0000:rm_real (true real mode)       ;
;              -> load regs from io -> int 0x10 -> store regs                  ;
;              -> restore CR0 -> jmp far dword [io.ret] (back to 32-bit pmode) ;
;              -> tail: copy output out, restore segments/esp/eflags/          ;
;                 callee-saved regs, then ret                                  ;
;                                                                              ;
;  The GDT already contains 16-bit descriptors at 0x30 (code) and 0x38         ;
;  (data) - see core/gdt.cpp.                                                  ;
; ---------------------------------------------------------------------------- ;

STUB_BASE      equ 0x004000   ; Low-memory home of the copied stub block.
RM_STACK_TOP   equ 0x006000   ; Real-mode stack, grows down from here.

; io block offset layout (must match struct BIOSRegisters + glue in vbe_bios.h)
IOM_IN_EAX equ 0x00
IOM_IN_EBX equ 0x04
IOM_IN_ECX equ 0x08
IOM_IN_EDX equ 0x0C
IOM_IN_EDI equ 0x10
IOM_IN_ESI equ 0x14
IOM_IN_ES  equ 0x18
IOM_IN_DS  equ 0x1A
IOM_IN_FS  equ 0x1C
IOM_IN_GS  equ 0x1E
IOM_OUT_EAX equ 0x20
IOM_OUT_EBX equ 0x24
IOM_OUT_ECX equ 0x28
IOM_OUT_EDX equ 0x2C
IOM_OUT_EDI equ 0x30
IOM_OUT_ESI equ 0x34
IOM_OUT_ES  equ 0x38
IOM_OUT_DS  equ 0x3A
IOM_OUT_FS  equ 0x3C
IOM_OUT_GS  equ 0x3E
IOM_ESP     equ 0x40   ; Saved 32-bit kernel ESP.
IOM_RET_EIP equ 0x44   ; Far-jump target (return-to-pmode) EIP.
IOM_RET_CS  equ 0x48   ; Far-jump target CS.
IOM_FLAGS   equ 0x4A   ; Saved EFLAGS.
IOM_OUT_PTR equ 0x4E   ; Caller's output BIOSRegisters pointer.
IOM_IDT_RM  equ 0x52   ; 6 bytes: real-mode IVT pointer (base 0, limit 0x3FF).
IOM_IDT_SV  equ 0x58   ; 6 bytes: saved kernel IDT descriptor.
IOM_SAVE_EBX equ 0x60  ; Callee-saved EBX parked for the real-mode trip.
IOM_SAVE_ESI equ 0x64  ; Callee-saved ESI parked for the real-mode trip.
IOM_SAVE_EDI equ 0x68  ; Callee-saved EDI parked for the real-mode trip.
IOM_SAVE_CR0 equ 0x6C  ; Caller's CR0 (PE+PG), restored across the mode switch.
IOM_SIZE    equ 0x70

section .text16 progbits alloc exec nowrite
[bits 16]
global realmode_stub_start
realmode_stub_start:

; Caller<->shim register block. Left zeroed by the copy; bios_int10 fills the
; IOM_IN_* half and reads IOM_OUT_* back.
io:
    times IOM_SIZE db 0

; Executed in 16-bit protected mode (CS = 0x30), at linear address STUB_BASE+here.
rm16_entry:
    cli
    mov ax, 0x38                     ; 16-bit data selector (see gdt_init)
    mov ds, ax
    mov es, ax
    ; Drop paging as well as protection. PG is only meaningful with PE set, so
    ; leaving it on would fault on the first real-mode instruction fetch. The
    ; wrapper snapshots the caller's CR0 and restores it verbatim on the way
    ; back, so a caller that was paging gets it again after int 0x10.
    mov eax, cr0
    and eax, 0x7FFFFFFE               ; Clear CR0.PG and CR0.PE.
    mov cr0, eax
    ; The mandatory serializing far jump immediately after clearing PE/PG. This
    ; is a real-mode absolute jump to the same physical bytes we are executing.
    jmp 0x0000:(STUB_BASE + (rm_real - realmode_stub_start))

; True real mode: flat-ish 16-bit addressing, segment bases used directly.
rm_real:
    xor ax, ax                       ; Real-mode segments (base 0).
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax
    mov esp, RM_STACK_TOP

    ; Load the caller's BIOS register values. ES:DI is loaded last because the
    ; int 0x10 entry must see the caller's far buffer pointer (always 0:0x54xx
    ; for VBE) and no segment register may be touched afterwards. ES is loaded
    ; straight from memory - 'mov es, ax' would clobber the AX in the function
    ; code already loaded above.
    mov eax, [STUB_BASE + (io + IOM_IN_EAX - realmode_stub_start)]
    mov ebx, [STUB_BASE + (io + IOM_IN_EBX - realmode_stub_start)]
    mov ecx, [STUB_BASE + (io + IOM_IN_ECX - realmode_stub_start)]
    mov edx, [STUB_BASE + (io + IOM_IN_EDX - realmode_stub_start)]
    mov esi, [STUB_BASE + (io + IOM_IN_ESI - realmode_stub_start)]
    mov es, [STUB_BASE + (io + IOM_IN_ES  - realmode_stub_start)]
    mov di, [STUB_BASE + (io + IOM_IN_EDI - realmode_stub_start)]

    int 0x10

    ; Store the returned registers for the resume path.
    mov [STUB_BASE + (io + IOM_OUT_EAX - realmode_stub_start)], eax
    mov [STUB_BASE + (io + IOM_OUT_EBX - realmode_stub_start)], ebx
    mov [STUB_BASE + (io + IOM_OUT_ECX - realmode_stub_start)], ecx
    mov [STUB_BASE + (io + IOM_OUT_EDX - realmode_stub_start)], edx
    mov [STUB_BASE + (io + IOM_OUT_EDI - realmode_stub_start)], edi
    mov [STUB_BASE + (io + IOM_OUT_ESI - realmode_stub_start)], esi
    mov [STUB_BASE + (io + IOM_OUT_ES  - realmode_stub_start)], es

    ; Re-enter protected mode with the caller's exact CR0, so both PE and PG come
    ; back the way they were. The stub lives below 1 MiB, which Paging
    ; identity-maps, so the far jump below is still fetchable with paging on.
    cli
    mov eax, [STUB_BASE + (io + IOM_SAVE_CR0 - realmode_stub_start)]
    mov cr0, eax
    ; Serializing far indirect jump (m16:32): reads EIP from io.ret_eip and CS
    ; from io.ret_cs, i.e. into the 32-bit tail below.
    db 0x66
    db 0xFF, 0x2E
    dw STUB_BASE + (io + IOM_RET_EIP - realmode_stub_start)

; Runs in 32-bit protected mode again (CS = kernel code, linked low). Repairs
; the flat kernel segment state and ESP/EFLAGS and returns to the caller.
[bits 32]
tail:
    ; Copy the 32-byte output block back to the caller's BIOSRegisters first:
    ; the loop below consumes ESI/EDI, so the caller's callee-saved values have
    ; to be restored after it runs, not before.
    mov esi, STUB_BASE + (io + IOM_OUT_EAX - realmode_stub_start)
    mov edi, [STUB_BASE + (io + IOM_OUT_PTR - realmode_stub_start)]
    mov ecx, 8
    cld
    rep movsd

    mov eax, 0x10                    ; Kernel data selector.
    mov ds, eax
    mov es, eax
    mov fs, eax
    mov gs, eax
    mov ss, eax
    mov esp, [STUB_BASE + (io + IOM_ESP - realmode_stub_start)]

    ; Bring the kernel's own IDT back (see the swap in bios_int10).
    lidt [STUB_BASE + (io + IOM_IDT_SV - realmode_stub_start)]

    ; Restore the callee-saved registers the wrapper parked (cdecl). This has
    ; to come after the copy loop above, which overwrites ESI and EDI.
    mov ebx, [STUB_BASE + (io + IOM_SAVE_EBX - realmode_stub_start)]
    mov esi, [STUB_BASE + (io + IOM_SAVE_ESI - realmode_stub_start)]
    mov edi, [STUB_BASE + (io + IOM_SAVE_EDI - realmode_stub_start)]

    ; Restore the caller's IF, sampled before bios_int10's cli.
    mov eax, [STUB_BASE + (io + IOM_FLAGS - realmode_stub_start)]
    push eax
    popfd

    ; Return normally. ESP is back to its entry value, so the return address is
    ; on top of the stack and 'ret' pops it, leaving the caller's stack
    ; balanced. Jumping to it instead would leave ESP four bytes short.
    ret

global realmode_stub_end
realmode_stub_end:

; ---------------------------------------------------------------------------- ;
;  32-bit protected-mode glue                                                  ;
; ---------------------------------------------------------------------------- ;
section .text
[bits 32]
global bios_int10
; void bios_int10(BIOSRegisters* in, BIOSRegisters* out)
;   in  - caller's BIOS register values (32 bytes).
;   out - receives the registers the BIOS returned (same layout).
; Disables interrupts for the whole call, copies the shim into low memory,
; drops to real mode, runs int 0x10, then resumes and returns.
bios_int10:
    ; Snapshot the caller's EFLAGS and CR0 *before* cli. The flags are restored
    ; by the tail, so sampling them here is what puts the caller's IF back; a
    ; snapshot taken after cli would bake IF=0 into the restore and leave the
    ; caller with interrupts off. CR0 has to be kept too, because the shim
    ; clears CR0.PG to reach real mode and has to put the caller's PG back.
    ; Both are parked on the stack until after the stub copy below, which
    ; re-initialises the io block and so forbids any io write before it.
    pushfd
    mov eax, cr0
    push eax

    cli

    mov eax, 0x10                    ; Flat kernel data for the copies.
    mov es, eax

    ; Copy the 16-bit block (including the 32-bit tail) to STUB_BASE.
    ; NOTE: this rep movsb also re-initializes the io block region to its
    ; link-time bytes, so ALL io fields must be written AFTER this copy.
    mov esi, realmode_stub_start
    mov edi, STUB_BASE
    mov ecx, realmode_stub_end - realmode_stub_start
    cld
    rep movsb

    ; Hand the pre-cli snapshots over to the io block.
    pop eax
    mov [STUB_BASE + (io + IOM_SAVE_CR0 - realmode_stub_start)], eax
    pop eax
    mov [STUB_BASE + (io + IOM_FLAGS - realmode_stub_start)], eax

    ; Copy the caller's 32-byte register block into io.in_*. The two pops above
    ; restored ESP to its entry value, so the arguments sit at their usual
    ; offsets: [esp+4] is 'in', [esp+8] is 'out'.
    mov esi, [esp + 4]               ; BIOSRegisters* in
    mov edi, STUB_BASE + (io + IOM_IN_EAX - realmode_stub_start)
    mov ecx, 8
    rep movsd

    ; Park the resumption state in the io block.
    mov [STUB_BASE + (io + IOM_ESP - realmode_stub_start)], esp
    mov eax, [esp + 8]               ; BIOSRegisters* out
    mov [STUB_BASE + (io + IOM_OUT_PTR - realmode_stub_start)], eax

    ; Park the callee-saved registers so the resumed 32-bit code sees them
    ; exactly as they were. cdecl requires them preserved across bios_int10.
    mov [STUB_BASE + (io + IOM_SAVE_EBX - realmode_stub_start)], ebx
    mov [STUB_BASE + (io + IOM_SAVE_ESI - realmode_stub_start)], esi
    mov [STUB_BASE + (io + IOM_SAVE_EDI - realmode_stub_start)], edi

    ; Program the far-jump target back into 32-bit kernel code (tail).
    mov dword [STUB_BASE + (io + IOM_RET_EIP - realmode_stub_start)], \
        STUB_BASE + (tail - realmode_stub_start)
    mov word  [STUB_BASE + (io + IOM_RET_CS - realmode_stub_start)], 0x08

    ; Swap the IDT for the real-mode IVT (base 0, limit 0x3FF). The kernel and
    ; the bootloader leave an empty IDT (limit 0), and real-mode 'int' faults
    ; (#GP) when the vector exceeds IDTR.limit, so the BIOS IVT must be active
    ; before int 0x10 runs. Restored in the tail after the mode switch back.
    sidt [STUB_BASE + (io + IOM_IDT_SV - realmode_stub_start)]
    mov word  [STUB_BASE + (io + IOM_IDT_RM - realmode_stub_start)], 0x03FF
    mov dword [STUB_BASE + (io + IOM_IDT_RM + 2 - realmode_stub_start)], 0
    lidt [STUB_BASE + (io + IOM_IDT_RM - realmode_stub_start)]

    ; Drop to the 16-bit code selector (0x30) at the copied entry.
    jmp 0x30:STUB_BASE + (rm16_entry - realmode_stub_start)
