ENTRY(_start)

SECTIONS
{
    /* This is where kloader will put us at boot */
	. = 1M;

    LD_KERNEL_START = .;

    /* Kernel code */
    .text :
    {
        . = ALIGN(8);
        KEEP(*(.multiboot))
        *(.text.boot)
		*(.text)
	}

	LD_KERNEL_CODE_END = .;

	/* Page align data for easy permission setup */
	. = ALIGN(4096);

	LD_KERNEL_DATA_START = .;

	/* Read-only data. */
	.rodata ALIGN(4):
	{
		*(.rodata)
	}

	/* Read-write data (initialized) */
	.data ALIGN(4):
	{
		*(.data)
	}

    LD_KERNEL_DATA_END = .;

	/* Read-write data (uninitialized) and stack */
	.bss ALIGN(4):
	{
		*(COMMON)
		*(.bss)

        LD_KERNEL_STACK_START = .;

		*(.bootstrap_stack)
	}

    .checksum ALIGN(4):
    {
        *(.checksum)
    }

    LD_KERNEL_END = .;
}
