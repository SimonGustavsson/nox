ENTRY(_start)

SECTIONS
{
    /* This is where we're loaded at runtime */
	. = 0x7C00;

    LD_KERNEL_START = .;
    LD_CODE_START = .;

    /* Code */
	.text :
	{
        *(.text.boot)
		*(.text)
	}

    LD_CODE_END = .;

    LD_RODATA_START = .;

	/* Read-only data. */
	.rodata :
	{
		*(.rodata)
	}

    LD_RODATA_END = .;
    LD_RWDATA_START = .;

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

    LD_RWDATA_END = .;
    LD_KERNEL_END = .;
}
