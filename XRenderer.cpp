//XRenderer.cpp
#include "XRenderer.h"
#include <iostream>
#include "pix_format.h"

extern "C" {
#include "sdl/SDL.h"
#include"libavcodec/avcodec.h"
}

#pragma comment(lib, "SDL2.lib")

XRenderer::XRenderer()
{
}

XRenderer::~XRenderer()
{
	if (sdl_texture_) {
		SDL_DestroyTexture(sdl_texture_);
		sdl_texture_ = nullptr;
	}
	if (sdl_renderer_) {
		SDL_DestroyRenderer(sdl_renderer_);
		sdl_renderer_ = nullptr;
	}
	if (sdl_win_) {
		SDL_DestroyWindow(sdl_win_);
		sdl_win_ = nullptr;
	}
}

bool XRenderer::InitLib()
{
	std::lock_guard<std::mutex> lock(mtx_);
	if (lib_initialized.load()) return true;

	// 设置全局提示（兼容性和默认值）
	// 集中设置所有提示（包括纹理质量）
	SDL_SetHint(SDL_HINT_RENDER_DRIVER, "direct3d11");
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
	SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");

	if (SDL_Init(SDL_INIT_VIDEO))
	{
		std::cerr << SDL_GetError() << std::endl;
		return false;
	}
	lib_initialized.store(true);
	return true;
}

bool XRenderer::Init(int width, int height, PixelFormat pix_fmt, void* win_id)
{
	InitLib();

	// 创建窗口
	if (win_id)
	{
		extern_win_id_ = win_id;
		sdl_win_ = SDL_CreateWindowFrom(win_id);
	}
	else
	{
		sdl_win_ = SDL_CreateWindow(
			"Video Window",
			SDL_WINDOWPOS_CENTERED,
			SDL_WINDOWPOS_CENTERED,
			width, height,
			SDL_WINDOW_SHOWN
		);
	}

	if (!sdl_win_) {
		std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
		return false;
	}

	sdl_renderer_ = SDL_CreateRenderer(sdl_win_, -1, SDL_RENDERER_ACCELERATED);
	if (!sdl_renderer_) {
		std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << std::endl;
		return false;
	}

	return ReconfigureTexture(width, height, pix_fmt);
}

void XRenderer::Close()
{
	// 锁定以确保线程安全（如果 Draw 可能被多线程调用）
	std::lock_guard<std::mutex> lock(mtx_);

	if (sdl_texture_) {
		SDL_DestroyTexture(sdl_texture_);
		sdl_texture_ = nullptr;
	}
	if (sdl_renderer_) {
		SDL_DestroyRenderer(sdl_renderer_);
		sdl_renderer_ = nullptr;
	}
	if (sdl_win_) {
		// 注意：如果窗口是通过 SDL_CreateWindowFrom() 创建的（外部窗口），
		// 则不应调用 SDL_DestroyWindow()，否则可能崩溃。
		// 所以这里只 destroy 自己创建的窗口
		if (!extern_win_id_) {
			SDL_DestroyWindow(sdl_win_);
		}
		sdl_win_ = nullptr;
	}

	// 重置状态
	texture_width_ = 0;
	texture_height_ = 0;
	texture_format_ = 0;
	lib_initialized.store(false); // 可选：是否全局 SDL_Quit？一般不由 Renderer 负责
}

bool XRenderer::ReconfigureTexture(int width, int height, PixelFormat pix_fmt)
{
	if (!sdl_renderer_) return false;

	Uint32 sdl_fmt = PixelFormatToSDL(pix_fmt);
	if (sdl_fmt == SDL_PIXELFORMAT_UNKNOWN) {
		std::cerr << "Unsupported pixel format!" << std::endl;
		return false;
	}

	// 销毁旧纹理（如果存在且尺寸/格式不同）
	if (sdl_texture_ && (texture_width_ != width || texture_height_ != height || texture_format_ != sdl_fmt)) {
		SDL_DestroyTexture(sdl_texture_);
		sdl_texture_ = nullptr;
	}

	if (!sdl_texture_) {
		sdl_texture_ = SDL_CreateTexture(
			sdl_renderer_,
			sdl_fmt,
			SDL_TEXTUREACCESS_STREAMING,
			width, height
		);
		if (!sdl_texture_) {
			std::cerr << "SDL_CreateTexture failed: " << SDL_GetError() << std::endl;
			return false;
		}
		texture_width_ = width;
		texture_height_ = height;
		texture_format_ = sdl_fmt;
	}

	return true;
}

bool XRenderer::Draw(std::shared_ptr<AVFrame> frame)
{
	if (!frame || !sdl_renderer_ || !sdl_texture_) {
		return false;
	}

	// 支持的格式：YUV420P 
	if (frame->format != AV_PIX_FMT_YUV420P) {
		std::cerr << "Unsupported frame format: " << frame->format << std::endl;
		return false;
	}

	const unsigned char* y_frame = frame->data[0];
	const unsigned char* u_frame = frame->data[1];
	const unsigned char* v_frame = frame->data[2];
	int y_pitch = frame->linesize[0];
	int u_pitch = frame->linesize[1];
	int v_pitch = frame->linesize[2];

	SDL_RenderClear(sdl_renderer_);

	SDL_UpdateYUVTexture(
		sdl_texture_, nullptr,
		y_frame, y_pitch,
		u_frame, u_pitch,
		v_frame, v_pitch
	);

	SDL_RenderCopy(sdl_renderer_, sdl_texture_, nullptr, nullptr);
	SDL_RenderPresent(sdl_renderer_);
	return true;
}
