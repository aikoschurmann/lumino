#ifndef LUMINO_H
#define LUMINO_H

#include <stdint.h>
#include <SDL.h>
#include "upscale.h"


#define LUMINO_SUCCESS 0
#define LUMINO_FAILURE 1
#define LUMINO_DIMS_TOO_SMALL 2
#define LUMINO_INVALID_UPSCALE 3
#define LUMINO_DIMS_NOT_DIVISIBLE_BY_4 4

// Define the structure to hold rendering state
// index_buffer holds the value of the color in the palette
// this gets transformed into the internal framebuffer
// internal_framebuffer is the framebuffer that gets upscaled


typedef struct {
    uint8_t palette[256 * 4];            // Color palette (RGBA)
    uint8_t palette_size;                // current size of the palette

    // 1-byte per pixel: stores palette indices (0–255)
    // Draw calls write into this buffer
    uint8_t* index_buffer;              
    
    // Expanded 32-bit RGBA colors at internal resolution (4 bytes per pixel)
    // result of palette lookup for each index
    uint32_t* internal_framebuffer;      
    
    // Final upscaled 32-bit framebuffer at window resolution:
    // result of upscaling internal framebuffer (4 bytes per pixel and 2^upscale_factor as many pixels)
    uint32_t* framebuffer;               
    
    int internal_width;                  // Internal rendering resolution width
    int internal_height;                 // Internal rendering resolution height
    int width;                           // Window width
    int height;                          // Window height
    int upscale_factor;                  // Upscale factor for internal framebuffer
    void (*upscale_fn)(uint32_t* out, const uint32_t* in, int width, int height); // Function pointer for upscaling
} LuminoRenderer;

// Function prototypes

// Initialize the renderer with specific width, height, and internal resolution
int lumino_init(LuminoRenderer* renderer, int upscale_factor, int internal_width, int internal_height);

// Clean up resources
void lumino_shutdown(LuminoRenderer* renderer);

// Clear the internal framebuffer
void lumino_clear(LuminoRenderer* renderer);

// Perform upscaling if necessary (i.e., copy from internal to final framebuffer)
void lumino_upscale(LuminoRenderer* renderer);

// Present the final framebuffer to the window
void lumino_present(LuminoRenderer* renderer);

// Check if the window should continue running (event handling)
int lumino_should_run(void);

// Add a color to the palette
void lumino_add_palette_color(LuminoRenderer* renderer, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

void lumino_get_error(int error_code, char* error_message, size_t message_size);

lumino_pallette_contains_color(uint8_t* palette, int palette_size, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
#endif // LUMINO_H
