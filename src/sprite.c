#include "sprite.h"
#include "primitives.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <math.h>
#ifdef __ARM_NEON__
#include <arm_neon.h>
#endif

float min(float a, float b) {
    return (a < b) ? a : b;
}

// load_png
lumino_sprite lumino_load_png(LuminoRenderer* renderer, const char* filename) {
    int channels;
    lumino_sprite sprite = {0};
    unsigned char* image = stbi_load(filename,
                                     &sprite.width,
                                     &sprite.height,
                                     &channels,
                                     4 /* force RGBA */);
    if (!image) {
        fprintf(stderr, "Error loading image %s: %s\n",
                filename,
                stbi_failure_reason());
        return sprite;  // sprite.data is already NULL
    }

    int pixelCount = sprite.width * sprite.height;
    lumino_color* sprite_data = (lumino_color*)malloc(pixelCount * sizeof(lumino_color));
    if (!sprite_data) {
        fprintf(stderr, "Out of memory allocating %zu bytes for sprite\n", pixelCount);
        stbi_image_free(image);
        return sprite;
    }

    // copy the pixel data from the image to the sprite data
    for (size_t i = 0; i < pixelCount; ++i) {
        // convert the RGBA pixel to an index in the palette
        uint8_t r = image[i * 4 + 0]; // r
        uint8_t g = image[i * 4 + 1]; // g
        uint8_t b = image[i * 4 + 2]; // b
        uint8_t a = image[i * 4 + 3]; // a

        // find the index in the palette
        lumino_color color = {r, g, b, a};
        sprite_data[i] = color;
    }

    

    // clean up the temporary RGBA buffer
    stbi_image_free(image);

    // …and *now* attach it to the sprite struct
    sprite.data = sprite_data;
    return sprite;
}


//-----------------------------------------------------------------------------
// Sprite blit: scalar + NEON, copy vs. alpha-blend
//-----------------------------------------------------------------------------

// Scalar copy
static void lumino_draw_sprite_scalar(LuminoRenderer* R,
                                      lumino_sprite sprite)
{
    int x = sprite.x;
    int y = sprite.y;
    uint32_t* fb    = R->internal_framebuffer;
    int       fbw   = R->internal_width;
    int       fb_h  = R->internal_height;
    int       w     = sprite.width;
    int       h     = sprite.height;
    lumino_color* src = sprite.data;

    for (int row = 0; row < h; row++) {
        int yy = y + row;
        if ((unsigned)yy >= (unsigned)fb_h) continue;
        for (int col = 0; col < w; col++) {
            int xx = x + col;
            if ((unsigned)xx >= (unsigned)fbw) continue;
            fb[yy * fbw + xx] = lumino_get_color(src[row * w + col]);
        }
    }
}

// Scalar blend
void lumino_draw_sprite_scalar_blend(LuminoRenderer* R,
                                            lumino_sprite sprite)
{
    int x = sprite.x;
    int y = sprite.y;
    int       fbw   = R->internal_width;
    int       fb_h  = R->internal_height;
    int       w     = sprite.width;
    int       h     = sprite.height;
    lumino_color* src = sprite.data;

    for (int row = 0; row < h; row++) {
        int yy = y + row;
        if ((unsigned)yy >= (unsigned)fb_h) continue;
        for (int col = 0; col < w; col++) {
            int xx = x + col;
            if ((unsigned)xx >= (unsigned)fbw) continue;
            lumino_draw_pixel_blend(R, xx, yy, src[row * w + col]);
        }
    }
}

