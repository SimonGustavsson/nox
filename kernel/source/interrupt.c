#include <kernel.h>
#include <debug.h>
#include <types.h>
#include <interrupt.h>
#include <terminal.h>

struct PACKED idt_descriptor
{
    uint16_t  limit; // Length of IDT in bytes - 1
    void*     base; // Linear start address
};

union idt_type_attribute
{
    uint8_t raw;

    struct PACKED {
        uint8_t        type:        4;
        uint8_t        segment:     1; // Storage segment (0 for interrupt gates)
        uint8_t        priv_level:  2; // Gate call protection (minimum caller level)
        uint8_t        present:     1; // 0 for unused interrupts
    } bits;
};

struct PACKED idt_entry
{
    uint16_t                    offset_low; // 15:0 of offset
    uint16_t                    selector;   // Code segment selector in GDT/LDT
    uint8_t                     zero;
    union idt_type_attribute    type_attr;
    uint16_t                    offset_high;  // 31:16 of offset
};

struct PACKED dispatcher
{
    uint8_t             pusha;
    uint8_t             push_esp;
    uint8_t             push_imm;
    uint8_t             push_imm_arg;
    uint8_t             call;
    uintptr_t           target;
    uint8_t             add_imm;
    uint8_t             add_imm_reg;
    uint8_t             add_imm_arg;
    uint8_t             popa;
    uint8_t             iret;
};

struct dispatcher_data
{
    interrupt_handler   handler;
};

// -------------------------------------------------------------------------
// Forward Declares
// -------------------------------------------------------------------------
static struct idt_descriptor        idt_get();
static void                         idt_install(struct idt_descriptor* idt);
static void                         idt_entry_setup(struct idt_entry* entry, uint8_t irq, gate_type type, uint8_t priv_level);
static enum kresult                 idt_entry_verify(struct idt_entry const * const entry, uint8_t const irq, gate_type const type, uint8_t const priv_level);
static void                         irq_dispatcher(uint8_t irq, struct irq_regs* regs);

static void                         double_fault(uint8_t irq, struct irq_regs* regs);
static void                         gpf(uint8_t irq, struct irq_regs* regs);
static void                         page_fault(uint8_t irq, struct irq_regs* regs);
static void                         stack_segment_fault(uint8_t irq, struct irq_regs* regs);
static void                         invalid_tss(uint8_t irq, struct irq_regs* regs);
static void                         segment_not_present(uint8_t irq, struct irq_regs* regs);

// -------------------------------------------------------------------------
// Globals
// -------------------------------------------------------------------------
static struct idt_descriptor        g_idt_descriptor = {};
static struct idt_entry             g_idt_entries[256] = {};

static struct dispatcher            g_dispatchers[256] = {};
static struct dispatcher_data       g_dispatcher_data[256] = {};

// -------------------------------------------------------------------------
// Exports
// -------------------------------------------------------------------------
void interrupt_init_system()
{
    g_idt_descriptor.limit = sizeof(g_idt_entries) - 1;
    g_idt_descriptor.base = &g_idt_entries;
    idt_install(&g_idt_descriptor);

    interrupt_receive_trap(0x08, double_fault);
    interrupt_receive_trap(0x0A, invalid_tss);
    interrupt_receive_trap(0x0B, segment_not_present);
    interrupt_receive_trap(0x0C, stack_segment_fault);
    interrupt_receive_trap(0x0D, gpf);
    interrupt_receive_trap(0x0E, page_fault);
}

void interrupt_disable_all()
{
    __asm("cli");
}

void interrupt_enable_all()
{
    __asm ("sti");
}

enum kresult interrupt_install_handler(uint8_t irq, interrupt_handler handler, gate_type type, uint8_t priv_level)
{
    enum kresult result;

    struct idt_descriptor idt_descriptor = idt_get();
    struct idt_entry* entries = (struct idt_entry*)idt_descriptor.base;
    struct idt_entry* entry = &entries[irq];

    if (entry->type_attr.bits.present == 0) {
        idt_entry_setup(entry, irq, type, priv_level);
    }
    else if (kresult_ok != (result = idt_entry_verify(entry, irq, type, priv_level))) {
        return result;
    }

    // Add the handler to the dispatch table
    struct dispatcher_data* data = &g_dispatcher_data[irq];
    data->handler = handler;

    return kresult_ok;
}

void interrupt_remove_handler(uint8_t irq)
{
    // TODO:
    // Remove the handler for the dispatcher chain
    // If the dispatcher chain reaches zero entries, remove the idt entry completely
}

