# Lumino

**Lumino** is a minimal, high-performance 2D graphics library written in C. It runs entirely on the CPU, supporting real-time pixel rendering with palette-based color, normal maps for lighting, and SIMD acceleration. Designed for pixel art games and retro-style engines, Lumino is ideal for systems without a GPU or for total control over your rendering pipeline.

---

## ✨ Features

- 🔲 Pixel buffer rendering (write pixel-by-pixel)
- 🎨 Palette-based color system
- 💡 Normal map lighting with directional control
- ⚡ SIMD-accelerated shading (NEON / SSE)
- 🧠 Dirty rectangle optimization (optional)
- 📐 Integer math and memory alignment for performance
- 🖥️ Supports macOS (native) and SDL (cross-platform fallback)

---

## 🚀 Getting Started

### Requirements

- C compiler (`clang`, `gcc`)
- macOS (M1/M2 recommended), Linux, or Windows (via SDL)
- Optional: NEON or SSE support for SIMD acceleration

### Build (Example)

```make```

