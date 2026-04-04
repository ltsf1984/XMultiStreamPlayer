// xrender_manager.cpp
#include "xrender_manager.h"
#include <iostream>

XRenderManager::XRenderManager()
{
}

XRenderManager::XRenderManager(const XRenderManagerConfig& config)
{
    Initialize(config);
}

XRenderManager::~XRenderManager()
{
    Stop();
}

bool XRenderManager::Initialize(const XRenderManagerConfig& config)
{
    config_ = config;

    // 初始化合成器（零拷贝版本）
    LayoutConfig layout_config;
    layout_config.rows = config_.rows;
    layout_config.cols = config_.cols;
    layout_config.target_width = config_.target_width;
    layout_config.target_height = config_.target_height;

    compositor_ = std::make_unique<XImageCompositor>();
    if (!compositor_->Initialize(layout_config)) {
        std::cerr << "Failed to initialize compositor" << std::endl;
        return false;
    }

    // 初始化帧数组为空
    for (auto& frame : current_frames_) {
        frame = nullptr;
    }

    last_stats_time_ = std::chrono::steady_clock::now();

    std::cout << "XRenderManager (zero-copy) initialized: "
        << config_.rows << "x" << config_.cols
        << ", target size: " << config_.target_width << "x" << config_.target_height
        << ", FPS: " << config_.render_fps
        << std::endl;

    return true;
}

void XRenderManager::SetD3DContext(std::shared_ptr<D3D11HardwareContext> d3d_ctx)
{
    d3d_ctx_ = d3d_ctx;
    if (compositor_ && d3d_ctx_) {
        compositor_->SetD3DDevice(d3d_ctx_->GetDevice());
    }
}

void XRenderManager::SetFrameDequeue(FrameDequeue callback)
{
    frame_dequeue_ = callback;
}

void XRenderManager::Start()
{
    if (running_) {
        return;
    }

    if (!d3d_ctx_) {
        std::cerr << "D3D context not set" << std::endl;
        return;
    }

    if (!frame_dequeue_) {
        std::cerr << "Frame dequeue callback not set" << std::endl;
        return;
    }

    running_ = true;
    paused_ = false;
    last_render_time_ = std::chrono::steady_clock::now();
    render_thread_ = std::make_unique<std::thread>(&XRenderManager::RenderLoop, this);

    std::cout << "XRenderManager started (zero-copy mode)" << std::endl;
}

void XRenderManager::Stop()
{
    if (!running_) {
        return;
    }

    running_ = false;
    pause_cv_.notify_all();

    if (render_thread_ && render_thread_->joinable()) {
        render_thread_->join();
    }

    std::cout << "XRenderManager stopped, total frames rendered: "
        << total_frames_rendered_.load() << std::endl;
}

void XRenderManager::Pause()
{
    paused_ = true;
    std::cout << "XRenderManager paused" << std::endl;
}

void XRenderManager::Resume()
{
    paused_ = false;
    pause_cv_.notify_one();
    std::cout << "XRenderManager resumed" << std::endl;
}

void XRenderManager::UpdateConfig(const XRenderManagerConfig& config)
{
    config_ = config;

    if (compositor_) {
        compositor_->SetLayout(config_.rows, config_.cols);
    }

    if (config_.render_fps > 0) {
        frame_interval_ = std::chrono::microseconds(1000000 / config_.render_fps);
    }
}

void XRenderManager::RenderLoop()
{
    std::cout << "Render loop started (zero-copy mode)" << std::endl;

    while (running_) {
        {
            std::unique_lock<std::mutex> lock(pause_mtx_);
            pause_cv_.wait(lock, [this]() {
                return !paused_ || !running_;
                });
        }

        if (!running_) break;

        auto now = std::chrono::steady_clock::now();
        auto elapsed = now - last_render_time_;

        if (elapsed < frame_interval_) {
            std::this_thread::sleep_for(frame_interval_ - elapsed);
            now = std::chrono::steady_clock::now();
        }

        CollectFrames();
        RenderTiled();

        last_render_time_ = now;
        total_frames_rendered_++;

        auto stats_elapsed = now - last_stats_time_;
        if (stats_elapsed >= std::chrono::seconds(5)) {
            PrintStats();
            last_stats_time_ = now;
        }
    }

    std::cout << "Render loop ended" << std::endl;
}

void XRenderManager::CollectFrames()
{
    if (!frame_dequeue_ || !compositor_) {
        return;
    }

    // 从队列获取所有流的当前帧
    for (size_t i = 0; i < 32; ++i) {
        AVFramePtr frame;

        // 尝试从队列获取帧
        if (frame_dequeue_(i, frame)) {
            // 成功获取帧（frame 一定有效）
            current_frames_[i] = std::move(frame);
            compositor_->UpdateCell(static_cast<int>(i), current_frames_[i].get());
        }
        else {
            // 队列为空，标记为无效（显示红色占位符）
            current_frames_[i] = nullptr;
            compositor_->UpdateCell(static_cast<int>(i), nullptr);
        }
    }
}

void XRenderManager::RenderTiled()
{
    if (!d3d_ctx_ || !compositor_) {
        return;
    }

    // 准备纹理数组（初始化所有为 nullptr）
    std::array<ID3D11Texture2D*, 32> textures_y = {};
    std::array<ID3D11Texture2D*, 32> textures_uv = {};
    std::array<int, 32> slice_indices = {};

    const auto& cells = compositor_->GetAllCells();
    for (int i = 0; i < 32; ++i) {
        if (cells[i].valid && cells[i].texture) {
            textures_y[i] = cells[i].texture;
            textures_uv[i] = cells[i].texture;
            slice_indices[i] = cells[i].slice_index;
        }
        // 无效的单元格 textures[i] 保持 nullptr
    }

    // 调用渲染器（零拷贝，Shader 直接采样）
    bool success = d3d_ctx_->RenderTiled(
        textures_y, textures_uv, slice_indices,
        config_.rows, config_.cols);

    if (!success) {
        static bool warned = false;
        if (!warned) {
            std::cerr << "Failed to render tiled display" << std::endl;
            warned = true;
        }
    }
}

void XRenderManager::PrintStats() const
{
    std::cout << "\n========== XRenderManager Statistics (Zero-Copy) ==========" << std::endl;
    std::cout << "Total frames rendered: " << total_frames_rendered_.load() << std::endl;
    std::cout << "Target FPS: " << config_.render_fps << std::endl;
    std::cout << "Resolution: " << config_.target_width << "x" << config_.target_height << std::endl;
    std::cout << "Grid: " << config_.rows << "x" << config_.cols << std::endl;

    // 统计有帧的流数量
    size_t active_streams = 0;
    for (const auto& frame : current_frames_) {
        if (frame) {
            active_streams++;
        }
    }
    std::cout << "Streams with data: " << active_streams << std::endl;

    std::cout << "============================================================" << std::endl;
}
