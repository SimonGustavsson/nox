rm myos.bin
./build.sh
qemu-system-i386 -kernel myos.bin -usb