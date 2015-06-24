#include <types.h>
#include <pio.h>
#include <kernel.h>
#include <debug.h>
#include <ata.h>
#include <terminal.h>

//static enum ata_drive primary_drive = ata_drive_unknown;
//static enum ata_drive secondary_drive = ata_drive_unknown;
void select_drive(enum ata_controller controller, enum ata_drive drive);

uint8_t ata_read(enum ata_controller controller, enum ata_register port)
{
    return INB(controller + port);
}

uint16_t ata_read_data(enum ata_controller controller)
{
    return INW(controller + ata_register_data);
}

void ata_write(enum ata_controller controller, enum ata_register port, uint8_t value)
{
    OUTB(controller + port, value);
}

// Note: a block count of 0 = 256 sectors
void ata_read_sectors(uint32_t lba, uint8_t block_count, uintptr_t* buffer)
{
// An example of a 28 bit LBA PIO mode read on the Primary bus:
//
//  Send 0xE0 for the "master" or 0xF0 for the "slave", ORed with the highest 4 bits of the LBA to port 0x1F6:
    enum ata_drive drive = ata_drive_master;
    // Structure of the drive_head register as it pertains to LBA is
    // 7   6   5   4    |    3  2   1   0
    // 1  LBA  1 Drive  | High 4 Bits of LBA
    ata_write(ata_controller_primary, ata_register_drive_head, 0xE0 | (drive << 4) | ((lba >> 24) & 0x0F));
//  Send a NULL byte to port 0x1F1, if you like (it is ignored and wastes lots of CPU time): outb(0x1F1, 0x00)
//  Send the sectorcount to port 0x1F2: outb(0x1F2, (unsigned char) count)
//  Send the low 8 bits of the LBA to port 0x1F3: outb(0x1F3, (unsigned char) LBA))
//  Send the next 8 bits of the LBA to port 0x1F4: outb(0x1F4, (unsigned char)(LBA >> 8))
//  Send the next 8 bits of the LBA to port 0x1F5: outb(0x1F5, (unsigned char)(LBA >> 16))
//  Send the "READ SECTORS" command (0x20) to port 0x1F7: outb(0x1F7, 0x20)
//  Wait for an IRQ or poll.
//  Transfer 256 16-bit values, a uint16_t at a time, into your buffer from I/O port 0x1F0. (In assembler, REP INSW works well for this.)
//  Then loop back to waiting for the next IRQ (or poll again -- see next note) for each successive sector.
}

void ata_init() {
    select_drive(ata_controller_primary, ata_drive_master);
}

void select_drive(enum ata_controller controller, enum ata_drive drive)
{
    ata_write(controller, ata_register_drive_head,
            1 << 7 | // Reserved
            1 << 6 | // Enable LBA
            1 << 5 | // Reserved
            drive << 4);

    ata_write(controller, ata_register_lba_low, 0);
    ata_write(controller, ata_register_lba_mid, 0);
    ata_write(controller, ata_register_lba_high, 0);

    // Send IDENTIFY
    ata_write(controller, ata_register_cmd_status, 0xEC);

    if(ata_read(controller, ata_register_cmd_status) == 0) {
        // Drive doesn't exist
        KWARN("DRIVE DOESNT EXIST");
    }
    else {
        // Poll the status register until bit 7 is clear
        while((ata_read(controller, ata_register_cmd_status) & 0x80) != 0) {
            // Just waiting for the controller...
        }

        if(ata_read(controller, ata_register_lba_mid) != 0 ||
           ata_read(controller, ata_register_lba_high) != 0)
        {
            // Not an ATA drive LOL
            KWARN("NOT ATA!");
            return;
        }

        while((ata_read(controller, ata_register_cmd_status) & ata_status_drq) == 0) {
            // Wait for data transfer request to complete
        }

        uint8_t err = ata_read(controller, ata_register_feat_err);
        if(err != 0) {
            // Some error doing something, TODO: Interpret this
            return;
        }

        // READY... Set.... GO!

        uint16_t data[256];
        for(int i = 0; i < 256; i++) {
            data[i] = ata_read_data(controller);
        }
        KINFO("SUCCESS!");

        typedef unsigned char byte_t;

        byte_t* bytes = (byte_t*)data;

        uint32_t* number_of_sectors_lba28 = (uint32_t*)&bytes[120];

        terminal_write_string("LBA28 Sector Count: ");
        terminal_write_uint32(*number_of_sectors_lba28);
        terminal_write_string("\n");

        int foo = data[0] + 5;
        terminal_write_uint32(foo);
    }
}

