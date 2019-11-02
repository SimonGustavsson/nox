# Nox

Welcome! Nox is a hobby kernel developed by two guys with a keen interest in kernels and operating systems.
This is our common project that we work on as much as time and life allows. It is most definitely *not* ready
for any sort of real world use.

# What does it do *now*?
* Protected mode
* Paging (in the simpliest form)
* Interrupts
* PIT support for timing
* PS/2 keyboard
* Scan code Set 1 interpreter
* PATA read

# What we're planning on doing
* Terminals (multiple, text and visual)
* Threads
* User mode
* USB stack
* PATA Write support
* Networking

# Running it
After building, just hit `make run` to test in Bochs.

# Building
`make`
Yes, it's that simple! Make creates a harddrive image file with the following:
* Custom MBR
* FAT16 Filesystem with a custom VBR
* A slimmed down version of the kernel acting as bootloader (BOOT.SYS)
* A 32-bit elf kernel (kernel.elf)

# Build requirements:

* [Bochs](http://bochs.sourceforge.net/) (2.6.6+ recommended)
* [Nasm](http://www.nasm.us/) (2.10.09+ recommended)
* [mtools](http://www.gnu.org/software/mtools/)
* gcc (See note at the bottom of this file)
* mkdosfs
* make

# Configuring Bochs
The default configuration for Bochs does not enable everything required
to run nox, to ensure Nox runs correctly, please configure Bochs with the following:

`./configure --enable-usb --enable-x86-64 --enable-sb16 --enable-debugger --disable-debugger-gui`

*Note:*--disable-debugger-gui is a personal preference and not required,
the remaining options are all required to run the kernel. For an explanation of the available
options see the [Bochs documentation](http://bochs.sourceforge.net/doc/docbook/user/compiling.html#CONFIG-OPTS).

# GCC
See [GCC Cross-Compiler](http://wiki.osdev.org/GCC_Cross-Compiler) for instructions on building
a cross compiler required to build this project.

# Boot process
###MBR (16-bit)
The MBR is loaded to `0x7C00`, immediately relocates itself to `0x600` and loads the VBR to `0x7C00` and jumps to it.

### VBR (16-bit)
The VBR is loaded to 0x7C00 and relocates itself to `0x600` overwriting the MBR and then tries to locate `BOOT    SYS`(kloader) on the FAT16 file system and loads it into memory at `0x7C00` and then jumps to it.

### kloader (Kernel Loader) (16-bit -> 32-bit switch)
`BOOT    SYS` aka `BOOT.SYS` aka "kloader" aka `boot.elf` is a slimmed down version of the kernel.

**The Stack Pointer is set to**: `0x7DFFFF`.

The loader will:
* Detecting the amount of RAM
* Set up the GDT
* Set a sensible video mode (vga 80x25, 16 colors),
* Enable A20
* Enter protected mode
* Load `KERNEL  ELF` `0x100000` from the first and only supported FAT partition on the first and only supported ATA device.
* Jump to the loaded kernel elf

# Memory map (Boot)

```
____________
|__________| 0x0000-0x03FF - Interrupt vector table
|__________| 0x0400-0x04FF - Bios data area
|__________| 0x0500-0x05FF - Free memory
|__________| 0x0600-0x06FF - VBR relocated on boot
|__________| 0x0700-0x7BFF - Free memory
|__________|   0x0600-0x0800 - MBR Relocated on boot
|__________|   ?x????-0x7B00 - Mbr stack (growing downwards)
|----------| 0x7C00-0x7E00 - MBR, VBR, Kloader Load location
|__________|
|__________|
```

# Memory map (Kernel)

```
For simplicity's sake we just reserve the bottom 1MB to ensure we never accidentally run into it.
____________
|__________| 0x000000-0x0FFFFF - Reserved, bios/hardware/ports
|__________| 0x07FFFF-0x???????? - Stack pointer (growing downwards)
|__________| 0x100000-0x??????? - Kernel (load location, final)
|__________| 0x??????-0x??????? - Memory allocator pages
|__________|
|----------|
|__________|

```

# Contributors
* [Simon Gustavsson](http://www.github.com/simongustavsson/)
* [Philip Stears](http://www.github.com/philipstears/)
