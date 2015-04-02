#ifndef NOX_INTERRUPT_H
#define NOX_INTERRUPT_H

typedef void (*interrupt_handler)();

typedef enum {
    gate_type_task32      = 0x5,
    gate_type_interrupt16 = 0x6,
    gate_type_trap16      = 0x7,
    gate_type_interrupt32 = 0xE,
    gate_type_trap32      = 0xF
} gate_type;

void interrupt_init_system();
void interrupt_install_handler(uint8_t irq, interrupt_handler handler, gate_type type, uint8_t priv_level);
void interrupt_enable_handler(uint8_t irq);
void interrupt_disable_handler(uint8_t irq);

#define interrupt_receive(irq, handler) interrupt_install_handler(irq, handler, gate_type_interrupt32, 0)
#define interrupt_receive_trap(irq, handler) interrupt_install_handler(irq, handler, gate_type_trap32, 0)

#endif
