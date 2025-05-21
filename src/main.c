#include "lumino.h"
#include "primitives.h"
#include "sprite.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#define WIDTH 100
#define HEIGHT 100
#define UPSCALE_FACTOR 8
#define SCREEN_WIDTH (WIDTH * UPSCALE_FACTOR)
#define SCREEN_HEIGHT (HEIGHT * UPSCALE_FACTOR)

int main() {
    LuminoRenderer renderer;
    float delta_time = 0.0f;
    uint64_t previous_counter = SDL_GetPerformanceCounter();
    uint64_t freq = SDL_GetPerformanceFrequency();

    // Initialize Lumino renderer
    if (lumino_init(&renderer, UPSCALE_FACTOR, WIDTH, HEIGHT) != 0) {
        fprintf(stderr, "Failed to initialize renderer\n");
        return 1;
    }

    // Load sprites
    lumino_sprite grass = lumino_load_png(&renderer, "images/grass.png");
    if (!grass.data) { fprintf(stderr, "Failed to load grass sprite\n"); return 1; }
    lumino_sprite fire = lumino_load_png(&renderer, "images/fire.png");
    if (!fire.data) { fprintf(stderr, "Failed to load fire sprite\n"); return 1; }
    lumino_sprite character = lumino_load_png(&renderer, "images/sprite.png");
    if (!character.data) { fprintf(stderr, "Failed to load character sprite\n"); return 1; }

    const int speed = 50; // pixels per second
    float character_x = 0;
    float character_y = 0;

    // Light to represent the fire
    lumino_light fire_light = {0};
    fire_light.x = WIDTH / 2;
    fire_light.y = HEIGHT / 2;
    fire.x = fire_light.x - fire.width / 2;
    fire.y = fire_light.y - fire.height / 2;
    fire_light.z = 0;
    fire_light.color.r = 255;
    fire_light.color.g = 200;
    fire_light.color.b = 150;
    fire_light.color.a = 255;
    fire_light.intensity = 0.8f;
    fire_light.range = 60.0f;
    fire_light.inv_range_sq = 1.0f / (fire_light.range * fire_light.range);
    fire_light.enabled = 1;

    // Main loop
    while (lumino_should_run()) {
        // Calculate delta time
        uint64_t current_counter = SDL_GetPerformanceCounter();
        delta_time = (double)(current_counter - previous_counter) / freq;
        previous_counter = current_counter;

        // Velocity input
        float velocity_x = 0.0f;
        float velocity_y = 0.0f;

        if (lumino_is_key_pressed(SDL_SCANCODE_Z) || lumino_is_key_pressed(SDL_SCANCODE_UP))    velocity_y = -1.0f;
        if (lumino_is_key_pressed(SDL_SCANCODE_S) || lumino_is_key_pressed(SDL_SCANCODE_DOWN))  velocity_y = 1.0f;
        if (lumino_is_key_pressed(SDL_SCANCODE_Q) || lumino_is_key_pressed(SDL_SCANCODE_LEFT))  velocity_x = -1.0f;
        if (lumino_is_key_pressed(SDL_SCANCODE_D) || lumino_is_key_pressed(SDL_SCANCODE_RIGHT)) velocity_x = 1.0f;

        float time = SDL_GetTicks() / 3000.0f;

        float flicker = 
            sin(time * 3.1f) * 1.5f +  // slow pulse
            sin(time * 11.2f) * 0.4f + // mid flicker
            sin(time * 23.7f) * 0.2f + // fast noise
            ((rand() % 100) / 500.0f - 0.1f); // slight jitter

        fire_light.range = 40.0f + flicker * 2;
        fire_light.inv_range_sq = 1.0f / (fire_light.range * fire_light.range);

        // Normalize diagonal movement
        float magnitude = sqrtf(velocity_x * velocity_x + velocity_y * velocity_y);
        if (magnitude > 0.0f) {
            velocity_x /= magnitude;
            velocity_y /= magnitude;
        }

        // Apply movement
        character_x += velocity_x * speed * delta_time;
        character_y += velocity_y * speed * delta_time;
        character.x = (int)character_x;
        character.y = (int)character_y;

        // Clear screen
        lumino_clear(&renderer);

        // Draw grass field by tiling the grass sprite
        for (int y = 0; y < HEIGHT; y += grass.height) {
            for (int x = 0; x < WIDTH; x += grass.width) {
                grass.x = x;
                grass.y = y;
                lumino_draw_sprite_lit(&renderer, grass, fire_light, 0.2f);
            }
        }

        // Draw fire sprite and lighting
        lumino_draw_sprite(&renderer, fire);

        // Draw character sprite with ambient lighting only
        lumino_draw_sprite_lit(&renderer, character, fire_light, 0.2f);

        // Present frame
        lumino_present(&renderer);
    }

    // Cleanup
    free(grass.data);
    free(fire.data);
    free(character.data);
    lumino_shutdown(&renderer);
    return 0;
}
