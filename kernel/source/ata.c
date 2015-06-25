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

enum ready_result {
    ready_result_ready,
    ready_result_df,
    ready_result_err,
};

static void wait_400ns(enum ata_controller controller)
{
    // Each IO port read takes 100ns, so to get a 400ns
    // delay (needed by the ATA hardware at various points)
    // - just read the status port 4 times.
    ata_read(controller, ata_register_cmd_status);
    ata_read(controller, ata_register_cmd_status);
    ata_read(controller, ata_register_cmd_status);
    ata_read(controller, ata_register_cmd_status);
}

static enum ready_result wait_until_ready(enum ata_controller controller)
{
    uint8_t status;

    for (;;) {
        status = ata_read(controller, ata_register_cmd_status);

        // We're not really sure about this yet, the info we've read so
        // far is ambiguous about whether the busy flag is cleared when
        // there is an error, so just check the error conditions.
        if ((status & ata_status_error) == ata_status_error) {
            return ready_result_err;
        }

        if ((status & ata_status_df) == ata_status_df) {
            return ready_result_df;
        }

        if ((status & ata_status_busy) == ata_status_busy) {
            continue;
        }

        if ((status & ata_status_drq) != ata_status_drq) {
            continue;
        }

        return ready_result_ready;
    }
}

// sector_count of 0 means 256 sectors, 1-255 mean what they say
bool ata_read_sectors(uint32_t lba, uint8_t sector_count, uintptr_t buffer)
{
    uint16_t* data = (uint16_t*)(buffer);

    enum ata_drive drive = ata_drive_master;
    // Structure of the drive_head register as it pertains to LBA is
    // 7   6   5   4    |    3  2   1   0
    // 1  LBA  1 Drive  | High 4 Bits of LBA
    ata_write(ata_controller_primary, ata_register_drive_head, 0xE0 | (drive << 4) | ((lba >> 24) & 0x0F));

    //  Send a NULL byte to port 0x1F1, if you like (it is ignored and wastes lots of CPU time): outb(0x1F1, 0x00)
    ata_write(ata_controller_primary, ata_register_feat_err,  0x00);

    ata_write(ata_controller_primary, ata_register_sector_count, sector_count);

    ata_write(ata_controller_primary, ata_register_lba_low, (uint8_t)(lba));
    ata_write(ata_controller_primary, ata_register_lba_mid, (uint8_t)(lba >> 8));
    ata_write(ata_controller_primary, ata_register_lba_high, (uint8_t)(lba >> 16));

    ata_write(ata_controller_primary, ata_register_cmd_status, ata_cmd_read_sectors);

    for (int sector = 0; sector < sector_count; sector++) {

        // Wait for the sector to be read by the controller
        if (ready_result_ready != wait_until_ready(ata_controller_primary)) {
            KERROR("Polling ATA Status returned an error condition");
            return false;
        }

        // Read the 512 bytes comprising the sector data
        for(int i = 0; i < 256; i++) {
            *data++ = ata_read_data(ata_controller_primary);
        }
    }

    wait_400ns(ata_controller_primary);

    return true;
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

        typedef unsigned char byte_t;

        byte_t* bytes = (byte_t*)data;

        uint32_t* number_of_sectors_lba28 = (uint32_t*)&bytes[120];

        terminal_write_string("ATA Done using LBA28. Sector Count is ");
        terminal_write_uint32(*number_of_sectors_lba28);
        terminal_write_string("\n");
    }
}

