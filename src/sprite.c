#include "sprite.h"
#include "primitives.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"


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
        uint8_t b = image[i * 4 + 0]; // r
        uint8_t g = image[i * 4 + 1]; // g
        uint8_t r = image[i * 4 + 2]; // b
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
                                      int x, int y,
                                      lumino_sprite sprite)
{
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
                                            int x, int y,
                                            lumino_sprite sprite)
{
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

#ifdef __ARM_NEON__
// NEON copy (4 pixels at a time)
static void lumino_draw_sprite_neon(LuminoRenderer* R,
                                    int x, int y,
                                    lumino_sprite sprite)
{
    uint32_t* fb    = R->internal_framebuffer;
    int       fbw   = R->internal_width;
    int       fb_h  = R->internal_height;
    int       w     = sprite.width;
    int       h     = sprite.height;
    uint32_t* src   = (uint32_t*)sprite.data;

    for (int row = 0; row < h; row++) {
        int yy = y + row;
        if ((unsigned)yy >= (unsigned)fb_h) continue;
        uint32_t* dst = fb + yy * fbw + x;
        int col = 0;
        // bulk copy
        for (; col <= w - 4; col += 4) {
            vst1q_u32(dst + col, vld1q_u32(src + row * w + col));
        }
        // tail
        for (; col < w; col++) {
            dst[col] = src[row * w + col];
        }
    }
}
#endif

#ifdef __ARM_NEON__
// NEON blend (4 pixels at a time) via vld4/vst4
static void lumino_draw_sprite_neon_blend(LuminoRenderer* R,
                                          int x, int y,
                                          lumino_sprite sprite)
{
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
            uint16x8_t sr = vmovl_u8(s.val[0]);
            uint16x8_t sg = vmovl_u8(s.val[1]);
            uint16x8_t sb = vmovl_u8(s.val[2]);
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



void lumino_draw_sprite(LuminoRenderer* renderer, int x, int y, lumino_sprite sprite) {
#if defined(__ARM_NEON__)
    lumino_draw_sprite_neon(renderer, x, y, sprite);
#else
    lumino_draw_sprite_scalar(renderer, x, y, sprite);
#endif
}

// Important Note: This function is not optimized for NEON yet so only use blend when needed
void lumino_draw_sprite_blend(LuminoRenderer* renderer, int x, int y, lumino_sprite sprite) {
#if defined(__ARM_NEON__) && !defined(LUMINO_NO_NEON)
    lumino_draw_sprite_neon_blend(renderer, x, y, sprite);
#else
    lumino_draw_sprite_scalar_blend(renderer, x, y, sprite);
#endif
}
