#include <types.h>
#include <screen.h>
#include <terminal.h>
#include "pci.h"
#include "uhci.h"
#include "uhci_device.h"
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

#define USB_DEBUG

// -------------------------------------------------------------------------
// Documentation
// -------------------------------------------------------------------------

/* Note: All address in the UHCI driver assumes 32-bit addresses
 *       This is intentional and can *not* be changed, as UHCI only
 *       supports 32-bit addresses
*/

// -------------------------------------------------------------------------
// Static Defines
// -------------------------------------------------------------------------
// TODO: Allocate mem for frame stack
#define IS_TERMINATE(link) ((((uint32_t)(uintptr_t)link) & td_link_ptr_terminate) == td_link_ptr_terminate)
#define GET_LINK_QUEUE(link) ((struct uhci_queue*) (uintptr_t) ((uint32_t)(uintptr_t)link & QUEUE_HEAD_LINK_ADDR_MASK))
#define QUEUE_PTR_CREATE(queue_ptr) (uint32_t)(uintptr_t) ((uint32_t)(uintptr_t)queue_ptr | td_link_ptr_queue);


// -------------------------------------------------------------------------
// Global Variables
// -------------------------------------------------------------------------
uint8_t g_irq_num;

uint32_t g_initialized_base_addr;
uint32_t g_foo[sizeof(struct uhci_queue)];
uint32_t g_frame_list;

struct uhci_queue* g_setup_queue;
struct transfer_descriptor* g_setup_td0;
struct transfer_descriptor* g_setup_td1;
struct transfer_descriptor* g_setup_td2;
struct device_request_packet* g_get_desc_data;
uint32_t g_setup_return_data_mem;

struct transfer_descriptor* g_td0_sized;
struct transfer_descriptor* g_td1_sized;
struct transfer_descriptor* g_td2_sized;
uintptr_t g_ret_mem_sized;
bool g_is_size_request_in_flight;

// Note: The head_link gets set up in a function, because, constant expressions SUCK
struct uhci_queue g_root_queues[16] ALIGN(16) = {
    /*   1ms Queue */ {td_link_ptr_terminate, td_link_ptr_terminate, NULL, 0},
    /*   2ms Queue */ {0, td_link_ptr_terminate, NULL, 0},
    /*   4ms Queue */ {0, td_link_ptr_terminate, NULL, 0},
    /*   8ms Queue */ {0, td_link_ptr_terminate, NULL, 0},
    /*  16ms Queue */ {0, td_link_ptr_terminate, NULL, 0},
    /*  32ms Queue */ {0, td_link_ptr_terminate, NULL, 0},
    /*  64ms Queue */ {0, td_link_ptr_terminate, NULL, 0},
    /* 128ms Queue */ {td_link_ptr_terminate, td_link_ptr_terminate, NULL, 0},
    /*  Low speed  */ {td_link_ptr_terminate, td_link_ptr_terminate, NULL, 0},
    /*  Full speed */ {td_link_ptr_terminate, td_link_ptr_terminate, NULL, 0},
    /*     ISO     */ {td_link_ptr_terminate, td_link_ptr_terminate, NULL, 0},
    /*  Reserved0  */ {td_link_ptr_terminate, td_link_ptr_terminate, NULL, 0},
    /*  Reserved1  */ {td_link_ptr_terminate, td_link_ptr_terminate, NULL, 0},
    /*  Reserved2  */ {td_link_ptr_terminate, td_link_ptr_terminate, NULL, 0},
    /*  Reserved3  */ {td_link_ptr_terminate, td_link_ptr_terminate, NULL, 0},
    /*  Reserved4  */ {td_link_ptr_terminate, td_link_ptr_terminate, NULL, 0},
};
#define UHCI_MAX_DEVICES 2

struct uhci_device g_devices[UHCI_MAX_DEVICES] = {
    { 0, uhci_state_default, NULL },
    { 1, uhci_state_default, NULL },
};

// -------------------------------------------------------------------------
// Forward Declarations
// -------------------------------------------------------------------------
static void handle_fl_complete();
static uint16_t get_cur_fp_index();
static void enqueue_get_descriptor();
static void enqueue_get_descriptor_with_size(uint8_t size);
static bool is_set(uint32_t val, uint32_t num);
static bool init_io(uint32_t base_addr, uint8_t irq);
static bool init_mm(pci_device* dev, uint32_t base_addr, uint8_t irq);
static bool init_mm32(uint32_t base_addr, uint8_t irq, bool below_1mb);
static bool init_mm64(uint64_t base_addr, uint8_t irq);
#ifdef USB_DEBUG
static void print_init_info(struct pci_address* addr, uint32_t base_addr);
#endif
static int32_t detect_root(uint16_t base_addr, bool memory_mapped);
static void setup(uint32_t base_addr, uint8_t irq, bool memory_mapped);
static void uhci_irq(uint8_t irq, struct irq_regs* regs);
static bool enable_port(uint32_t base_addr, uint8_t port);
static uint32_t port_count_get(uint32_t base_addr);
static void schedule_init(uint32_t frame_list_addr);
static void schedule_queue_remove(struct uhci_queue* queue);
// static void poll_status_interrupt(); // Not used (yet?)
static void print_frame_list_entry(uint32_t* entry);
static void print_td(struct transfer_descriptor* td);
static void initialize_root_queues();
static void start_schedule(uint16_t base_addr);

// -------------------------------------------------------------------------
// Public Contract
// -------------------------------------------------------------------------
static void print_status_register(uint32_t base_addr);
static void print_status_register2()
{
    if(g_initialized_base_addr == NULL) {
        KERROR("THe UHCI has not been initialized yet!");
        return;
    }

    print_status_register(g_initialized_base_addr);
}

