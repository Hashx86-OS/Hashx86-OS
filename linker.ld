/*
MIT License

Copyright (c) 2025 Malaka Gunawardana

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

ENTRY(loader)
OUTPUT_FORMAT(elf32-i386)
OUTPUT_ARCH(i386:i386)

SECTIONS
{
    . = 0x0100000;
    __kernel_section_start = .;
    .text : ALIGN(4)
    {
        __kernel_text_section_start = .;
        *(.multiboot)  /* Must be in first 8KB of file */
        code = .; _code = .; __code = .;
        *(.text*)
        . = ALIGN(4096);
        __kernel_text_section_end = .;
    }

    .data : ALIGN(4)
    {
        __kernel_data_section_start = .;
        data = .; _data = .; __data = .;
        *(.data)
        . = ALIGN(4096);
        __kernel_data_section_end = .;
    }

    .init_array : ALIGN(4)
    {
        start_ctors = .;
        KEEP(*(.init_array))
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
        end_ctors = .;
    }

    .rodata : ALIGN(4)
    {
        __kernel_rodata_section_start = .;
        *(.rodata)
        __kernel_rodata_section_end = .;
    }

    .bss : ALIGN(4)
    {
        __kernel_bss_section_start = .;
        bss = .; _bss = .; __bss = .;
        *(.bss)
        *(COMMON)
        . = ALIGN(4096);
        __kernel_bss_section_end = .;
    }

    .fini_array : ALIGN(4)
    {
        __kernel_fini_array_start = .;
        KEEP(*(.fini_array))
        KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*)))
        __kernel_fini_array_end = .;
    }

    /DISCARD/ :
    {
        *(.comment)
    }

    end = .; _end = .; __end = .;
    __kernel_section_end = .;
}
