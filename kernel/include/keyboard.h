#ifndef KEYBOARD_H
#define KEYBOARD_H

enum keys {
    keys_a,
    keys_b,
    keys_c,
    keys_d,
    keys_e,
    keys_f,
    keys_g,
    keys_h,
    keys_i,
    keys_j,

    keys_shift      = 16384,
    keys_ctrl       = 16385,
    keys_alt        = 16386,

    keys_up         = 32768,
};

struct kb_subscriber {
    void (*down)(enum keys key);
    void (*up)(enum keys key);
};

void kb_subscribe(struct kb_subscriber* subscriber);
void kb_init();
void kb_unsubscribe(struct kb_subscriber* subscriber);
char kb_key_to_ascii(enum keys key);

#endif
