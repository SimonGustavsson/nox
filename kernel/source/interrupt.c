#include <kernel.h>
#include <debug.h>
#include <types.h>
#include <interrupt.h>

struct PACKED idt_descriptor {
    uint16_t  limit; // Length of IDT in bytes - 1
    void*     base; // Linear start address
};

typedef union {
    uint8_t raw;

    struct PACKED {
        uint8_t        type:        4;
        uint8_t        segment:     1; // Storage segment (0 for interrupt gates)
        uint8_t        priv_level:  2; // Gate call protection (minimum caller level)
        uint8_t        present:     1; // 0 for unused interrupts
    } bits;
} idt_type_attribute;

typedef struct PACKED {
    uint16_t              offset_low; // 15:0 of offset
    uint16_t              selector;   // Code segment selector in GDT/LDT
    uint8_t               zero;
    idt_type_attribute    type_attr;
    uint16_t              offset_high;  // 31:16 of offset
} idt_entry;

// -------------------------------------------------------------------------
// Forward Declares
// -------------------------------------------------------------------------
static struct idt_descriptor idt_get();
static void idt_install(struct idt_descriptor* idt);
static void default_dispatchers_init();

// -------------------------------------------------------------------------
// Globals
// -------------------------------------------------------------------------
static struct idt_descriptor idt_descriptor = {};
static idt_entry idt_entries[256] = {};

// -------------------------------------------------------------------------
// Exports
// -------------------------------------------------------------------------
void interrupt_init_system()
{
    idt_descriptor.limit = sizeof(idt_entries) - 1;
    idt_descriptor.base = &idt_entries;
    idt_install(&idt_descriptor);

    default_dispatchers_init();
}

void interrupt_disable_all()
{
    __asm("cli");
}

void interrupt_enable_all()
{
    __asm ("sti");
}

void interrupt_install_handler(uint8_t irq, interrupt_handler handler, gate_type type, uint8_t priv_level)
{
    struct idt_descriptor idt_descriptor;
    idt_descriptor = idt_get();
    uintptr_t handler_ptr = (uintptr_t)handler;

    idt_entry* idts = (idt_entry*)idt_descriptor.base;
    idt_entry* entry = &idts[irq];
    entry->offset_low = (uint16_t)(handler_ptr & 0xFFFF);
    entry->offset_high = (uint16_t)((handler_ptr >> 16) & 0xFFFF);
    entry->selector = 0x08;
    entry->type_attr.bits.present = 1;
    entry->type_attr.bits.priv_level = priv_level;
    entry->type_attr.bits.segment = 0;
    entry->type_attr.bits.type = type;
}

void interrupt_remove_handler(uint8_t irq)
{
    // DO IT
}

void interrupt_enable_handler(uint8_t irq)
{
    struct idt_descriptor idt_descriptor = idt_get();

    idt_entry* idts = (idt_entry*)idt_descriptor.base;
    idt_entry* entry = &idts[irq];

    entry->type_attr.bits.present = 1;
}

void interrupt_disableHandler(uint8_t irq)
{
    struct idt_descriptor idt_descriptor = idt_get();

    idt_entry* idts = (idt_entry*)idt_descriptor.base;
    idt_entry* entry = &idts[irq];

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

struct PACKED default_dispatcher
{
    uint8_t pusha;
    uint8_t push_imm;
    uint8_t push_imm_arg;
    uint8_t call;
    void*   target;
    uint8_t add_imm;
    uint8_t add_imm_reg;
    uint8_t add_imm_arg;
    uint8_t popa;
    uint8_t iret;
};

static struct default_dispatcher default_dispatchers[256] = {};

static struct default_dispatcher mk_default_dispatcher(void* target, uint8_t which)
{
    // CALL is relative to the next instruction
    void* next_instruction = &default_dispatchers[which].add_imm;
    void* relative = target - next_instruction;

    struct default_dispatcher result = {
        //
        // PUSHA
        .pusha = 0x60,
        .push_imm = 0x6A,
        .push_imm_arg = which,

        // CALL target
        .call = 0xE8,
        .target = relative,

        // ADD ESP, 0x04 (unwind arg from CALL)
        .add_imm = 0x83,
        .add_imm_reg = 0xC4,
        .add_imm_arg = 0x04,

        // POPA
        .popa = 0x61,

        // IRET
        .iret = 0xCF
    };

    return result;
}

static void default_handler(uint8_t which)
{
    // TODO: PoC only...
    #include <terminal.h>

    terminal_write_string("In unknown interrupt: ");
    terminal_write_hex(which);
    terminal_write_string("\n");
}

static void default_dispatchers_init()
{
    for (int i = 0x00; i <= 0xFF; i++) {
        default_dispatchers[i] = mk_default_dispatcher(default_handler, i);
    }

    for (int i = 0x20; i <= 0xFF; i++) {
        interrupt_install_handler(i, (void*)&default_dispatchers[i], gate_type_interrupt32, 0);
    }
}

