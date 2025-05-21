#include <stdint.h>
#include "lumino.h"
#include "primitives.h"
#include <arm_neon.h>
#include <math.h>



//-----------------------
// Draw Pixel
//-----------------------

// Draw a single pixel at (x, y) with a color
void lumino_draw_pixel(LuminoRenderer* renderer, int x, int y, lumino_color color) {
    // Check bounds
    if (x < 0 || x >= renderer->internal_width || y < 0 || y >= renderer->internal_height) {
        return;  // Out of bounds
    }
    // Set the pixel color in the index buffer
    renderer->internal_framebuffer[y * renderer->internal_width + x] = lumino_get_color(color);
}

inline void lumino_draw_pixel_blend(LuminoRenderer* R, int x, int y, lumino_color c) {
    if ((unsigned)x >= (unsigned)R->internal_width ||
        (unsigned)y >= (unsigned)R->internal_height) return;

    uint32_t src = lumino_get_color(c);
    uint8_t sa = (src >> 24) & 0xFF;
    // fully transparent?
    if (sa == 0) return;
    // fully opaque?
    if (sa == 255) {
        R->internal_framebuffer[y * R->internal_width + x] = src;
        return;
    }

    uint32_t* p   = &R->internal_framebuffer[y * R->internal_width + x];
    uint32_t dst  = *p;

    // unpack
    uint8_t sr =  src        & 0xFF;
    uint8_t sg = (src >>  8) & 0xFF;
    uint8_t sb = (src >> 16) & 0xFF;
    uint8_t dr =  dst        & 0xFF;
    uint8_t dg = (dst >>  8) & 0xFF;
    uint8_t db = (dst >> 16) & 0xFF;
    uint8_t da = (dst >> 24) & 0xFF;

    // integer src-over: out = (s*Sa + d*(255−Sa)) / 255
    uint8_t inv = 255 - sa;
    uint8_t out_r = (uint8_t)((sr * sa + dr * inv + 127) / 255);
    uint8_t out_g = (uint8_t)((sg * sa + dg * inv + 127) / 255);
    uint8_t out_b = (uint8_t)((sb * sa + db * inv + 127) / 255);
    // composite alpha: Aout = Sa + Da*(1−Sa)
    uint8_t out_a = (uint8_t)(sa + (da * inv + 127) / 255);

    *p = lumino_get_color((lumino_color){ out_r, out_g, out_b, out_a });
}



//------------------------
// Draw Line
//------------------------

