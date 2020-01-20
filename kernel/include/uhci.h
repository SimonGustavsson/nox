// uhci.h
#ifndef UHCI_H
#define UHCI_H

// These are the offsets to the predefined registers in IO space
// Starting att UHCI_BASEADDR
// WC = Write Clear
// R = Read
// W = Write
// RO = Read-Only
#define UHCI_CMD_OFFSET            (0x0) // 2 bytes (R/W)
#define UHCI_STATUS_OFFSET         (0x2) // 2 bytes (R/WC)
#define UHCI_INTERRUPT_OFFSET      (0x4) // 2 bytes (R/W)
#define UHCI_FRAME_NUM_OFFSET      (0x6) // 2 bytes (R/W word only)
#define UHCI_FRAME_BASEADDR_OFFSET (0x8) // 4 bytes (R/W)
#define UHCI_SOFMOD_OFFSET         (0xC) // 1 byte  (R/W)
#define UHCI_RES_OFFSET            (0xD) // 3 bytes (RO)
#define UHCI_PORTSC1_OFFSET       (0x10) // 2 bytes (R/WC word only)
#define UHCI_PORTSC2_OFFSET       (0x12) // 2 bytes (R/WC word only)
// Note: There *may* be more ports mapped after 0x12.
// This depends in the host controller

#include <stdint.h>
#include <stdbool.h>
#include "kernel.h"

// -------------------------------------------------------------------------
// Static Types
// -------------------------------------------------------------------------
enum mm_space {
    mm_space_32bit = 0,
    mm_space_below1mb = 1,
    mm_space_64bit = 2
};

enum uhci_cmd {
    uhci_cmd_runstop        = 1 << 0,
    uhci_cmd_host_reset     = 1 << 1,
    uhci_cmd_global_reset   = 1 << 2,
    uhci_cmd_global_suspend = 1 << 3,
    uhci_cmd_force_g_resume = 1 << 4,
    uhci_cmd_swdebug        = 1 << 5,
    uhci_cmd_config_flag    = 1 << 6,
    uhci_cmd_max_packet     = 1 << 7,
    uhci_cmd_loopback_test  = 1 << 8
};

enum uhci_status {
    // 16:6 - reserved
    uhci_status_halted            = 1 << 5,
    uhci_status_process_error     = 1 << 4,
    uhci_status_host_system_error = 1 << 3,
    uhci_status_resume_detected   = 1 << 2,
    uhci_status_error_interrupt   = 1 << 1,
    uhci_status_usb_interrupt     = 1 << 0
};

enum uhci_irpt {
    uhci_irpt_timeout_enable      = 1 << 0,
    uhci_irpt_resume_irpt_enable  = 1 << 1,
    uhci_irpt_oncomplete_enable   = 1 << 2,
    uhci_irpt_short_packet_enable = 1 << 3
};

enum uhci_portsc {
    // On startup: 0000_0101_1000_1010b
    // Not connected
    // Status has changed <--- really should be set, so that we will read bit 0 (on empty ports, this is 0)
    // NOT enabled
    // Enable has changed (will not be set on empty ports, as they can't be enabled)
    // Status D+ not set  \ these can't be set, as no device is attached
    // Status D- not set  /
    // Bit-7 set, always set, reserved
    // Low speed device attached
    // Port is not in reset   <--- This really shouldn't be set
    // Not suspended
    uhci_portsc_connect_status         = 1 << 0, // RO
    uhci_portsc_connect_status_changed = 1 << 1, // R/WC
    uhci_portsc_port_enabled           = 1 << 2, // R/W
    uhci_portsc_port_enabled_changed   = 1 << 3, // R/WC

    uhci_portsc_line_status_dpos       = 1 << 4, // RO
    uhci_portsc_line_status_dneg       = 1 << 5, // RO
    uhci_portsc_resume_detect          = 1 << 6, // R/W
    uhci_portsc_reserved               = 1 << 7, // Reserved
    uhci_portsc_low_speed_attached     = 1 << 8, // RO

