#include "aqueduct.h"

#include "map/grid.h"

#define WATER_ACCESS_OFFSET 7
#define IMAGE_MASK 0x7f

static grid_u8 aqueduct;
MAP_GRID_REGISTER(aqueduct, u8);
static grid_u8 aqueduct_backup;

MAP_GRID_REGISTER(aqueduct_backup, u8);
/* Les grilles d'aqueduc sont indexees directement : un offset hors limites
 * (bord de carte, voisin calcule par addition) lisait hors du tampon alloue
 * sur le tas. Toutes les entrees sont donc bornees ici. */
int map_aqueduct_has_water_access_at(int grid_offset)
{
    if (!map_grid_is_valid_offset(grid_offset)) {
        return 0;
    }
    return aqueduct.items[grid_offset] >> WATER_ACCESS_OFFSET;
}

int map_aqueduct_image_at(int grid_offset)
{
    if (!map_grid_is_valid_offset(grid_offset)) {
        return 0;
    }
    return aqueduct.items[grid_offset] & IMAGE_MASK;
}

void map_aqueduct_set_water_access(int grid_offset, int value)
{
    if (!map_grid_is_valid_offset(grid_offset)) {
        return;
    }
    aqueduct.items[grid_offset] = (value << WATER_ACCESS_OFFSET) | (aqueduct.items[grid_offset] & IMAGE_MASK);
}

void map_aqueduct_set_image(int grid_offset, int value)
{
    if (!map_grid_is_valid_offset(grid_offset)) {
        return;
    }
    aqueduct.items[grid_offset] = (aqueduct.items[grid_offset] & ~IMAGE_MASK) | value;
}

void map_aqueduct_remove(int grid_offset)
{
    if (!map_grid_is_valid_offset(grid_offset)) {
        return;
    }
    aqueduct.items[grid_offset] = 0;
    int up = map_grid_add_delta(grid_offset, 0, -1);
    int right = map_grid_add_delta(grid_offset, 1, 0);
    int down = map_grid_add_delta(grid_offset, 0, 1);
    int left = map_grid_add_delta(grid_offset, -1, 0);
    if (map_aqueduct_image_at(up) == 5) {
        map_aqueduct_set_image(up, 1);
    }
    if (map_aqueduct_image_at(right) == 6) {
        map_aqueduct_set_image(right, 2);
    }
    if (map_aqueduct_image_at(down) == 5) {
        map_aqueduct_set_image(down, 3);
    }
    if (map_aqueduct_image_at(left) == 6) {
        map_aqueduct_set_image(left, 4);
    }
}

void map_aqueduct_clear(void)
{
    map_grid_clear_u8(aqueduct.items);
}

void map_aqueduct_backup(void)
{
    map_grid_copy_u8(aqueduct.items, aqueduct_backup.items);
}

void map_aqueduct_restore(void)
{
    map_grid_copy_u8(aqueduct_backup.items, aqueduct.items);
}

void map_aqueduct_save_state(buffer *buf, buffer *backup)
{
    map_grid_save_state_u8(aqueduct.items, buf);
    map_grid_save_state_u8(aqueduct_backup.items, backup);
}

void map_aqueduct_load_state(buffer *buf, buffer *backup)
{
    map_grid_load_state_u8(aqueduct.items, buf);
    map_grid_load_state_u8(aqueduct_backup.items, backup);
}
