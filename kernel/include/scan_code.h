enum sc_map_entry_type {
    sc_map_entry_type_press,
    sc_map_entry_type_release,
    sc_map_entry_type_map
};

struct sc_map_entry {
    uint8_t                 scan_code;
    enum sc_map_entry_type  type;
    uint32_t                data;
};

enum sc_map_index {
    sc_map_index_1 = 0,
    sc_map_index_2 = 1,
    sc_map_index_3 = 2,
    sc_map_index_4 = 3,
    sc_map_index_5 = 4,
    sc_map_index_6 = 5
};

struct sc_map {
    uintmax_t               entry_count;
    struct sc_map_entry*    entries;
};

struct sc_set {
    struct sc_map           maps[6];
};

struct sc_set*              sc_get_set_1();
struct sc_set*              sc_get_set_2();
int                         sc_get_entry_index(struct sc_map* map, uint8_t scan_code);
