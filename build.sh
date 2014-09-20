~/opt/cross/bin/i686-elf-as boot.s -o boot.o
~/opt/cross/bin/i686-elf-gcc -c pci.c -o pci.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra
~/opt/cross/bin/i686-elf-gcc -c terminal.c -o terminal.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra
~/opt/cross/bin/i686-elf-gcc -c kernel.c -o kernel.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra
~/opt/cross/bin/i686-elf-gcc -T linker.ld -o myos.bin -ffreestanding -O2 -nostdlib boot.o pci.o terminal.o kernel.o  -lgcc