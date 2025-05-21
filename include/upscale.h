#ifndef __UPSCALE_H__
#define __UPSCALE_H__

#ifdef __ARM_NEON__
#include <arm_neon.h>
#include "lumino.h"
#endif
#include <stdint.h>

void copyBuffer(uint32_t* dst, const uint32_t* src, int w, int h);
void upscale2x(uint32_t* dst, const uint32_t* src, int w, int h);
void upscale4x(uint32_t* dst, const uint32_t* src, int w, int h);
void upscale8x(uint32_t* dst, const uint32_t* src, int w, int h);

void palette_convert(uint32_t* dst, const uint8_t* src,const uint32_t* palette, int N);

#endif // __UPSCALE_H__