    uhci_portsc_port_reset             = 1 << 9, // R/W
    uhci_portsc_suspend                = 1 << 12 // R/W
};

enum frame_list_ptr {
    frame_list_ptr_terminate = 1 << 0,
    frame_list_ptr_queue     = 1 << 1
};

enum td_link_ptr {
    td_link_ptr_terminate   = 1 << 0,
    td_link_ptr_queue       = 1 << 1,
    td_link_ptr_depth_first = 1 << 2,
    // Bit 3 reserved
    // 31:4 Link pointer to another TD or Queue
};

enum td_ctrl_status {
    // 10:0 Actual length

    // 15:11 reserved
    // 23:16 Status bits (16 is reserved)
    td_status_bitstuff_error    = 1 << 17,
    td_status_crc_timeout       = 1 << 18,
    td_status_nak_received      = 1 << 19,
    td_status_babble_detected   = 1 << 20,
    td_status_data_buffer_error = 1 << 21,
    td_status_stalled           = 1 << 22,
    td_status_active            = 1 << 23,

    td_ctrl_ioc          = 1 << 24,
    td_ctrl_ios          = 1 << 25, // Isochronous
    td_ctrl_lowspeed     = 1 << 26,

    // 28:27 Error numbers
    td_ctrl_3errors      = 3 << 27,
    td_ctrl_2errors      = 2 << 27,
    td_ctrl_1error       = 1 << 27,
    td_ctrl_short_packet = 1 << 29,
    // 31:30 reserved
};

enum uhci_packet_id {
    uhci_packet_id_setup = 0x2D,
    uhci_packet_id_in = 0x69,
    uhci_packet_id_out = 0xE1
};

enum td_token {
    // 7:0   Packet Identification
    // 14:8  Device Address
    // 18:15 End point
    td_token_data_toggle = 1 << 19,
    // 20    reserved
    // 31:21 Maximum Length
};

struct transfer_descriptor {
    uint32_t link_ptr;
    uint32_t ctrl_status;
    uint32_t token;
    uint32_t buffer_ptr;    // 32-bit pointer to data of this transfer
    uint32_t software_use0;
    uint32_t software_use1;
    uint32_t software_use2;
    uint32_t software_use3;
} PACKED;

struct uhci_descriptor {
    uint8_t desc_length;
    uint8_t type;
};

struct uhci_string_lang_descriptor {
    uint8_t desc_length;
    uint8_t type; // Should be 0x3 for string language descriptors
    uint16_t lang_id_0; // see LANG_ID_EN_US etc..
    uint16_t lang_id_1;
    // uint16_t lang_id_2
    // uint16_t lang_id_n.. (depending on device)
} PACKED;

struct uhci_string_descriptor {
    uint8_t desc_length;
    uint8_t type; // Should be 0x3 for string descriptors
    uint8_t first_byte; // The remaining bytes follow this
} PACKED;

#define LANG_ENGLISH (0x09)
#define LANG_REGION_US (0x01)
#define LANG_REGION_UK (0x02)
#define LANG_REGION_AUS (0x03)
#define LANG_REGION_CAN (0x04)
#define LANG_ID_EN_US 0x0409
#define LANG_ID_GER   0x407

// Bit 7 and 4:0 are reserved
#define UHCI_CONF_ATTR_SELF_POWERED (1 << 6)
#define UHCI_CONF_ATTR_REMOTE_WKUP (1 << 5)

struct configuration_descriptor {
    uint8_t length; // Size of this descriptor
    uint8_t type; // Should be 0x02 for this
    // Total length of all data, including interface and endpoint
    // descriptors that follows this in memory
    uint16_t total_length;
    uint8_t num_interfaces;
    uint8_t config_val; // Value for this config
    uint8_t config_string_index; // String descriptor index of config
    uint8_t attributes; // See UHCI_CONF_ATTR
    uint8_t max_power; // Maximum power consumption

