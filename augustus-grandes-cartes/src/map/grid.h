#ifndef MAP_GRID_H
#define MAP_GRID_H

#include "core/buffer.h"

#include <stddef.h>
#include <stdint.h>

/**
 * Taille historique d'une grille Caesar 3 : 162x162 tuiles, bords compris.
 *
 * Cette valeur n'est plus une limite structurelle : elle sert uniquement de
 * taille par defaut et de repere pour la compatibilite des formats de fichier
 * existants (sauvegardes et scenarios originaux, qui ne contiennent aucune
 * dimension et supposent donc 162x162).
 */
#define MAP_GRID_LEGACY_SIZE 162


/*
 * Taille historique utilisee UNIQUEMENT par les tables de voisinage
 * precalculees au chargement du programme (initialiseurs statiques), qui ne
 * peuvent pas dependre du pas d'execution. Ces tables devront etre converties
 * en calculs dynamiques pour que les cartes plus grandes soient exactes.
 */
#define MAP_GRID_STATIC_STRIDE MAP_GRID_LEGACY_SIZE
#define MAP_GRID_STATIC_TILES (MAP_GRID_LEGACY_SIZE * MAP_GRID_LEGACY_SIZE)

/** Marge minimale (en tuiles) entre la zone jouable et le bord de la grille. */
#define MAP_GRID_MIN_BORDER 2

/** Garde-fou superieur sur un cote de grille (la vraie limite est la RAM). */
#define MAP_GRID_MAX_DIMENSION 8192

/**
 * Cote (en tuiles) vers lequel les cartes de scenario sont etendues au
 * chargement. La carte d'origine est centree dans cette grille et le reste
 * est rempli de terre constructible.
 */
#define MAP_GRID_EXPANDED_SIZE 512

/**
 * Pas de la grille allouee : nombre de tuiles par ligne.
 *
 * C'est l'ancien GRID_SIZE, mais connu a l'execution. Toute l'arithmetique
 * d'offsets (voisinage, lignes, colonnes) doit passer par cette valeur.
 */
extern int map_grid_stride;

/** Nombre total de tuiles allouees (map_grid_stride * map_grid_stride). */
extern int map_grid_tiles;

/** Ancien MAX_SLICE_SIZE, desormais egal au nombre de tuiles allouees. */
#define MAX_SLICE_SIZE (map_grid_tiles)

/**
 * Represents a collection of grid offsets
 *
 * Used to store multiple grid coordinates as a contiguous array of offsets,
 * for operations on groups of tiles - allows easy iteration through uneven shapes
 *
 * @var grid_offsets Array containing the grid offset positions (heap allocated)
 * @var size 1-based count of valid entries in grid_offsets, 0 means empty arrray, 1 means [0] is valid, etc.
 * @var capacity Number of entries actually allocated in grid_offsets
 */
typedef struct grid_slice {
    int *grid_offsets;
    int size;
    int capacity;
} grid_slice;

/*
 * Les grilles sont des vues sur des tableaux alloues sur le tas.
 * Le champ "items" garde exactement le meme nom et le meme usage qu'avant,
 * donc tout le code qui fait grid.items[offset] reste valide.
 */
typedef struct {
    uint8_t *items;
} grid_u8;

typedef struct {
    int8_t *items;
} grid_i8;

typedef struct {
    uint16_t *items;
} grid_u16;

typedef struct {
    int16_t *items;
} grid_i16;

typedef struct {
    uint32_t *items;
} grid_u32;

/**
 * Enregistre une grille aupres de l'allocateur de carte.
 *
 * A appeler une seule fois par grille, idealement via la macro
 * MAP_GRID_REGISTER placee juste apres la declaration de la grille.
 * L'allocateur (re)alloue et vide toutes les grilles enregistrees a chaque
 * changement de taille de carte.
 */
void map_grid_register(void **slot, size_t element_size);

/** Alloue (ou realloue) toutes les grilles enregistrees et les remet a zero. */
void map_grid_allocate_all(void);

