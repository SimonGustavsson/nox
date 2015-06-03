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

        int foo = data[0] + 5;
        terminal_write_uint32(foo);
    }
}

