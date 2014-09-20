# Constants for Multiboot header
.set ALIGN,    1<<0             # align loaded modules on page boundaries
.set MEMINFO,  1<<1             # provide memory map
.set FLAGS,    ALIGN | MEMINFO  # this is the Multiboot 'flag' field
.set MAGIC,    0x1BADB002       # 'magic number' lets bootloader find the header
.set CHECKSUM, -(MAGIC + FLAGS) # checksum of above, to prove we are multiboot

# Multiboot header
.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

# Allocate room for temporary stack
.section .bootstrap_stack, "aw", @nobits
stack_bottom:
.skip 16384 # 16 KiB
stack_top:

# Kernel entry point
.section .text
.global _start
.type _start, @function
_start:

	# Setup the stack
	movl $stack_top, %esp

	# Jump to C entry point
	call kernel_main

	# kernel_main should never return, but if it does...
	cli
	hlt
.Lhang:
	jmp .Lhang

# Set the size of the _start symbol to the current location '.' minus its _start
# This isuseful when debugging so we can have stack traces
.size _start, . - _start
