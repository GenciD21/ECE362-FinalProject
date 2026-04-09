/* Custom linker script for loadable game binaries on RP2350.
 *
 * Based on pico-sdk's memmap_no_flash.ld but modified to place
 * everything at 0x20040000 (upper SRAM), leaving the lower 256KB
 * for the loader running on core 0.
 *
 * Memory layout:
 *   0x20000000 - 0x2003FFFF  (256KB) = Loader (core 0)
 *   0x20040000 - 0x2007BFFF  (240KB) = Game binary (core 1)
 *   0x2007C000 - 0x2007FFFF  ( 16KB) = Core 1 stack
 *   0x20080000 - 0x20080FFF  (  4KB) = SCRATCH_X
 *   0x20081000 - 0x20081FFF  (  4KB) = SCRATCH_Y
 */

MEMORY
{
    /* Game code + data region in upper SRAM */
    RAM(rwx) : ORIGIN = 0x20040000, LENGTH = 240k
    /* Core 1 stack region */
    CORE1_STACK(rw) : ORIGIN = 0x2007C000, LENGTH = 16k
    /* Scratch memories (available but typically used by loader/core0) */
    SCRATCH_X(rwx) : ORIGIN = 0x20080000, LENGTH = 4k
    SCRATCH_Y(rwx) : ORIGIN = 0x20081000, LENGTH = 4k
}

ENTRY(_entry_point)

