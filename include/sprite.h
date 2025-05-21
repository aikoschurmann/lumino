#ifndef __SPRITE_H__
#define __SPRITE_H__

#include "lumino.h"

typedef struct {
    int width;
    int height;
    lumino_color* data; // Pointer to the sprite data (RGBA)
} lumino_sprite;

// Function prototypes

// Load a PNG image and convert it to a sprite
lumino_sprite lumino_load_png(LuminoRenderer* renderer, const char* filename);

// Draw a sprite at a specific position
// The sprite is drawn at the top-left corner (x, y)
void lumino_draw_sprite(LuminoRenderer* renderer, int x, int y, lumino_sprite sprite);

void lumino_draw_sprite_blend(LuminoRenderer* renderer, int x, int y, lumino_sprite sprite);


#endif // __SPRITE_H__