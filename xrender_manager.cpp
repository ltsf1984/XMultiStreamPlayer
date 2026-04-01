// xrender_manager.cpp
#include "xrender_manager.h"
#include <iostream>
#include <chrono>

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

    // 初始化合成器
    XCompositorConfig compositor_config;
    compositor_config.rows = config_.rows;
    compositor_config.cols = config_.cols;
    compositor_config.target_width = config_.target_width;
    compositor_config.target_height = config_.target_height;
    compositor_config.placeholder_r = 255;
    compositor_config.placeholder_g = 0;
    compositor_config.placeholder_b = 0;
    compositor_config.placeholder_a = 255;

    compositor_ = std::make_unique<XImageCompositor>();
    if (!compositor_->Initialize(compositor_config)) {
        std::cerr << "Failed to initialize compositor" << std::endl;
        return false;
    }

    // 初始化帧容器
    current_frames_.resize(config_.max_streams, nullptr);

    // 计算帧间隔
    if (config_.render_fps > 0) {
        frame_interval_ = std::chrono::microseconds(1000000 / config_.render_fps);
    }

    last_stats_time_ = std::chrono::steady_clock::now();

    std::cout << "XRenderManager initialized: "
        << config_.rows << "x" << config_.cols
        << ", target size: " << config_.target_width << "x" << config_.target_height
        << ", FPS: " << config_.render_fps
        << ", max streams: " << config_.max_streams
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
        std::cout << "XRenderManager already running" << std::endl;
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

    std::cout << "XRenderManager started" << std::endl;
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
        << total_frames_rendered_.load()
        << ", missed: " << total_frames_missed_.load() << std::endl;
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

    // 更新合成器配置
    XCompositorConfig compositor_config;
    compositor_config.rows = config_.rows;
    compositor_config.cols = config_.cols;
    compositor_config.target_width = config_.target_width;
    compositor_config.target_height = config_.target_height;
    compositor_config.placeholder_r = 255;
    compositor_config.placeholder_g = 0;
    compositor_config.placeholder_b = 0;
    compositor_config.placeholder_a = 255;

    if (compositor_) {
        compositor_->UpdateConfig(compositor_config);
    }

    // 更新帧间隔
    if (config_.render_fps > 0) {
        frame_interval_ = std::chrono::microseconds(1000000 / config_.render_fps);
    }

    // 调整帧容器大小
    if (config_.max_streams != current_frames_.size()) {
        current_frames_.resize(config_.max_streams, nullptr);
    }

    std::cout << "XRenderManager config updated" << std::endl;
}

void XRenderManager::RenderLoop()
{
    std::cout << "Render loop started" << std::endl;

    while (running_) {
        // 检查暂停状态
        {
            std::unique_lock<std::mutex> lock(pause_mtx_);
            pause_cv_.wait(lock, [this]() {
                return !paused_ || !running_;
                });
        }

        if (!running_) break;

        // 帧率控制
        auto now = std::chrono::steady_clock::now();
        auto elapsed = now - last_render_time_;

        if (elapsed < frame_interval_) {
            auto sleep_time = frame_interval_ - elapsed;
            std::this_thread::sleep_for(sleep_time);
            now = std::chrono::steady_clock::now();
        }

        // 收集所有流的当前帧
        CollectFrames();

        // 合成并渲染
        RenderCompositeTexture();

        last_render_time_ = now;
        total_frames_rendered_++;

        // 定期打印统计（每 5 秒）
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
    if (!frame_dequeue_) {
        return;
    }

    // 遍历所有流
    for (size_t i = 0; i < current_frames_.size(); ++i) {
        AVFramePtr frame;

        // 尝试从队列获取帧（非阻塞）
        if (frame_dequeue_(i, frame)) {
            if (frame) {
                // 成功获取新帧，替换旧帧
                current_frames_[i] = std::move(frame);
            }
            else {
                // 队列空，记录未命中
                total_frames_missed_++;
            }
        }
    }
}

void XRenderManager::RenderCompositeTexture()
{
    if (!compositor_ || !d3d_ctx_) {
        return;
    }

    // 准备帧指针数组（传递给合成器）
    std::vector<AVFrame*> frame_ptrs;
    frame_ptrs.reserve(current_frames_.size());

    for (auto& frame : current_frames_) {
        frame_ptrs.push_back(frame.get());
    }

    // 调用合成器合成图片
    ID3D11Texture2D* composite_texture = compositor_->Composite(frame_ptrs);

    if (composite_texture) {
        // 渲染合成后的纹理到窗口
        bool success = d3d_ctx_->RenderFrame(composite_texture, 0);

        if (!success) {
            static bool warned = false;
            if (!warned) {
                std::cerr << "Failed to render composite texture" << std::endl;
                warned = true;
            }
        }
    }
    else {
        static bool warned = false;
        if (!warned) {
            std::cerr << "Failed to composite frames" << std::endl;
            warned = true;
        }
    }
}

void XRenderManager::PrintStats() const
{
    std::cout << "\n========== XRenderManager Statistics ==========" << std::endl;
    std::cout << "Total frames rendered: " << total_frames_rendered_.load() << std::endl;
    std::cout << "Total frames missed: " << total_frames_missed_.load() << std::endl;
    std::cout << "Frame miss rate: "
        << (total_frames_rendered_.load() > 0 ?
            (total_frames_missed_.load() * 100.0 / total_frames_rendered_.load()) : 0)
        << "%" << std::endl;
    std::cout << "Target FPS: " << config_.render_fps << std::endl;
    std::cout << "Resolution: " << config_.target_width << "x" << config_.target_height << std::endl;
    std::cout << "Grid: " << config_.rows << "x" << config_.cols << std::endl;
    std::cout << "Active streams: " << current_frames_.size() << std::endl;

    // 统计有帧的流数量
    size_t active_streams = 0;
    for (const auto& frame : current_frames_) {
        if (frame) {
            active_streams++;
        }
    }
    std::cout << "Streams with data: " << active_streams << std::endl;

    if (compositor_) {
        std::cout << "Total frames composited: " << compositor_->GetTotalFramesComposited() << std::endl;
    }

    std::cout << "================================================" << std::endl;
}

void XRenderManager::UpdateFrameRate()
{
    if (config_.render_fps > 0) {
        frame_interval_ = std::chrono::microseconds(1000000 / config_.render_fps);
    }
}