    // Interface/Endpoint descriptors go here
};

struct interface_descriptor {
    uint8_t length; // Size of this descriptor
    uint8_t type; // 0x4
    uint8_t interface_num; // Number of the interface
    uint8_t alternate_set; // Used to select alt. setting.
    uint8_t num_endpoints; // Number of endpoints in config
    uint8_t class_code;
    uint8_t sub_class;
    uint8_t protocol;
    uint8_t interface_string_index; // String Descriptor index of interface
};

struct endpoint_descriptor {
    uint8_t length;
    uint8_t type; // 0x05
    uint8_t addr; // Address. 3:0=endpoint num, 6:4=reserved, [7] 0==out, 1==in

    // 1:0 = Transfer type (00=control,01=isochronous,10=bulk,11=interrupt)
    // 3:2 = For ISO endpoints only (00=no sync,01=async,10=adaptive,11=sync)
    // 5:4 = For ISO endpoints only (00=data,01=feedback,10=explicit feedback, 11=reserved)
    // 7:6 = Reserved
    //
    uint8_t attributes;
    uint16_t max_packet_size;
};

struct usb_device_descriptor {
    // Size of this struct (should be 18-bytes)
    uint8_t  desc_length;

    // Constant - Always 1 for device descriptors
    uint8_t  type;

    // Binary coded decimal version of USB spec this device complies with
    // e.g. v2.10 - 0x210
    uint16_t release_num;        // Version of USB spec this device complies with

    // If this field is reset to zero, each interface within a configuration specifies its own
    // class information and the various interfaces operate independently
    //
    // If this field is set to a value between 1 and FEH, the device supports different class
    // specifications on different interfaces and the interfaces may not operate independently
    uint8_t  device_class;
    uint8_t  sub_class;
    uint8_t  protocol;
    uint8_t  max_packet_size;    // Max packet size for endpoint 0 (possible values: 8, 16, 32, 64)

    // -------- END 8 first bytes (size of initial request) ----------

    uint16_t vendor_id;
    uint16_t product_id;
    uint16_t device_rel;
    uint8_t  manufacturer;       // Index of string descriptor (0 = no descriptor)
    uint8_t  product;            // Index of string descriptor (0 = no descriptor)
    uint8_t  serial_num;         // Index of string descriptor (0 = no descriptor)
    uint8_t  num_configurations; // Number of possible configurations
} PACKED;

enum usb_request_type {
    usb_request_type_device_to_host = 1 << 7,
    usb_request_type_host_to_device = 0 << 7,

    // 6:5 Type
    usb_request_type_standard       = 0 << 5,
    usb_request_type_class          = 1 << 5,
    usb_request_type_vendor         = 2 << 5,
    usb_request_type_reserved       = 3 << 5,

    // 4:0 Recipient
    usb_request_recip_device        = 0 << 0,
    usb_request_recip_interface     = 1 << 0,
    usb_request_recip_endpoint      = 2 << 0,
    usb_request_recip_other         = 3 << 0
};

#define UHCI_REQUEST_GET_STATUS        (0x00)
#define UHCI_REQUEST_CLEAR_FEATURE     (0x01)
#define UHCI_REQUEST_SET_FEATURE       (0x03)
#define UHCI_REQUEST_SET_ADDRESS       (0x05)
#define UHCI_REQUEST_GET_DESCRIPTOR    (0x06)
#define UHCI_REQUEST_SET_DESCRIPTOR    (0x07)
#define UHCI_REQUEST_GET_CONFIGURATION (0x08)
#define UHCI_REQUEST_SET_CONFIGURATION (0x09)
#define UHCI_REQUEST_GET_INTERFACE     (0x10)
#define UHCI_REQUEST_SET_INTERFACE     (0x11)
#define UHCI_REQUEST_SYNC_FRAME        (0x12)

