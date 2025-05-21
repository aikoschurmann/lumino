#ifndef __PRIMITIVES_H__
#define __PRIMITIVES_H__

#include "lumino.h"



// Function prototypes for drawing primitives

// draw a single pixel at (x, y) with a color (does not blend)
void lumino_draw_pixel(LuminoRenderer* renderer, int x, int y, lumino_color color);

// draw a single pixel at (x, y) with a color (blends)
void lumino_draw_pixel_blend(LuminoRenderer* renderer, int x, int y, lumino_color color);


// LINE DRAWING

// draw a line from (x1, y1) to (x2, y2) with a color
void lumino_draw_line(LuminoRenderer* renderer, int x1, int y1, int x2, int y2, lumino_color color);

// draw a line from (x1, y1) to (x2, y2) with a color (blends)
void lumino_draw_line_blend(LuminoRenderer* renderer, int x1, int y1, int x2, int y2, lumino_color color);

void lumino_draw_line_scalar_blend(LuminoRenderer* renderer, int x1, int y1, int x2, int y2, lumino_color color);
// RECTANGLE DRAWING

// draw a rectangle at (x, y) with width and height with a color
void lumino_draw_rectangle(LuminoRenderer* renderer, int x, int y, int width, int height, lumino_color color);

// fill a rectangle at (x, y) with width and height with a color
void lumino_fill_rectangle(LuminoRenderer* renderer, int x, int y, int width, int height, lumino_color color);

// fill a rectangle at (x, y) with width and height with a color (blends)
void lumino_fill_rectangle_blend(LuminoRenderer* renderer, int x, int y, int width, int height, lumino_color color);


#endif // __PRIMITIVES_H__