static void lumino_draw_sprite_neon(LuminoRenderer* R,
                                    lumino_sprite sprite)
{
    int x = sprite.x;
    int y = sprite.y;
    uint32_t* fb    = R->internal_framebuffer;
    int       fbw   = R->internal_width;
    int       fb_h  = R->internal_height;
    int       w     = sprite.width;
    int       h     = sprite.height;
    uint32_t* src   = (uint32_t*)sprite.data;

    // Byte shuffle indices to swap R and B in each pixel: [2,1,0,3] per pixel
    static const uint8_t shuffle_idx_data[16] = {
        2, 1, 0, 3,  6, 5, 4, 7,
       10, 9, 8,11, 14,13,12,15
    };
    const uint8x16_t shuffle_idx = vld1q_u8(shuffle_idx_data);

    for (int row = 0; row < h; row++) {
        int yy = y + row;
        if ((unsigned)yy >= (unsigned)fb_h) continue;
        uint32_t* dst = fb + yy * fbw + x;
        int col = 0;
        // bulk copy with R/B swap
        for (; col <= w - 4; col += 4) {
            uint32x4_t pixels = vld1q_u32(src + row * w + col);
            uint8x16_t bytes  = vreinterpretq_u8_u32(pixels);
            uint8x16_t out    = vqtbl1q_u8(bytes, shuffle_idx);
            vst1q_u32(dst + col, vreinterpretq_u32_u8(out));
        }
        // tail pixels
        for (; col < w; col++) {
            uint32_t p = src[row * w + col];
            uint8_t* c = (uint8_t*)&p;
            // assuming little-endian RGBA in memory: [B,G,R,A]
            uint8_t b = c[0], g = c[1], r = c[2], a = c[3];
            uint32_t swapped = (a << 24) | (b << 16) | (g << 8) | r;
            // after swap: memory [R,G,B,A]
            dst[col] = swapped;
        }
    }
}


#ifdef __ARM_NEON__
// NEON blend (4 pixels at a time) via vld4/vst4
static void lumino_draw_sprite_neon_blend(LuminoRenderer* R,
                                          lumino_sprite sprite)
{   
    int x = sprite.x;
    int y = sprite.y;
    uint8_t* fb_bytes = (uint8_t*)R->internal_framebuffer;
    int fbw = R->internal_width;
    int fbh = R->internal_height;
    int w   = sprite.width;
    int h   = sprite.height;
    uint8_t* src_bytes = (uint8_t*)sprite.data;

    for (int row = 0; row < h; row++) {
        int yy = y + row;
        if ((unsigned)yy >= (unsigned)fbh) continue;

        uint8_t* dst_row = fb_bytes + (yy * fbw + x) * 4;
        uint8_t* src_row = src_bytes + (row * w) * 4;

        int col = 0;
        for (; col <= w - 8; col += 8) {
            // load 8 pixels de-interleaved: R,G,B,A each into val[0..3]
            uint8x8x4_t s = vld4_u8(src_row + col*4);
            uint8x8x4_t d = vld4_u8(dst_row + col*4);

            // widen to 16-bit
            uint16x8_t sr = vmovl_u8(s.val[2]);
            uint16x8_t sg = vmovl_u8(s.val[1]);
            uint16x8_t sb = vmovl_u8(s.val[0]);
            uint16x8_t sa = vmovl_u8(s.val[3]);

            uint16x8_t dr = vmovl_u8(d.val[0]);
            uint16x8_t dg = vmovl_u8(d.val[1]);
            uint16x8_t db = vmovl_u8(d.val[2]);

            // inv_alpha = 255 - alpha
            uint16x8_t ia = vsubq_u16(vdupq_n_u16(255), sa);

            // out = (src*alpha + dst*inv_alpha + 127) / 255
            uint16x8_t rr = vrshrq_n_u16(vmlaq_u16(vmulq_u16(sr, sa),
                                                   dr, ia),
                                         8);
            uint16x8_t gg = vrshrq_n_u16(vmlaq_u16(vmulq_u16(sg, sa),
                                                   dg, ia),
                                         8);
            uint16x8_t bb = vrshrq_n_u16(vmlaq_u16(vmulq_u16(sb, sa),
                                                   db, ia),
                                         8);

            // narrow back to 8-bit
            uint8x8x4_t out;
            out.val[0] = vmovn_u16(rr);
            out.val[1] = vmovn_u16(gg);
            out.val[2] = vmovn_u16(bb);
            out.val[3] = vmovn_u16(sa);  // keep source alpha in dest

            // store 8 pixels
            vst4_u8(dst_row + col*4, out);
        }

        // tail pixels: fall back to scalar
        for (; col < w; col++) {
            lumino_draw_pixel_blend(R, x + col, yy, sprite.data[row * w + col]);
        }
    }
}
#endif