void interrupt_enable_handler(uint8_t irq)
{
    struct idt_descriptor idt_descriptor = idt_get();

    struct idt_entry* entries = (struct idt_entry*)idt_descriptor.base;
    struct idt_entry* entry = &entries[irq];

    entry->type_attr.bits.present = 1;
}

void interrupt_disableHandler(uint8_t irq)
{
    struct idt_descriptor idt_descriptor = idt_get();

    struct idt_entry* entries = (struct idt_entry*)idt_descriptor.base;
    struct idt_entry* entry = &entries[irq];

    entry->type_attr.bits.present = 0;
}

// -------------------------------------------------------------------------
// Private
// -------------------------------------------------------------------------
static void idt_install(struct idt_descriptor* idt)
{
    __asm("LIDT %0": /* no out */ : "m"(*idt));
}

static struct idt_descriptor idt_get()
{
    struct idt_descriptor addr;

    __asm("SIDT %0": /* no out */ : "m"(addr));

    return addr;
}

static void irq_dispatcher(uint8_t irq, struct irq_regs* regs)
{
    struct dispatcher_data* data = &g_dispatcher_data[irq];

    if (data->handler == 0) {
        terminal_write_string("irq_dispatcher, missing handler for interrupt ");
        terminal_write_uint32_x(irq);
        terminal_write_string("\n");
        return;
    }

    data->handler(irq, regs);
}

static void irq_dispatcher_create(struct dispatcher* destination, uint8_t irq)
{
    // CALL is relative to the next instruction
    uintptr_t next_instruction = (uintptr_t)&destination->add_imm;
    uintptr_t target = (uintptr_t)irq_dispatcher;
    uintptr_t relative = target - next_instruction;

    struct dispatcher result = {
        //
        // PUSHA
        .pusha = 0x60,

        // PUSH a pointer to the registers
        // saved on the stack
        .push_esp = 0x54,

        // PUSH the IRQ Number
        .push_imm = 0x6A,
        .push_imm_arg = irq,

        // CALL target
        .call = 0xE8,
        .target = relative,

        // ADD ESP, 0x08 (unwind args from CALL)
        .add_imm = 0x83,
        .add_imm_reg = 0xC4,
        .add_imm_arg = 0x08,

        // POPA
        .popa = 0x61,

        // IRET
        .iret = 0xCF
    };

    *destination = result;
}

static void idt_entry_setup(struct idt_entry* entry, uint8_t irq, gate_type type, uint8_t priv_level)
{
    struct dispatcher* dispatcher = &g_dispatchers[irq];
    irq_dispatcher_create(dispatcher, irq);

    uintptr_t handler_ptr = (uintptr_t)dispatcher;

    entry->offset_low = (uint16_t)(handler_ptr & 0xFFFF);
    entry->offset_high = (uint16_t)((handler_ptr >> 16) & 0xFFFF);
    entry->selector = 0x08;
    entry->type_attr.bits.present = 1;
    entry->type_attr.bits.priv_level = priv_level;
    entry->type_attr.bits.segment = 0;
    entry->type_attr.bits.type = type;
}

static enum kresult idt_entry_verify(struct idt_entry const * const entry, uint8_t const irq, gate_type type, uint8_t const priv_level)
{
    const bool is_valid =
        entry->selector == 0x08 &&
        entry->type_attr.bits.present == 1 &&
        entry->type_attr.bits.segment == 0 &&
        entry->type_attr.bits.priv_level == priv_level &&
        entry->type_attr.bits.type == type;

    return is_valid ? kresult_ok : kresult_invalid;
}

static void gpf(uint8_t irq, struct irq_regs* regs)
{
    KERROR("FAULT: Generation Protection Fault!");
    BREAK();
}

static void page_fault(uint8_t irq, struct irq_regs* regs)
{
    KERROR("FAULT: Page fault!");
    BREAK();
}

static void stack_segment_fault(uint8_t irq, struct irq_regs* regs)
{
    KERROR("FAULT: Stack segment fault!");
    BREAK();
}

static void invalid_tss(uint8_t irq, struct irq_regs* regs)
{
    KERROR("FAULT: Invalid-TSS!");
    BREAK();
}

static void segment_not_present(uint8_t irq, struct irq_regs* regs)
{
    KERROR("FAULT: Segment not present");
    BREAK();
}

static void double_fault(uint8_t irq, struct irq_regs* regs)
{
    KERROR("Double fault!");
    BREAK();
}

