#include "benchmarks.h"


void benchmark_lines(LuminoRenderer* r) {
    const int N = 10;
    uint32_t start, end;

    // Warm-up
    for (int i = 0; i < 100; ++i) {
        lumino_draw_line(r, rand() % r->internal_width - 1, rand() % r->internal_height - 1,
                            rand() % r->internal_width - 1, rand() % r->internal_height - 1, 3);
    }

    
    // -------------------------
    // Scalar version benchmark
    // -------------------------
    start = SDL_GetTicks();
    for (int i = 0; i < N; ++i) {
        lumino_draw_line_scalar(r, rand() % r->internal_width, rand() % r->internal_height,
                            rand() % r->internal_width, rand() % r->internal_height, 3);
    }
    end = SDL_GetTicks();
    printf("Scalar Bresenham: %d ms for %d lines\n", end - start, N);

    // Clear the buffer between runs if needed
    memset(r->index_buffer, 0, r->internal_width * r->internal_height);

    // -------------------------
    // NEON version benchmark
    // -------------------------
    start = SDL_GetTicks();
    for (int i = 0; i < N; ++i) {
        lumino_draw_line_neon(r, rand() % r->internal_width, rand() % r->internal_height,
                                 rand() % r->internal_width, rand() % r->internal_height, 3);
    }
    end = SDL_GetTicks();
    printf("NEON float line:  %d ms for %d lines\n", end - start, N);
}