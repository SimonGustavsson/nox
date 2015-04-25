#include <stdint.h>
#include <keyboard.h>
#include <scan_code.h>

// ------------------------------------------------------------
// Common Code
// ------------------------------------------------------------
int sc_get_entry_index(struct sc_map* map, uint8_t scan_code)
{
    for (int i = 0; i < map->entry_count; i++) {
        uint8_t compare_code = map->entries[i].scan_code;

        if (compare_code == scan_code) {
            return i;
        }
        else if (compare_code > scan_code) {
            return -1;
        }
    }

    return -1;
}

// ------------------------------------------------------------
// SET 1 - NOTE: IT IS IMPORTANT TO KEEP THESE SORTED!
// ------------------------------------------------------------
static struct sc_map_entry g_level1[] = {
    { 0x02, sc_map_entry_type_press,   keys_1 },
    { 0x03, sc_map_entry_type_press,   keys_2 },
    { 0x04, sc_map_entry_type_press,   keys_3 },
    { 0x05, sc_map_entry_type_press,   keys_4 },
    { 0x06, sc_map_entry_type_press,   keys_5 },
    { 0x07, sc_map_entry_type_press,   keys_6 },
    { 0x08, sc_map_entry_type_press,   keys_7 },
    { 0x09, sc_map_entry_type_press,   keys_8 },
    { 0x0A, sc_map_entry_type_press,   keys_9 },
    { 0x0B, sc_map_entry_type_press,   keys_0 },
    { 0x10, sc_map_entry_type_press,   keys_q },
    { 0x11, sc_map_entry_type_press,   keys_w },
    { 0x12, sc_map_entry_type_press,   keys_e },
    { 0x13, sc_map_entry_type_press,   keys_r },
    { 0x14, sc_map_entry_type_press,   keys_t },
    { 0x15, sc_map_entry_type_press,   keys_y },
    { 0x16, sc_map_entry_type_press,   keys_u },
    { 0x17, sc_map_entry_type_press,   keys_i },
    { 0x18, sc_map_entry_type_press,   keys_o },
    { 0x19, sc_map_entry_type_press,   keys_p },
    { 0x1E, sc_map_entry_type_press,   keys_a },
    { 0x1F, sc_map_entry_type_press,   keys_s },
    { 0x20, sc_map_entry_type_press,   keys_d },
    { 0x21, sc_map_entry_type_press,   keys_f },
    { 0x22, sc_map_entry_type_press,   keys_g },
    { 0x23, sc_map_entry_type_press,   keys_h },
    { 0x24, sc_map_entry_type_press,   keys_j },
    { 0x25, sc_map_entry_type_press,   keys_k },
    { 0x26, sc_map_entry_type_press,   keys_l },
    { 0x2C, sc_map_entry_type_press,   keys_z },
    { 0x2D, sc_map_entry_type_press,   keys_x },
    { 0x2E, sc_map_entry_type_press,   keys_c },
    { 0x2F, sc_map_entry_type_press,   keys_v },
    { 0x30, sc_map_entry_type_press,   keys_b },
    { 0x31, sc_map_entry_type_press,   keys_n },
    { 0x32, sc_map_entry_type_press,   keys_m },
    { 0x82, sc_map_entry_type_release, keys_1 },
    { 0x83, sc_map_entry_type_release, keys_2 },
    { 0x84, sc_map_entry_type_release, keys_3 },
    { 0x85, sc_map_entry_type_release, keys_4 },
    { 0x86, sc_map_entry_type_release, keys_5 },
    { 0x87, sc_map_entry_type_release, keys_6 },
    { 0x88, sc_map_entry_type_release, keys_7 },
    { 0x89, sc_map_entry_type_release, keys_8 },
    { 0x8A, sc_map_entry_type_release, keys_9 },
    { 0x8B, sc_map_entry_type_release, keys_0 },
    { 0x90, sc_map_entry_type_release, keys_q },
    { 0x91, sc_map_entry_type_release, keys_w },
    { 0x92, sc_map_entry_type_release, keys_e },
    { 0x93, sc_map_entry_type_release, keys_r },
    { 0x94, sc_map_entry_type_release, keys_t },
    { 0x95, sc_map_entry_type_release, keys_y },
    { 0x96, sc_map_entry_type_release, keys_u },
    { 0x97, sc_map_entry_type_release, keys_i },
    { 0x98, sc_map_entry_type_release, keys_o },
    { 0x99, sc_map_entry_type_release, keys_p },
    { 0x9E, sc_map_entry_type_release, keys_a },
    { 0x9F, sc_map_entry_type_release, keys_s },
    { 0xA0, sc_map_entry_type_release, keys_d },
    { 0xA1, sc_map_entry_type_release, keys_f },
    { 0xA2, sc_map_entry_type_release, keys_g },
    { 0xA3, sc_map_entry_type_release, keys_h },
    { 0xA4, sc_map_entry_type_release, keys_j },
    { 0xA5, sc_map_entry_type_release, keys_k },
    { 0xA6, sc_map_entry_type_release, keys_l },
    { 0xAC, sc_map_entry_type_release, keys_z },
    { 0xAD, sc_map_entry_type_release, keys_x },
    { 0xAE, sc_map_entry_type_release, keys_c },
    { 0xAF, sc_map_entry_type_release, keys_v },
    { 0xB0, sc_map_entry_type_release, keys_b },
    { 0xB1, sc_map_entry_type_release, keys_n },
    { 0xB2, sc_map_entry_type_release, keys_m },
    { 0xE0, sc_map_entry_type_map,     sc_map_index_2 },
};

static struct sc_map_entry g_level2[] = {
    { 0x48, sc_map_entry_type_press,   keys_up },
    { 0xC8, sc_map_entry_type_release, keys_up },
};

static struct sc_set g_set = {
    .maps = {
        [0] = { sizeof(g_level1) / sizeof(struct sc_map_entry), g_level1 },
        [1] = { sizeof(g_level2) / sizeof(struct sc_map_entry), g_level2 },
    }
};

struct sc_set* sc_get_set_1() {
    return &g_set;
}
