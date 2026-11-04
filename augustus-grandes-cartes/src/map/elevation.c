#include "elevation.h"

#include "map/data.h"
#include "map/grid.h"
#include "map/terrain.h"

static grid_u8 elevation;

MAP_GRID_REGISTER(elevation, u8);
int map_elevation_at(int grid_offset)
{
    return map_grid_is_valid_offset(grid_offset) ? elevation.items[grid_offset] : 0;
}

void map_elevation_set(int grid_offset, int value)
{
    if (map_grid_is_valid_offset(grid_offset)) {
        elevation.items[grid_offset] = value;
    }
}

/* Lecture d'un voisin sans depasser la carte : sans bordure, une addition
 * directe sortait du tampon (offset negatif) sur la premiere colonne/ligne. */
static int neighbor_elevation(int grid_offset, int dx, int dy, int max)
{
    int neighbor = map_grid_add_delta(grid_offset, dx, dy);
    if (!map_grid_is_valid_offset(neighbor)) {
        return 0;
    }
    return elevation.items[neighbor] < max && map_terrain_get(neighbor) != TERRAIN_MAP_EDGE;
}

void map_elevation_clear(void)
{
    map_grid_clear_u8(elevation.items);
}

static void fix_cliff_tiles(int grid_offset)
{
    // reduce elevation when the surrounding tiles are at least 2 lower
    // treat edges as being equal elevations
    int max = elevation.items[grid_offset] - 1;
    if (neighbor_elevation(grid_offset, -1, 0, max) ||
        neighbor_elevation(grid_offset, 0, -1, max) ||
        neighbor_elevation(grid_offset, 1, 0, max) ||
        neighbor_elevation(grid_offset, 0, 1, max)) {
        elevation.items[grid_offset]--;
    }
}

void map_elevation_remove_cliffs(void)
{
    // elevation is max 5, so we need 4 passes to fix the lot
    for (int level = 0; level < 4; level++) {
        int grid_offset = map_data.start_offset;
        for (int y = 0; y < map_data.height; y++, grid_offset += map_data.border_size) {
            for (int x = 0; x < map_data.width; x++, grid_offset++) {
                if (elevation.items[grid_offset] > 0) {
                    fix_cliff_tiles(grid_offset);
                }
            }
        }
    }
}

void map_elevation_save_state(buffer *buf)
{
    map_grid_save_state_u8(elevation.items, buf);
}

void map_elevation_load_state(buffer *buf)
{
    map_grid_load_state_u8(elevation.items, buf);
}
