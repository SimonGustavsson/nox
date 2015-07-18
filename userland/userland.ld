/* The bootloader will look at this image and start execution at the symbol
   designated as the entry point. */
ENTRY(main)

/* Tell where the various sections of the object files will be put in the final
   kernel image. */
SECTIONS
{
	/* Begin putting sections at 1 MiB, a conventional place for kernels to be
	   loaded at by the bootloader. */
	. = 0x20000;

    LD_KERNEL_START = .;

    /* Kernel code */
	.text :
	{
        *(.text.start)
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


	/* The compiler may produce other sections, by default it will put them in
	   a segment with the same name. Simply add stuff here as needed. */
}