static void print_status_register(uint32_t base_addr)
{
    uint16_t status = INW(base_addr + UHCI_STATUS_OFFSET);
    terminal_write_string("UHCI Status Register. Base: ");
    terminal_write_uint32_x(base_addr);
    terminal_write_string(". Value: ");
    terminal_write_uint16_x(status);
    terminal_write_char('\n');

    if(is_set(status, uhci_status_halted))            KINFO("Halted - Yes");
    if(is_set(status, uhci_status_process_error))     KERROR("Host Controller Process Error - Yes");
    if(is_set(status, uhci_status_host_system_error)) KERROR("Host System Error - Yes");
    if(is_set(status, uhci_status_resume_detected))   KINFO("Resume Detected - Yes");
    if(is_set(status, uhci_status_error_interrupt))   KERROR("Error Interrupt - Yes");

    if ( (status & ((uint16_t)uhci_status_error_interrupt)) == uhci_status_error_interrupt)   KERROR("Error Interrupt - Yes");

    if((status & uhci_status_usb_interrupt) == uhci_status_usb_interrupt)
    {
        KINFO("USB interrupt - Yes");

        if( *((uint32_t*)g_setup_return_data_mem) != 0)
        {
            struct usb_device_descriptor* descriptor = (struct usb_device_descriptor*)( (uint32_t*)g_setup_return_data_mem);

            KINFO("GET_DESCRIPTOR response");
            SHOWVAL_U8("Descriptor size: ", descriptor->desc_length);
            SHOWVAL_U8("Type: ", descriptor->type);
            SHOWVAL_U16("USB Ver: ", descriptor->release_num);
            SHOWVAL_U8("Device Class:", descriptor->device_class);
            SHOWVAL_U8("Sub Class:", descriptor->sub_class);
        }
    }
}

void print_queue_type(enum nox_uhci_queue type)
{
    switch(type) {
        case nox_uhci_queue_1: terminal_write_string("1ms");   return;
        case nox_uhci_queue_2: terminal_write_string("2ms");   return;
        case nox_uhci_queue_4: terminal_write_string("4ms");   return;
        case nox_uhci_queue_8: terminal_write_string("8ms");   return;
        case nox_uhci_queue_16: terminal_write_string("16ms");  return;
        case nox_uhci_queue_32: terminal_write_string("32ms");  return;
        case nox_uhci_queue_64: terminal_write_string("64ms");  return;
        case nox_uhci_queue_128: terminal_write_string("128ms"); return;
        default: terminal_write_string("Unknown"); return;
    }
}

void uhci_command(char** args, size_t arg_count)
{
    if(kstrcmp(args[1], "fl")) {
        SHOWVAL_U32("Dumping 20 first frames from list at: ", g_frame_list);

        uint32_t* fl = (uint32_t*)(uintptr_t)(g_frame_list);
        for(size_t i = 0; i < 20; i++) {
            print_frame_list_entry(fl++);
        }
    }
    else if(kstrcmp(args[1], "status")) {
       print_status_register2();
    }
    else if(kstrcmp(args[1], "info")) {

        if(g_initialized_base_addr == 0) {
            KERROR("Unable to retrieve info for uninitialized host controller");
            return;
        }

        // long names, are too long!
        uint32_t ba = g_initialized_base_addr;

        uint16_t cmd = INW(ba + UHCI_CMD_OFFSET);
        SHOWVAL_U32("Command Register: ", cmd);
        uint16_t status = INW(ba + UHCI_STATUS_OFFSET);
        SHOWVAL_U32("Status Register: ", status);
        uint16_t irpt = INW(ba + UHCI_STATUS_OFFSET);
        SHOWVAL_U32("Interrupt Register: ", irpt);
        uint16_t frnum = INW(ba + UHCI_FRAME_NUM_OFFSET);
        SHOWVAL_U32("Frame Number Register: ", frnum);
        uint32_t frbase = IND(ba + UHCI_FRAME_BASEADDR_OFFSET);
        SHOWVAL_U32("Frame Base Register: ", frbase);
        uint8_t sofmod = INB(ba + UHCI_SOFMOD_OFFSET);
        SHOWVAL_U32("Sofmod Register: ", sofmod);
        uint16_t portsc1 = INW(ba + UHCI_PORTSC1_OFFSET);
        SHOWVAL_U32("Port Status/Command 1 Register: ", portsc1);
        uint16_t portsc2 = INW(ba + UHCI_PORTSC2_OFFSET);
        SHOWVAL_U32("Port Status/Command 2 Register: ", portsc2);
    }
    else if(kstrcmp(args[1], "fle")) {
        if(arg_count < 3) {
            KWARN("But which frame list entry?");
            return;
        }

        uint32_t* flptr = (uint32_t*)(uintptr_t)g_frame_list;

        uint32_t entry_index = uatoi(args[2]);
        if(entry_index > 1023) {
            KWARN("Cmon, you know there are only 1024 entries!");
            return;
        }

        terminal_write_string("Frame List Entry [");
        terminal_write_uint32(entry_index);
        terminal_write_string("]: ");
        print_frame_list_entry(&flptr[entry_index]);
        terminal_write_char('\n');
    }
    else if(kstrcmp(args[1], "insert")) {
        enqueue_get_descriptor();
    }
}