// Draw a line from (x1, y1) to (x2, y2) with a color
void lumino_draw_line_scalar(LuminoRenderer* renderer, int x1, int y1, int x2, int y2, lumino_color color) {
    // Bresenham's line algorithm
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    while (1) {
        lumino_draw_pixel(renderer, x1, y1, color);
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
void lumino_draw_line_scalar_blend(LuminoRenderer* renderer, int x1, int y1, int x2, int y2, lumino_color color) {
    // Bresenham's line algorithm
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    while (1) {
        lumino_draw_pixel_blend(renderer, x1, y1, color);
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



void lumino_draw_line_neon(LuminoRenderer* R,
                           int x0, int y0, int x1, int y1,
                           lumino_color color)
{
    int steps = (int)fmaxf(fabsf(x1 - x0), fabsf(y1 - y0));
    if (steps == 0) {
        lumino_draw_pixel(R, x0, y0, color);
        return;
    }

    float dx = (x1 - x0) / (float)steps;
    float dy = (y1 - y0) / (float)steps;
    float32x4_t dx_v = vdupq_n_f32(dx);
    float32x4_t dy_v = vdupq_n_f32(dy);
    float32x4_t x0_v = vdupq_n_f32((float)x0);
    float32x4_t y0_v = vdupq_n_f32((float)y0);

    uint32_t packed = lumino_get_color(color);
    uint32x4_t pack_v = vdupq_n_u32(packed);

    int width = R->internal_width, height = R->internal_height;
    uint32_t* fb = R->internal_framebuffer;

    for (int i = 0; i <= steps; i += 4) {
        float32x4_t t = { (float)i, (float)(i+1), (float)(i+2), (float)(i+3) };
        float32x4_t xf = vmlaq_f32(x0_v, t, dx_v);
        float32x4_t yf = vmlaq_f32(y0_v, t, dy_v);

        int32x4_t xi = vcvtq_s32_f32(vrndnq_f32(xf));
        int32x4_t yi = vcvtq_s32_f32(vrndnq_f32(yf));

        int32_t xs[4], ys[4];
        vst1q_s32(xs, xi);
        vst1q_s32(ys, yi);

        for (int j = 0; j < 4; j++) {
            int xx = xs[j], yy = ys[j];
            if ((unsigned)xx < (unsigned)width &&
                (unsigned)yy < (unsigned)height)
            {
                fb[yy * width + xx] = packed;
            }
        }
    }
}

void lumino_draw_line_neon_blend(LuminoRenderer* R,
                                 int x0, int y0, int x1, int y1,
                                 lumino_color color)
{
    int steps = (int)fmaxf(fabsf(x1 - x0), fabsf(y1 - y0));
    if (steps == 0) {
        lumino_draw_pixel_blend(R, x0, y0, color);
        return;
    }

    // Compute delta per step
    float dx = (x1 - x0) / (float)steps;
    float dy = (y1 - y0) / (float)steps;
    float32x4_t dx_v  = vdupq_n_f32(dx);
    float32x4_t dy_v  = vdupq_n_f32(dy);
    float32x4_t x0_v  = vdupq_n_f32((float)x0);
    float32x4_t y0_v  = vdupq_n_f32((float)y0);

    // We'll generate t = [i, i+1, i+2, i+3] each loop
    // and step i by 4 each iteration
    for (int i = 0; i <= steps; i += 4) {
        // build a vector [i, i+1, i+2, i+3]
        float32x4_t t = { (float)i,
                          (float)(i + 1 <= steps ? i + 1 : steps),
                          (float)(i + 2 <= steps ? i + 2 : steps),
                          (float)(i + 3 <= steps ? i + 3 : steps) };

        // x = x0 + t*dx, y = y0 + t*dy
        float32x4_t xf = vmlaq_f32(x0_v, t, dx_v);
        float32x4_t yf = vmlaq_f32(y0_v, t, dy_v);

        // round to nearest integer
        int32x4_t xi = vcvtq_s32_f32(vrndnq_f32(xf));
        int32x4_t yi = vcvtq_s32_f32(vrndnq_f32(yf));

        // store lanes to arrays
        int32_t xs[4], ys[4];
        vst1q_s32(xs, xi);
        vst1q_s32(ys, yi);

        // blend each pixel
        for (int j = 0; j < 4; j++) {
            int xx = xs[j], yy = ys[j];
            // check bounds once per point
            if ((unsigned)xx < (unsigned)R->internal_width &&
                (unsigned)yy < (unsigned)R->internal_height)
            {
                lumino_draw_pixel_blend(R, xx, yy, color);
            }
        }
    }
}



    
void lumino_draw_line(LuminoRenderer* renderer, int x1, int y1, int x2, int y2, lumino_color color) {
    #if defined(__ARM_NEON__) && !defined(LUMINO_NO_NEON)
        lumino_draw_line_neon(renderer, x1, y1, x2, y2, color);
    #else
        lumino_draw_line_scalar(renderer, x1, y1, x2, y2, color);
    #endif
}

void lumino_draw_line_blend(LuminoRenderer* renderer, int x1, int y1, int x2, int y2, lumino_color color) {
    #if defined(__ARM_NEON__) && !defined(LUMINO_NO_NEON)
        lumino_draw_line_neon_blend(renderer, x1, y1, x2, y2, color);
    #else
        lumino_draw_line_scalar_blend(renderer, x1, y1, x2, y2, color);
    #endif
}

//------------------------
// Draw Rectangle
//------------------------


void lumino_draw_rectangle(LuminoRenderer* renderer, int x, int y, int width, int height, lumino_color color) {
    lumino_draw_line(renderer, x, y, x + width - 1, y, color);                   // Top
    lumino_draw_line(renderer, x, y, x, y + height - 1, color);                  // Left
    lumino_draw_line(renderer, x + width - 1, y, x + width - 1, y + height - 1, color); // Right
    lumino_draw_line(renderer, x, y + height - 1, x + width - 1, y + height - 1, color); // Bottom
}


//--------------------------
// Fill Rectangle
//--------------------------


// fill rectangle
void lumino_fill_rectangle_scalar(LuminoRenderer* R,
                           int x, int y, int w, int h,
                           lumino_color color)
{
    uint32_t packed = lumino_get_color(color);
    int fbw = R->internal_width;
    uint32_t* buf = R->internal_framebuffer;

    for (int row = 0; row < h; row++) {
        uint32_t* dst = buf + (y + row) * fbw + x;
        for (int i = 0; i < w; i++) {
            dst[i] = packed;
        }
    }
}

// fill rectangle with blending
void lumino_fill_rectangle_scalar_blend(LuminoRenderer* R,
                                        int x, int y, int w, int h,
                                        lumino_color color)
{
    int fbw = R->internal_width;
    uint32_t* buf = R->internal_framebuffer;

    for (int row = 0; row < h; row++) {
        int yy = y + row;
        for (int i = 0; i < w; i++) {
            int xx = x + i;
            lumino_draw_pixel_blend(R, xx, yy, color);
        }
    }
}



void lumino_fill_rectangle_neon(LuminoRenderer* R,
                           int x, int y, int w, int h,
                           lumino_color color)
{
    uint32_t packed = lumino_get_color(color);
    uint32x4_t pack_v = vdupq_n_u32(packed);
    int fbw = R->internal_width;
    uint32_t* buf = R->internal_framebuffer;

    for (int row = 0; row < h; row++) {
        uint32_t* dst = buf + (y + row) * fbw + x;
        int i = 0;
        // SIMD store 4 pixels at a time
        for (; i <= w - 4; i += 4) {
            vst1q_u32(dst + i, pack_v);
        }
        // Remainder
        for (; i < w; i++) {
            dst[i] = packed;
        }
    }
}

// Blend‐fill a single scanline of length count at dst[], blending packed_src over existing pixels.
static void blend_row_neon(uint32_t* dst, int count, uint32_t packed_src) {
    // unpack source RGBA into 8-bit lanes
    uint8_t sa = (packed_src >> 24) & 0xFF;
    // prepare NEON vectors
    uint32x4_t src_u32 = vdupq_n_u32(packed_src);
    uint8x16_t src_u8  = vreinterpretq_u8_u32(src_u32);
    uint16x8_t alpha   = vdupq_n_u16(sa);
    uint16x8_t inv_a   = vdupq_n_u16(255 - sa);

    int i = 0;
    for (; i <= count - 4; i += 4) {
        // load 4 destination pixels
        uint32x4_t dst_u32 = vld1q_u32(dst + i);
        uint8x16_t dst_u8  = vreinterpretq_u8_u32(dst_u32);

        // widen to 16 bits
        uint16x8_t dst_lo = vmovl_u8(vget_low_u8(dst_u8));
        uint16x8_t dst_hi = vmovl_u8(vget_high_u8(dst_u8));
        uint16x8_t src_lo = vmovl_u8(vget_low_u8(src_u8));
        uint16x8_t src_hi = vmovl_u8(vget_high_u8(src_u8));

        // compute blended = (src*α + dst*(255-α)) >> 8
        uint16x8_t out_lo = vshrq_n_u16(
            vaddq_u16(vmulq_u16(src_lo, alpha), vmulq_u16(dst_lo, inv_a)),
            8
        );
        uint16x8_t out_hi = vshrq_n_u16(
            vaddq_u16(vmulq_u16(src_hi, alpha), vmulq_u16(dst_hi, inv_a)),
            8
        );

        // narrow back to bytes and store
        uint8x8_t  o_lo8 = vmovn_u16(out_lo);
        uint8x8_t  o_hi8 = vmovn_u16(out_hi);
        uint8x16_t out_u8 = vcombine_u8(o_lo8, o_hi8);
        vst1q_u32(dst + i, vreinterpretq_u32_u8(out_u8));
    }

    // tail pixels: scalar blend
    for (; i < count; i++) {
        // unpack dest
        uint32_t d = dst[i];
        uint8_t dr = d & 0xFF, dg = (d>>8)&0xFF, db = (d>>16)&0xFF, da = (d>>24)&0xFF;
        // unpack src
        uint8_t sr = packed_src & 0xFF, sg = (packed_src>>8)&0xFF, sb = (packed_src>>16)&0xFF;
        // blend
        float af = sa/255.0f, invf = 1.0f - af;
        uint8_t rr = (uint8_t)(sr*af + dr*invf + .5f);
        uint8_t gg = (uint8_t)(sg*af + dg*invf + .5f);
        uint8_t bb = (uint8_t)(sb*af + db*invf + .5f);
        uint8_t aa = (uint8_t)(sa + da*invf + .5f);
        dst[i] = (aa<<24)|(bb<<16)|(gg<<8)|rr;
    }
}

// fill rectangle with blending (SIMD + scalar tail)
void lumino_fill_rectangle_neon_blend(LuminoRenderer* R,
                                 int x, int y, int w, int h,
                                 lumino_color color)
{
    uint32_t packed = lumino_get_color(color);
    int fbw = R->internal_width;
    uint32_t* base = R->internal_framebuffer + y*fbw + x;

    for (int row = 0; row < h; row++) {
        blend_row_neon(base + row*fbw, w, packed);
    }
}



void lumino_fill_rectangle(LuminoRenderer* renderer, int x, int y, int width, int height, lumino_color color) {
    #if defined(__ARM_NEON__) && !defined(LUMINO_NO_NEON)
        lumino_fill_rectangle_neon(renderer, x, y, width, height, color);
    #else
        lumino_fill_rectangle_scalar(renderer, x, y, width, height, color);
    #endif
}

void lumino_fill_rectangle_blend(LuminoRenderer* renderer, int x, int y, int width, int height, lumino_color color) {
    #if defined(__ARM_NEON__) && !defined(LUMINO_NO_NEON)
        lumino_fill_rectangle_neon_blend(renderer, x, y, width, height, color);
    #else
        lumino_fill_rectangle_scalar_blend(renderer, x, y, width, height, color);
    #endif
}