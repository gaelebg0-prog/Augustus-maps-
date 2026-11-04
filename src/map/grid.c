#include "grid.h"

#include "building/building.h"
#include "core/log.h"
#include "map/building.h"
#include "map/data.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define OFFSET(x,y) (x + map_grid_stride * y)

struct map_data_t map_data;

int map_grid_stride = MAP_GRID_LEGACY_SIZE;
int map_grid_tiles = MAP_GRID_LEGACY_SIZE * MAP_GRID_LEGACY_SIZE;
int map_grid_expand_offset_x;
int map_grid_expand_offset_y;
int map_grid_expand_stride_delta;

/* --- Registre des grilles allouees sur le tas --- */
#define MAP_GRID_MAX_REGISTRATIONS 128

static struct {
    void **slot;
    size_t element_size;
    int allocated_tiles; // nombre de tuiles pour lequel le tampon a ete alloue
} grid_registry[MAP_GRID_MAX_REGISTRATIONS];
static int grid_registry_count;

void map_grid_register(void **slot, size_t element_size)
{
    for (int i = 0; i < grid_registry_count; i++) {
        if (grid_registry[i].slot == slot) {
            return;
        }
    }
    if (grid_registry_count >= MAP_GRID_MAX_REGISTRATIONS) {
        log_error("Too many map grids registered", 0, 0);
        return;
    }
    grid_registry[grid_registry_count].slot = slot;
    grid_registry[grid_registry_count].element_size = element_size;
    grid_registry[grid_registry_count].allocated_tiles = 0;
    grid_registry_count++;
}

/*
 * Alloue une grille du registre.
 * Si la geometrie n'a pas change, le tampon existant est conserve : les
 * donnees deja chargees (scenario ou sauvegarde) ne doivent pas etre effacees
 * par un appel tardif a map_grid_init/scenario_map_init.
 */
static void grid_allocate_entry(int index)
{
    void **slot = grid_registry[index].slot;
    if (*slot && grid_registry[index].allocated_tiles == map_grid_tiles) {
        return;
    }
    if (*slot) {
        free(*slot);
        *slot = NULL;
    }
    size_t bytes = (size_t) map_grid_tiles * grid_registry[index].element_size;
    *slot = calloc(1, bytes ? bytes : 1);
    if (!*slot) {
        log_error("Out of memory allocating map grid", 0, 0);
        exit(1);
    }
    grid_registry[index].allocated_tiles = map_grid_tiles;
}

void map_grid_allocate_all(void)
{
    for (int i = 0; i < grid_registry_count; i++) {
        grid_allocate_entry(i);
    }
}

void map_grid_release_all(void)
{
    for (int i = 0; i < grid_registry_count; i++) {
        if (*grid_registry[i].slot) {
            free(*grid_registry[i].slot);
            *grid_registry[i].slot = NULL;
        }
        grid_registry[i].allocated_tiles = 0;
    }
}

void *map_grid_items(void **slot, size_t element_size)
{
    map_grid_register(slot, element_size);
    for (int i = 0; i < grid_registry_count; i++) {
        if (grid_registry[i].slot == slot) {
            grid_allocate_entry(i);
            break;
        }
    }
    return *slot;
}

static int direction_delta[8];

/* Voisinage orthogonal/diagonal, dependant du pas de la grille. */
static void compute_direction_delta(void)
{
    int s = map_grid_stride;
    direction_delta[0] = -s;
    direction_delta[1] = 1 - s;
    direction_delta[2] = 1;
    direction_delta[3] = 1 + s;
    direction_delta[4] = s;
    direction_delta[5] = s - 1;
    direction_delta[6] = -1;
    direction_delta[7] = -1 - s;
}

/* Anneau de voisinage pour les tailles 1..7, construit a l'execution. */
static int adjacent_offsets[8][29];

static void compute_adjacent_offsets(void)
{
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 29; j++) {
            adjacent_offsets[i][j] = 0;
        }
    }
    for (int size = 1; size <= 7; size++) {
        if (size == 6) {
            continue; // conserve le comportement historique (anneau vide)
        }
        int *dst = adjacent_offsets[size];
        int n = 0;
        for (int x = 0; x < size; x++) {
            dst[n++] = OFFSET(x, -1);
        }
        for (int y = 0; y < size; y++) {
            dst[n++] = OFFSET(size, y);
        }
        for (int x = size - 1; x >= 0; x--) {
            dst[n++] = OFFSET(x, size);
        }
        for (int y = size - 1; y >= 0; y--) {
            dst[n++] = OFFSET(-1, y);
        }
        dst[n] = 0;
    }
}

