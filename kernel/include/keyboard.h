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
    keys_k,
    keys_l,
    keys_m,
    keys_n,
    keys_o,
    keys_p,
    keys_q,
    keys_r,
    keys_s,
    keys_t,
    keys_u,
    keys_v,
    keys_w,
    keys_x,
    keys_y,
    keys_z,

    keys_0,
    keys_1,
    keys_2,
    keys_3,
    keys_4,
    keys_5,
    keys_6,
    keys_7,
    keys_8,
    keys_9,

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
