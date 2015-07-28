ENTRY(_start)

SECTIONS
{
    /* This is where kloader will put us at boot */
	. = 0x100000;

    LD_KERNEL_START = .;

    /* Kernel code */
	.text ALIGN(4K) :
	{
        LD_CODE_START = .;
        *(.text.boot)
		*(.text)
        LD_CODE_END = .;
	}



	/* Read-only data. */
	.rodata ALIGN(4K):
	{
        LD_RODATA_START = .;
		*(.rodata)
        LD_RODATA_END = .;
	}


	/* Read-write data (initialized) */
	.data ALIGN(4K):
	{
        LD_RWDATA_START = .;
		*(.data)
	}

	/* Read-write data (uninitialized) and stack */
	.bss ALIGN(4K):
	{
		*(COMMON)
		*(.bss)
		*(.bootstrap_stack)
	}

    LD_RWDATA_END = .;

    .checksum ALIGN(4):
    {
        *(.checksum)
    }

    LD_KERNEL_END = .;
}