static void compute_grid_offsets(void)
{
    compute_direction_delta();
    compute_adjacent_offsets();
}

/* --- Slice pool (100 entries) with ring eviction --- */
#define GRID_SLICE_POOL_CAP 100
static grid_slice grid_slice_pool[GRID_SLICE_POOL_CAP];
static uint8_t grid_slice_pool_used[GRID_SLICE_POOL_CAP];
static int grid_slice_pool_next = 0;

/* Zone de travail partagee pour construire un slice avant de le valider. */
static int *grid_slice_scratch_buffer;
static int grid_slice_scratch_capacity;

static int *grid_slice_scratch(void)
{
    if (grid_slice_scratch_capacity != map_grid_tiles) {
        free(grid_slice_scratch_buffer);
        grid_slice_scratch_buffer = malloc(sizeof(int) * (size_t) map_grid_tiles);
        grid_slice_scratch_capacity = map_grid_tiles;
    }
    return grid_slice_scratch_buffer;
}

static void grid_slice_pool_reset(void)
{
    for (int i = 0; i < GRID_SLICE_POOL_CAP; i++) {
        free(grid_slice_pool[i].grid_offsets);
        grid_slice_pool[i].grid_offsets = NULL;
        grid_slice_pool[i].size = 0;
        grid_slice_pool[i].capacity = 0;
        grid_slice_pool_used[i] = 0;
    }
    grid_slice_pool_next = 0;
}

static grid_slice *grid_slice_pool_commit(const int *offsets, int count)
{
    if (count < 0) {
        count = 0;
    }
    if (count > map_grid_tiles) {
        count = map_grid_tiles;
    }
    int slot = -1;
    for (int i = 0; i < GRID_SLICE_POOL_CAP; i++) {
        if (!grid_slice_pool_used[i]) {
            slot = i;
            break;
        }
    }

    /* none free -> evict oldest (ring) and release its memory */
    if (slot == -1) {
        slot = grid_slice_pool_next;
        grid_slice_pool_next = (grid_slice_pool_next + 1) % GRID_SLICE_POOL_CAP;
        free(grid_slice_pool[slot].grid_offsets);
        grid_slice_pool[slot].grid_offsets = NULL;
        grid_slice_pool[slot].capacity = 0;
    }

    grid_slice_pool_used[slot] = 1;
    grid_slice *s = &grid_slice_pool[slot];
    if (s->capacity < count) {
        int *grown = realloc(s->grid_offsets, sizeof(int) * (size_t) count);
        if (!grown) {
            s->size = 0;
            return s;
        }
        s->grid_offsets = grown;
        s->capacity = count;
    }
    s->size = count;
    if (count > 0 && s->grid_offsets) {
        memcpy(s->grid_offsets, offsets, sizeof(int) * (size_t) count);
    }
    return s;
}

int map_grid_expand(int size)
{
    if (size <= map_data.width && size <= map_data.height) {
        return 0;
    }
    if (size > MAP_GRID_MAX_DIMENSION) {
        size = MAP_GRID_MAX_DIMENSION;
    }
    int old_stride = map_grid_stride;
    int old_start = map_data.start_offset;
    int width = map_data.width;
    int height = map_data.height;
    int new_tiles = size * size;
    int offset_x = (size - width) / 2;
    int offset_y = (size - height) / 2;
    map_grid_expand_offset_x = offset_x;
    map_grid_expand_offset_y = offset_y;
    map_grid_expand_stride_delta = size - map_grid_stride;

    /* Toutes les grilles enregistrees sont transposees dans la nouvelle
     * geometrie : la carte d'origine reste au centre, le reste est vierge. */
    for (int i = 0; i < grid_registry_count; i++) {
        void **slot = grid_registry[i].slot;
        if (!*slot) {
            continue;
        }
        size_t element_size = grid_registry[i].element_size;
        uint8_t *new_data = calloc((size_t) new_tiles, element_size);
        if (!new_data) {
            log_error("Out of memory expanding the map grid", 0, 0);
            return 0;
        }
        const uint8_t *src = (const uint8_t *) *slot;
        for (int y = 0; y < height; y++) {
            size_t src_row = (size_t) (old_start + y * old_stride) * element_size;
            size_t dst_row = (size_t) ((offset_y + y) * size + offset_x) * element_size;
            memcpy(new_data + dst_row, src + src_row, (size_t) width * element_size);
        }
        free(*slot);
        *slot = new_data;
        grid_registry[i].allocated_tiles = new_tiles;
    }

    map_grid_stride = size;
    map_grid_tiles = new_tiles;
    map_data.width = size;
    map_data.height = size;
    map_data.border_size = 0;
    map_data.start_offset = 0;
    map_data.stride = size;
    grid_slice_pool_reset();
    compute_grid_offsets();
    return 1;
}

