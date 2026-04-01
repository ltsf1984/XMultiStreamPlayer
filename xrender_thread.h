#pragma once
#include <memory>
#include <atomic>
#include <thread>
#include <condition_variable>
#include "XRenderer.h"
#include "av_utils.h"

// 前向声明，避免循环依赖
class D3D11HardwareContext;

//struct AVFrame;

class XRenderThread
{
public:
    XRenderThread();

    bool Init(
        int width, int height,
        PixelFormat pix_fmt,
        void* win_id, AVRational video_time_base,
        std::shared_ptr< D3D11HardwareContext> d3d_ctx);

    // 线程开始和停止
    bool Start();
    bool Stop();
    void Pause();
    void Resume();
    bool IsRunning();

private:
    void Run();
    bool Render(std::shared_ptr<AVFrame> frame);
    // 辅助函数：从 AVFrame 获取切片索引
    int GetSliceIndexFromFrame(AVFrame* frame);

private:
    XRenderer renderer_;
    std::unique_ptr<std::thread> thread_;
    std::atomic<bool> running_{ false };
    std::atomic<bool> should_exit_{ false };
    std::atomic<bool> paused{ false };
    std::condition_variable pause_cv_;
    std::mutex mtx_;
    AVRational video_time_base_{ 1, 1000000 };
    // 
    std::shared_ptr< D3D11HardwareContext> d3d_ctx_{ nullptr };
};
