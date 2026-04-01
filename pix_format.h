// pixfmt.h
#pragma once
typedef unsigned int       uint32_t;
typedef uint32_t Uint32;

enum class PixelFormat : uint32_t {
    UNKNOWN = 0,

    // RGB格式
    RGB24,      // 24位RGB
    RGBA32,     // 32位RGBA
    ARGB32,     // 32位ARGB
    BGRA32,     // 32位BGRA (Windows常用)

    // YUV格式
    YUV420P,    // YUV 4:2:0 平面
    NV12,       // YUV 4:2:0 半平面
    NV21,       // YUV 4:2:0 半平面

    // 特殊格式
    GRAY8,      // 8位灰度
};

Uint32 PixelFormatToSDL(PixelFormat fmt);
PixelFormat SDLToPixelFormat(Uint32 sdl_fmt);