void map_grid_init(int width, int height, int start_offset, int border_size)
{
    (void) start_offset;

    map_data.width = width > 0 ? width : MAP_GRID_LEGACY_SIZE;
    map_data.height = height > 0 ? height : MAP_GRID_LEGACY_SIZE;

    if (border_size < 0) {
        border_size = MAP_GRID_MIN_BORDER * 2;
    }

    int stride = map_data.width + border_size;
    if (stride < map_data.height + border_size) {
        stride = map_data.height + border_size;
    }
    if (stride < MAP_GRID_LEGACY_SIZE) {
        stride = MAP_GRID_LEGACY_SIZE;
    }
    if (stride > MAP_GRID_MAX_DIMENSION) {
        log_error("Map grid dimension exceeded the maximum, clamped", 0, 0);
        stride = MAP_GRID_MAX_DIMENSION;
    }

    map_data.border_size = border_size > 0 ? border_size : stride - map_data.width;

    /* La carte est centree dans la grille allouee, comme dans Caesar 3. Pour
     * les fichiers existants, l'offset fourni est utilise tel quel : il est
     * solidaire du pas de grille et doit rester identique a l'original. */
    if (start_offset > 0 && start_offset < stride * stride) {
        map_data.start_offset = start_offset;
    } else {
        int x_offset = (stride - map_data.width) / 2;
        int y_offset = (stride - map_data.height) / 2;
        map_data.start_offset = y_offset * stride + x_offset;
    }
    map_data.stride = stride;

    if (stride != map_grid_stride) {
        map_grid_stride = stride;
        map_grid_tiles = stride * stride;
        grid_slice_pool_reset();
    }
    /* Les offsets de voisinage dependent du pas : ils doivent etre calcules
     * meme quand la taille ne change pas (premier chargement a 162). */
    compute_grid_offsets();
    map_grid_allocate_all();
}

/* allocate from pool based on already computed offsets */
static grid_slice *allocate_grid_slice_memory_from_offsets(const int *offsets, int count)
{
    return grid_slice_pool_commit(offsets, count);
}

grid_slice *map_grid_get_grid_slice(int *grid_offsets, int size)
{
    /* size may be larger than MAX_SLICE_SIZE; cap on commit */
    return allocate_grid_slice_memory_from_offsets(grid_offsets, size);
}

grid_slice *map_grid_get_grid_slice_from_corners(int start_x, int start_y, int end_x, int end_y)
{
    int x_min = (start_x < end_x) ? start_x : end_x;
    int x_max = (start_x > end_x) ? start_x : end_x;
    int y_min = (start_y < end_y) ? start_y : end_y;
    int y_max = (start_y > end_y) ? start_y : end_y;

    int width = x_max - x_min + 1;
    int height = y_max - y_min + 1;

    return map_grid_get_grid_slice_rectangle(map_grid_offset(x_min, y_min), width, height);
}

grid_slice *map_grid_get_grid_slice_from_corner_offsets(int corner_offset_1, int corner_offset_2)
{
    int start_x = map_grid_offset_to_x(corner_offset_1);
    int start_y = map_grid_offset_to_y(corner_offset_1);
    int end_x = map_grid_offset_to_x(corner_offset_2);
    int end_y = map_grid_offset_to_y(corner_offset_2);

    return map_grid_get_grid_slice_from_corners(start_x, start_y, end_x, end_y);
}

