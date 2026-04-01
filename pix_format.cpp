#include "pix_format.h"
#include <sdl/SDL.h>

Uint32 PixelFormatToSDL(PixelFormat fmt) {
    switch (fmt) {
    case PixelFormat::RGB24:    return SDL_PIXELFORMAT_RGB24;
    case PixelFormat::RGBA32:   return SDL_PIXELFORMAT_RGBA8888;
    case PixelFormat::ARGB32:   return SDL_PIXELFORMAT_ARGB8888;
    case PixelFormat::BGRA32:   return SDL_PIXELFORMAT_BGRA8888;
    case PixelFormat::YUV420P:  return SDL_PIXELFORMAT_IYUV;
    case PixelFormat::NV12:     return SDL_PIXELFORMAT_NV12;
    case PixelFormat::NV21:     return SDL_PIXELFORMAT_NV21;
    case PixelFormat::GRAY8:    return SDL_PIXELFORMAT_INDEX8;
    default:                    return SDL_PIXELFORMAT_UNKNOWN;
    }
}

PixelFormat SDLToPixelFormat(Uint32 sdl_fmt) {
    switch (sdl_fmt) {
    case SDL_PIXELFORMAT_RGB24:     return PixelFormat::RGB24;
    case SDL_PIXELFORMAT_RGBA8888:  return PixelFormat::RGBA32;
    case SDL_PIXELFORMAT_ARGB8888:  return PixelFormat::ARGB32;
    case SDL_PIXELFORMAT_BGRA8888:  return PixelFormat::BGRA32;
    case SDL_PIXELFORMAT_IYUV:      return PixelFormat::YUV420P;
    case SDL_PIXELFORMAT_NV12:      return PixelFormat::NV12;
    case SDL_PIXELFORMAT_NV21:      return PixelFormat::NV21;
    case SDL_PIXELFORMAT_INDEX8:    return PixelFormat::GRAY8;
    default:                        return PixelFormat::UNKNOWN;
    }
}