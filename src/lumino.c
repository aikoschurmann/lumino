// lumino.c - Core framebuffer handling and window initialization

#include "lumino.h"
#include <stdlib.h>
#include <string.h>
#include "upscale.h"

// the SDL window
static SDL_Window* window = NULL;
// the wrapper for back and front buffer by SDL
static SDL_Renderer* sdl_renderer = NULL;
// the texture / buffer that will hold the pixel data to be copied to the back buffer
static SDL_Texture* texture = NULL;
static uint32_t* framebuffer = NULL;
static int running = 1;
static int frame_count = 0;
static Uint32 last_time = 0;


// Buffer flow:
// +-----------------+        +----------------+        +------------------+        +-------------------+
// |   Framebuffer   |        |   Texture      |        |   Back Buffer    |        |   Front Buffer    |
// |   (CPU-side)    | --1--> |   (GPU-side)   | --2--> |    (hidden)      | --3--> |    (visible)      |
// +-----------------+        +----------------+        +------------------+        +-------------------+

// 1 SDL_UpdateTexture(texture, NULL, framebuffer, LUMINO_WIDTH * sizeof(uint32_t));
// 2 SDL_RenderCopy(renderer, texture, NULL, NULL);
// 3 SDL_RenderPresent(renderer);

// initialize the lumino renderer returns 0 on success and 1 on failure
int lumino_renderer_init(LuminoRenderer* renderer, int upscale_factor, int internal_width, int internal_height) {
    
    if (upscale_factor != 1 && upscale_factor != 2 && upscale_factor != 4 && upscale_factor != 8) {
        return LUMINO_INVALID_UPSCALE;  // Invalid upscale factor
    }

    if (internal_width <= 99 || internal_height <= 99) {
        return LUMINO_DIMS_TOO_SMALL;  // Invalid internal dimensions
    }

    // check if dims are divisible by 4 due to SIMD
    if (internal_width % 4 != 0 || internal_height % 4 != 0) {
        return LUMINO_DIMS_NOT_DIVISIBLE_BY_4;  // Invalid internal dimensions
    }

    // Initialize the renderer structure
    renderer->palette_size = 1;  // Initialize palette size
    renderer->internal_width = internal_width;
    renderer->internal_height = internal_height;
    renderer->upscale_factor = upscale_factor;
    renderer->width = internal_width * upscale_factor;
    renderer->height = internal_height * upscale_factor;

    switch (upscale_factor) {
        case 1:
            renderer->upscale_fn = copyBuffer;
            break;
        case 2:
            renderer->upscale_fn = upscale2x;
            break;
        case 4:
            renderer->upscale_fn = upscale4x;
            break;
        case 8:
            renderer->upscale_fn = upscale8x;
            break;
        
        default:
            return LUMINO_FAILURE;  // Invalid upscale factor
    }

    // Initialize the index buffer (1 byte per pixel)
    renderer->index_buffer = (uint8_t*)malloc(internal_width * internal_height * sizeof(uint8_t));
    
    // Allocate memory for the internal framebuffer
    renderer->internal_framebuffer = (uint32_t*)malloc(internal_width * internal_height * sizeof(uint32_t));
    if (!renderer->internal_framebuffer) {
        return LUMINO_FAILURE;  // Memory allocation failed
    }
    
    renderer->framebuffer = (uint32_t*)malloc(renderer->width * renderer->height * sizeof(uint32_t));
    if (!renderer->framebuffer) {
        free(renderer->internal_framebuffer);
        return LUMINO_FAILURE;  // Memory allocation failed
    }

    return LUMINO_SUCCESS;  // Success
}