int map_grid_get_corner_offsets_from_grid_slice(const grid_slice *slice, int *top_left_offset, int *bottom_right_offset)
{
    if (!slice || slice->size == 0) {
        return 0;
    }
    int x_min = 2147483647, y_min = 2147483647; // no max offset values defined, just use INT_MAX
    int x_max = 0, y_max = 0;
    for (int i = 0; i < slice->size; i++) {
        int offset = slice->grid_offsets[i];
        int x = map_grid_offset_to_x(offset);
        int y = map_grid_offset_to_y(offset);
        if (x < x_min) x_min = x;
        if (y < y_min) y_min = y;
        if (x > x_max) x_max = x;
        if (y > y_max) y_max = y;
    }

    *top_left_offset = map_grid_offset(x_min, y_min);
    *bottom_right_offset = map_grid_offset(x_max, y_max);
    return 1;
}

grid_slice *map_grid_get_grid_slice_rectangle(int start_grid_offset, int width, int height)
{
    int x = map_grid_offset_to_x(start_grid_offset);
    int y = map_grid_offset_to_y(start_grid_offset);

    int *tmp = grid_slice_scratch();
    int count = 0;

    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            if (count >= MAX_SLICE_SIZE) {
                return allocate_grid_slice_memory_from_offsets(tmp, count);
            }
            int offset = map_grid_offset(x + j, y + i);
            if (map_grid_is_valid_offset(offset)) {
                tmp[count++] = offset;
            } else {
                /* skip invalid */
            }
        }
    }
    return allocate_grid_slice_memory_from_offsets(tmp, count);
}

grid_slice *map_grid_get_grid_slice_house(unsigned int building_id, int check_rubble)
{
    int *tmp = grid_slice_scratch();
    int count = 0;

    building *b = building_get(building_id);
    int starting_x = map_grid_offset_to_x(b->grid_offset);
    int starting_y = map_grid_offset_to_y(b->grid_offset);
    for (int i = 0; i < 4; i++) // max house size is 4x4
    {
        for (int j = 0; j < 4; j++) {
            if (count >= MAX_SLICE_SIZE) {
                return allocate_grid_slice_memory_from_offsets(tmp, count);
            }
            int offset = map_grid_offset(starting_x + j, starting_y + i);
            if ((check_rubble ? map_building_rubble_building_id(offset) : map_building_at(offset)) == building_id) {
                tmp[count++] = offset;
                continue;
            }
        }
    }
    return allocate_grid_slice_memory_from_offsets(tmp, count);
}

grid_slice *map_grid_get_grid_slice_square(int start_grid_offset, int size)
{
    grid_slice *slice = map_grid_get_grid_slice_rectangle(start_grid_offset, size, size);
    return slice;
}

grid_slice *map_grid_get_grid_slice_ring(int center_grid_offset, int inner_radius, int outer_radius)
{
    int center_x = map_grid_offset_to_x(center_grid_offset);
    int center_y = map_grid_offset_to_y(center_grid_offset);
    int *tmp = grid_slice_scratch();
    int count = 0;

    for (int dy = -outer_radius; dy <= outer_radius; dy++) {
        for (int dx = -outer_radius; dx <= outer_radius; dx++) {
            if (count >= MAX_SLICE_SIZE) {
                return allocate_grid_slice_memory_from_offsets(tmp, count);
            }
            int distance = (abs(dx) > abs(dy)) ? abs(dx) : abs(dy);

            // Include if within ring bounds (greater than inner radius, less than or equal to outer radius)
            if (distance > inner_radius && distance <= outer_radius) {
                int x = center_x + dx;
                int y = center_y + dy;
                if (x < 0 || y < 0) {
                    continue;
                }
                if (x > map_grid_stride || y > map_grid_stride) {
                    continue;
                }
                int offset = map_grid_offset(x, y);
                if (map_grid_is_valid_offset(offset)) {
                    tmp[count++] = offset;
                }
            }
        }
    }
    return allocate_grid_slice_memory_from_offsets(tmp, count);
}

grid_slice *map_grid_get_grid_slice_from_center(int center_grid_offset, int radius)
{
    // A circle is just a ring with inner_radius = 0
    return map_grid_get_grid_slice_ring(center_grid_offset, 0, radius);
}

grid_slice *map_grid_slice_contains(int grid_offset, grid_slice *slice)
{
    if (!slice) {
        return NULL;
    }
    for (int i = 0; i < slice->size; i++) {
        if (slice->grid_offsets[i] == grid_offset) {
            return slice;
        }
    }
    return NULL;
}

