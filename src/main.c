#include "lumino.h"
#include "primitives.h"
#include "sprite.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define WIDTH 100
#define HEIGHT 100
#define UPSCALE_FACTOR 8

// Function to draw a sprite at a specific position
int main() {
    LuminoRenderer renderer;
    float time = 0.0f;  
    Uint32 previous_time = 0;  // Store the time of the previous frame
    float delta_time = 0.0f;   // Store the delta time between frames

    // Initialize Lumino renderer
    int result = lumino_init(&renderer, UPSCALE_FACTOR, WIDTH, HEIGHT);

    // Load sprite
    lumino_sprite sprite = lumino_load_png(&renderer, "images/sprite.png");
    if (sprite.data == NULL) {
        fprintf(stderr, "Failed to load sprite\n");
        return 1;
    }

    // Main render loop
    while (lumino_should_run()) {
        // Time management for delta time
        Uint32 current_time = SDL_GetTicks();  // Get the current time in milliseconds
        delta_time = (current_time - previous_time) / 1000.0f;  // Calculate delta time in seconds
        previous_time = current_time;  // Update previous time for the next frame

        // Clear the screen
        lumino_clear(&renderer);

        // draw sprite
        for (int i = 0; i < 10000; ++i) {
            int x = rand() % WIDTH ;
            int y = rand() % HEIGHT ;
            lumino_draw_sprite_blend(&renderer, x, y, sprite);  // Draw the sprite at random position
        }

        
        

        // draw a line
        lumino_draw_line_blend(&renderer, 0, 0, 100, 100, (lumino_color){255, 255, 255, 100}); // White line
        

        time += delta_time * 0.05f;  

        // Present the rendered frame
        lumino_present(&renderer);
    }

    // Clean up
    free(sprite.data);  // Free the loaded sprite
    lumino_shutdown(&renderer);

    return 0;
}