// Initialize the window and framebuffer
int lumino_init(LuminoRenderer* renderer, int upscale_factor, int internal_width, int internal_height) {
    int result = lumino_renderer_init(renderer, upscale_factor, internal_width, internal_height);
    if (result != LUMINO_SUCCESS) {
        return result;  // Return error code from renderer initialization
    }

    // Initialize the SDL2 library
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        // If SDL initialization fails, return an error code
        return LUMINO_FAILURE;
    }

    // Create an SDL window at the center of the screen with specified width and height
    window = SDL_CreateWindow("Lumino Renderer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, renderer->width, renderer->height, SDL_WINDOW_SHOWN);
    if (!window) {
        // If window creation fails, clean up SDL and return error code
        SDL_Quit();
        return LUMINO_FAILURE;
    }

    // Create a renderer that will draw directly to the window, with hardware acceleration
    sdl_renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        // If renderer creation fails, clean up the window and SDL, then return error code
        SDL_DestroyWindow(window);
        SDL_Quit();
        return LUMINO_FAILURE;
    }

    // Create an SDL texture that will hold the pixel data to be rendered
    texture = SDL_CreateTexture(sdl_renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, renderer->width, renderer->height);
    if (!texture) {
        // If texture creation fails, clean up resources and return error code
        SDL_DestroyRenderer(sdl_renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return LUMINO_FAILURE;
    }

    return LUMINO_SUCCESS;  // Return success
}

// Clean up SDL and resources
void lumino_shutdown(LuminoRenderer* renderer) {
    // Free the internal framebuffer
    free(renderer->internal_framebuffer);
    free(renderer->framebuffer);
    
    // Destroy the texture, renderer, and window
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(sdl_renderer);
    SDL_DestroyWindow(window);
    
    // Quit SDL
    SDL_Quit();
}

// Check if the window should continue running (basic SDL event handling)
int lumino_should_run(void) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            running = 0;
        }
    }
    return running;
}

// Clear the framebuffer to black
void lumino_clear(LuminoRenderer* renderer) {
    memset(renderer->index_buffer, 0, renderer->internal_width * renderer->internal_height * sizeof(uint8_t));
}



// Add a color to the palette
void lumino_add_palette_color(LuminoRenderer* renderer, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (renderer->palette_size < 256) {
        renderer->palette[renderer->palette_size * 4] = r;
        renderer->palette[renderer->palette_size * 4 + 1] = g;
        renderer->palette[renderer->palette_size * 4 + 2] = b;
        renderer->palette[renderer->palette_size * 4 + 3] = a;
        renderer->palette_size++;
    }
}

// Present the framebuffer to the window
void lumino_present(LuminoRenderer* renderer) {
    // convert the index buffer to the internal framebuffer
    palette_convert(renderer->internal_framebuffer, renderer->index_buffer, (uint8_t*)renderer->palette, renderer->width * renderer->height);
   

    if(renderer->upscale_fn){
        // upscale the internal framebuffer to the final framebuffer
        renderer->upscale_fn(renderer->framebuffer, renderer->internal_framebuffer, renderer->internal_width, renderer->internal_height);
    }

    
    // copy the framebuffer to the texture (which is actually also a framebuffer)
    SDL_UpdateTexture(texture, NULL, renderer->framebuffer, renderer->width * sizeof(uint32_t));
    
    // Clear the back buffer (SDL's internal buffer where the next frame will be drawn)
    SDL_RenderClear(renderer);

    // Copy the texture (which holds the pixel data from the framebuffer) to the renderer's back buffer
    SDL_RenderCopy(sdl_renderer, texture, NULL, NULL);
    
    // Swap the back buffer to the front buffer, making the updated frame visible
    SDL_RenderPresent(sdl_renderer);

}

void lumino_get_error(int error_code, char* error_message, size_t message_size) {
    switch (error_code) {
        case LUMINO_SUCCESS:
            snprintf(error_message, message_size, "Success");
            break;
        case LUMINO_FAILURE:
            snprintf(error_message, message_size, "Failure");
            break;
        case LUMINO_DIMS_TOO_SMALL:
            snprintf(error_message, message_size, "Dimensions too small");
            break;
        case LUMINO_INVALID_UPSCALE:
            snprintf(error_message, message_size, "Invalid upscale factor");
            break;
        case LUMINO_DIMS_NOT_DIVISIBLE_BY_4:
            snprintf(error_message, message_size, "Dimensions not divisible by 4");
            break;
        default:
            snprintf(error_message, message_size, "Unknown error code: %d", error_code);
            break;
    }
}

lumino_pallette_contains_color(uint8_t* palette, int palette_size, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    for (int i = 0; i < palette_size; i++) {
        if (palette[i * 4] == r && palette[i * 4 + 1] == g && palette[i * 4 + 2] == b && palette[i * 4 + 3] == a) {
            return i; // Return the index of the color in the palette
        }
    }
    return -1; // Color not found in the palette
}