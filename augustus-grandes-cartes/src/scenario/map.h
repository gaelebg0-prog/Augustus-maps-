#ifndef SCENARIO_MAP_H
#define SCENARIO_MAP_H

#include "map/point.h"

void scenario_map_init(void);

/**
 * Etend la carte du scenario a MAP_GRID_EXPANDED_SIZE, contenu centre.
 * @return 1 si la carte a ete etendue, 0 sinon
 */
int scenario_map_expand(void);

int scenario_map_size(void);

/** Nombre de tailles proposees par l'option "taille de la carte". */
#define MAP_EXPAND_SIZE_COUNT 9

/** Tailles disponibles (en tuiles de cote), de la carte d'origine a 2048. */
extern const int map_expand_size_options[MAP_EXPAND_SIZE_COUNT];

void scenario_map_init_entry_exit(void);

map_point scenario_map_entry(void);

map_point scenario_map_exit(void);

int scenario_map_has_river_entry(void);

map_point scenario_map_river_entry(void);

int scenario_map_has_river_exit(void);

map_point scenario_map_river_exit(void);

void scenario_map_foreach_herd_point(void (*callback)(int x, int y));

void scenario_map_foreach_fishing_point(void (*callback)(int x, int y));

int scenario_map_closest_fishing_point(int x, int y, map_point *fish);

int scenario_map_has_flotsam(void);

#endif // SCENARIO_MAP_H
