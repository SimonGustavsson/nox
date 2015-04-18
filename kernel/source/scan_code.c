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
    { 0x1E, sc_map_entry_type_press,   keys_a },
    { 0x2E, sc_map_entry_type_press,   keys_c },
    { 0x30, sc_map_entry_type_press,   keys_b },
    { 0x9E, sc_map_entry_type_release, keys_a },
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
