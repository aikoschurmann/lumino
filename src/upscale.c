#include "upscale.h"
#include <stdlib.h>
#include <string.h>

void copyBuffer(uint32_t* dst, const uint32_t* src, int w, int h) {
    memcpy(dst, src, w * h * sizeof(uint32_t));
}

// Scalar fallback (in case NEON isn't available)
static void upscale2x_scalar(uint32_t* dst, const uint32_t* src,
                             int w, int h) {
    int dw = w * 2;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint32_t c = src[y*w + x];
            int dx = x*2, dy = y*2;
            dst[ (dy  )*dw + (dx  ) ] = c;
            dst[ (dy  )*dw + (dx+1) ] = c;
            dst[ (dy+1)*dw + (dx  ) ] = c;
            dst[ (dy+1)*dw + (dx+1) ] = c;
        }
    }
}

#if defined(__ARM_NEON__)
static void upscale2x_neon(uint32_t* dst, const uint32_t* src,
                           int w, int h) {
    int dw = w * 2;
    for (int y = 0; y < h; y++) {
        // srow = current row in src
        const uint32_t* srow = src + y*w;
        // drow0 = current row in dst
        uint32_t* drow0 = dst + (y*2)*dw;
        // drow1 = next row in dst
        uint32_t* drow1 = dst + (y*2+1)*dw;
        for (int x = 0; x < w; x += 4) {
            // Load 4 pixels
            uint32x4_t pix = vld1q_u32(srow + x);
            // Duplicate each lane: zip pix with itself
            uint32x4x2_t dup = vzipq_u32(pix, pix);
            // dup.val[0] = [p0,p0,p2,p2], dup.val[1] = [p1,p1,p3,p3]
            vst1q_u32(drow0 + x*2,     dup.val[0]);
            vst1q_u32(drow0 + x*2 + 4, dup.val[1]);
            vst1q_u32(drow1 + x*2,     dup.val[0]);
            vst1q_u32(drow1 + x*2 + 4, dup.val[1]);
        }
    }
}
#endif





void upscale4x(uint32_t* dst, const uint32_t* src,
                   int w, int h) {
    // create intermediate buffer
    int w2 = w * 2;
    int h2 = h * 2;
    uint32_t* tmp = (uint32_t*)malloc(w2 * h2 * sizeof(uint32_t));
    if (!tmp) {
        return;  // Memory allocation failed
    }
    // upscale to 2x
    upscale2x(tmp, src, w, h);
    // upscale to 4x
    upscale2x(dst, tmp, w2, h2);
    // free intermediate buffer
    free(tmp);
}

void upscale8x(uint32_t* dst, const uint32_t* src,
                   int w, int h) {
    // create intermediate buffer
    int w4 = w * 4;
    int h4 = h * 4;
    uint32_t* tmp = (uint32_t*)malloc(w4 * h4 * sizeof(uint32_t));
    if (!tmp) {
        return;  // Memory allocation failed
    }
    // upscale to 4x
    upscale4x(tmp, src, w, h);
    // upscale to 8x
    upscale2x(dst, tmp, w4, h4);
    // free intermediate buffer
    free(tmp);

}

void upscale16x(uint32_t* dst, const uint32_t* src,
                   int w, int h) {
    // create intermediate buffer
    int w8 = w * 8;
    int h8 = h * 8;
    uint32_t* tmp = (uint32_t*)malloc(w8 * h8 * sizeof(uint32_t));
    if (!tmp) {
        return;  // Memory allocation failed
    }
    // upscale to 8x
    upscale8x(tmp, src, w, h);
    // upscale to 16x
    upscale2x(dst, tmp, w8, h8);
    // free intermediate buffer
    free(tmp);
}

// Single 2× dispatcher
void upscale2x(uint32_t* dst, const uint32_t* src, int w, int h) {
  #if defined(__ARM_NEON__) && !defined(LUMINO_NO_NEON)
    upscale2x_neon(dst, src, w, h);
  #else
    upscale2x_scalar(dst, src, w, h);
  #endif
}