SECTIONS
{
    /* Vector table must be at the very start, 512-byte aligned for Cortex-M33 */
    .text : {
        __logical_binary_start = .;
        KEEP (*(.vectors))
        KEEP (*(.binary_info_header))
        __binary_info_header_end = .;
        KEEP (*(.embedded_block))
        __embedded_block_end = .;
        __reset_start = .;
        KEEP (*(.reset))
        __reset_end = .;
        *(.time_critical*)
        *(.text*)
        . = ALIGN(4);
        *(.init)
        *(.fini)
        /* Pull all c'tors into .text */
        *crtbegin.o(.ctors)
        *crtbegin?.o(.ctors)
        *(EXCLUDE_FILE(*crtend?.o *crtend.o) .ctors)
        *(SORT(.ctors.*))
        *(.ctors)
        /* Followed by destructors */
        *crtbegin.o(.dtors)
        *crtbegin?.o(.dtors)
        *(EXCLUDE_FILE(*crtend?.o *crtend.o) .dtors)
        *(SORT(.dtors.*))
        *(.dtors)

        *(.eh_frame*)
    } > RAM

    .rodata : {
        . = ALIGN(4);
        *(.rodata*)
        *(.srodata*)
        . = ALIGN(4);
        *(SORT_BY_ALIGNMENT(SORT_BY_NAME(.flashdata*)))
        . = ALIGN(4);
    } > RAM

    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > RAM

    __exidx_start = .;
    .ARM.exidx :
    {
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    } > RAM
    __exidx_end = .;

    /* Machine inspectable binary information */
    . = ALIGN(4);
    __binary_info_start = .;
    .binary_info :
    {
        KEEP(*(.binary_info.keep.*))
        *(.binary_info.*)
    } > RAM
    __binary_info_end = .;
    . = ALIGN(4);

    .data : {
        __data_start__ = .;
        *(vtable)
        *(.data*)
        *(.sdata*)

        . = ALIGN(4);
        *(.after_data.*)
        . = ALIGN(4);
        /* preinit data */
        PROVIDE_HIDDEN (__mutex_array_start = .);
        KEEP(*(SORT(.mutex_array.*)))
        KEEP(*(.mutex_array))
        PROVIDE_HIDDEN (__mutex_array_end = .);

        . = ALIGN(4);
        /* preinit data */
        PROVIDE_HIDDEN (__preinit_array_start = .);
        KEEP(*(SORT(.preinit_array.*)))
        KEEP(*(.preinit_array))
        PROVIDE_HIDDEN (__preinit_array_end = .);

        . = ALIGN(4);
        /* init data */
        PROVIDE_HIDDEN (__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array))
        PROVIDE_HIDDEN (__init_array_end = .);

        . = ALIGN(4);
        /* finit data */
        PROVIDE_HIDDEN (__fini_array_start = .);
        *(SORT(.fini_array.*))
        *(.fini_array)
        PROVIDE_HIDDEN (__fini_array_end = .);

        *(.jcr)
        . = ALIGN(4);
   } > RAM

    .tdata : {
        . = ALIGN(4);
		*(.tdata .tdata.* .gnu.linkonce.td.*)
        /* All data end */
        __tdata_end = .;
    } > RAM
    PROVIDE(__data_end__ = .);

    .uninitialized_data (NOLOAD): {
        . = ALIGN(4);
        *(.uninitialized_data*)
    } > RAM
    /* __etext is (for backwards compatibility) the name of the .data init source pointer (...) */
    __etext = LOADADDR(.data);

    .tbss (NOLOAD) : {
        . = ALIGN(4);
        __bss_start__ = .;
        __tls_base = .;
        *(.tbss .tbss.* .gnu.linkonce.tb.*)
        *(.tcommon)

        __tls_end = .;
    } > RAM

    .bss (NOLOAD) : {
        . = ALIGN(4);
        __tbss_end = .;

        *(SORT_BY_ALIGNMENT(SORT_BY_NAME(.bss*)))
        *(COMMON)
        PROVIDE(__global_pointer$ = . + 2K);
        *(.sbss*)
        . = ALIGN(4);
        __bss_end__ = .;
    } > RAM

    .heap (NOLOAD):
    {
        __end__ = .;
        end = __end__;
        KEEP(*(.heap*))
    } > RAM
    /* Heap grows up to end of game RAM region */
    __HeapLimit = ORIGIN(RAM) + LENGTH(RAM);

    /* Start and end symbols must be word-aligned */
    .scratch_x : {
        __scratch_x_start__ = .;
        *(.scratch_x.*)
        . = ALIGN(4);
        __scratch_x_end__ = .;
    } > SCRATCH_X
    __scratch_x_source__ = LOADADDR(.scratch_x);

    .scratch_y : {
        __scratch_y_start__ = .;
        *(.scratch_y.*)
        . = ALIGN(4);
        __scratch_y_end__ = .;
    } > SCRATCH_Y
    __scratch_y_source__ = LOADADDR(.scratch_y);

    /* Core 1 stack in dedicated region */
    .stack1_dummy (NOLOAD):
    {
        *(.stack1*)
    } > CORE1_STACK
    .stack_dummy (NOLOAD):
    {
        KEEP(*(.stack*))
    } > CORE1_STACK

    /* stack limit is poorly named, but historically is maximum heap ptr */
    __StackLimit = ORIGIN(RAM) + LENGTH(RAM);
    __StackOneTop = ORIGIN(CORE1_STACK) + LENGTH(CORE1_STACK);
    __StackTop = ORIGIN(CORE1_STACK) + LENGTH(CORE1_STACK);
    __StackOneBottom = __StackOneTop - SIZEOF(.stack1_dummy);
    __StackBottom = __StackTop - SIZEOF(.stack_dummy);
    PROVIDE(__stack = __StackTop);

    /* picolibc and LLVM */
    PROVIDE (__heap_start = __end__);
    PROVIDE (__heap_end = __HeapLimit);
    PROVIDE( __tls_align = MAX(ALIGNOF(.tdata), ALIGNOF(.tbss)) );
    PROVIDE( __tls_size_align = (__tls_size + __tls_align - 1) & ~(__tls_align - 1));
    PROVIDE( __arm32_tls_tcb_offset = MAX(8, __tls_align) );

    /* llvm-libc */
    PROVIDE (_end = __end__);
    PROVIDE (__llvm_libc_heap_limit = __HeapLimit);

    /* Check if data + heap + stack exceeds RAM limit */
    ASSERT(__StackLimit >= __HeapLimit, "region RAM overflowed")

    ASSERT( __binary_info_header_end - __logical_binary_start <= 1024, "Binary info must be in first 1024 bytes of the binary")
    ASSERT( __embedded_block_end - __logical_binary_start <= 4096, "Embedded block must be in first 4096 bytes of the binary")
}
