ENTRY(_start)

SECTIONS
{
    /* This is where kloader will put us at boot */
	. = 0x10000;

    LD_KERNEL_START = .;

    /* Kernel code */
	.text :
	{
        *(.text.boot)
		*(.text)
	}

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
		*(.bootstrap_stack)
	}

    LD_KERNEL_END = .;
}
