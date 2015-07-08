ENTRY(_start)

SECTIONS
{
    /* This is where we're loaded at runtime */
	. = 0x7C00;

    LD_KERNEL_START = .;

    /* Code */
	.text : 
	{
        *(.text.boot)
		*(.text)
	}

	/* Read-only data. */
	.rodata : 
	{
		*(.rodata)
	}

	/* Read-write data (initialized) */
	.data : 
	{
		*(.data)
	}

    LD_KERNEL_DATA_END = .;

	/* Read-write data (uninitialized) and stack */
	.bss : 
	{
		*(COMMON)
		*(.bss)
		*(.bootstrap_stack)
	}

    LD_KERNEL_END = .;
}