int map_grid_is_valid_offset(int grid_offset)
{
    return grid_offset >= 0 && grid_offset < map_grid_tiles;
}

int map_grid_offset(int x, int y)
{
    return map_data.start_offset + x + y * map_grid_stride;
}

int map_grid_offset_to_x(int grid_offset)
{
    return (grid_offset - map_data.start_offset) % map_grid_stride;
}

int map_grid_offset_to_y(int grid_offset)
{
    return (grid_offset - map_data.start_offset) / map_grid_stride;
}

int map_grid_delta(int x, int y)
{
    return y * map_grid_stride + x;
}

/* Les tables historiques expriment leurs decalages dans une grille de 162
 * tuiles de cote. Sur une carte agrandie ces valeurs doivent etre
 * converties vers le pas reellement utilise, sinon les voisins calcules
 * tombent sur la mauvaise ligne. */
int map_grid_convert_legacy_offset(int legacy_offset)
{
    int x = legacy_offset % MAP_GRID_LEGACY_SIZE;
    int y = legacy_offset / MAP_GRID_LEGACY_SIZE;
    return x + map_grid_stride * y;
}

int map_grid_add_delta(int grid_offset, int x, int y)
{
    int raw_x = grid_offset % map_grid_stride;
    int raw_y = grid_offset / map_grid_stride;
    if (raw_x + x < 0 || raw_x + x >= map_grid_stride ||
        raw_y + y < 0 || raw_y + y >= map_grid_stride) {
        return -1;
    }
    return grid_offset + map_grid_delta(x, y);
}

int map_grid_direction_delta(int direction)
{
    if (direction >= 0 && direction < 8) {
        return direction_delta[direction];
    } else {
        return 0;
    }
}

int map_grid_chess_distance(int offset1, int offset2)
{
    int x1 = map_grid_offset_to_x(offset1);
    int y1 = map_grid_offset_to_y(offset1);
    int x2 = map_grid_offset_to_x(offset2);
    int y2 = map_grid_offset_to_y(offset2);

    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);

    return dx > dy ? dx : dy;
}

void map_grid_size(int *width, int *height)
{
    *width = map_data.width;
    *height = map_data.height;
}

int map_grid_width(void)
{
    return map_data.width;
}

int map_grid_height(void)
{
    return map_data.height;
}

void map_grid_bound(int *x, int *y)
{
    if (*x < 0) {
        *x = 0;
    }
    if (*y < 0) {
        *y = 0;
    }
    if (*x >= map_data.width) {
        *x = map_data.width - 1;
    }
    if (*y >= map_data.height) {
        *y = map_data.height - 1;
    }
}

void map_grid_bound_area(int *x_min, int *y_min, int *x_max, int *y_max)
{
    if (*x_min < 0) {
        *x_min = 0;
    }
    if (*y_min < 0) {
        *y_min = 0;
    }
    if (*x_max >= map_data.width) {
        *x_max = map_data.width - 1;
    }
    if (*y_max >= map_data.height) {
        *y_max = map_data.height - 1;
    }
}

void map_grid_get_area(int x, int y, int size, int radius, int *x_min, int *y_min, int *x_max, int *y_max)
{
    *x_min = x - radius;
    *y_min = y - radius;
    *x_max = x + size + radius - 1;
    *y_max = y + size + radius - 1;
    map_grid_bound_area(x_min, y_min, x_max, y_max);
}

void map_grid_start_end_to_area(
    int x_start, int y_start, int x_end, int y_end, int *x_min, int *y_min, int *x_max, int *y_max)
{
    if (x_start < x_end) {
        *x_min = x_start;
        *x_max = x_end;
    } else {
        *x_min = x_end;
        *x_max = x_start;
    }
    if (y_start < y_end) {
        *y_min = y_start;
        *y_max = y_end;
    } else {
        *y_min = y_end;
        *y_max = y_start;
    }
    map_grid_bound_area(x_min, y_min, x_max, y_max);
}

int map_grid_is_inside(int x, int y, int size)
{
    return x >= 0 && x + size <= map_data.width && y >= 0 && y + size <= map_data.height;
}

const int *map_grid_adjacent_offsets(int size)
{
    return adjacent_offsets[size];
}

