void cpu_reset()
{
    __asm("mov $0xFE, %al;"
          "out %al, $0x64;");
}
