# Nox

Welcome! Nox is a hobby kernel developed by two guys with a keen interest in kernels and operating system.
This is our common project that we work on as much as time and life allows. It is most definitely *not* ready
for any sort of real world use.

# What does it do *now*?
* Protected mode
* Paging (in the simpliest form)
* Interrupts
* PIT support for timing
* PS/2 keyboard
* Scan code Set 1 interpreter

# What we're planning on doing
* Terminals (multiple, text and visual)
* Threads
* User mode
* USB stack
* PATA support
* Networking

# Running it
After building, just hit `make run` to test in Bochs.

# Building
`make`
Yes, it's that simple! Make creates a harddrive image file with the following:
* Custom MBR
* FAT16 Filesystem with a custom VBR
* A kernel bootloader (BOOT.SYS) on the FAT16 partition
* A 32-bit flat binary kernel (KERNEL.BIN)

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

`./configure --enable-usb --enable-x86-64 --enable-sb16 --enable-debugger --disable-debugger-gui --disable-gtk`

*Note:*--disable-debugger-gui and --disable-gtk is a personal preference and not required,
the remaining options are all required to run the kernel. For an explanation of the available
options see the [Bochs documentation](http://bochs.sourceforge.net/doc/docbook/user/compiling.html#CONFIG-OPTS).

# GCC
See [GCC Cross-Compiler](http://wiki.osdev.org/GCC_Cross-Compiler) for instructions on building
a cross compiler required to build this project.

# Contributors
* [Simon Gustavsson](http://www.github.com/simongustavsson/)
* [Philip Stears](http://www.github.com/philipstears/)
