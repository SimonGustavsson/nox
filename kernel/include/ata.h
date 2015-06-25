#ifndef NOX_ATA_H
#define NOX_ATA_H

#define ATA_CONTROL_REG_PORT 0x3F8
#define ATA_CONTROL_ALTERNATIVE_REG_PORT 0x376

// NOTE: The values for these are chosen to be
//       the base port numbers used to access the
//       various registers for each controller
enum ata_controller {
    ata_controller_primary      = 0x1F0,
    ata_controller_secondary    = 0x170
};

enum ata_register {
    ata_register_data            = 0,
    ata_register_feat_err        = 1,
    ata_register_sector_count    = 2,
    ata_register_lba_low         = 3,
    ata_register_lba_mid         = 4,
    ata_register_lba_high        = 5,
    ata_register_drive_head = 6,
    ata_register_cmd_status = 7
};

enum control_reg_byte {
    NIEN = (1 << 1),
    SRST = (1 << 2),
    HOB  = (1 << 7)
};

enum ata_status_byte {
    ata_status_busy        = (1 << 7),
    ata_status_ready       = (1 << 6),
    ata_status_df          = (1 << 5),
    ata_status_srv         = (1 << 4),
    ata_status_drq         = (1 << 3),
    ata_status_error       = (1 << 0)
};

// NOTE: The values for master and slave
//       are specifically chosen to match the
//       values used in the drive/head register
enum ata_drive {
    ata_drive_unknown  = -1,
    ata_drive_master   = 0,
    ata_drive_slave    = 1
};

enum ata_cmd {
    ata_cmd_read_sectors = 0x20
};

void ata_init();
bool ata_read_sectors(uint32_t lba, uint8_t block_count, uintptr_t buffer);

#endif

