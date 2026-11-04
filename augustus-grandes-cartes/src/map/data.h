#ifndef MAP_DATA_H
#define MAP_DATA_H

extern struct map_data_t {
    int width;
    int height;
    int start_offset;
    int border_size;
    int stride; // pas de la grille allouee (>= width + bords)
} map_data;

#endif // MAP_DATA_H