static void enqueue_get_descriptor()
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
    g_setup_queue = queue;

    // Zero out page (Page size = 4096 bytes = 1024 uint_32s)
    uint32_t* tmp = (uint32_t*)q_mem;
    for (int i = 0; i < 1024; i++)
    {
        *(tmp + i) = 0;
    }

    uintptr_t td0_mem = (uintptr_t)q_mem + (sizeof(struct uhci_queue) * 1);
    uintptr_t td1_mem = (uintptr_t)((uint32_t)td0_mem)  + sizeof(struct transfer_descriptor);
    uintptr_t td2_mem = (uintptr_t)((uint32_t)td1_mem)  + sizeof(struct transfer_descriptor);
    uintptr_t request_packet_mem = (uintptr_t)((uint32_t)td2_mem) + sizeof(struct transfer_descriptor);
    uintptr_t return_data_mem = (uintptr_t)(request_packet_mem + 0x10); // data is 8 bytes, but align it
    g_setup_return_data_mem = return_data_mem;

    // Step 2: Initial SETUP packet
    struct transfer_descriptor* td0 = (struct transfer_descriptor*)td0_mem;
    g_setup_td0 = td0;
    td0->link_ptr = ((uint32_t)td1_mem) | (td_link_ptr_depth_first);

    td0->td_ctrl_status = td_ctrl_3errors | td_status_active | td_ctrl_lowspeed;
    td0->td_token = (0x7 << 21) | uhci_packet_id_setup;
    td0->buffer_ptr = (uint32_t)request_packet_mem;

    // Step 3: Follow-up IN packet to read data from the device
    struct transfer_descriptor* td1 = (struct transfer_descriptor*)td1_mem;
    g_setup_td1 = td1;
    td1->link_ptr = (uint32_t)td2_mem | td_link_ptr_depth_first;
    td1->td_ctrl_status = td_ctrl_3errors | td_status_active | td_ctrl_lowspeed;
    td1->td_token = uhci_packet_id_in | (0x7 << 21) | td_token_data_toggle;
    td1->buffer_ptr = (uint32_t)return_data_mem;

    // Step 4:  The OUT packet to acknowledge transfer
    struct transfer_descriptor* td2 = (struct transfer_descriptor*)td2_mem;
    g_setup_td2 = td2;
    td2->link_ptr = td_link_ptr_terminate;
    td2->td_ctrl_status = td_ctrl_3errors | td_status_active | td_ctrl_ioc | td_ctrl_lowspeed;
    td2->td_token = uhci_packet_id_out | (0x7FF << 21) | td_token_data_toggle;

    // Step 5: Setup queue to point to first TD
    // Switched these to maintain head_link. (HC will trash element_link!)
    // TODO:
    // WARNING:
    // NOTE:
    // SIMON:
    // This doesn't work. used to just set elemnt_link, that worked.
    // Setting head_link, seeminfgly just crashes everything
    queue->head_link = ((uint32_t)td0_mem) | td_link_ptr_terminate;
    queue->element_link = ((uint32_t)td0_mem);

    // Step 6: initialize data packet to send
    struct device_request_packet* data = (struct device_request_packet*) (uint8_t*)request_packet_mem;
    g_get_desc_data = data;
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

static void enqueue_get_descriptor_with_size(uint8_t descriptor_size)
{
    g_is_size_request_in_flight = true;

    // Step 0: Allocate memory for our stuff
    uint32_t q_mem = (uint32_t)(uintptr_t)mem_page_get();
    struct uhci_queue* queue = (struct uhci_queue*)(uintptr_t)q_mem;
    g_setup_queue = queue;

    // Zero out page (Page size = 4096 bytes = 1024 uint_32s)
    for (int i = 0; i < 1024; i++) *(((uint32_t*)q_mem) + i) = 0;

    uintptr_t td0_mem = (uintptr_t)q_mem + (sizeof(struct uhci_queue) * 1);
    uintptr_t td1_mem = (uintptr_t)((uint32_t)td0_mem)  + sizeof(struct transfer_descriptor);
    uintptr_t td2_mem = (uintptr_t)((uint32_t)td1_mem)  + sizeof(struct transfer_descriptor);
    uintptr_t request_packet_mem = (uintptr_t)((uint32_t)td2_mem) + sizeof(struct transfer_descriptor);
    uintptr_t return_data_mem = (uintptr_t)(request_packet_mem + 0x10); // data is 8 bytes, but align it
    g_ret_mem_sized = return_data_mem;

    // Step 2: Initial SETUP packet
    struct transfer_descriptor* td0 = (struct transfer_descriptor*)td0_mem;
    g_td0_sized = td0;
    td0->link_ptr = ((uint32_t)td1_mem) | (td_link_ptr_depth_first);

    td0->td_ctrl_status = td_ctrl_3errors | td_status_active | td_ctrl_lowspeed;
    td0->td_token = (0x7 << 21) | uhci_packet_id_setup;
    td0->buffer_ptr = (uint32_t)request_packet_mem;

    // Store the descriptor length so we can read it back when
    // the data transfer completes
    td0->software_use0 = descriptor_size;

    // Step 3: Follow-up IN packet to read data from the device
    struct transfer_descriptor* td1 = (struct transfer_descriptor*)td1_mem;
    g_td1_sized = td1;
    td1->link_ptr = (uint32_t)td2_mem | td_link_ptr_depth_first;
    td1->td_ctrl_status = td_ctrl_3errors | td_status_active | td_ctrl_lowspeed;
    td1->td_token = uhci_packet_id_in | (descriptor_size << 21) | td_token_data_toggle;
    td1->buffer_ptr = (uint32_t)return_data_mem;

    // Step 4:  The OUT packet to acknowledge transfer
    struct transfer_descriptor* td2 = (struct transfer_descriptor*)td2_mem;
    g_td2_sized = td2;
    td2->link_ptr = td_link_ptr_terminate;
    td2->td_ctrl_status = td_ctrl_3errors | td_status_active | td_ctrl_ioc | td_ctrl_lowspeed;
    td2->td_token = uhci_packet_id_out | (0x7FF << 21) | td_token_data_toggle;

    // Step 5: Setup queue to point to first TD
    queue->element_link = ((uint32_t)td0_mem);
    queue->head_link = td_link_ptr_terminate;
    queue->first_element_link = queue->element_link;

    // Step 6: initialize data packet to send
    struct device_request_packet* data = (struct device_request_packet*) (uint8_t*)request_packet_mem;
    g_get_desc_data = data;
    data->type = usb_request_type_standard | usb_request_type_device_to_host;
    data->request = 0x06; // GET_DESCRIPTOR

    // High byte = 0x01 for DEVICE. Low byte = 0. The two bytes are written
    // as a 16-bit word in little-endian
    data->value = 0x1 << 8; // Device
    data->length = descriptor_size;

    // Step 7: Add queue too root queue to schedule for execution
    //         we'll have to wait for an interrupt now
    schedule_queue_insert(queue, nox_uhci_queue_1);
}

