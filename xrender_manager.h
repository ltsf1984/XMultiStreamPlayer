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
#include "ximage_compositor.h"
#include "d3d11_hardware_context.h"
#include "av_utils.h"

// 渲染管理器配置
struct XRenderManagerConfig {
    int rows = 4;               // 行数
    int cols = 8;               // 列数
    int target_width = 1920;    // 目标宽度
    int target_height = 1080;   // 目标高度
    int render_fps = 30;        // 渲染帧率
    int max_streams = 32;       // 最大流数量
};

// 渲染管理器：管理帧获取、合成和渲染
class XRenderManager
{
public:
    XRenderManager();
    explicit XRenderManager(const XRenderManagerConfig& config);
    ~XRenderManager();

    // ========== 初始化 ==========
    bool Initialize(const XRenderManagerConfig& config);
    void SetD3DContext(std::shared_ptr<D3D11HardwareContext> d3d_ctx);

    // 设置帧出队回调（从外部获取帧）
    using FrameDequeue = std::function<bool(size_t stream_id, AVFramePtr& frame)>;
    void SetFrameDequeue(FrameDequeue callback);

    // ========== 控制 ==========
    void Start();
    void Stop();
    void Pause();
    void Resume();
    bool IsRunning() const { return running_; }

    // ========== 配置 ==========
    void UpdateConfig(const XRenderManagerConfig& config);
    const XRenderManagerConfig& GetConfig() const { return config_; }

    // ========== 统计 ==========
    void PrintStats() const;
    size_t GetTotalFramesRendered() const { return total_frames_rendered_; }
    size_t GetTotalFramesMissed() const { return total_frames_missed_; }

private:
    // ========== 线程函数 ==========
    void RenderLoop();

    // ========== 辅助函数 ==========
    void CollectFrames();
    void RenderCompositeTexture();
    void UpdateFrameRate();

private:
    XRenderManagerConfig config_;

    // 组件
    std::shared_ptr<D3D11HardwareContext> d3d_ctx_;
    std::unique_ptr<XImageCompositor> compositor_;

    // 回调函数
    FrameDequeue frame_dequeue_;

    // 帧数据（存储当前要渲染的帧）
    std::vector<AVFramePtr> current_frames_;

    // 渲染线程
    std::unique_ptr<std::thread> render_thread_;
    std::atomic<bool> running_{ false };
    std::atomic<bool> paused_{ false };
    std::condition_variable pause_cv_;
    std::mutex pause_mtx_;

    // 统计
    std::atomic<size_t> total_frames_rendered_{ 0 };
    std::atomic<size_t> total_frames_missed_{ 0 };
    std::chrono::steady_clock::time_point last_stats_time_;

    // 帧率控制
    std::chrono::steady_clock::time_point last_render_time_;
    std::chrono::microseconds frame_interval_{ 33333 };
};
