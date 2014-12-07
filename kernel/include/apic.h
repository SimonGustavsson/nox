#define APIC_MEM_BASE              0xFEE00000
#define APIC_REG_ID_OFFSET         0x020 // Local APIC ID Register Read/Write.
#define APIC_REG_VERSION_OFFSET    0x030 // Local APIC Version Register Read Only.
#define APIC_REG_TPR_OFFSET        0x080 // Task Priority Register (TPR) Read/Write.
#define APIC_REG_APR_OFFSET        0x090 // Arbitration Priority Register (1*) (APR) Read Only.
#define APIC_REG_PPR_OFFSET        0x0A0 // Processor Priority Register (PPR) Read Only.
#define APIC_REG_EOI_OFFSET        0x0B0 // EOI Register Write Only.
#define APIC_REG_RRD_OFFSET        0x0C0 // Remote Read Register (1*) (RRD) Read Only
#define APIC_REG_LDR_OFFSET        0x0D0 // Logical Destination Register Read/Write.
#define APIC_REG_DFR_OFFSET        0x0E0 // Destination Format Register Read/Write (see Section 10.6.2.2).
#define APIC_REG_SIVR_OFFSET       0x0F0 // Spurious Interrupt Vector Register Read/Write (see Section 10.9.
#define APIC_REG_ISR0_OFFSET       0x100 // In-Service Register (ISR); bits 31:0 Read Only.
#define APIC_REG_ISR1_OFFSET       0x110 // In-Service Register (ISR); bits 63:32 Read Only.
#define APIC_REG_ISR2_OFFSET       0x120 // In-Service Register (ISR); bits 95:64 Read Only.
#define APIC_REG_ISR3_OFFSET       0x130 // In-Service Register (ISR); bits 127:96 Read Only.
#define APIC_REG_ISR4_OFFSET       0x140 // In-Service Register (ISR); bits 159:128 Read Only.
#define APIC_REG_ISR5_OFFSET       0x150 // In-Service Register (ISR); bits 191:160 Read Only.
#define APIC_REG_ISR6_OFFSET       0x160 // In-Service Register (ISR); bits 223:192 Read Only.
#define APIC_REG_ISR7_OFFSET       0x170 // In-Service Register (ISR); bits 255:224 Read Only.
#define APIC_REG_TMR0_OFFSET       0x180 // Trigger Mode Register (TMR); bits 31:0 Read Only.
#define APIC_REG_TMR1_OFFSET       0x190 // Trigger Mode Register (TMR); bits 63:32 Read Only.
#define APIC_REG_TMR2_OFFSET       0x1A0 // Trigger Mode Register (TMR); bits 95:64 Read Only.
#define APIC_REG_TMR3_OFFSET       0x1B0 // Trigger Mode Register (TMR); bits 127:96 Read Only.
#define APIC_REG_TMR4_OFFSET       0x1C0 // Trigger Mode Register (TMR); bits 159:128 Read Only.
#define APIC_REG_TMR5_OFFSET       0x1D0 // Trigger Mode Register (TMR); bits 191:160 Read Only.
#define APIC_REG_TMR6_OFFSET       0x1E0 // Trigger Mode Register (TMR); bits 223:192 Read Only.
#define APIC_REG_TMR7_OFFSET       0x1F0 // Trigger Mode Register (TMR); bits 255:224 Read Only.
#define APIC_REG_IRR0_OFFSET       0x200 // Interrupt Request Register (IRR); bits 31:0 Read Only.
#define APIC_REG_IRR1_OFFSET       0x210 // Interrupt Request Register (IRR); bits 63:32 Read Only.
#define APIC_REG_IRR2_OFFSET       0x220 // Interrupt Request Register (IRR); bits 95:64 Read Only.
#define APIC_REG_IRR3_OFFSET       0x230 // Interrupt Request Register (IRR); bits 127:96 Read Only.
#define APIC_REG_IRR4_OFFSET       0x240 // Interrupt Request Register (IRR); bits 159:128 Read Only.
#define APIC_REG_IRR5_OFFSET       0x250 // Interrupt Request Register (IRR); bits 191:160 Read Only.
#define APIC_REG_IRR6_OFFSET       0x260 // Interrupt Request Register (IRR); bits 223:192 Read Only.
#define APIC_REG_IRR7_OFFSET       0x270 // Interrupt Request Register (IRR); bits 255:224 Read Only.
#define APIC_REG_ESR_OFFSET        0x280 // Error Status Register Read Only.
// 0x290 through 0x2E0  is Reserved
#define APIC_REG_LVT_CMCI_OFFSET   0x2F0 // LVT CMCI Register Read/Write.
#define APIC_REG_ICR0_OFFSET       0x300 // Interrupt Command Register (ICR); bits 0-31 Read/Write.
#define APIC_REG_ICR1_OFFSET       0x310 // Interrupt Command Register (ICR); bits 32-63 Read/Write.
#define APIC_REG_LVT_TIMER_OFFSET  0x320 // LVT Timer Register Read/Write.
#define APIC_REG_LVT_THER_OFFSET   0x330 // LVT Thermal Sensor Register (2*) Read/Write.
#define APIC_REG_LVT_PERF_OFFSET   0x340 // LVT Performance Monitoring Counters Register (3*) Read/Write.
#define APIC_REG_LVT_LINT0_OFFSET  0x350 // LVT LINT0 Register Read/Write.
#define APIC_REG_LVT_LINT1_OFFSET  0x360 // LVT LINT1 Register Read/Write.
#define APIC_REG_LVT_ERR_OFFSET    0x370 // LVT Error Register Read/Write.
#define APIC_REG_TMR_ICR_OFFSET    0x380 // Initial Count Register (for Timer) Read/Write.
#define APIC_REG_TMR_CCR_OFFSET    0x390 // Current Count Register (for Timer) Read Only.
// 0x3A0 through 0x3D0 is reserved
#define APIC_REG_TMR_DCR_OFFSET    0x3E0 // Divide Configuration Register (for Timer) Read/Write.
// 0x3F0 is reserved

/*
    Notes:

    1* - Not supported in the Pentium 4 and Intel Xeon processors. 
         The Illegal Register Access bit (7) of the ESR will not be set when writing
         to these registers.

    2* - Introduced in the Pentium 4 and Intel Xeon processors. 
         This APIC register and its associated function are implementation dependent
         and may not be present in future IA-32 or Intel 64 processors.

    3* - Introduced in the Pentium Pro processor. 
         This APIC register and its associated function are implementation dependent and may not
         be present in future IA-32 or Intel 64 processors.
*/