/**
 * Etend la carte a une grille carree de `size` cotes.
 * Le contenu existant est conserve et centre ; la nouvelle surface est videe.
 * @return 1 si la carte a ete etendue, 0 si elle etait deja assez grande
 */
int map_grid_expand(int size);

/** Decalage applique au contenu existant par le dernier map_grid_expand. */
extern int map_grid_expand_offset_x;
extern int map_grid_expand_offset_y;
extern int map_grid_expand_stride_delta;

/** Libere toutes les grilles enregistrees. */
void map_grid_release_all(void);

/** Retourne la grille allouee, en l'allouant si necessaire. */
void *map_grid_items(void **slot, size_t element_size);

#define MAP_GRID_CONCAT_(a, b) a##b
#define MAP_GRID_CONCAT(a, b) MAP_GRID_CONCAT_(a, b)

/*
 * Enregistrement automatique au demarrage.
 * GCC/Clang utilisent l'attribut constructor ; MSVC utilise la meme mecanique
 * que le CRT (.CRT$XCU), donc le code reste portable.
 */
#if defined(_MSC_VER)
#define MAP_GRID_REGISTER(name, type)                                                                   \
    static void MAP_GRID_CONCAT(map_grid_register_, __LINE__)(void);                                    \
    __declspec(allocate(".CRT$XCU")) static void (*MAP_GRID_CONCAT(map_grid_ctor_, __LINE__))(void) =   \
        MAP_GRID_CONCAT(map_grid_register_, __LINE__);                                                  \
    static void MAP_GRID_CONCAT(map_grid_register_, __LINE__)(void)                                     \
    {                                                                                                   \
        map_grid_register((void **) &(name).items, sizeof((name).items[0]));                                       \
    }
#else
#define MAP_GRID_REGISTER(name, type)                                                                   \
    static void MAP_GRID_CONCAT(map_grid_register_, __LINE__)(void) __attribute__((constructor));        \
    static void MAP_GRID_CONCAT(map_grid_register_, __LINE__)(void)                                     \
    {                                                                                                   \
        map_grid_register((void **) &(name).items, sizeof((name).items[0]));                                       \
    }
#endif

void map_grid_init(int width, int height, int start_offset, int border_size);

grid_slice *map_grid_get_grid_slice(int *grid_offsets, int size);

grid_slice *map_grid_get_grid_slice_from_corners(int start_x, int start_y, int end_x, int end_y);
grid_slice *map_grid_get_grid_slice_from_corner_offsets(int corner_offset_1, int corner_offset_2);
/** @brief Uses a loop instead of first and last position, since a grid slice is not ncessarily an ordered array
 * @returns 1 on success, 0 on failure
 */
int map_grid_get_corner_offsets_from_grid_slice(const grid_slice *slice, int *top_left_offset, int *bottom_right_offset);

int map_grid_is_valid_offset(int grid_offset);

int map_grid_offset(int x, int y);

int map_grid_offset_to_x(int grid_offset);

int map_grid_offset_to_y(int grid_offset);

int map_grid_delta(int x, int y);

/**
 * Adds the specified X and Y to the given offset with error checking
 * @return New grid offset, or -1 if the x/y would wrap around to a different row/column
 */
int map_grid_add_delta(int grid_offset, int x, int y);

/** Convertit un decalage calcule pour la grille historique 162x162 vers la grille courante. */
int map_grid_convert_legacy_offset(int legacy_offset);

/** Applique un offset historique en respectant les limites X/Y de la grille. */
int map_grid_add_legacy_offset(int grid_offset, int legacy_offset);

int map_grid_direction_delta(int direction);

int map_grid_chess_distance(int offset1, int offset2);

void map_grid_size(int *width, int *height);

int map_grid_width(void);

int map_grid_height(void);

void map_grid_bound(int *x, int *y);

void map_grid_bound_area(int *x_min, int *y_min, int *x_max, int *y_max);

void map_grid_get_area(int x, int y, int size, int radius, int *x_min, int *y_min, int *x_max, int *y_max);

void map_grid_start_end_to_area(int x_start, int y_start, int x_end, int y_end, int *x_min, int *y_min, int *x_max, int *y_max);

int map_grid_is_inside(int x, int y, int size);