void lumino_draw_sprite(LuminoRenderer* renderer, lumino_sprite sprite) {
#if defined(__ARM_NEON__)
    lumino_draw_sprite_neon(renderer, sprite);
#else
    lumino_draw_sprite_scalar(renderer, x, y, sprite);
#endif
}

// Important Note: This function is not optimized for NEON yet so only use blend when needed
void lumino_draw_sprite_blend(LuminoRenderer* renderer, lumino_sprite sprite) {
#if defined(__ARM_NEON__) && !defined(LUMINO_NO_NEON)
    lumino_draw_sprite_neon_blend(renderer, sprite);
#else
    lumino_draw_sprite_scalar_blend(renderer, x, y, sprite);
#endif
}


// simple point‐light, flat shading (normal = (0,0,1))
// ----------------------------------------------------------------------------
void lumino_draw_sprite_lit(LuminoRenderer* R,
                             lumino_sprite sprite,
                             lumino_light light,
                             float ambient)
{
    int fbw = R->internal_width;
    int fbh = R->internal_height;
    uint32_t* fb = R->internal_framebuffer;
    int w = sprite.width;
    int h = sprite.height;
    uint32_t* src = sprite.data;

    // Precompute squared range and its inverse
    float range_sq    = light.range * light.range;
    float inv_range_sq = 1.0f / range_sq;

    // Normalize light color to [0..1]
    float light_r = light.color.r / 255.0f;
    float light_g = light.color.g / 255.0f;
    float light_b = light.color.b / 255.0f;

    for (int row = 0; row < h; row++) {
        int yy = sprite.y + row;
        if ((unsigned)yy >= (unsigned)fbh) continue;

        for (int col = 0; col < w; col++) {
            int xx = sprite.x + col;
            if ((unsigned)xx >= (unsigned)fbw) continue;

            int idx = row * w + col;
            uint32_t c_src = src[idx];
            lumino_color c = {
                .r = (c_src      ) & 0xFF,
                .g = (c_src >>  8) & 0xFF,
                .b = (c_src >> 16) & 0xFF,
                .a = (c_src >> 24) & 0xFF
            };
            if (c.a == 0) continue;

            // squared distance
            int dx = xx - light.x;
            int dy = yy - light.y;
            float dist_sq = (float)(dx*dx + dy*dy);

            if (dist_sq >= range_sq) {
                // outside the light radius → only ambient
                uint32_t col_amb = lumino_get_color((lumino_color){
                    (uint8_t)(c.r * ambient),
                    (uint8_t)(c.g * ambient),
                    (uint8_t)(c.b * ambient),
                    c.a
                });
                fb[yy * fbw + xx] = col_amb;
                continue;
            }

            // attenuation = (1 - (dist^2 / R^2)) * intensity
            float atten = (1.0f - dist_sq * inv_range_sq) * light.intensity;

            // combine ambient + light; convert c to [0..1] first
            float sr = c.r / 255.0f;
            float sg = c.g / 255.0f;
            float sb = c.b / 255.0f;

            float out_r = (sr * ambient + sr * atten * light_r);
            float out_g = (sg * ambient + sg * atten * light_g);
            float out_b = (sb * ambient + sb * atten * light_b);

            // back to 0..255 and clamp
            uint8_t fr = (uint8_t)(fminf(fmaxf(out_r * 255.0f, 0.0f), 255.0f));
            uint8_t fg = (uint8_t)(fminf(fmaxf(out_g * 255.0f, 0.0f), 255.0f));
            uint8_t fb_ = (uint8_t)(fminf(fmaxf(out_b * 255.0f, 0.0f), 255.0f));

            fb[yy * fbw + xx] = lumino_get_color((lumino_color){fr, fg, fb_, c.a});
        }
    }
}