/*
static void print_queue(struct uhci_queue* queue)
{
    terminal_write_string("Queue [");
    terminal_write_uint32_x((uint32_t)(uintptr_t)queue);
    terminal_write_string("] - Head: ");
    terminal_write_uint32_x((uint32_t)(uintptr_t)queue->head_link);
    terminal_write_string(" Element: ");
    terminal_write_uint32_x((uint32_t)(uintptr_t)queue->element_link);
}
*/

static void print_td(struct transfer_descriptor* td)
{
    KINFO("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
    terminal_write_string("TD [");

    terminal_write_uint32_x((uint32_t)(uintptr_t)td);
    terminal_write_string("] (");

    if((td->td_ctrl_status & td_status_active) == td_status_active)
        terminal_write_string("Active)");
    else
        terminal_write_string("Inactive)");
    terminal_write_char('\n');

    terminal_write_string("Link: ");
    terminal_write_uint32_x(td->link_ptr);
    terminal_write_char('\n');

    terminal_write_string("Token: ");
    terminal_write_uint32_x(td->td_token);
    if(is_set(td->td_token, td_token_data_toggle)) terminal_write_string(" (DataToggle)");
    terminal_write_string(" Length: ");
    terminal_write_uint32_x(td->td_token >> 21);
    terminal_write_char('\n');

    terminal_write_string("Status: ");
    terminal_write_uint32_x(td->td_ctrl_status);
    terminal_write_char(' ');
    if (is_set(td->td_ctrl_status, td_ctrl_ioc))                 terminal_write_string(" IOC ");
    if (is_set(td->td_ctrl_status, td_ctrl_ios))                 terminal_write_string(" IOS ");
    if (is_set(td->td_ctrl_status, td_ctrl_lowspeed))            terminal_write_string(" LowSpeed ");
    if (is_set(td->td_ctrl_status, td_ctrl_short_packet))        terminal_write_string(" SPS ");
    if (is_set(td->td_ctrl_status, td_status_stalled))           terminal_write_string(" Stalled ");
    if (is_set(td->td_ctrl_status, td_status_crc_timeout))       terminal_write_string(" CRCTimeout ");
    if (is_set(td->td_ctrl_status, td_status_nak_received))      terminal_write_string(" NAK ");
    if (is_set(td->td_ctrl_status, td_status_babble_detected))   terminal_write_string(" BABBLE ");
    if (is_set(td->td_ctrl_status, td_status_data_buffer_error)) terminal_write_string(" BUF_ERR ");
    if (is_set(td->td_ctrl_status, td_status_bitstuff_error))    terminal_write_string(" BITSTUFF_ERR ");

    terminal_write_char('\n');

    if ( (td->td_ctrl_status & td_status_bitstuff_error) == td_status_bitstuff_error) {
        KWARN("BIT STUFF");
    }

    terminal_write_string("Buffer: ");
    terminal_write_uint32_x(td->buffer_ptr);
    terminal_write_string(" (Contents: ");
    terminal_write_uint32_x(*((uint32_t*)(uintptr_t)td->buffer_ptr));
    terminal_write_string(")\n");

    KINFO("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
}

static bool is_set(uint32_t val, uint32_t num)
{
    return (val & (num)) == (num);
}

static void print_frame_list_entry_core(uint32_t* entry, uint8_t indent)
{
    for (uint8_t i = 0; i < indent; i++) terminal_write_char('\t');

    terminal_write_string("[");
    terminal_write_uint32_x((uint32_t)entry);
    terminal_write_string("]: ");
    terminal_write_uint32_x(((*entry) & 0xFFFFFFF0));

    bool is_queue = ((*entry) & frame_list_ptr_queue) == frame_list_ptr_queue;
    if (is_queue) {
        terminal_write_string(" Q ");
        if ( ((*entry) & td_link_ptr_depth_first) == td_link_ptr_depth_first)
            terminal_write_string(" Depth ");

        uint32_t entry_addr = (uint32_t)entry;
        uint32_t root_addr = (uint32_t) (&g_root_queues[0]);
        uint32_t root_size = sizeof(struct uhci_queue) * 16;

        if(entry_addr >= root_addr && entry_addr <= (root_addr + root_size)) {

            entry_addr = entry_addr & 0xFFFFFFF; // Remove lower bits

            uint32_t diff = entry_addr - root_addr;
            diff /= 16;
            enum nox_uhci_queue type = (enum nox_uhci_queue)diff;
            print_queue_type(type);
        }
    }
    else
        terminal_write_string(" TD ");

    bool is_terminate = ((*entry) & frame_list_ptr_terminate) == frame_list_ptr_terminate;
    if (is_terminate) {
        terminal_write_string(" Terminate\n");
        return;
    }

    terminal_write_char('\n');

    uint32_t* next_entry = ((uint32_t*)(uintptr_t)((*entry) & 0xFFFFFFF0));

    print_frame_list_entry_core(next_entry, indent + 1);
}

static void print_frame_list_entry(uint32_t* entry)
{
    print_frame_list_entry_core(entry, 0);
}

void uhci_init(uint32_t base_addr, pci_device* dev, struct pci_address* addr, uint8_t irq)
{
    // IRQs are remapped with IRQ0 being at 0x20
    irq += 0x20;

    SHOWVAL_U8("UHCI IRQ set to ", irq);

    initialize_root_queues();
    cli_cmd_handler_register("uhci", uhci_command);

    // Unused functions (so far.. make sure they're "used")
    if(1 == 0) {
        schedule_queue_insert(NULL, nox_uhci_queue_1);
        schedule_queue_remove(NULL);
    }

    terminal_write_string("UHCI Initializing Host Controller\n");
    terminal_indentation_increase();

#ifdef USB_DEBUG
    print_init_info(addr, base_addr);
#endif

    bool result = false;
    if((base_addr & 0x1) == 0) {
        //uint32_t size = prepare_pci_device(addr, 9, true);

        result = init_mm(dev, base_addr, irq);
    }
    else {
        // It's an IO port mapped device
        result = init_io(base_addr, irq);
    }

    terminal_indentation_decrease();

    if(!result) {
        KERROR("UHCI Host controller initialization failed.");
    }
    else {
        SHOWVAL_U32("UHCI: Initialized base address: ", g_initialized_base_addr);

        SHOWVAL_U32("Root queue start: ", (uint32_t)&g_root_queues[0]);
        SHOWVAL_U32("Root queue end: ", (uint32_t)&g_root_queues[15]);

        enqueue_get_descriptor();
    }
}

// -------------------------------------------------------------------------
// Initialization
// -------------------------------------------------------------------------
static bool init_io(uint32_t base_addr, uint8_t irq)
{
    // Scrap bit 1:0 as it's not a part of the address
    uint32_t io_addr = (base_addr & ~(1));

    g_initialized_base_addr = io_addr;

#ifdef USB_DEBUG
    terminal_write_string("I/O port: ");
    terminal_write_uint32_x(io_addr);
    terminal_write_string("\n");
#endif

    if(0 != detect_root(io_addr, false)) {
        KERROR("Failed to detect root device");
        return false;
    }

    setup(io_addr, irq, false);

    uint32_t port_count = port_count_get(io_addr);

    SHOWVAL_U32("UHCI Done detecting ports. Found ", port_count);

    for(size_t i = 0; i < port_count; i++) {

        // Ports are 1-based
        uint8_t port_num = i + 1;

        if(enable_port(io_addr, port_num)) {

            // Initialize our schedule skeleton
            uint32_t frame_list_addr = IND(io_addr + UHCI_FRAME_BASEADDR_OFFSET);

            terminal_write_string("Initializing FL at: ");
            terminal_write_uint32_x(frame_list_addr);
            terminal_write_char('\n');

            schedule_init(frame_list_addr);

            start_schedule(io_addr);

            // Happy days :-)
            terminal_write_string("Device on port ");
            terminal_write_uint8_x(port_num);
            terminal_write_string(" is ready for use\n");
        }
    }

    uint16_t fp_index = get_cur_fp_index();
    terminal_write_string("Initial FP Index: ");
    terminal_write_uint32(fp_index);
    terminal_write_string("!\n");

    return true;
}

static bool init_mm(pci_device* dev, uint32_t base_addr, uint8_t irq)
{
    // Where in memory space this is is stored in bits 2:1
    uint8_t address_space = ((base_addr >> 1) & 0x3);
    switch(address_space) {
        case mm_space_32bit:    return init_mm32(base_addr, irq, false);
        case mm_space_below1mb: return init_mm32(base_addr, irq, true);
        case mm_space_64bit:    return init_mm64(((((uint64_t)dev->base_addr5) << 32) | base_addr), irq);
        default:
        {
            // TODO: Can we recover? assume 32-bit?
            KERROR("Invalid memory space for memory mapped UHCI device");
            return false;
        }
    }
}

static bool init_mm32(uint32_t base_addr, uint8_t irq, bool below_1mb)
{
    return false;
}

static bool init_mm64(uint64_t base_addr, uint8_t irq)
{
    return false;
}

static int32_t detect_root(uint16_t base_addr, bool memory_mapped)
{
#ifdef USB_DEBUG
    terminal_write_string("Searching for root device @ ");
    terminal_write_uint32_x(base_addr);
    terminal_write_string("\n");
#endif

    if(memory_mapped)
    {
        KWARN("Memory mapped UHCI is not supported!");
        return -1;
    }

    // To reset the UHCI we write send the global reset command
    // wait 10ms+, then reset the reset bit to 0 (HC won't do this for us)
    // five times
    for(int i = 0; i < 5; i++)
    {
        OUTW(base_addr + UHCI_CMD_OFFSET, uhci_cmd_global_reset);
        pit_wait(25);
        OUTW(base_addr + UHCI_CMD_OFFSET, 0x0);
    }

    if(INW(base_addr + UHCI_CMD_OFFSET) != 0x0) {
        KERROR("CMD register does not have the default value '0'");
        SHOWVAL_U32("Value is: ", INW(base_addr + UHCI_CMD_OFFSET));
        return -2;
    }

    if(INW(base_addr + UHCI_STATUS_OFFSET) != 0x20) {
        KERROR("Status Reg does not have its default value of 0x20");
        return -3;
    }

    // Clear out status reg (it's WC)
    OUTW(base_addr + UHCI_STATUS_OFFSET, 0x00FF);

    if(INB(base_addr + UHCI_SOFMOD_OFFSET) != 0x40) {
        KERROR("Sofmod register does not have its default value of 0x40");
        return -4;
    }

    // If we set bit 1, the controller should reset it to 0
    OUTW(base_addr + UHCI_CMD_OFFSET, uhci_cmd_host_reset);

    // Supposedly, we have to give the HC at least 42ms to reset
    pit_wait(42);

    if((INW(base_addr + UHCI_CMD_OFFSET) & uhci_cmd_host_reset) == uhci_cmd_host_reset) {
        KERROR("Controller did not reset bit :(");

        return -5;
    }

    return 0; // Looks good
}

static void setup(uint32_t base_addr, uint8_t irq, bool memory_mapped)
{
    if(memory_mapped)
    {
        KWARN("Cannot setup memory mapped UHCI");
        return;
    }

    // Enable interrupts
    OUTW(base_addr + UHCI_INTERRUPT_OFFSET,
            uhci_irpt_short_packet_enable |
            uhci_irpt_oncomplete_enable |
            uhci_irpt_resume_irpt_enable |
            uhci_irpt_timeout_enable);

    // Start on frame 0
    OUTW(base_addr + UHCI_FRAME_NUM_OFFSET, 0x0000);

    // Allocate a stack for use by the driver
    uint32_t stack_frame = (uint32_t)(uintptr_t)mem_page_get();

    SHOWVAL_U32("Allocated page for Frame List at ", stack_frame);

    // Currently - We will only have one UHCI, so it's safe to store
    // some global state about it - however, this is mostly used for debugging
    // so if/when we support multiple host controllers, this can go. :-)
    g_frame_list = stack_frame;

    OUTD(base_addr + UHCI_FRAME_BASEADDR_OFFSET, stack_frame);

    // The sofmod regisster *should* already be set to 0x40
    // as it's its default value after we reset, but just to be sure...
    OUTB(base_addr + UHCI_SOFMOD_OFFSET, 0x40);

    // Clear the entire status register (it's WC)
    OUTW(base_addr + UHCI_STATUS_OFFSET, 0xFFFF);

    print_status_register(base_addr);

#ifdef USB_DEBUG
    terminal_write_string("UHCI set up for use\n");
#endif

    // Install the interrupt handler before we enable the schedule
    // So we don't miss any interrupts!
    g_irq_num = irq;
    interrupt_receive(irq, uhci_irq);
    pic_enable_irq(irq);
}

static void start_schedule(uint16_t base_addr)
{
    uint16_t cmd = INW(base_addr + UHCI_CMD_OFFSET);

    // We don't yet know if this HC supports 64-byte packets,
    // so just to be sure, set it to 32-byte packets
    cmd &= ~uhci_cmd_max_packet;

    KINFO("Starting UHCI schedule");

    // Go go go!
    OUTW(base_addr + UHCI_CMD_OFFSET, cmd |= uhci_cmd_runstop);
}

static uint32_t port_count_get(uint32_t base_addr)
{
    uint32_t count = 0;
    uint32_t io_port = base_addr + UHCI_PORTSC1_OFFSET;

    // Continue enumerating ports until we get a port that doesn't behave
    // like a port status/control register
    while(true) {
        //
        // Try to poke the reserved bit 7 to see if it behaves as expected
        //

        // Bit 7 is reserved, and always set to 1
        if((INW(io_port) & uhci_portsc_reserved) == 0)
            break;

        // Try to clear bit 7, we're not supposed to be able to
        // so if we manage to clear this bit, it means it's not a valid port
        OUTW(io_port, INW(io_port) & ~uhci_portsc_reserved);
        if((INW(io_port) & uhci_portsc_reserved) == 0)
            break;

        // Try to Write Clear bit 7, it's not supposed to do anything
        // If it does reset the bit, it's not a valid port register
        OUTW(io_port, INW(io_port) & uhci_portsc_reserved);
        if((INW(io_port) & uhci_portsc_reserved) == 0)
            break;

        //
        // Try to Write-Clear the Connected Status Changed and
        // the Port Enabled changed bits, do they clear?
        //
        OUTW(io_port, INW(io_port) | uhci_portsc_connect_status_changed | uhci_portsc_port_enabled_changed);
        if((INW(io_port) & (uhci_portsc_connect_status_changed | uhci_portsc_port_enabled_changed)) != 0)
            break;

        // if we got this far, we have a valid port
        // Increment the count and try the next register
        count++;
        io_port += 2;
    }

    return count;
}

static bool enable_port(uint32_t base_addr, uint8_t port)
{
    uint32_t offset = UHCI_PORTSC1_OFFSET + ((port - 1) * 2);
    uint16_t portsc = INW(base_addr + offset);
    uint16_t attempts = 0;

    //
    // First reset the port
    //

    // Make sure the port is connected before we bother resetting it
    if((portsc & uhci_portsc_connect_status) != uhci_portsc_connect_status) {
        return false;
    }

    // Write enable bit
    portsc |= uhci_portsc_port_reset;
    OUTW(base_addr + offset, portsc);

    // USB specification says to give the HC 50ms to reset
    pit_wait(50);

    // Clear the bit, the port should be reset now
    portsc = INW(base_addr + offset);
    portsc &= ~uhci_portsc_port_reset;

    OUTW(base_addr + offset, portsc);

    // Wait 10ms for the recovery time
    pit_wait(10);

    //
    // Then enable the port
    //
    do {
        portsc = INW(base_addr + offset);

        // If we have a connection changed or enabled changed, just reset the bits
        // and try again
        if( ((portsc & uhci_portsc_port_enabled_changed) == uhci_portsc_port_enabled_changed) ||
            ((portsc & uhci_portsc_connect_status_changed) == uhci_portsc_connect_status_changed)) {

            // Write the value back (should write-clear both bits)
            OUTW(base_addr + offset, portsc);

            continue;
        }

        if( ((portsc & uhci_portsc_port_enabled) == uhci_portsc_port_enabled)) {
            return true;
        }

        // Set it to enabled, and clear enabled changed whilst we're at it
        OUTW(base_addr + offset, portsc | uhci_portsc_port_enabled);

        pit_wait(10);

        attempts++;
    } while(attempts < 16);

    return (portsc & uhci_portsc_port_enabled) != uhci_portsc_port_enabled;
}

static void schedule_queue_remove(struct uhci_queue* queue)
{
    struct uhci_queue* parent = (struct uhci_queue*)(uintptr_t)queue->parent;

    // Reset the head link to a terminate entry
    parent->head_link = td_link_ptr_queue | td_link_ptr_terminate;
}

void schedule_queue_insert(struct uhci_queue* queue, enum nox_uhci_queue root)
{
    struct uhci_queue* parent = &g_root_queues[root];

    // Advance horizontally through the queue pointers until we
    // find the terminating entry, that's where we'll insert this one
    while(!IS_TERMINATE(parent->head_link)) {
       parent = GET_LINK_QUEUE(parent->head_link);
    }

    // Attach the parent's address so we can locate it super easily
    // when we want to remove this queue after it has been executed
    queue->parent = (uint32_t*)(uintptr_t)parent;

    terminal_write_string("Inserted queue at ");
    terminal_write_uint32_x((uint32_t)queue);
    terminal_write_string(" into queue at ");
    terminal_write_uint32_x((uint32_t)parent);
    terminal_write_char('\n');

    // It's very important that we do this last, as as soon as
    // we set it, the Host Controller will pick it up and start processing
    parent->head_link = QUEUE_PTR_CREATE(queue);
    parent->first_element_link = (uint32_t) queue;
}

static void schedule_init(uint32_t frame_list_addr)
{
    // In Nox, we maintain 16 queues in the frame list, these queues
    // are static, and we should never have to modify the frame list after this point
    // If we want to queue a TD or Queue, we stick it in one of these pre-defined queues
    // Which will cause it to be executed at the desired interval
    //
    // nox_uhci_queue_1 executes every 1ms,
    // nox_uhci_queue_2 executes every 2ms etc.
    // This means the first frame will point to queue_1
    // The second frame will point to queue_2, which in turn points to queue_1 etc
    //
    // So our Frame List will end up looking like this (where --> means "points to"):
    // [0] --> queue_1
    // [1] --> queue_2 --> queue_1
    // [2] --> queue_1
    // [3] --> queue_4 --> queue_2 --> queue_1
    // NOTE: The queues have already been linked up at compile time, all we have to do now is install them!

    SHOWVAL_U32("Initializing skeleton schedule @ ", frame_list_addr);

    // Install them into the frame list
    uint32_t* frame_list = (uint32_t*)(uintptr_t)frame_list_addr;
    for(size_t i = 0; i < 1024; i++) {

        // Because the frames go from 0 -> 1023, we add one to the index
        // to figure out which type of queue to insert
        uint32_t frame_num = i + 1;

        if(frame_num % 128 == 0) {
            frame_list[i] = (uint32_t)(uintptr_t)(&g_root_queues[nox_uhci_queue_128]) | frame_list_ptr_queue;
        }
        else if(frame_num % 64 == 0) {
            frame_list[i] = (uint32_t)(uintptr_t)(&g_root_queues[nox_uhci_queue_64]) | frame_list_ptr_queue;
        }
        else if(frame_num % 32 == 0) {
            frame_list[i] = (uint32_t)(uintptr_t)(&g_root_queues[nox_uhci_queue_32]) | frame_list_ptr_queue;
        }
        else if(frame_num % 16 == 0) {
            frame_list[i] = (uint32_t)(uintptr_t)(&g_root_queues[nox_uhci_queue_16]) | frame_list_ptr_queue;
        }
        else if(frame_num % 8 == 0) {
            frame_list[i] = (uint32_t)(uintptr_t)(&g_root_queues[nox_uhci_queue_8]) | frame_list_ptr_queue;
        }
        else if(frame_num % 4 == 0) {
            frame_list[i] = (uint32_t)(uintptr_t)(&g_root_queues[nox_uhci_queue_4]) | frame_list_ptr_queue;
        }
        else if(frame_num % 2 == 0) {
            frame_list[i] = (uint32_t)(uintptr_t)(&g_root_queues[nox_uhci_queue_2]) | frame_list_ptr_queue;
        }
        else if(frame_num % 1 == 0) {
            frame_list[i] = ((uint32_t)(uintptr_t)(&g_root_queues[nox_uhci_queue_1])) | frame_list_ptr_queue;
        }
    }
}

static void initialize_root_queues()
{
    terminal_write_string("Initializing root UHCI queues\n");

    g_root_queues[0].head_link = td_link_ptr_terminate;
    g_root_queues[1].head_link = ((uint32_t)(uintptr_t)&g_root_queues[nox_uhci_queue_1] | td_link_ptr_queue);
    g_root_queues[2].head_link = ((uint32_t)(uintptr_t)&g_root_queues[nox_uhci_queue_2] | td_link_ptr_queue);
    g_root_queues[3].head_link = ((uint32_t)(uintptr_t)&g_root_queues[nox_uhci_queue_4] | td_link_ptr_queue);
    g_root_queues[4].head_link = ((uint32_t)(uintptr_t)&g_root_queues[nox_uhci_queue_8] | td_link_ptr_queue);
    g_root_queues[5].head_link = ((uint32_t)(uintptr_t)&g_root_queues[nox_uhci_queue_16] | td_link_ptr_queue);
    g_root_queues[6].head_link = ((uint32_t)(uintptr_t)&g_root_queues[nox_uhci_queue_32] | td_link_ptr_queue);
    g_root_queues[7].head_link = ((uint32_t)(uintptr_t)&g_root_queues[nox_uhci_queue_64] | td_link_ptr_queue);

    // Ensure sane values (just in case memory was dirty)
    g_root_queues[0].first_element_link = 0;
    g_root_queues[1].first_element_link = 0;
    g_root_queues[2].first_element_link = 0;
    g_root_queues[3].first_element_link = 0;
    g_root_queues[4].first_element_link = 0;
    g_root_queues[5].first_element_link = 0;
    g_root_queues[6].first_element_link = 0;
    g_root_queues[7].first_element_link = 0;

    SHOWVAL_U32("Root Queue 1 head_link: ", g_root_queues[0].head_link);
    SHOWVAL_U32("Root Queue 2 head_link: ", g_root_queues[1].head_link);
    SHOWVAL_U32("Root Queue 3 head_link: ", g_root_queues[2].head_link);
    SHOWVAL_U32("Root Queue 4 head_link: ", g_root_queues[3].head_link);
    SHOWVAL_U32("Root Queue 5 head_link: ", g_root_queues[4].head_link);
    SHOWVAL_U32("Root Queue 6 head_link: ", g_root_queues[5].head_link);
    SHOWVAL_U32("Root Queue 7 head_link: ", g_root_queues[6].head_link);
    SHOWVAL_U32("Root Queue 8 head_link: ", g_root_queues[7].head_link);
}

/* Not yet used (can be used instead of setting IOC on TDs)
static void poll_status_interrupt()
{
  uint16_t status = 0;
  do {
    status = INW(g_initialized_base_addr + UHCI_STATUS_OFFSET);

  } while ((status & uhci_status_usb_interrupt) != uhci_status_usb_interrupt);
}
*/

// -------------------------------------------------------------------------
// IRQ Handler
// -------------------------------------------------------------------------
static void uhci_irq_core(uint8_t irq, struct irq_regs* regs)
{
    print_status_register2();

    uint16_t status = INW(g_initialized_base_addr + UHCI_STATUS_OFFSET);

    if ( (status & uhci_status_error_interrupt) == uhci_status_error_interrupt) {
        KPANIC("UHCI Error interrupt - Panicking!");
        return;
    }

    if((status & uhci_status_usb_interrupt) != uhci_status_usb_interrupt) {
        KINFO("NOT a usb_interrupt, bailing..");
        return;
    }

    // Send the completed TDs to the devices
    handle_fl_complete();

    // It's a usb_interrupt, clear status
    // Just blast ALL the bits (this might trash some stuff we want
    // to react to, like process_error?)
    OUTW(g_initialized_base_addr + UHCI_STATUS_OFFSET, 0xFFFF);

    if (g_is_size_request_in_flight) {

        if (1 == 0)
            print_td(g_td2_sized);

        struct usb_device_descriptor* dev = (struct usb_device_descriptor*)g_ret_mem_sized;
        SHOWVAL_U16("Descriptor size: ", dev->desc_length);
        SHOWVAL_U16("VendorId: ", dev->vendor_id);
        SHOWVAL_U16("prouct id: ", dev->product_id);
        SHOWVAL_U8("Max Packet Len: ", dev->max_packet_size);
        SHOWVAL_U8("Manufactorer: ", dev->manufacturer);

        KERROR("TODO: Retrieve the device descriptor string");
    } else {
        schedule_queue_remove(g_setup_queue);

        // Dealing with the first, size-less 8byte request
        if( *((uint32_t*)g_setup_return_data_mem) == 0) {
            KERROR("No return data, bailing");
            return;
        }

        if (0 == 1 ) {
        uint8_t* response = (uint8_t*) g_setup_return_data_mem;

        // Read out the size and enqueue a new packet to get the entire descriptor
        enqueue_get_descriptor_with_size(*response);
        } else {
            KWARN("Not requesting with size, debugging! :)");
        }
    }
}

static void uhci_irq(uint8_t irq, struct irq_regs* regs)
{
    KINFO("~~~~~~~~~~~~~~uhci irq fired~~~~~~~~~~~~~~~~~~");

    uhci_irq_core(irq, regs);

    pic_send_eoi(g_irq_num);

    KINFO("~~~~~~~~~~~~~~End IRQ~~~~~~~~~~~~~~~~~~");
}

static uint16_t get_cur_fp_index()
{
    uint16_t cur_frame = INW(g_initialized_base_addr + UHCI_FRAME_NUM_OFFSET);

    // Bits 15:11 are reserved, mask off
    return cur_frame & 0x3FF;
}

typedef enum {
    lp_type_td,
    lp_type_queue,
    lp_type_lp
} lp_type;

static void handle_fl_complete_core(uint32_t* link_ptr, lp_type);

static lp_type ptr_to_lp_type(uint32_t ptr)
{
    return (ptr & frame_list_ptr_queue) == frame_list_ptr_queue ?
        lp_type_queue :
        lp_type_td;
}

static uint32_t* lp_get_addr(uint32_t* link_ptr)
{
    return (uint32_t*) ((uint32_t)link_ptr & ~0xF);
}

static void handle_fl_complete_core(uint32_t* link_ptr, lp_type type)
{
    uint32_t* next = 0;
    lp_type next_type;

    switch (type) {
        case lp_type_td:
            {
                struct transfer_descriptor* td = (struct transfer_descriptor*) lp_get_addr(link_ptr);

                uint32_t td_lp = (td->link_ptr & ~0xF);

                for (int i = 0; i < UHCI_MAX_DEVICES; i++) {
                    uhci_dev_handle_td(&g_devices[i], td);
                }

                if (lp_get_addr((uint32_t*)td->link_ptr) != 0) {
                    next_type = ptr_to_lp_type(td->link_ptr);
                    next = (uint32_t*) td_lp;

                    handle_fl_complete_core(next, next_type);
                }

            break;
            }
        case lp_type_queue:
            {
                struct uhci_queue* queue = (struct uhci_queue*) lp_get_addr(link_ptr);

                if (lp_get_addr((uint32_t*)queue->head_link) != 0) {
                    next_type = ptr_to_lp_type(queue->head_link);
                    next = (uint32_t*)queue->head_link;

                    handle_fl_complete_core(next, next_type);
                } else {
                    SHOWVAL_U32("Found queue that doesn't point to anything active, e.g.", queue->head_link);
                }
                break;
            }
        case lp_type_lp:
            {
                next = (uint32_t*)*link_ptr;
                next_type = ptr_to_lp_type((uint32_t) next);

                if ((uint32_t) next != 0)
                    handle_fl_complete_core(next, next_type);

                break;
            }
        default:
            {
                KERROR("Unhandled lp_type_");
                break;
            }
    }
}

static void handle_fl_complete()
{
    uint16_t cur_fp_index = get_cur_fp_index();
    cur_fp_index -= 1;

    uint32_t* frame_list = (uint32_t*)g_frame_list;

    uint32_t* cur_fp = frame_list + cur_fp_index;

    handle_fl_complete_core(cur_fp, lp_type_lp);
}

// -------------------------------------------------------------------------
// Static Utilities
// -------------------------------------------------------------------------
#ifdef USB_DEBUG
static void print_init_info(struct pci_address* addr, uint32_t base_addr)
{
    terminal_write_string("PCI Address [Bus: ");
    terminal_write_uint32(addr->bus);
    terminal_write_string(" Device: ");
    terminal_write_uint32(addr->device);
    terminal_write_string(" Function: ");
    terminal_write_uint32(addr->func);
    terminal_write_string(" Base: ");
    terminal_write_uint32_x(base_addr);
    terminal_write_string("]\n");
}
#endif
