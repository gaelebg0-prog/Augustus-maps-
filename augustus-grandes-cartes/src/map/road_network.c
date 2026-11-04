#include "road_network.h"

#include "city/map.h"
#include "map/data.h"
#include "map/grid.h"
#include "map/routing_terrain.h"
#include "map/terrain.h"

#include <stdlib.h>
#include <string.h>

static const int ADJACENT_OFFSETS[] = {-MAP_GRID_STATIC_STRIDE, 1, MAP_GRID_STATIC_STRIDE, -1};

static grid_u8 network;

MAP_GRID_REGISTER(network, u8);
/* La file doit suivre la taille de la carte : une carte 512x512 compte bien
 * plus de tuiles de route que les 162x162 d'origine. */
static struct {
    int *items;
    int capacity;
    int head;
    int tail;
} queue;

static void ensure_queue_capacity(void)
{
    int capacity = map_grid_tiles + 1;
    if (queue.items && queue.capacity == capacity) {
        return;
    }
    free(queue.items);
    queue.items = malloc(sizeof(int) * (size_t) capacity);
    queue.capacity = queue.items ? capacity : 0;
    queue.head = 0;
    queue.tail = 0;
}

void map_road_network_clear(void)
{
    map_grid_clear_u8(network.items);
}

int map_road_network_get(int grid_offset)
{
    ensure_queue_capacity();
    if (!queue.items) {
        return 0;
    }
    return network.items[grid_offset];
}

static int mark_road_network(int grid_offset, uint8_t network_id)
{
    ensure_queue_capacity();
    if (!queue.items) {
        return 0;
    }
    queue.head = 0;
    queue.tail = 0;
    int guard = 0;
    int next_offset;
    int size = 1;
    do {
        if (++guard >= map_grid_tiles) {
            break;
        }
        network.items[grid_offset] = network_id;
        next_offset = -1;
        for (int i = 0; i < 4; i++) {
            int new_offset = map_grid_add_legacy_offset(grid_offset, ADJACENT_OFFSETS[i]);
            if (map_routing_citizen_is_passable(new_offset) && !network.items[new_offset]) {
                if (
                    map_routing_citizen_is_road(new_offset) ||
                    map_terrain_is(new_offset, TERRAIN_ACCESS_RAMP) ||
                    map_routing_citizen_is_highway(new_offset)
                ) {
                    network.items[new_offset] = network_id;
                    size++;
                    if (next_offset == -1) {
                        next_offset = new_offset;
                    } else {
                        queue.items[queue.tail++] = new_offset;
                        if (queue.tail >= queue.capacity) {
                            queue.tail = 0;
                        }
                    }
                }
            }
        }
        if (next_offset == -1) {
            if (queue.head == queue.tail) {
                return size;
            }
            next_offset = queue.items[queue.head++];
            if (queue.head >= queue.capacity) {
                queue.head = 0;
            }
        }
        grid_offset = next_offset;
    } while (next_offset > -1);
    return size;
}

void map_road_network_update(void)
{
    city_map_clear_largest_road_networks();
    map_grid_clear_u8(network.items);
    int network_id = 1;
    int grid_offset = map_data.start_offset;
    for (int y = 0; y < map_data.height; y++, grid_offset += map_data.border_size) {
        for (int x = 0; x < map_data.width; x++, grid_offset++) {
            if (map_terrain_is(grid_offset, TERRAIN_ROAD) && !network.items[grid_offset]) {
                int size = mark_road_network(grid_offset, network_id);
                city_map_add_to_largest_road_networks(network_id, size);
                network_id++;
            }
        }
    }
}
