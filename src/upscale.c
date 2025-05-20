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
  #if defined(__ARM_NEON__)
    upscale2x_neon(dst, src, w, h);
  #else
    upscale2x_scalar(dst, src, w, h);
  #endif
}

void palette_convert_scalar(uint32_t* out, const uint8_t* in,
                            const uint32_t* palette,
                            int N)
{
    for (int i = 0; i < N; i++) {
        out[i] = palette[in[i]];
    }
}
void palette_convert_neon(uint32_t* out,
                          const uint8_t* in,
                          const uint32_t* palette,
                          int N)
{
#if !defined(__ARM_NEON__)
    // Fallback to scalar if no NEON
    palette_convert_scalar(out, in, palette, N);
    return;
#else
    // First, build four 256-byte tables: R, G, B, A
    static uint8_t pr[256], pg[256], pb[256], pa[256];
    for (int i = 0; i < 256; i++) {
        uint32_t c = palette[i];
        pr[i] = (c >> 24) & 0xFF;  // assuming RGBA in big-endian order
        pg[i] = (c >> 16) & 0xFF;
        pb[i] = (c >> 8 ) & 0xFF;
        pa[i] = (c      ) & 0xFF;
    }

    // Process 16 pixels per loop
    int i = 0;
    int limit = N & ~15;
    for (; i < limit; i += 16) {
        // Load 16 indices
        uint8x16_t idx = vld1q_u8(in + i);

        // Lookup each channel
        uint8x16_t r = vqtbl1q_u8(vld1q_u8(pr), idx);
        uint8x16_t g = vqtbl1q_u8(vld1q_u8(pg), idx);
        uint8x16_t b = vqtbl1q_u8(vld1q_u8(pb), idx);
        uint8x16_t a = vqtbl1q_u8(vld1q_u8(pa), idx);

        // Now pack r,g,b,a bytes into 4× uint32 vectors
        // Step 1: pairwise interleave bytes to halfwords
        uint16x8_t rg_lo = vzipq_u8(r, g).val[0];   // r0,g0,r1,g1,r2,g2,r3,g3
        uint16x8_t rg_hi = vzipq_u8(r, g).val[1];   // r4,g4...
        uint16x8_t ba_lo = vzipq_u8(b, a).val[0];
        uint16x8_t ba_hi = vzipq_u8(b, a).val[1];

        // Step 2: interleave halfwords to words
        uint32x4_t pix01 = vzipq_u16(rg_lo, ba_lo).val[0]; // pixel 0,1
        uint32x4_t pix23 = vzipq_u16(rg_lo, ba_lo).val[1]; // pixel 2,3
        uint32x4_t pix45 = vzipq_u16(rg_hi, ba_hi).val[0]; // pixel 4,5
        uint32x4_t pix67 = vzipq_u16(rg_hi, ba_hi).val[1]; // pixel 6,7

        // Store four pixels at a time
        vst1q_u32(out + i +  0, pix01);
        vst1q_u32(out + i +  4, pix23);
        vst1q_u32(out + i +  8, pix45);
        vst1q_u32(out + i + 12, pix67);
    }

    // Tail
    for (; i < N; i++) {
        out[i] = palette[in[i]];
    }
#endif
}

void palette_convert(uint32_t* out, const uint8_t* in,
                            const uint32_t* palette, int N)
{
    #if defined(__ARM_NEON__)
        palette_convert_neon(out, in, palette, N);
    #else
        palette_convert_scalar(out, in, palette, N);
    #endif
}