const int *map_grid_adjacent_offsets(int size);

void map_grid_get_corner_tiles(int start_x, int start_y, int x, int y, int *c1x, int *c1y, int *c2x, int *c2y);

void map_grid_clear_u8(uint8_t *grid);

void map_grid_clear_i8(int8_t *grid);

void map_grid_clear_u16(uint16_t *grid);

void map_grid_clear_i16(int16_t *grid);

void map_grid_clear_u32(uint32_t *grid);

void map_grid_init_i8(int8_t *grid, int8_t value);

void map_grid_and_u8(uint8_t *grid, uint8_t mask);

void map_grid_and_u16(uint16_t *grid, uint16_t mask);

void map_grid_and_u32(uint32_t *grid, uint32_t mask);

void map_grid_copy_u8(const uint8_t *src, uint8_t *dst);

void map_grid_copy_u16(const uint16_t *src, uint16_t *dst);

void map_grid_copy_u32(const uint32_t *src, uint32_t *dst);


void map_grid_save_state_u8(const uint8_t *grid, buffer *buf);

void map_grid_save_state_i8(const int8_t *grid, buffer *buf);

void map_grid_save_state_u16(const uint16_t *grid, buffer *buf);

void map_grid_save_state_u32_to_u16(const uint32_t *grid, buffer *buf);

void map_grid_save_state_u32(const uint32_t *grid, buffer *buf);

void map_grid_load_state_u8(uint8_t *grid, buffer *buf);

void map_grid_load_state_i8(int8_t *grid, buffer *buf);

void map_grid_load_state_u16(uint16_t *grid, buffer *buf);

void map_grid_load_state_u16_to_u32(uint32_t *grid, buffer *buf);

void map_grid_load_state_u32(uint32_t *grid, buffer *buf);

/**
 * @brief Creates a grid slice representing a rectangular area starting from the given grid offset.
 * All grid points within the specified width and height are included.
 *
 * @param start_grid_offset Grid offset of the top-left corner of the rectangle
 * @param width Width of the rectangle in grid units
 * @param height Height of the rectangle in grid units
 * @return Allocated grid_slice containing rectangle coordinates, or NULL on failure
 */
grid_slice *map_grid_get_grid_slice_rectangle(int start_grid_offset, int width, int height);

/**
 * @brief Creates a grid slice representing area occupied by the given house building.
 *
 * @param building_id id to search for
 * @param check_rubble if true, will check for rubble info grid instead of building grid
 * @return Allocated grid_slice containing rectangle coordinates, or NULL on failure
 */
grid_slice *map_grid_get_grid_slice_house(unsigned int building_id, int check_rubble);

/**
 * @brief Creates a grid slice representing a square area starting from the given grid offset.
 * All grid points within the specified size x size area are included.
 *
 * @param start_grid_offset Grid offset of the top-left corner of the square
 * @param size Side length of the square in grid units
 * @return Allocated grid_slice containing square coordinates, or NULL on failure
 */
grid_slice *map_grid_get_grid_slice_square(int start_grid_offset, int size);

/**
 * @brief Creates a grid slice representing a square ring centered at the given grid offset.
 *
 * @param center_grid_offset Grid offset of the ring center
 * @param inner_radius Inner radius of the ring (exclusive, using chess distance)
 * @param outer_radius Outer radius of the ring (inclusive, using chess distance)
 * @return Allocated grid_slice containing ring coordinates, or NULL on failure
 */
grid_slice *map_grid_get_grid_slice_ring(int center_grid_offset, int inner_radius, int outer_radius);

/**
 * @brief Creates a grid slice representing a square centered at the given grid offset.
 * All grid points within the specified radius are included using chess distance, producing a square shape.
 *
 * @param center_grid_offset Grid offset of the circle center
 * @param radius Radius of the shape (using chess distance)
 * @return Allocated grid_slice, or NULL on failure
 */
grid_slice *map_grid_get_grid_slice_from_center(int center_grid_offset, int radius);

grid_slice *map_grid_slice_contains(int grid_offset, grid_slice *slice);

#endif // MAP_GRID_H
