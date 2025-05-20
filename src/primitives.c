#include <stdint.h>
#include "lumino.h"
#include "primitives.h"
#include <arm_neon.h>
#include <math.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"


// Draw a single pixel at (x, y) with a color
void lumino_draw_pixel(LuminoRenderer* renderer, int x, int y, uint8_t color_index) {
    // Check bounds
    if (x < 0 || x >= renderer->internal_width || y < 0 || y >= renderer->internal_height) {
        return;  // Out of bounds
    }
    renderer->index_buffer[y * renderer->internal_width + x] = color_index;
}

// Draw a line from (x1, y1) to (x2, y2) with a color
void lumino_draw_line_scalar(LuminoRenderer* renderer, int x1, int y1, int x2, int y2, uint8_t color_index) {
    // Bresenham's line algorithm
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    while (1) {
        lumino_draw_pixel(renderer, x1, y1, color_index);
        if (x1 == x2 && y1 == y2) break;
        int err2 = err * 2;
        if (err2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (err2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}



void lumino_draw_line_neon(LuminoRenderer* r, int x0, int y0, int x1, int y1, uint8_t color_index) {
    int steps = fmaxf(fabsf(x1 - x0), fabsf(y1 - y0));
    if (steps == 0) {
        lumino_draw_pixel(r, x0, y0, color_index);
        return;
    }

    float dx = (float)(x1 - x0) / steps;
    float dy = (float)(y1 - y0) / steps;

    // Broadcast constants
    float32x4_t dx_v = vdupq_n_f32(dx);
    float32x4_t dy_v = vdupq_n_f32(dy);
    float32x4_t x0_v = vdupq_n_f32((float)x0);
    float32x4_t y0_v = vdupq_n_f32((float)y0);

    for (int i = 0; i <= steps; i += 4) {
        // t = [i, i+1, i+2, i+3]
        float32x4_t t = {
            (float)(i),
            (float)(i + 1),
            (float)(i + 2),
            (float)(i + 3)
        };

        // x = x0 + t * dx
        float32x4_t x_f = vmlaq_f32(x0_v, t, dx_v);
        float32x4_t y_f = vmlaq_f32(y0_v, t, dy_v);

        // Convert to integers with rounding
        int32x4_t x_i = vcvtq_s32_f32(vrndnq_f32(x_f));
        int32x4_t y_i = vcvtq_s32_f32(vrndnq_f32(y_f));

        // Store to scalar arrays
        int x[4], y[4];
        vst1q_s32(x, x_i);
        vst1q_s32(y, y_i);

        // Draw pixels (fallback to scalar writes)
        for (int j = 0; j < 4; j++) {
            if (x[j] >= 0 && x[j] < r->internal_width && y[j] >= 0 && y[j] < r->internal_height) {
                r->index_buffer[y[j] * r->internal_width + x[j]] = color_index;
            }
        }
    }
}

void lumino_draw_line(LuminoRenderer* renderer, int x1, int y1, int x2, int y2, uint8_t color_index) {
    #if defined(__ARM_NEON__)
        lumino_draw_line_neon(renderer, x1, y1, x2, y2, color_index);
    #else
        lumino_draw_line_scalar(renderer, x1, y1, x2, y2, color_index);
    #endif
}


// draw rectangle
void lumino_draw_rectangle(LuminoRenderer* renderer, int x, int y, int width, int height, uint8_t color_index) {
    lumino_draw_line(renderer, x, y, x + width - 1, y, color_index);                   // Top
    lumino_draw_line(renderer, x, y, x, y + height - 1, color_index);                  // Left
    lumino_draw_line(renderer, x + width - 1, y, x + width - 1, y + height - 1, color_index); // Right
    lumino_draw_line(renderer, x, y + height - 1, x + width - 1, y + height - 1, color_index); // Bottom
}


// fill rectangle
void lumino_fill_rectangle(LuminoRenderer* renderer, int x, int y, int width, int height, uint8_t color_index) {
    int fb_width = renderer->internal_width;
    uint8_t* buffer = renderer->index_buffer;

    for (int row = 0; row < height; ++row) {
        uint8_t* dst = &buffer[(y + row) * fb_width + x];
        int i = 0;

        // SIMD fill 16 pixels at a time
        uint8x16_t color_vec = vdupq_n_u8(color_index);
        for (; i <= width - 16; i += 16) {
            vst1q_u8(dst + i, color_vec);
        }

        // Handle the remaining few pixels
        for (; i < width; ++i) {
            dst[i] = color_index;
        }
    }
}

// load_png
uint8_t* lumino_load_png(LuminoRenderer* renderer, const char* filename, int* out_width, int* out_height) {
    int channels;
    unsigned char *image = stbi_load(filename, out_width, out_height, &channels, 4);

   

    // allocate memory for the output image
    uint8_t* out_image = (uint8_t*)malloc((*out_width) * (*out_height) * sizeof(uint8_t));

    for (int i = 0; i < (*out_width) * (*out_height); i++) {
        int r = image[i * 4];
        int g = image[i * 4 + 1];
        int b = image[i * 4 + 2];
        int a = image[i * 4 + 3];

        int color_index = lumino_pallette_contains_color(renderer->palette, renderer->palette_size, r, g, b, a);
        if (color_index == -1) {
            // Add color to palette if not already present
            lumino_add_palette_color(renderer, r, g, b, a);
            color_index = renderer->palette_size - 1; // Get the new index
        }
        out_image[i] = color_index; // Store the palette index
    }

    return out_image; // Return the image with palette indices
}

void lumino_draw_sprite(LuminoRenderer* renderer, int x, int y, uint8_t* sprite, int sprite_width, int sprite_height) {
    for (int j = 0; j < sprite_height; j++) {
        for (int i = 0; i < sprite_width; i++) {
            uint8_t color_index = sprite[j * sprite_width + i];
            lumino_draw_pixel(renderer, x + i, y + j, color_index);
        }
    }
}

