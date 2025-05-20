#ifndef __PRIMITIVES_H__
#define __PRIMITIVES_H__

#include "lumino.h"



void lumino_draw_pixel(LuminoRenderer* renderer, int x, int y, uint8_t color_index);
void lumino_draw_line_scalar(LuminoRenderer* renderer, int x1, int y1, int x2, int y2, uint8_t color_index);
void lumino_draw_line_neon(LuminoRenderer* renderer, int x1, int y1, int x2, int y2, uint8_t color_index);
void lumino_draw_line(LuminoRenderer* renderer, int x1, int y1, int x2, int y2, uint8_t color_index);
void lumino_draw_rectangle(LuminoRenderer* renderer, int x, int y, int width, int height, uint8_t color_index);
void lumino_fill_rectangle(LuminoRenderer* renderer, int x, int y, int width, int height, uint8_t color_index);
uint8_t* lumino_load_png(LuminoRenderer* renderer, const char* filename, int* out_width, int* out_height);
void lumino_draw_sprite(LuminoRenderer* renderer, int x, int y, uint8_t* sprite, int sprite_width, int sprite_height);
#endif // __PRIMITIVES_H__