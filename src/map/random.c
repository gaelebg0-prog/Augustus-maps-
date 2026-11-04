#include "random.h"

#include "core/random.h"
#include "map/grid.h"

static grid_u8 random;

MAP_GRID_REGISTER(random, u8);
void map_random_clear(void)
{
    map_grid_clear_u8(random.items);
}

void map_random_init(void)
{
    int grid_offset = 0;
    for (int y = 0; y < map_grid_stride; y++) {
        for (int x = 0; x < map_grid_stride; x++, grid_offset++) {
            random_generate_next();
            random.items[grid_offset] = (uint8_t) random_short();
        }
    }
}

void map_random_seed(int grid_offset)
{
    random_generate_next();
    random.items[grid_offset] = (uint8_t) random_short();
}

int map_random_get(int grid_offset)
{
    return random.items[grid_offset];
}

int map_random_get_from_buffer(buffer *buf, int grid_offset)
{
    buffer_set(buf, grid_offset);
    return buffer_read_u8(buf);
}

void map_random_save_state(buffer *buf)
{
    map_grid_save_state_u8(random.items, buf);
}

void map_random_load_state(buffer *buf)
{
    map_grid_load_state_u8(random.items, buf);
}
