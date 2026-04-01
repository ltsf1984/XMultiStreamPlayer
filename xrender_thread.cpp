#include <iostream>
#include "xrender_thread.h"
#include "media_queues.h"
#include "d3d11_hardware_context.h"

extern "C" {
#include <libavutil/time.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_d3d11va.h>
}

XRenderThread::XRenderThread()
{
}

bool XRenderThread::Init(
    int width, int height,
    PixelFormat pix_fmt,
    void* win_id, AVRational video_time_base,
    std::shared_ptr< D3D11HardwareContext> d3d_ctx)
{
    video_time_base_ = video_time_base;
    d3d_ctx_ = d3d_ctx;

    if (!renderer_.Init(width, height, pix_fmt, win_id)) {
        std::cerr << "Failed to initialize SDL renderer" << std::endl;
        return false;
    }

    return true;
}

bool XRenderThread::Start()
{
    if (running_)
    {
        std::cout << "render thread is already running!" << std::endl;
        return false;
    }

    running_ = true;
    should_exit_ = false;
    thread_ = std::make_unique<std::thread>(&XRenderThread::Run, this);
    return true;
}

bool XRenderThread::Stop()
{
    if (thread_ && thread_->joinable())
    {
        running_ = false;
        renderer_.Close();
        MediaQueues::Instance().VideoFrameQueue().Clear();
        MediaQueues::Instance().VideoFrameQueue().Push(nullptr);
        thread_->join();
    }
    return false;
}

void XRenderThread::Pause()
{
    paused.store(true);
}

void XRenderThread::Resume()
{
    paused.store(false);
    pause_cv_.notify_one();
}

bool XRenderThread::IsRunning()
{
    return running_;
}

// 从 AVFrame 获取切片索引
int XRenderThread::GetSliceIndexFromFrame(AVFrame* frame)
{
    if (!frame || frame->format != AV_PIX_FMT_D3D11) {
        return 0;
    }

    // 切片索引存储在 data[1] 中！
    // data[1] 是一个指针，但实际存储的是整数值
    uintptr_t slice_index = (uintptr_t)frame->data[1];

    static bool printed = false;
    if (!printed) {
        std::cout << "Slice index from data[1]: " << slice_index << std::endl;
        printed = true;
    }

    // 检查是否有效
    if (slice_index > 0 && slice_index < 256) {
        return (int)slice_index;
    }

    // 备用：从 crop_top 获取
    if (frame->crop_top > 0 && frame->crop_top < 256) {
        return (int)frame->crop_top;
    }
    return 0;
}

void XRenderThread::Run()
{
    while (!should_exit_)
    {
        {
            std::unique_lock<std::mutex> lock(mtx_);
            pause_cv_.wait(lock, [this]() { return !paused; });
        }//释放锁

        AVFramePtr frame = make_frame();
        MediaQueues::Instance().VideoFrameQueue().Pop(frame);
        if (!frame) {
            std::cout << " --------XRenderThread exit!--------" << std::endl;
            return;
        }

        bool render_success = false;
        if (d3d_ctx_ && frame->format == AV_PIX_FMT_D3D11) {
            ID3D11Texture2D* tex = (ID3D11Texture2D*)frame->data[0];
            if (tex) {
                // 获取切片索引（会自动缓存正确的索引）
                int slice_index = GetSliceIndexFromFrame(frame.get());
                render_success = d3d_ctx_->RenderFrame(tex, slice_index);
            }
        }
        else {
            if (frame->data[0]) {
                render_success = renderer_.Draw(frame);
                if (!render_success) {
                    std::cout << "SDL render failed" << std::endl;
                }
            }
        }
    }
    std::cout << " =========XRenderThread exit!=========" << std::endl;
}

bool XRenderThread::Render(std::shared_ptr<AVFrame> frame)
{
    return false;
}