void map_grid_get_corner_tiles(int start_x, int start_y, int x, int y, int *c1x, int *c1y, int *c2x, int *c2y)
{
    if (x - start_x != 0) {
        *c1x = x;
        *c1y = y - 1;
        *c2x = x;
        *c2y = y + 1;
    } else {
        *c1x = x - 1;
        *c1y = y;
        *c2x = x + 1;
        *c2y = y;
    }
}

void map_grid_clear_i8(int8_t *grid)
{
    memset(grid, 0, map_grid_tiles * sizeof(int8_t));
}

void map_grid_clear_u8(uint8_t *grid)
{
    memset(grid, 0, map_grid_tiles * sizeof(uint8_t));
}

void map_grid_clear_u16(uint16_t *grid)
{
    memset(grid, 0, map_grid_tiles * sizeof(uint16_t));
}

void map_grid_clear_u32(uint32_t *grid)
{
    memset(grid, 0, map_grid_tiles * sizeof(uint32_t));
}

void map_grid_clear_i16(int16_t *grid)
{
    memset(grid, 0, map_grid_tiles * sizeof(int16_t));
}

void map_grid_init_i8(int8_t *grid, int8_t value)
{
    memset(grid, value, map_grid_tiles * sizeof(int8_t));
}

void map_grid_and_u8(uint8_t *grid, uint8_t mask)
{
    for (int i = 0; i < map_grid_tiles; i++) {
        grid[i] &= mask;
    }
}

void map_grid_and_u16(uint16_t *grid, uint16_t mask)
{
    for (int i = 0; i < map_grid_tiles; i++) {
        grid[i] &= mask;
    }
}

void map_grid_and_u32(uint32_t *grid, uint32_t mask)
{
    for (int i = 0; i < map_grid_tiles; i++) {
        grid[i] &= mask;
    }
}

void map_grid_copy_u8(const uint8_t *src, uint8_t *dst)
{
    memcpy(dst, src, map_grid_tiles * sizeof(uint8_t));
}

void map_grid_copy_u16(const uint16_t *src, uint16_t *dst)
{
    memcpy(dst, src, map_grid_tiles * sizeof(uint16_t));
}

void map_grid_copy_u32(const uint32_t *src, uint32_t *dst)
{
    memcpy(dst, src, map_grid_tiles * sizeof(uint32_t));
}

void map_grid_save_state_u8(const uint8_t *grid, buffer *buf)
{
    buffer_write_raw(buf, grid, map_grid_tiles);
}

void map_grid_save_state_i8(const int8_t *grid, buffer *buf)
{
    buffer_write_raw(buf, grid, map_grid_tiles);
}

void map_grid_save_state_u16(const uint16_t *grid, buffer *buf)
{
    for (int i = 0; i < map_grid_tiles; i++) {
        buffer_write_u16(buf, grid[i]);
    }
}

void map_grid_save_state_u32_to_u16(const uint32_t *grid, buffer *buf)
{
    for (int i = 0; i < map_grid_tiles; i++) {
        buffer_write_u16(buf, (uint16_t) grid[i]);
    }
}

void map_grid_save_state_u32(const uint32_t *grid, buffer *buf)
{
    for (int i = 0; i < map_grid_tiles; i++) {
        buffer_write_u32(buf, grid[i]);
    }
}

void map_grid_load_state_u8(uint8_t *grid, buffer *buf)
{
    buffer_read_raw(buf, grid, map_grid_tiles);
}

void map_grid_load_state_i8(int8_t *grid, buffer *buf)
{
    buffer_read_raw(buf, grid, map_grid_tiles);
}

void map_grid_load_state_u16(uint16_t *grid, buffer *buf)
{
    for (int i = 0; i < map_grid_tiles; i++) {
        grid[i] = buffer_read_u16(buf);
    }
}

void map_grid_load_state_u16_to_u32(uint32_t *grid, buffer *buf)
{
    for (int i = 0; i < map_grid_tiles; i++) {
        grid[i] = buffer_read_u16(buf);
    }
}

void map_grid_load_state_u32(uint32_t *grid, buffer *buf)
{
    for (int i = 0; i < map_grid_tiles; i++) {
        grid[i] = buffer_read_u32(buf);
    }
}

int map_grid_get_stride(void)
{
    return map_grid_stride;
}

int map_grid_get_tile_count(void)
{
    return map_grid_tiles;
}
