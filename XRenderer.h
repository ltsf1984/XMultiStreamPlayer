#pragma once
#include <mutex>
#include <atomic>
#include <memory>
#include "pix_format.h"

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;
struct AVFrame;

class XRenderer
{
public:
	XRenderer();
	~XRenderer();

	// 初始化渲染库
	bool InitLib();
	// 
	bool Init(int width, int height, PixelFormat pix_fmt, void* win_id);
	void Close();
	bool Draw(std::shared_ptr<AVFrame> frame);

private:
	bool ReconfigureTexture(int width, int height, PixelFormat pix_fmt);

private:
	std::mutex mtx_;
	std::atomic<bool> lib_initialized{ false };

	SDL_Window* sdl_win_ = nullptr;
	SDL_Renderer* sdl_renderer_ = nullptr;
	SDL_Texture* sdl_texture_ = nullptr;
	void* extern_win_id_{ nullptr };
	int texture_width_{ 0 };
	int texture_height_{ 0 };
	Uint32 texture_format_{ 0 };
};

