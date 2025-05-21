#ifndef LUMINO_H
#define LUMINO_H

#include <stdint.h>
#include <SDL.h>
#include "upscale.h"

//#define LUMINO_NO_NEON 

#define LUMINO_SUCCESS 0
#define LUMINO_FAILURE 1
#define LUMINO_DIMS_TOO_SMALL 2
#define LUMINO_INVALID_UPSCALE 3
#define LUMINO_DIMS_NOT_DIVISIBLE_BY_4 4

int mouse_location[2];
int mouse_clicked;

static const Uint8 *keyboard_state;
static Uint8 previous_keyboard_state[SDL_NUM_SCANCODES]; // Previous keyboard state

// Define the structure to hold rendering state
// index_buffer holds the value of the color in the palette
// this gets transformed into the internal framebuffer
// internal_framebuffer is the framebuffer that gets upscaled


typedef struct {
    uint8_t palette[256 * 4];            // Color palette (RGBA)
    uint8_t palette_size;                // current size of the palette

        
    // 
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

typedef struct  {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
    
} lumino_color;

typedef struct lumino_light {
    // Position in world‐space
    float x, y, z;

    // Color & intensity
    lumino_color color;     // e.g. {r, g, b} each in [0…1] or [0…255]
    float intensity;        // overall brightness multiplier

    // Attenuation (range‐based falloff)
    float range;            // maximum distance light reaches
    float inv_range_sq;     // precomputed 1.0f / (range * range)

    // Runtime toggle
    int enabled;           // quickly turn on/off without removing

} lumino_light;


// Function prototypes

// input keys
void lumino_initialize_keyboard_state(void);

void lumino_update_keyboard_state(void);

// Check if a key is pressed
int lumino_is_key_pressed(SDL_Scancode key);

// Check if a key is down
int lumino_is_key_down(SDL_Scancode key);

// Check if a key is up
int lumino_is_key_up(SDL_Scancode key);



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


uint32_t lumino_get_color(lumino_color color);
#endif // LUMINO_H
