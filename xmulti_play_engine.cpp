// xmulti_play_engine.cpp
#include "xmulti_play_engine.h"
#include <iostream>

// ========== 构造和析构 ==========
XMultiPlayEngine::XMultiPlayEngine()
    : config_()
{
}

XMultiPlayEngine::XMultiPlayEngine(const PlayEngineConfig& config)
    : config_(config)
{
    Initialize();
}

XMultiPlayEngine::~XMultiPlayEngine()
{
    StopAllStreams();
    StopRendering();
}

// ========== 初始化 ==========
bool XMultiPlayEngine::Initialize()
{
    if (initialized_) {
        return true;
    }

    std::cout << "Initializing XMultiPlayEngine..." << std::endl;

    // 1. 初始化队列管理器
    if (!InitializeQueueManager()) {
        std::cerr << "Failed to initialize queue manager" << std::endl;
        return false;
    }

    // 2. 初始化解封装管理器
    if (!InitializeDemuxerManager()) {
        std::cerr << "Failed to initialize demuxer manager" << std::endl;
        return false;
    }

    // 3. 初始化解码器管理器
    if (!InitializeDecoderManager()) {
        std::cerr << "Failed to initialize decoder manager" << std::endl;
        return false;
    }

    // 4. 初始化渲染管理器
    if (!InitializeRenderManager()) {
        std::cerr << "Failed to initialize render manager" << std::endl;
        return false;
    }

    // 5. 设置回调
    SetupCallbacks();

    // 6. 启动解封装器轮询（持续运行）
    if (demuxer_manager_) {
        demuxer_manager_->StartPolling();
        std::cout << "Demuxer polling thread started" << std::endl;
    }

    initialized_ = true;
    running_ = true;

    std::cout << "XMultiPlayEngine initialized successfully" << std::endl;
    return true;
}

bool XMultiPlayEngine::InitializeQueueManager()
{
    QueueConfig queue_config;
    queue_config.queue_count = config_.max_streams;
    queue_config.packet_queue_size = config_.packet_queue_size;
    queue_config.frame_queue_size = config_.frame_queue_size;

    queue_manager_ = std::make_unique<XMultiQueueManager>(queue_config);
    return true;
}

bool XMultiPlayEngine::InitializeDemuxerManager()
{
    DemuxConfig demux_config;
    demux_config.max_sources = config_.max_streams;
    demux_config.poll_interval_us = config_.demux_poll_interval_us;
    demux_config.enable_auto_expand = true;

    demuxer_manager_ = std::make_unique<XMultiDemuxerManager>(demux_config);
    return true;
}

bool XMultiPlayEngine::InitializeDecoderManager()
{
    DecoderConfig decoder_config;
    decoder_config.decoder_count = config_.max_streams;
    decoder_config.use_hardware = config_.use_hardware_decoder;
    decoder_config.d3d_ctx = d3d_ctx_;
    decoder_config.max_retry_count = 3;

    decoder_manager_ = std::make_unique<XMultiDecoderManager>(decoder_config);
    return true;
}

bool XMultiPlayEngine::InitializeRenderManager()
{
    XRenderManagerConfig render_config;
    render_config.rows = config_.render_rows;
    render_config.cols = config_.render_cols;
    render_config.target_width = config_.render_target_width;
    render_config.target_height = config_.render_target_height;
    render_config.render_fps = config_.render_fps;
    render_config.max_streams = config_.max_streams;

    render_manager_ = std::make_unique<XRenderManager>();
    if (!render_manager_->Initialize(render_config)) {
        return false;
    }

    return true;
}

