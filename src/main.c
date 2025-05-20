#include "lumino.h"
#include "primitives.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define WIDTH 100
#define HEIGHT 100
#define UPSCALE_FACTOR 4

// Function to generate random integers in a given range
int random_in_range(int min, int max) {
    return min + rand() % (max - min + 1);
}

int main() {
    LuminoRenderer renderer;
    float time = 0.0f;  
    Uint32 previous_time = 0;  // Store the time of the previous frame
    float delta_time = 0.0f;   // Store the delta time between frames

    // Initialize Lumino renderer
    int result = lumino_init(&renderer, UPSCALE_FACTOR, WIDTH, HEIGHT);


    // Load sprite
    int sprite_width, sprite_height;
    uint8_t* sprite = lumino_load_png(&renderer, "images/sprite.png", &sprite_width, &sprite_height);

    // Main render loop
    while (lumino_should_run()) {
        // Time management for delta time
        Uint32 current_time = SDL_GetTicks();  // Get the current time in milliseconds
        delta_time = (current_time - previous_time) / 1000.0f;  // Calculate delta time in seconds
        previous_time = current_time;  // Update previous time for the next frame

        // Clear the screen
        lumino_clear(&renderer);

        // Draw the sprite at position (20, 20)
        lumino_draw_sprite(&renderer, 20, 20, sprite, sprite_width, sprite_height);

        // Increment time to animate the spiral (optional)
        time += delta_time * 0.05f;  // Adjust the speed of the spiral (if needed)

        // Present the rendered frame
        lumino_present(&renderer);
    }

    // Clean up
    free(sprite);  // Free the loaded sprite
    lumino_shutdown(&renderer);

    return 0;
}
