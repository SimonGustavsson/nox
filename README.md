#nox
===

x86 kernel for USB HC development

# Running it

After building it, just hit: `make run` to launch the kernel in Bochs.

# Building
`make`
Yes, it's that simple! Make creates a harddrive image file with the following:
* Custom MBR
* FAT16 Filesystem with a custom VBR

# Build requirements:

* [Bochs](http://bochs.sourceforge.net/) (2.6.6+ recommended)
* [Nasm](http://www.nasm.us/) (2.10.09+ recommended)
* Gcc (See note at the bottom of this file)
* [mtools](http://www.gnu.org/software/mtools/)
* mkdosfs

# Configuring Bochs
The default configuration for Bochs does not enable everything required
to run nox, to ensure Nox runs correctly, please configure Bochs with the following:

`./configure --enable-usb --enable-x86-64 --enable-sb16 --enable-debugger --disable-debugger-gui --disable-gtk`

*Note:*--disable-debugger-gui and --disable-gtk is a personal preference and not required,
the remaining options are all required to run the kernel. For an explanation of the available
options see the [Bochs documentation](http://bochs.sourceforge.net/doc/docbook/user/compiling.html#CONFIG-OPTS).

See [GCC Cross-Compiler](http://wiki.osdev.org/GCC_Cross-Compiler) for instructions on building
a cross compiler required to build this project.
