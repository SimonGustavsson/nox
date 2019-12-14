#include <types.h>
#include <screen.h>
#include <terminal.h>
#include "pci.h"
#include "uhci.h"
#include "pio.h"
#include <pci.h>
#include <kernel.h>
#include <interrupt.h>
#include <pit.h>
#include <mem_mgr.h>
#include <string.h>
#include <cli.h>
#include <pic.h>
#include <debug.h>

struct transfer_descriptor* init_td(struct uhci_device* device,
                                    uintptr_t td_mem,
                                    uint8_t packet_id,
                                    uint32_t link,
                                    uint8_t* buf);
void handle_td_default(struct uhci_device* device, struct transfer_descriptor* td);
void handle_td_addressed(struct uhci_device* device, struct transfer_descriptor* td);
static void uhci_device_get_descriptor(struct uhci_device* device);

void try_init(struct uhci_device* device)
{
    uhci_device_get_descriptor(device);
}

#define TD_TOKEN_SIZE(bytes) ((bytes - 1) << 21)

static void uhci_device_get_descriptor(struct uhci_device* device)
{
    // This is a bit of a strange one. As far as I can tell,
    // you don't address the GET_DESCRIPTOR to any particular device
    // you set the address to '0' (valid addresses are 1-127),
    // this means the HC will broadcast the message and the first device
    // that's in the "default" state will respond to it.
    //
    // So we *should* be able to send GET_DESCRIPTOR to get the
    // descriptor, assign a device number to the device that repsonded
    // and keep doing this for each of the connected devices.
    terminal_write_string("Running GET_DESCRIPTOR for device 0\n");

    // Step 0: Allocate memory for our stuff
    uint32_t q_mem = (uint32_t)(uintptr_t)mem_page_get();
    struct uhci_queue* queue = (struct uhci_queue*)(uintptr_t)q_mem;

    // Zero out page (Page size = 4096 bytes = 1024 uint_32s)
    uint32_t* tmp = (uint32_t*)q_mem;
    for (int i = 0; i < 1024; i++)
    {
        *(tmp + i) = 0;
    }

    uintptr_t td0_mem = (uintptr_t)q_mem + (sizeof(struct uhci_queue) * 1);
    uintptr_t td1_mem = (uintptr_t)((uint32_t)td0_mem)  + sizeof(struct transfer_descriptor);
    uintptr_t td2_mem = (uintptr_t)((uint32_t)td1_mem)  + sizeof(struct transfer_descriptor);
    uint8_t* request_packet_mem = (uint8_t*)((uint32_t)td2_mem) + sizeof(struct transfer_descriptor);
    uint8_t* return_data_mem = (uint8_t*)(request_packet_mem + 0x10); // data is 8 bytes, but align it

    // Step 2: Initial SETUP packet
    struct transfer_descriptor* td0 = init_td(device,
                                              td0_mem,
                                              uhci_packet_id_setup,
                                              ((uint32_t)td1_mem) | (td_link_ptr_depth_first),
                                              request_packet_mem);
    if (td0 == NULL) {
        KERROR("This will never happen, just avoid 'unused' error");
    }

    // Step 3: Follow-up IN packet to read data from the device
    struct transfer_descriptor* td1 = init_td(device,
                                              td1_mem,
                                              uhci_packet_id_in,
                                              (uint32_t)td2_mem | td_link_ptr_depth_first,
                                              return_data_mem);
    td1->token |= td_token_data_toggle;

    // Step 4: The OUT packet to acknowledge transfer
    struct transfer_descriptor* td2 = init_td(device,
                                              td2_mem,
                                              uhci_packet_id_out,
                                              td_link_ptr_terminate,
                                              (uint8_t*)0);
    td2->token |= TD_TOKEN_SIZE(0x7FF) | td_token_data_toggle;

    // Step 5: Setup queue to point to first TD
    queue->element_link = ((uint32_t)td0_mem);
    queue->head_link = td_link_ptr_terminate;

    // Step 6: initialize data packet to send
    struct device_request_packet* data = (struct device_request_packet*) (uint8_t*)request_packet_mem;
    data->type = usb_request_type_standard | usb_request_type_device_to_host;
    data->request = 0x06; // GET_DESCRIPTOR

    // High byte = 0x01 for DEVICE. Low byte = 0. The two bytes are written
    // as a 16-bit word in little-endian
    data->value = 0x1 << 8; // Device
    // We only read 8 bytes as right now we don't know how big transfers this device
    // supports. So start off with 8 as all devices support that, we'll adjust this later
    data->length = 0x8;

    // Step 7: Add queue too root queue to schedule for execution
    //         we'll have to wait for an interrupt now
    schedule_queue_insert(queue, nox_uhci_queue_1);
}

struct transfer_descriptor* init_td(struct uhci_device* device, uintptr_t td_mem, uint8_t packet_id, uint32_t link, uint8_t* buf)
{
    struct transfer_descriptor* td = (struct transfer_descriptor*)td_mem;

    uint8_t max_packet_size =
        device->descriptor == NULL ?
        8 :
        device->descriptor->max_packet_size;

    td->token = (TD_TOKEN_SIZE(max_packet_size) | packet_id);
    td->link_ptr = td_link_ptr_terminate;
    td->ctrl_status = td_ctrl_3errors | td_status_active | td_ctrl_lowspeed;
    td->buffer_ptr = (uint32_t)buf;

    return td;
}

void uhci_dev_handle_td(struct uhci_device* device, struct transfer_descriptor* td)
{
    switch (device->state) {
        case uhci_state_default:
            handle_td_default(device, td);
            break;
        case uhci_state_addressed:
            handle_td_addressed(device, td);
            break;
        default:
            KERROR("Unexpted UHCI Device state");
    }
}

void handle_td_default(struct uhci_device* device, struct transfer_descriptor* td)
{
    printf("Device %d handling TD with status '%h'", device->num, td->ctrl_status);
}

void handle_td_addressed(struct uhci_device* device, struct transfer_descriptor* td)
{
    KINFO("handle_td_addressed");
}