#define DESCRIPTOR_TYPE_DEVICE    (0x01)
#define DESCRIPTOR_TYPE_CONFIG    (0x02)
#define DESCRIPTOR_TYPE_STRING    (0x03)
#define DESCRIPTOR_TYPE_INTERFACE (0x04)
#define DESCRIPTOR_TYPE_ENDPOINT  (0x05)


struct device_request_packet {
    uint8_t  type;    // See usb_request_type
    uint8_t  request; // The desired request
    uint16_t value;   // Request specific
    uint16_t index;   // Request specific
    uint16_t length;  // If a data phase, number of bytes to transfer
} PACKED;

// Address occupies everything but the low 4 bits
#define QUEUE_HEAD_LINK_ADDR_MASK (0xFFFFFFF0)

typedef enum {
    uhci_state_default,
    uhci_state_addressed
} device_state;

// uhci_device is not in the UHCI spec, this is a Nox struct
// to keep track of connected devices
struct uhci_device {
    // Device number, statically assigned by Nox
    // This will be the address we set the HC to
    uint8_t num;
    int8_t port; // which pci port it resides on
    device_state state;
    struct usb_device_descriptor* desc;
    uint16_t lang_id;
} PACKED;

struct uhci_queue {
    uint32_t head_link;
    uint32_t element_link;

    // The host controller only cares about the first two
    // uint32 in this struct, the remaining fields are added because queues
    // have to be aligned to 16-byte boundary, making it easy to line up
    // multiple queues, and it allows us to attach some useful information! :-)
    uint32_t* parent;

    // NON-standard, Nox specific. Store the first element link so that
    // we can traverse all TDs in a completed queue
    uint32_t first_element_link;
} ALIGN(16);

struct uhci_hc {
    bool active;
    bool io;
    bool interrupt_fired;
    uint32_t base_addr;
    uint8_t irq_num;
    uint32_t frame_list;
    struct uhci_queue root_queues[16];
    struct uhci_device devices[128];
};

enum nox_uhci_queue {
    nox_uhci_queue_1,        // Executed every 1ms
    nox_uhci_queue_2,        // Executed every 2ms
    nox_uhci_queue_4,        // Executed every 4ms
    nox_uhci_queue_8,        // ...etc...
    nox_uhci_queue_16,
    nox_uhci_queue_32,
    nox_uhci_queue_64,
    nox_uhci_queue_128,
    nox_uhci_queue_lowspeed,
    nox_uhci_queue_fullspeed,
    nox_uhci_queue_iso,       // Not sure what this means?
    nox_uhci_queue_reserved0, // Not used (yet)
    nox_uhci_queue_reserved1, // Not used (yet)
    nox_uhci_queue_reserved2, // Not used (yet)
    nox_uhci_queue_reserved3, // Not used (yet)
    nox_uhci_queue_reserved4, // Not used (yet)
};

struct ctrl_transfer_data {
    struct uhci_queue queue;
    struct transfer_descriptor setup;

    uint32_t num_in_descriptors;
    uint8_t padding[8]; // Packing to align the request :)
    struct transfer_descriptor* request;

    struct transfer_descriptor ack;
    struct device_request_packet request_packet;
    uint32_t response_buffer_size;
    uint8_t* response_buffer;
};

struct uhci_set_addr_data {
    struct uhci_queue queue;
    struct transfer_descriptor setup;
    struct transfer_descriptor ack;
    struct device_request_packet request;
};

#define UHCI_GET_DEVICE_DESCRIPTOR_REQUEST_SIZE (0x8)

int32_t uhci_detect_root(uint16_t base_addr, bool io_addr);
void uhci_init(uint32_t base_addr, pci_device* dev, struct pci_address* addr, uint8_t irq);
void uhci_command(char** args, size_t arg_count);
void handle_td(struct uhci_device* device, struct transfer_descriptor* td);
void schedule_queue_insert(struct uhci_hc* hc, struct uhci_queue* queue, enum nox_uhci_queue root);

#endif
