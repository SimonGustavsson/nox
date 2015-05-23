#ifndef NOX_ATA_H
#define NOX_ATA_H

#define ATA_BASE_PORT_PRIMARY 0x1F0
#define ATA_BASE_PORT_OFFSET 0x170

#define ATA_CONTROL_REG_PORT 0x3F8
#define ATA_CONTROL_ALTERNATIVE_REG_PORT 0x376

enum ata_port_offsets {
    ata_port_data_offset = 0,
    ata_port_feat_err_offset = 1,
    ata_port_sector_count_offset = 2,
    ata_port_lba_low_offset = 3,
    ata_port_lba_mid_offset = 4,
    ata_port_lba_high_offset = 5,
    ata_port_drive_head_offset = 6,
    ata_port_cmd_status_offset = 7
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
    ata_status_error       = (0 << 0)
};

#endif

