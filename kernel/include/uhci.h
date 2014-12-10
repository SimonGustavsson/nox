// uhci.h

// These are the offsets to the predefined registers in IO space
// Starting att UHCI_BASEADDR
#define UHCI_CMD_OFFSET (0x0)        // 2 bytes
#define UHCI_STS_OFFSET (0x2)        // 2 bytes
#define UHCI_INTR_OFFSET (0x4)       // 2 bytes
#define UHCI_FRNUM_OFFSET (0x6)      // 2 bytes
#define UHCI_FRBASEADDR_OFFSET (0x8) // 4 bytes
#define UHCI_SOFMOD_OFFSET (0xC)     // 1 byte
#define UHCI_PORTSC1_OFFSET (0x10)   // 2 bytes
#define UHCI_PORTSC2_OFFSET (0x12)   // 2 bytes


// UHCI Command register
//15:8 - Reserved
//   7 - Max Packet (MAXP). 1 = 64 bytes, 0 = 32 bytes
//   6 - Configure Flag (CF)
//   5 - Software Debug (SWDBG). 1 = debug mode, 0 = normal mode
//   4 - Force Global Resume (FGR)
//   3 - Enter Global Suspend Mode (EGSM)
//   2 - Global Reset (GRESET)
//   1 - Host Controller Reset (HCRESET)
//   0 - Run/Stop (RS). 1 = run, 0 = stop

// UHCI Status Register
// 15:6 - Reserved
//    5 - Controller Halted
//    4 - Host Controller Process Error
//    3 - Host System Error
//    2 - Resume Detect
//    1 - USB Error Interrupt
//    0 - USB Interrupt (USBINT)

// UHCI Interrupt Enable Register
// 15:4 - Reserved
//    3 - Short Packet Interupt enabled
//    2 - Interrupt On Complete (IOC) Enable
//    1 - Resume Interupt Enable
//    0 - Timeout / CRC Interrupt Enable

// UHCI Frame Number Regsiter
// 15:11 - Reserved
// 10:0  - Frame List Current Index/Frame Number

// UHCI Frame Base Register
// 31:12 - Base Address
// 11:0  - Reserved. Write Zero (Making Base Addr 4K aligned)

// UHCI Start of Frame Register
//   7 - Reserved
// 6:0 - Start Of Frame Timing Value

// UHCI Port Status / Control
// 15:13 - Reserved
//    12 - Suspend (R/W). 1 = suspended, 0 = normal
// 11:10 - Reserved
//     9 - Port Reset (R/W). 1 = Port is in reset
//     8 - Low speed device attached. 1 = Low speed device attached, 0 = full speed device attached
//     7 - Reserved. Read as 1
//     6 - Resume detect (R/W). 1 = Resume detected on port
//   5:4 - Line status (RO)
//     3 - Port Enable/Disable change (R/WC). 1 = Port Enable/Disable has changed
//     2 - Port Enable/Disable (R/W). 1 = Enable port, 0 = Disable port
//     1 - Connect status change (R/WC), 1 = Change in current connect status.
//     0 - Current connect status (RO). 1 = Device is present

#ifndef UHCI_H
#define UHCI_H

#include <stdint.h>
#include <stdbool.h>

#define wait(x) // x is MS

int32_t uhci_detectRoot(uint16_t baseAddr, bool ioAddr);

#endif
