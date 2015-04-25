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
    { 0x12, sc_map_entry_type_press,   keys_e },
    { 0x17, sc_map_entry_type_press,   keys_i },
    { 0x1E, sc_map_entry_type_press,   keys_a },
    { 0x20, sc_map_entry_type_press,   keys_d },
    { 0x21, sc_map_entry_type_press,   keys_f },
    { 0x22, sc_map_entry_type_press,   keys_g },
    { 0x23, sc_map_entry_type_press,   keys_h },
    { 0x24, sc_map_entry_type_press,   keys_j },
    { 0x25, sc_map_entry_type_press,   keys_k },
    { 0x2E, sc_map_entry_type_press,   keys_c },
    { 0x30, sc_map_entry_type_press,   keys_b },
    { 0x92, sc_map_entry_type_release, keys_e },
    { 0x97, sc_map_entry_type_release, keys_i },
    { 0x9E, sc_map_entry_type_release, keys_a },
    { 0xA0, sc_map_entry_type_release, keys_d },
    { 0xA1, sc_map_entry_type_release, keys_f },
    { 0xA2, sc_map_entry_type_release, keys_g },
    { 0xA3, sc_map_entry_type_release, keys_h },
    { 0xA4, sc_map_entry_type_release, keys_j },
    { 0xA5, sc_map_entry_type_release, keys_k },
    { 0xAE, sc_map_entry_type_release, keys_c },
    { 0xB0, sc_map_entry_type_release, keys_b },
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