void XMultiPlayEngine::SetupCallbacks()
{
    // ========== 数据流向 ==========
    // Demuxer → PacketEnqueue → PacketQueue →
    // PacketDequeue → Decoder → FrameEnqueue → FrameQueue →
    // FrameDequeue → Renderer
    // =================================

    // 1. 解封装器 → packet 队列（packet 入队）
    demuxer_manager_->SetPacketEnqueue(
        [this](size_t stream_id, AVPacketPtr packet) {
            if (queue_manager_) {
                queue_manager_->PushPacket(stream_id, std::move(packet));
                total_packets_processed_++;
            }
        }
    );

    // 2. 解码器回调设置
    DecoderCallbacks decoder_callbacks;

    // packet 出队回调：解码器需要 packet 时从队列取出
    decoder_callbacks.on_packet_dequeue =
        [this](size_t stream_id, AVPacketPtr& packet) -> bool {
        if (!queue_manager_) {
            return false;
        }
        // 检查全局暂停
        if (global_paused_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            return true; // 继续等待
        }
        return queue_manager_->TryPopPacket(stream_id, packet);
    };

    // frame 入队回调：解码器产出 frame 时放入队列
    decoder_callbacks.on_frame_enqueue =
        [this](size_t stream_id, AVFramePtr frame) {
        if (queue_manager_ && frame) {
            queue_manager_->PushFrame(stream_id, std::move(frame));
            total_frames_processed_++;
            UpdateStreamInfo(stream_id);
        }
    };

    // 解码器错误回调
    decoder_callbacks.on_error =
        [this](size_t stream_id, const std::string& error) {
        OnDecoderError(stream_id, error);
    };

    decoder_manager_->SetCallbacks(decoder_callbacks);

    // 3. 渲染器 frame 出队回调
    if (render_manager_) {
        render_manager_->SetFrameDequeue(
            [this](size_t stream_id, AVFramePtr& frame) -> bool {
                return queue_manager_ ? queue_manager_->TryPopFrame(stream_id, frame) : false;
            }
        );

        // 设置 D3D 上下文
        if (d3d_ctx_) {
            render_manager_->SetD3DContext(d3d_ctx_);
        }
    }
}

void XMultiPlayEngine::SetConfig(const PlayEngineConfig& config)
{
    config_ = config;
    if (initialized_) {
        // 重新初始化
        initialized_ = false;
        Initialize();
    }
}

void XMultiPlayEngine::SetCallbacks(const PlayEngineCallbacks& callbacks)
{
    callbacks_ = callbacks;
}

void XMultiPlayEngine::SetD3DContext(std::shared_ptr<D3D11HardwareContext> d3d_ctx)
{
    d3d_ctx_ = d3d_ctx;
    if (decoder_manager_) {
        decoder_manager_->SetD3DContext(d3d_ctx);
    }
    if (render_manager_) {
        render_manager_->SetD3DContext(d3d_ctx);
    }
}

// ========== 流管理 ==========
bool XMultiPlayEngine::AddStream(size_t stream_id, const std::string& file_path)
{
    if (!initialized_) {
        std::cerr << "Engine not initialized" << std::endl;
        return false;
    }

    if (!demuxer_manager_->AddSource(stream_id, file_path)) {
        std::cerr << "Failed to add stream " << stream_id << ": " << file_path << std::endl;
        return false;
    }

    // 获取视频参数
    auto* codecpar = demuxer_manager_->GetVideoCodecpar(stream_id);
    if (!codecpar) {
        std::cerr << "Failed to get codec parameters for stream " << stream_id << std::endl;
        demuxer_manager_->RemoveSource(stream_id);
        return false;
    }

    // 初始化解码器
    if (!decoder_manager_->InitDecoder(stream_id, codecpar)) {
        std::cerr << "Failed to initialize decoder for stream " << stream_id << std::endl;
        demuxer_manager_->RemoveSource(stream_id);
        return false;
    }

    // 初始化流信息
    {
        std::lock_guard<std::mutex> lock(stream_info_mutex_);
        StreamInfo info;
        info.stream_id = stream_id;
        info.file_path = file_path;
        info.is_playing = false;
        info.is_paused = false;
        info.packet_queue_size = 0;
        info.frame_queue_size = 0;
        info.packets_decoded = 0;
        info.frames_rendered = 0;
        info.last_pts = 0;
        stream_info_map_[stream_id] = info;
    }

    NotifyStreamAdded(stream_id);
    std::cout << "Stream " << stream_id << " added: " << file_path << std::endl;

    // 自动启动
    if (config_.auto_start) {
        StartStream(stream_id);
    }

    return true;
}

bool XMultiPlayEngine::RemoveStream(size_t stream_id)
{
    if (!initialized_) {
        return false;
    }

    // 停止流
    StopStream(stream_id);

    // 移除解封装器
    demuxer_manager_->RemoveSource(stream_id);

    // 移除流信息
    {
        std::lock_guard<std::mutex> lock(stream_info_mutex_);
        stream_info_map_.erase(stream_id);
    }

    NotifyStreamRemoved(stream_id);
    std::cout << "Stream " << stream_id << " removed" << std::endl;

    return true;
}

bool XMultiPlayEngine::StartStream(size_t stream_id)
{
    if (!initialized_) {
        return false;
    }

    if (!demuxer_manager_->HasSource(stream_id)) {
        std::cerr << "Stream " << stream_id << " does not exist" << std::endl;
        return false;
    }

    // 启动解码器
    if (!decoder_manager_->StartDecoder(stream_id)) {
        std::cerr << "Failed to start decoder for stream " << stream_id << std::endl;
        return false;
    }

    // 更新流信息
    {
        std::lock_guard<std::mutex> lock(stream_info_mutex_);
        auto it = stream_info_map_.find(stream_id);
        if (it != stream_info_map_.end()) {
            it->second.is_playing = true;
            it->second.is_paused = false;
        }
    }

    NotifyStreamStarted(stream_id);
    std::cout << "Stream " << stream_id << " started" << std::endl;

    return true;
}

bool XMultiPlayEngine::StopStream(size_t stream_id)
{
    if (!initialized_) {
        return false;
    }

    // 停止解码器
    decoder_manager_->StopDecoder(stream_id, true);

    // 更新流信息
    {
        std::lock_guard<std::mutex> lock(stream_info_mutex_);
        auto it = stream_info_map_.find(stream_id);
        if (it != stream_info_map_.end()) {
            it->second.is_playing = false;
            it->second.is_paused = false;
        }
    }

    NotifyStreamStopped(stream_id);
    std::cout << "Stream " << stream_id << " stopped" << std::endl;

    return true;
}

bool XMultiPlayEngine::PauseStream(size_t stream_id)
{
    if (!initialized_) {
        return false;
    }

    if (!demuxer_manager_->HasSource(stream_id)) {
        return false;
    }

    decoder_manager_->PauseDecoder(stream_id);

    {
        std::lock_guard<std::mutex> lock(stream_info_mutex_);
        auto it = stream_info_map_.find(stream_id);
        if (it != stream_info_map_.end()) {
            it->second.is_paused = true;
        }
    }

    NotifyStreamPaused(stream_id);
    std::cout << "Stream " << stream_id << " paused" << std::endl;

    return true;
}

bool XMultiPlayEngine::ResumeStream(size_t stream_id)
{
    if (!initialized_) {
        return false;
    }

    if (!demuxer_manager_->HasSource(stream_id)) {
        return false;
    }

    decoder_manager_->ResumeDecoder(stream_id);

    {
        std::lock_guard<std::mutex> lock(stream_info_mutex_);
        auto it = stream_info_map_.find(stream_id);
        if (it != stream_info_map_.end()) {
            it->second.is_paused = false;
        }
    }

    NotifyStreamResumed(stream_id);
    std::cout << "Stream " << stream_id << " resumed" << std::endl;

    return true;
}

void XMultiPlayEngine::StartAllStreams()
{
    auto stream_ids = demuxer_manager_->GetValidSourceIds();
    for (auto id : stream_ids) {
        StartStream(id);
    }
}

void XMultiPlayEngine::StopAllStreams()
{
    auto stream_ids = demuxer_manager_->GetValidSourceIds();
    for (auto id : stream_ids) {
        StopStream(id);
    }
}

void XMultiPlayEngine::PauseAllStreams()
{
    auto stream_ids = demuxer_manager_->GetValidSourceIds();
    for (auto id : stream_ids) {
        PauseStream(id);
    }
}

void XMultiPlayEngine::ResumeAllStreams()
{
    auto stream_ids = demuxer_manager_->GetValidSourceIds();
    for (auto id : stream_ids) {
        ResumeStream(id);
    }
}

// ========== 查询接口 ==========
bool XMultiPlayEngine::HasStream(size_t stream_id) const
{
    return demuxer_manager_ && demuxer_manager_->HasSource(stream_id);
}

StreamInfo XMultiPlayEngine::GetStreamInfo(size_t stream_id) const
{
    StreamInfo info;
    info.stream_id = stream_id;
    info.is_playing = false;
    info.is_paused = false;

    if (!demuxer_manager_ || !demuxer_manager_->HasSource(stream_id)) {
        return info;
    }

    {
        std::lock_guard<std::mutex> lock(stream_info_mutex_);
        auto it = stream_info_map_.find(stream_id);
        if (it != stream_info_map_.end()) {
            info = it->second;
        }
    }

    // 获取实时队列大小
    if (queue_manager_) {
        info.packet_queue_size = queue_manager_->GetPacketQueueSize(stream_id);
        info.frame_queue_size = queue_manager_->GetFrameQueueSize(stream_id);
    }

    // 获取解码器统计
    if (decoder_manager_) {
        auto stats = decoder_manager_->GetStats(stream_id);
        info.packets_decoded = stats.packets_decoded;
        info.frames_rendered = stats.frames_output;
        info.last_pts = stats.last_pts;
        info.error = stats.error_msg;
    }

    return info;
}

std::vector<StreamInfo> XMultiPlayEngine::GetAllStreamInfo() const
{
    std::vector<StreamInfo> infos;
    auto stream_ids = demuxer_manager_->GetValidSourceIds();
    for (auto id : stream_ids) {
        infos.push_back(GetStreamInfo(id));
    }
    return infos;
}

size_t XMultiPlayEngine::GetActiveStreamCount() const
{
    return demuxer_manager_ ? demuxer_manager_->GetActiveCount() : 0;
}

bool XMultiPlayEngine::IsStreamPlaying(size_t stream_id) const
{
    std::lock_guard<std::mutex> lock(stream_info_mutex_);
    auto it = stream_info_map_.find(stream_id);
    return it != stream_info_map_.end() && it->second.is_playing;
}

bool XMultiPlayEngine::IsAllStreamsPlaying() const
{
    auto infos = GetAllStreamInfo();
    for (const auto& info : infos) {
        if (!info.is_playing) {
            return false;
        }
    }
    return infos.size() > 0;
}

// ========== 控制 ==========
void XMultiPlayEngine::SetGlobalPause(bool paused)
{
    global_paused_ = paused;
    std::cout << "Global pause: " << (paused ? "paused" : "resumed") << std::endl;
}

// ========== 渲染管理 ==========
void XMultiPlayEngine::StartRendering()
{
    if (render_manager_) {
        render_manager_->Start();
        std::cout << "Rendering started" << std::endl;
    }
}

void XMultiPlayEngine::StopRendering()
{
    if (render_manager_) {
        render_manager_->Stop();
        std::cout << "Rendering stopped" << std::endl;
    }
}

void XMultiPlayEngine::PauseRendering()
{
    if (render_manager_) {
        render_manager_->Pause();
        std::cout << "Rendering paused" << std::endl;
    }
}

void XMultiPlayEngine::ResumeRendering()
{
    if (render_manager_) {
        render_manager_->Resume();
        std::cout << "Rendering resumed" << std::endl;
    }
}

void XMultiPlayEngine::SetRenderConfig(const XRenderManagerConfig& config)
{
    if (render_manager_) {
        render_manager_->UpdateConfig(config);
    }
}

// ========== 回调处理 ==========
void XMultiPlayEngine::OnDecoderError(size_t stream_id, const std::string& error)
{
    std::cerr << "Decoder error for stream " << stream_id << ": " << error << std::endl;
    NotifyError(stream_id, error);
}

void XMultiPlayEngine::OnDemuxerError(size_t stream_id, const std::string& error)
{
    std::cerr << "Demuxer error for stream " << stream_id << ": " << error << std::endl;
    NotifyError(stream_id, error);
}

// ========== 流状态更新 ==========
void XMultiPlayEngine::UpdateStreamInfo(size_t stream_id)
{
    std::lock_guard<std::mutex> lock(stream_info_mutex_);
    auto it = stream_info_map_.find(stream_id);
    if (it != stream_info_map_.end()) {
        if (queue_manager_) {
            it->second.packet_queue_size = queue_manager_->GetPacketQueueSize(stream_id);
            it->second.frame_queue_size = queue_manager_->GetFrameQueueSize(stream_id);
        }
        // 更新解码统计
        if (decoder_manager_) {
            auto stats = decoder_manager_->GetStats(stream_id);
            it->second.packets_decoded = stats.packets_decoded;
            it->second.frames_rendered = stats.frames_output;
            it->second.last_pts = stats.last_pts;
        }
    }
}

void XMultiPlayEngine::NotifyStreamAdded(size_t stream_id)
{
    if (callbacks_.on_stream_added) {
        callbacks_.on_stream_added(stream_id, GetStreamInfo(stream_id));
    }
}

void XMultiPlayEngine::NotifyStreamRemoved(size_t stream_id)
{
    if (callbacks_.on_stream_removed) {
        callbacks_.on_stream_removed(stream_id, GetStreamInfo(stream_id));
    }
}

void XMultiPlayEngine::NotifyStreamStarted(size_t stream_id)
{
    if (callbacks_.on_stream_started) {
        callbacks_.on_stream_started(stream_id, GetStreamInfo(stream_id));
    }
}

void XMultiPlayEngine::NotifyStreamStopped(size_t stream_id)
{
    if (callbacks_.on_stream_stopped) {
        callbacks_.on_stream_stopped(stream_id, GetStreamInfo(stream_id));
    }
}

void XMultiPlayEngine::NotifyStreamPaused(size_t stream_id)
{
    if (callbacks_.on_stream_paused) {
        callbacks_.on_stream_paused(stream_id, GetStreamInfo(stream_id));
    }
}

void XMultiPlayEngine::NotifyStreamResumed(size_t stream_id)
{
    if (callbacks_.on_stream_resumed) {
        callbacks_.on_stream_resumed(stream_id, GetStreamInfo(stream_id));
    }
}

void XMultiPlayEngine::NotifyError(size_t stream_id, const std::string& error)
{
    if (callbacks_.on_error) {
        callbacks_.on_error(stream_id, error);
    }
}

// ========== 统计 ==========
void XMultiPlayEngine::PrintStatistics() const
{
    std::cout << "\n========== XMultiPlayEngine Statistics ==========" << std::endl;
    std::cout << "Total packets processed: " << total_packets_processed_.load() << std::endl;
    std::cout << "Total frames processed: " << total_frames_processed_.load() << std::endl;
    std::cout << "Active streams: " << GetActiveStreamCount() << std::endl;
    std::cout << "Global paused: " << (global_paused_ ? "Yes" : "No") << std::endl;
    std::cout << std::endl;

    if (demuxer_manager_) {
        demuxer_manager_->PrintStats();
    }

    if (decoder_manager_) {
        decoder_manager_->PrintStats();
    }

    if (queue_manager_) {
        queue_manager_->PrintStats();
    }

    if (render_manager_) {
        render_manager_->PrintStats();
    }

    std::cout << "=================================================" << std::endl;
}
