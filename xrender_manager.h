// xrender_manager.h
#pragma once

#include <memory>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <chrono>
#include <array>
#include "ximage_compositor.h"
#include "d3d11_hardware_context.h"
#include "av_utils.h"

// 渲染管理器配置
struct XRenderManagerConfig {
    int rows = 4;
    int cols = 8;
    int target_width = 800;
    int target_height = 600;
    int render_fps = 30;
    int max_streams = 32;
};

// 渲染管理器（零拷贝版本）
class XRenderManager
{
public:
    XRenderManager();
    explicit XRenderManager(const XRenderManagerConfig& config);
    ~XRenderManager();

    // 初始化
    bool Initialize(const XRenderManagerConfig& config);
    void SetD3DContext(std::shared_ptr<D3D11HardwareContext> d3d_ctx);

    // 设置帧出队回调
    using FrameDequeue = std::function<bool(size_t stream_id, AVFramePtr& frame)>;
    void SetFrameDequeue(FrameDequeue callback);

    // 控制
    void Start();
    void Stop();
    void Pause();
    void Resume();
    bool IsRunning() const { return running_; }

    // 配置
    void UpdateConfig(const XRenderManagerConfig& config);
    const XRenderManagerConfig& GetConfig() const { return config_; }

    // 统计
    void PrintStats() const;
    size_t GetTotalFramesRendered() const { return total_frames_rendered_; }

private:
    void RenderLoop();
    void CollectFrames();
    void RenderTiled();

private:
    XRenderManagerConfig config_;

    std::shared_ptr<D3D11HardwareContext> d3d_ctx_;
    std::unique_ptr<XImageCompositor> compositor_;

    FrameDequeue frame_dequeue_;

    // 当前帧的纹理映射
    std::array<AVFramePtr, 32> current_frames_;

    std::unique_ptr<std::thread> render_thread_;
    std::atomic<bool> running_{ false };
    std::atomic<bool> paused_{ false };
    std::condition_variable pause_cv_;
    std::mutex pause_mtx_;

    std::atomic<size_t> total_frames_rendered_{ 0 };
    std::chrono::steady_clock::time_point last_stats_time_;
    std::chrono::steady_clock::time_point last_render_time_;
    std::chrono::microseconds frame_interval_{ 33333 };
};
