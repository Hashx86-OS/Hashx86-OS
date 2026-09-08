; Installer loader — VGA text mode multiboot kernel entry point
; Uses FLAGS without VIDINFO so GRUB boots in 80x25 text mode.

MBALIGN  equ 1<<0
MEMINFO  equ 1<<1
FLAGS    equ MBALIGN | MEMINFO
MAGIC    equ 0x1BADB002
CHECKSUM equ -(MAGIC + FLAGS)

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

section .text
extern kernelMain
extern callConstructors
global loader

loader:
    mov esp, kernel_stack
    push eax
    push ebx
    cld
    call callConstructors
    call kernelMain
_stop:
    cli
    hlt
    jmp _stop

section .bss
resb 262144               ; 256 KB stack
kernel_stack:
