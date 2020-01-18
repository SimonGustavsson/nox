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
#include <util.h>
#include <memory.h>

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
#define IS_TERMINATE(link) ((((uint32_t)(uintptr_t)link) & td_link_ptr_terminate) == td_link_ptr_terminate)
#define GET_LINK_QUEUE(link) ((struct uhci_queue*) (uintptr_t) ((uint32_t)(uintptr_t)link & QUEUE_HEAD_LINK_ADDR_MASK))
#define QUEUE_PTR_CREATE(queue_ptr) (uint32_t)(uintptr_t) ((uint32_t)(uintptr_t)queue_ptr | td_link_ptr_queue);


// -------------------------------------------------------------------------
// Global Variables
// -------------------------------------------------------------------------
uint8_t num_hcs = 0;

#define UHCI_MAX_HCS 2

// Note: The head_links gets set up in a function, because, constant expressions SUCK
struct uhci_hc g_host_controllers[UHCI_MAX_HCS] = {
    { false, false, 0, 0, 0, {}, {
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
        /*  Reserved4  */ {td_link_ptr_terminate, td_link_ptr_terminate, NULL, 0}} },
    { false, false, 0, 0, 0, {}, {
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
        /*  Reserved4  */ {td_link_ptr_terminate, td_link_ptr_terminate, NULL, 0}} }
};

typedef enum {
    lp_type_td,
    lp_type_queue,
    lp_type_lp
} lp_type;

// -------------------------------------------------------------------------
// Forward Declarations
// -------------------------------------------------------------------------
static void handle_fl_complete();
static uint16_t get_cur_fp_index(struct uhci_hc* hc);
static struct get_device_desc_data* get_initial_device_descriptor_dev0();
static bool init_io(struct uhci_hc* hc, uint32_t base_addr, uint8_t irq);
static bool init_mm(struct uhci_hc* hc, pci_device* dev, uint32_t base_addr, uint8_t irq);
static bool init_mm32(struct uhci_hc* hc, uint32_t base_addr, uint8_t irq, bool below_1mb);
static bool init_mm64(struct uhci_hc* hc, uint64_t base_addr, uint8_t irq);
#ifdef USB_DEBUG
static void print_init_info(struct pci_address* addr, uint32_t base_addr);
#endif
static int32_t detect_root(uint16_t base_addr, bool memory_mapped);
static void setup(struct uhci_hc* hc, uint32_t base_addr, uint8_t irq, bool memory_mapped);
static void uhci_irq(uint8_t irq, struct irq_regs* regs);
static bool enable_port(uint32_t base_addr, uint8_t port);
static uint32_t port_count_get(uint32_t base_addr);
static void schedule_init(struct uhci_hc* hc, uint32_t frame_list_addr);
static void schedule_queue_remove(struct uhci_queue* queue);
static void handle_fl_complete_core(struct uhci_hc* hc, uint32_t* link_ptr, lp_type);
static bool poll_status_interrupt();
static void print_frame_list_entry(struct uhci_hc* hc, uint32_t* entry);
static void print_td(struct transfer_descriptor* td);
static void initialize_root_queues(struct uhci_hc* hc);
static void start_schedule(uint16_t base_addr);

// -------------------------------------------------------------------------
// Public Contract
// -------------------------------------------------------------------------
static void print_status_register(struct uhci_hc* hc)
{
    if(hc->base_addr == NULL) {
        KERROR("THe UHCI has not been initialized yet!");
        return;
    }

    uint32_t base_addr = hc->base_addr;

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
    if(is_set(status, uhci_status_usb_interrupt))     KINFO("USB interrupt - Yes");
    terminal_write_string("___________________\n");
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
    // All comments have been disabled for now as I'm refactoring UHCI
    // to support multiple host controllers and cleaning up
    /*
    if(kstrcmp(args[1], "fl")) {

        SHOWVAL_U32("Dumping 20 first frames from list at: ", g_frame_list);

        uint32_t* fl = (uint32_t*)(uintptr_t)(g_frame_list);
        for(size_t i = 0; i < 20; i++) {
            print_frame_list_entry(fl++);
        }
    }
    else if(kstrcmp(args[1], "tds")) {
        if (g_last_request == NULL) {
            KERROR("No request in flight\n");
            return;
        }

        KINFO("Showing inflight TDS");
        print_td(&g_last_request->setup);

        for (int i = 0; i < g_last_request->num_in_descriptors; i++) {
            struct transfer_descriptor* td = (g_last_request->request + i);
            print_td(td);
        }

        print_td(&g_last_request->ack);
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
        get_initial_device_descriptor_dev0();
    }
    */
}

static struct get_device_desc_data* ctrl_read(
        struct uhci_hc* hc,
        uint32_t bytes_to_read,
        uint32_t max_packet_size,
        uint8_t device_addr,
        struct device_request_packet* request);

static struct get_device_desc_data* get_device_descriptor_core(
        struct uhci_hc* hc,
        uint32_t descriptor_size,
        uint32_t max_packet_size,
        uint8_t device_addr);

static struct get_device_desc_data* get_initial_device_descriptor_dev0(struct uhci_hc* hc)
{
    // Ensure status is CLEAN as a whistle to begin with
    OUTW(hc->base_addr + UHCI_STATUS_OFFSET, 0x00FF);

    // We don't know the max packet size of un-addressed devices so:
    //
    // * Request first 8 bytes of the descriptor
    // * Get max_packet_size from response (n)
    // * Request n bytes of the descriptor
    struct get_device_desc_data* data = get_device_descriptor_core(hc, 8, 8, 0);
    if (data == NULL) {
        return NULL;
    }

    printf("Waiting for interrupt on status... ");
    if (poll_status_interrupt()) {
        printf("done!\n");
        return data;
    }

    KWARN("FAiled to retrieve initial device descriptor");
    return NULL;
}

static struct get_device_desc_data* get_full_device_descriptor_dev0(struct uhci_hc* hc, uint32_t max_packet_size, uint8_t device_addr)
{
    return get_device_descriptor_core(hc, sizeof(struct usb_device_descriptor), max_packet_size, device_addr);
}

static struct get_device_desc_data* get_device_descriptor_core(struct uhci_hc* hc, uint32_t descriptor_size, uint32_t max_packet_size, uint8_t device_addr)
{
    struct device_request_packet request;
    request.type = usb_request_type_standard | usb_request_type_device_to_host;
    request.request = UHCI_REQUEST_GET_DESCRIPTOR;

    // High byte = 0x01 for DEVICE. Low byte = 0. The two bytes are written
    // as a 16-bit word in little-endian
    request.value = DESCRIPTOR_TYPE_DEVICE << 8;
    request.length = descriptor_size;

    return ctrl_read(hc, descriptor_size, max_packet_size, device_addr, &request);
}

static struct get_device_desc_data* ctrl_read(struct uhci_hc* hc, uint32_t bytes_to_read,
        uint32_t max_packet_size,
        uint8_t device_addr,
        struct device_request_packet* request)
{
    printf("ctrl_read(%d, %d)", bytes_to_read, max_packet_size);

    if (max_packet_size > 0x3FF) {
        // We only have 10 bits to store the value, so clamp it
        KWARN("max_packet_size too large, value clamped to 0x400");
        max_packet_size = 0x400;
    }

    // Note: Queue needs to be 16-bit aligned
    intptr_t data_ptr = (intptr_t) aligned_palloc(sizeof(struct get_device_desc_data), 16);
    if (data_ptr == NULL) {
        KERROR("UHCI Get Device Desc: Failed to allocate memory for data");
        return NULL;
    }

    my_memset((void*) data_ptr, 0, sizeof(struct get_device_desc_data));

    uint32_t num_packages = max_packet_size >= bytes_to_read
        ? 1
        : ( (bytes_to_read / max_packet_size) +
                (bytes_to_read % max_packet_size ? 1 : 0));

    intptr_t in_td_ptr = (intptr_t) aligned_palloc(sizeof(struct transfer_descriptor) * num_packages, 16);
    if (in_td_ptr == NULL) {
        KERROR("UHCI Get Device Desc: Failed to allocate memory for IN tds");
        phree((void*) data_ptr);
        return NULL;
    }

    my_memset((void*) in_td_ptr, 0, sizeof(struct transfer_descriptor) * num_packages);

    intptr_t response_buffer_ptr = (intptr_t) pcalloc(sizeof(uint8_t), bytes_to_read);
    if (response_buffer_ptr == NULL) {
        KERROR("UHCI ctrl_read: Failed to allocate memory for response buffer");
        phree((void*) data_ptr);
        phree((void*)in_td_ptr);
        return NULL;
    }

    struct get_device_desc_data* data = (struct get_device_desc_data*) data_ptr;

    data->num_in_descriptors = num_packages;
    data->request = (struct transfer_descriptor*) in_td_ptr;
    data->response_buffer_size = bytes_to_read;
    data->response_buffer = (uint8_t*) response_buffer_ptr;

    data->setup.link_ptr = td_link_ptr_depth_first | ((uint32_t) in_td_ptr) ;
    data->setup.ctrl_status = td_ctrl_3errors | td_status_active;
    data->setup.token = ( (8 - 1) << 21) | uhci_packet_id_setup; // SETUP pkt is always 8 bytes
    data->setup.buffer_ptr = (uint32_t)&data->request_packet;

    // !important - set up a link from the setup TD so we can
    //              find this struct again (to free it) once it's all done
    data->setup.software_use0 = (uint32_t) data;

    printf("[SETUP PACKAGE] %P, link: %P, status: %P, token: %P, buffer: %P\n",
            (uint32_t) &data->setup,
            data->setup.link_ptr,
            data->setup.ctrl_status,
            data->setup.token,
            data->setup.buffer_ptr);

    // Populate these requests backwards for ease of link ptr setup
    uint32_t bytes_left_to_read = bytes_to_read;
    uint32_t bytes_read = 0;
    for (int i = 0; i < num_packages; i++) {
        struct transfer_descriptor* cur = data->request + i;

        uint32_t data_toggle = i % 2 == 0
            ? td_token_data_toggle
            : 0;

        uint32_t total_bytes_to_read = bytes_left_to_read < max_packet_size
            ? bytes_left_to_read
            :max_packet_size;
        bytes_left_to_read -= total_bytes_to_read;

        uint32_t buffer_offset = bytes_read;
        bytes_read += total_bytes_to_read;

        printf("Buf offset, i=%d,offset=%d,toRead:%d bytes (total left: %d)\n",
                i, buffer_offset, total_bytes_to_read, bytes_left_to_read);

        cur->link_ptr = td_link_ptr_depth_first; // link_ptr populated later
        cur->ctrl_status = td_ctrl_3errors | td_status_active; // | td_ctrl_ioc;
        cur->token = uhci_packet_id_in | ((total_bytes_to_read - 1) << 21) | data_toggle;
        cur->buffer_ptr = ((uint32_t)data->response_buffer) + buffer_offset;

        printf("IN TD[%d] %P, link: %P, status: %P, token: %P, buffer: %P\n",
                i,
                (uint32_t) cur,
                cur->link_ptr,
                cur->ctrl_status,
                cur->token,
                cur->buffer_ptr);
    }

    for (int i = num_packages - 1; i >= 0; i--) {
        struct transfer_descriptor* cur = data->request + i;

        if (i == num_packages - 1) {
            cur->link_ptr |= (uint32_t) &data->ack;
        } else {
            struct transfer_descriptor* next = data->request + i + 1;
            cur->link_ptr |= (uint32_t) next;
        }
    }

    // Step 4: the OUT package to acknowledge transfer
    data->ack.link_ptr = td_link_ptr_terminate;
    data->ack.ctrl_status = td_ctrl_3errors | td_status_active; // | td_ctrl_ioc;
    data->ack.token = uhci_packet_id_out | (0x7FF << 21) | td_token_data_toggle;

    printf("ACK TD: %P, Link:%P, Ctrl:%P, Token:%P, Buffer:%P\n",
            (uint32_t) &data->ack,
            data->ack.link_ptr,
            data->ack.ctrl_status,
            data->ack.token,
            data->ack.buffer_ptr);

    // Step 5: Setup queue to point to first TD
    data->queue.head_link = ( ((uint32_t) &data->setup) | td_link_ptr_terminate);
    data->queue.element_link = (uint32_t) &data->setup;

    printf("Queue is @ %P, head: %P, element: %P\n",
            &data->queue,
            data->queue.head_link,
            data->queue.element_link);

    // Step 6: initialize data packet to send
    data->request_packet.type = request->type;
    data->request_packet.request = request->request;
    data->request_packet.value = request->value;
    data->request_packet.length = request->length;

    // Step 7: Add queue too root queue to schedule for execution
    //         we'll have to wait for an interrupt now
    schedule_queue_insert(hc, &data->queue, nox_uhci_queue_1);

    return data;
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

    printf("TD [%P] (", (uint32_t)(uintptr_t)td);

    if(is_set(td->ctrl_status, td_status_active))
        terminal_write_string("Active)\n");
    else
        terminal_write_string("Inactive)\n");

    printf("Link: %P\n", td->link_ptr);
    printf("Token: %P ", td->token);

    switch ( (td->token & 0xFF) ) {
        case uhci_packet_id_setup:
            printf(" SETUP");
            break;
        case uhci_packet_id_in:
            printf(" IN");
            break;
        case uhci_packet_id_out:
            printf(" OUT");
            break;
    }

    if(is_set(td->token, td_token_data_toggle))
        terminal_write_string(" DT");

    printf(" Length: %P\n", td->token >> 21);

    printf("Status: %P ", td->ctrl_status);
    if (is_set(td->ctrl_status, td_ctrl_ioc))                 terminal_write_string(" IOC ");
    if (is_set(td->ctrl_status, td_ctrl_ios))                 terminal_write_string(" IOS ");
    if (is_set(td->ctrl_status, td_ctrl_lowspeed))            terminal_write_string(" LowSpeed ");
    if (is_set(td->ctrl_status, td_ctrl_short_packet))        terminal_write_string(" SPS ");
    if (is_set(td->ctrl_status, td_status_stalled))           terminal_write_string(" Stalled ");
    if (is_set(td->ctrl_status, td_status_crc_timeout))       terminal_write_string(" CRCTimeout ");
    if (is_set(td->ctrl_status, td_status_nak_received))      terminal_write_string(" NAK ");
    if (is_set(td->ctrl_status, td_status_babble_detected))   terminal_write_string(" BABBLE ");
    if (is_set(td->ctrl_status, td_status_data_buffer_error)) terminal_write_string(" BUF_ERR ");
    if (is_set(td->ctrl_status, td_status_bitstuff_error))    terminal_write_string(" BITSTUFF_ERR ");

    printf("\nBuffer: %P\n", td->buffer_ptr);

    KINFO("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
}

static void print_frame_list_entry_core(struct uhci_hc* hc, uint32_t* entry, uint8_t indent)
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
        uint32_t root_addr = (uint32_t) (&hc->root_queues[0]);
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

    print_frame_list_entry_core(hc, next_entry, indent + 1);
}

static void print_frame_list_entry(struct uhci_hc* hc, uint32_t* entry)
{
    print_frame_list_entry_core(hc, entry, 0);
}

void uhci_init(uint32_t base_addr, pci_device* dev, struct pci_address* addr, uint8_t irq)
{
    if (num_hcs >= (UHCI_MAX_HCS - 1)) { // num_hcs is 0-based
        KPANIC("UHCI:: Only 2 host controllers are supported");
        return;
    }

    // Allocate a HC to this init request
    struct uhci_hc* hc = &g_host_controllers[num_hcs++];

    // IRQs are remapped with IRQ0 being at 0x20
    irq += 0x20;

    SHOWVAL_U8("UHCI IRQ set to ", irq);

    initialize_root_queues(hc);
    cli_cmd_handler_register("uhci", uhci_command);

    terminal_write_string("UHCI Initializing Host Controller\n");
    terminal_indentation_increase();

#ifdef USB_DEBUG
    print_init_info(addr, base_addr);
#endif

    bool result = false;
    if((base_addr & 0x1) == 0) {
        //uint32_t size = prepare_pci_device(addr, 9, true);
        hc->io = false;
        result = init_mm(hc, dev, base_addr, irq);
    }
    else {
        // It's an IO port mapped device
        hc->io = true;
        result = init_io(hc, base_addr, irq);
    }

    terminal_indentation_decrease();

    if(!result) {
        KERROR("UHCI Host controller initialization failed.");
    }
    else {
        SHOWVAL_U32("UHCI: Initialized base address: ", hc->base_addr);

        SHOWVAL_U32("Root queue start: ", (uint32_t)&hc->root_queues[0]);
        SHOWVAL_U32("Root queue end: ", (uint32_t)&hc->root_queues[15]);
    }
}

// -------------------------------------------------------------------------
// Initialization
// -------------------------------------------------------------------------
static bool init_io(struct uhci_hc* hc, uint32_t base_addr, uint8_t irq)
{
    // Scrap bit 1:0 as it's not a part of the address
    uint32_t io_addr = (base_addr & ~(1));
    hc->base_addr = io_addr;

#ifdef USB_DEBUG
    printf("UHCI init_io (addr: %P)\n", io_addr);
#endif

    if(0 != detect_root(io_addr, false)) {
        KERROR("Failed to detect root device");
        return false;
    }

    setup(hc, io_addr, irq, false);

    // Initialize our schedule skeleton
    uint32_t frame_list_addr = IND(io_addr + UHCI_FRAME_BASEADDR_OFFSET);

    schedule_init(hc, frame_list_addr);
    start_schedule(io_addr);


    uint32_t port_count = port_count_get(io_addr);

    if (port_count == 0) {
        printf("UHCI Done detecting ports. Found 0, returning...\n");
        return true;
    } else {
        printf("UHCI Done detecting ports. Found %d, enabling ports...\n", port_count);
    }

    // Detect all ports on the HC
    bool has_enabled_port = false;
    for (size_t i = 0; i < port_count; i++) {

        // Ports are 1-based
        uint8_t port_num = i + 1;

        if(!enable_port(io_addr, port_num)) {
            continue;
        }

        // Set flag to perform device enumeration
        has_enabled_port = true;
        printf("UHCI: Found device on port %d\n", port_num);

#ifdef USB_DEBUG
        printf("Initializing FL at %P\n", frame_list_addr);
#endif
    }

    // Get the device descriptor of the first device we found
    // NOTE: Host Controllers in the default state will respond to device number "0".
    //       Valid addresses are all non-zero. So we can only retrieve the device descriptor
    //       of one HC at the time
    if (!has_enabled_port) {
        return true;
    }

    printf("Retrieving initial device descriptor for first HC\n");
    struct get_device_desc_data* data = get_initial_device_descriptor_dev0(hc);

    if (data == NULL) {
        return false;
    }

    print_td(&data->setup);
    for (int i = 0; i < data->num_in_descriptors; i++) {
        print_td(data->request + i);
    }
    print_td(&data->ack);

    // First things first - make sure the HC forgets about this queue
    // We don't want to free the memory, repurpose the address, and have the HC
    // pick up on random bytes as if it was a TD and fire a bunch of errors.
    // Not that this has happened...
    schedule_queue_remove(&data->queue);

    // Max buffer size stored in 31:21 (upper 10 bits)
    uint8_t* response_buffer = (uint8_t*)data->response_buffer;

    // Request descriptor again, but now using
    // usb_device_descriptor->max_packet_size as buffer size to get entire descriptor
    printf("Got small INITIAL device desc (first %d bytes, @ %P).\n", data->response_buffer_size, data->response_buffer);

    struct usb_device_descriptor* desc = (struct usb_device_descriptor*)response_buffer;
    printf("Device descriptor size: %d, type: %d, release num: %P, max packet: %d\n",
            desc->desc_length,
            desc->type,
            desc->release_num,
            desc->max_packet_size);

    if (1 == 0) {
        // Throw away calls to avoid unused warnings
        get_full_device_descriptor_dev0(hc, 0, 0);
        print_frame_list_entry(hc, NULL);
    }

    return true;
}

/*
static bool enumerate_devices() {
    return true;
}
*/

static bool init_mm(struct uhci_hc* hc, pci_device* dev, uint32_t base_addr, uint8_t irq)
{
    // Where in memory space this is is stored in bits 2:1
    uint8_t address_space = ((base_addr >> 1) & 0x3);
    switch(address_space) {
        case mm_space_32bit:    return init_mm32(hc, base_addr, irq, false);
        case mm_space_below1mb: return init_mm32(hc, base_addr, irq, true);
        case mm_space_64bit:    return init_mm64(hc, ((((uint64_t)dev->base_addr5) << 32) | base_addr), irq);
        default:
        {
            // TODO: Can we recover? assume 32-bit?
            KERROR("Invalid memory space for memory mapped UHCI device");
            return false;
        }
    }
}

static bool init_mm32(struct uhci_hc* hc, uint32_t base_addr, uint8_t irq, bool below_1mb)
{
    return false;
}

static bool init_mm64(struct uhci_hc* hc, uint64_t base_addr, uint8_t irq)
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

static void setup(struct uhci_hc* hc, uint32_t base_addr, uint8_t irq, bool memory_mapped)
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
    // ^ Now supporting multiple, but keeping the comment because its cute ^
    hc->frame_list = stack_frame;

    OUTD(base_addr + UHCI_FRAME_BASEADDR_OFFSET, stack_frame);

    // The sofmod regisster *should* already be set to 0x40
    // as it's its default value after we reset, but just to be sure...
    OUTB(base_addr + UHCI_SOFMOD_OFFSET, 0x40);

    // Clear the entire status register (it's WC)
    OUTW(base_addr + UHCI_STATUS_OFFSET, 0xFFFF);

    print_status_register(hc);

#ifdef USB_DEBUG
    terminal_write_string("UHCI set up for use\n");
#endif

    // Install the interrupt handler before we enable the schedule
    // So we don't miss any interrupts!
    hc->irq_num = irq;
    hc->active = true;
    interrupt_receive(irq, uhci_irq);
    pic_enable_irq(irq);
}

static void start_schedule(uint16_t base_addr)
{
    uint16_t cmd = INW(base_addr + UHCI_CMD_OFFSET);

    // We don't yet know if this HC supports 64-byte packets,
    // so just to be sure, set it to 32-byte packets
    cmd &= ~uhci_cmd_max_packet;

    printf("Starting UHCI schedule on base_addr '%P'\n", base_addr);

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

void schedule_queue_insert(struct uhci_hc* hc, struct uhci_queue* queue, enum nox_uhci_queue root)
{
    uint32_t queue_addr = (uint32_t) queue;
    if ( (queue_addr & 0xF) != 0) {
        KPANIC("An attempt was made to schedule a queue that isn't 16-byte aligned");
        return;
    }

    struct uhci_queue* parent = &hc->root_queues[root];

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

static void schedule_init(struct uhci_hc* hc, uint32_t frame_list_addr)
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
            frame_list[i] = (uint32_t)(uintptr_t)(&hc->root_queues[nox_uhci_queue_128]) | frame_list_ptr_queue;
        }
        else if(frame_num % 64 == 0) {
            frame_list[i] = (uint32_t)(uintptr_t)(&hc->root_queues[nox_uhci_queue_64]) | frame_list_ptr_queue;
        }
        else if(frame_num % 32 == 0) {
            frame_list[i] = (uint32_t)(uintptr_t)(&hc->root_queues[nox_uhci_queue_32]) | frame_list_ptr_queue;
        }
        else if(frame_num % 16 == 0) {
            frame_list[i] = (uint32_t)(uintptr_t)(&hc->root_queues[nox_uhci_queue_16]) | frame_list_ptr_queue;
        }
        else if(frame_num % 8 == 0) {
            frame_list[i] = (uint32_t)(uintptr_t)(&hc->root_queues[nox_uhci_queue_8]) | frame_list_ptr_queue;
        }
        else if(frame_num % 4 == 0) {
            frame_list[i] = (uint32_t)(uintptr_t)(&hc->root_queues[nox_uhci_queue_4]) | frame_list_ptr_queue;
        }
        else if(frame_num % 2 == 0) {
            frame_list[i] = (uint32_t)(uintptr_t)(&hc->root_queues[nox_uhci_queue_2]) | frame_list_ptr_queue;
        }
        else if(frame_num % 1 == 0) {
            frame_list[i] = ((uint32_t)(uintptr_t)(&hc->root_queues[nox_uhci_queue_1])) | frame_list_ptr_queue;
        }
    }
}

static void initialize_root_queues(struct uhci_hc* hc)
{
    terminal_write_string("Initializing root UHCI queues\n");

    hc->root_queues[0].head_link = td_link_ptr_terminate;
    hc->root_queues[1].head_link = ((uint32_t)(uintptr_t)&hc->root_queues[nox_uhci_queue_1] | td_link_ptr_queue);
    hc->root_queues[2].head_link = ((uint32_t)(uintptr_t)&hc->root_queues[nox_uhci_queue_2] | td_link_ptr_queue);
    hc->root_queues[3].head_link = ((uint32_t)(uintptr_t)&hc->root_queues[nox_uhci_queue_4] | td_link_ptr_queue);
    hc->root_queues[4].head_link = ((uint32_t)(uintptr_t)&hc->root_queues[nox_uhci_queue_8] | td_link_ptr_queue);
    hc->root_queues[5].head_link = ((uint32_t)(uintptr_t)&hc->root_queues[nox_uhci_queue_16] | td_link_ptr_queue);
    hc->root_queues[6].head_link = ((uint32_t)(uintptr_t)&hc->root_queues[nox_uhci_queue_32] | td_link_ptr_queue);
    hc->root_queues[7].head_link = ((uint32_t)(uintptr_t)&hc->root_queues[nox_uhci_queue_64] | td_link_ptr_queue);

    // Ensure sane values (just in case memory was dirty)
    hc->root_queues[0].first_element_link = 0;
    hc->root_queues[1].first_element_link = 0;
    hc->root_queues[2].first_element_link = 0;
    hc->root_queues[3].first_element_link = 0;
    hc->root_queues[4].first_element_link = 0;
    hc->root_queues[5].first_element_link = 0;
    hc->root_queues[6].first_element_link = 0;
    hc->root_queues[7].first_element_link = 0;

    SHOWVAL_U32("Root Queue 1 head_link: ", hc->root_queues[0].head_link);
    SHOWVAL_U32("Root Queue 2 head_link: ", hc->root_queues[1].head_link);
    SHOWVAL_U32("Root Queue 3 head_link: ", hc->root_queues[2].head_link);
    SHOWVAL_U32("Root Queue 4 head_link: ", hc->root_queues[3].head_link);
    SHOWVAL_U32("Root Queue 5 head_link: ", hc->root_queues[4].head_link);
    SHOWVAL_U32("Root Queue 6 head_link: ", hc->root_queues[5].head_link);
    SHOWVAL_U32("Root Queue 7 head_link: ", hc->root_queues[6].head_link);
    SHOWVAL_U32("Root Queue 8 head_link: ", hc->root_queues[7].head_link);
}

static bool poll_status_interrupt(struct uhci_hc* hc)
{
  uint16_t status = 0;
  uint32_t attempts = 0;
  uint32_t max_attempts = 500; // 5s

  while (true) {
    status = INW(hc->base_addr + UHCI_STATUS_OFFSET);

    if ((status & uhci_status_usb_interrupt) == uhci_status_usb_interrupt) {
        return true;
    }

    attempts++;

    if (attempts >= max_attempts) {
        KERROR("Timed out waiting for sync status interrupt");
        return false;
    }

    pit_wait(10);
  }
}

static struct uhci_hc* get_hc_with_irq(uint8_t irq)
{
    for (int i = 0; i < UHCI_MAX_HCS; i++ ) {
        struct uhci_hc* hc = &g_host_controllers[i];

        // TODO: Add active check too?
        if (hc->irq_num == irq) {
            return hc;
        }
    }

    return NULL;
}

// -------------------------------------------------------------------------
// IRQ Handler
// -------------------------------------------------------------------------
static void uhci_irq_core(struct uhci_hc* hc, uint8_t irq, struct irq_regs* regs)
{
    uint16_t status = INW(hc->base_addr + UHCI_STATUS_OFFSET);

    if(is_set(status, uhci_status_process_error)) {
        KPANIC("UHCI Error interrupt - Panicking!");
        return;
    }

    if((status & uhci_status_usb_interrupt) != uhci_status_usb_interrupt) {
        KINFO("NOT a usb_interrupt, bailing..");
        return;
    }

    // Send the completed TDs to the devices
    handle_fl_complete(hc);

    // It's a usb_interrupt, clear status
    // Just blast ALL the bits (this might trash some stuff we want
    // to react to, like process_error?)
    OUTW(hc->base_addr + UHCI_STATUS_OFFSET, 0xFFFF);
}

static void uhci_irq(uint8_t irq, struct irq_regs* regs)
{
    KINFO("~~~~~~~~~~~~~~uhci irq fired~~~~~~~~~~~~~~~~~~");

    struct uhci_hc* hc = get_hc_with_irq(irq);
    if (hc == NULL) {
        KPANIC("Unable to locate HC that can handle this IRQ!");
    }

    uhci_irq_core(hc, irq, regs);

    pic_send_eoi(hc->irq_num);

    KINFO("~~~~~~~~~~~~~~End IRQ~~~~~~~~~~~~~~~~~~");
}

static uint16_t get_cur_fp_index(struct uhci_hc* hc)
{
    uint16_t cur_frame = INW(hc->base_addr + UHCI_FRAME_NUM_OFFSET);

    // Bits 15:11 are reserved, mask off
    return cur_frame & 0x3FF;
}

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


static bool uhci_hc_handle_td(struct uhci_hc* hc, struct transfer_descriptor* td)
{
    if (1 == 0) print_td(td); // "unused" warnings meh

    // TODO: Redirect to the correct device
    return true;
}

static void handle_fl_complete_core(struct uhci_hc* hc, uint32_t* link_ptr, lp_type type)
{
    uint32_t* next = 0;
    lp_type next_type;

    switch (type) {
        case lp_type_td:
            {
                struct transfer_descriptor* td = (struct transfer_descriptor*) lp_get_addr(link_ptr);

                uint32_t td_lp = (td->link_ptr & ~0xF);

                if (!uhci_hc_handle_td(hc, td)) {
                    KERROR("Encountered TD that went unhandled");
                }

                if (lp_get_addr((uint32_t*)td->link_ptr) != 0) {
                    next_type = ptr_to_lp_type(td->link_ptr);
                    next = (uint32_t*) td_lp;

                    handle_fl_complete_core(hc, next, next_type);
                }

            break;
            }
        case lp_type_queue:
            {
                struct uhci_queue* queue = (struct uhci_queue*) lp_get_addr(link_ptr);

                if (lp_get_addr((uint32_t*)queue->head_link) != 0) {
                    next_type = ptr_to_lp_type(queue->head_link);
                    next = (uint32_t*)queue->head_link;

                    handle_fl_complete_core(hc, next, next_type);
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
                    handle_fl_complete_core(hc, next, next_type);

                break;
            }
        default:
            {
                KERROR("Unhandled lp_type_");
                break;
            }
    }
}

static void handle_fl_complete(struct uhci_hc* hc)
{
    uint16_t cur_fp_index = get_cur_fp_index(hc);
    cur_fp_index -= 1;

    uint32_t* frame_list = (uint32_t*)hc->frame_list;

    uint32_t* cur_fp = frame_list + cur_fp_index;

    handle_fl_complete_core(hc, cur_fp, lp_type_lp);
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
