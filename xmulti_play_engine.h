// xmulti_play_engine.h
#pragma once

#include <memory>
#include <atomic>
#include <thread>
#include <vector>
#include <functional>
#include <unordered_map>
#include "xmulti_queue_manager.h"
#include "xmulti_demuxer_manager.h"
#include "xmulti_decoder_manager.h"
#include "xrender_manager.h"
#include "d3d11_hardware_context.h"
#include "av_utils.h"

// 播放引擎配置
struct PlayEngineConfig {
    size_t max_streams = 32;                 // 最大流数量
    std::string default_video_path;          // 默认视频路径
    bool use_hardware_decoder = true;        // 是否使用硬件解码
    bool auto_start = false;                 // 是否自动启动
    int demux_poll_interval_us = 100;        // 解封装轮询间隔（微秒）
    size_t packet_queue_size = 50;           // packet队列大小
    size_t frame_queue_size = 5;             // frame队列大小

    // 渲染配置
    int render_rows = 4;                     // 渲染行数
    int render_cols = 8;                     // 渲染列数
    int render_target_width = 800;          // 渲染目标宽度
    int render_target_height = 600;         // 渲染目标高度
    int render_fps = 30;                     // 渲染帧率
    bool auto_start_rendering = true;        // 是否自动启动渲染
};

// 流信息
struct StreamInfo {
    size_t stream_id;
    std::string file_path;
    bool is_playing;
    bool is_paused;
    size_t packet_queue_size;
    size_t frame_queue_size;
    size_t packets_decoded;
    size_t frames_rendered;
    int64_t last_pts;
    std::string error;
};

// 播放引擎事件回调
struct PlayEngineCallbacks {
    std::function<void(size_t stream_id, const StreamInfo& info)> on_stream_added;
    std::function<void(size_t stream_id, const StreamInfo& info)> on_stream_removed;
    std::function<void(size_t stream_id, const StreamInfo& info)> on_stream_started;
    std::function<void(size_t stream_id, const StreamInfo& info)> on_stream_stopped;
    std::function<void(size_t stream_id, const StreamInfo& info)> on_stream_paused;
    std::function<void(size_t stream_id, const StreamInfo& info)> on_stream_resumed;
    std::function<void(size_t stream_id, const std::string& error)> on_error;
};

// 多流播放引擎
class XMultiPlayEngine
{
public:
    XMultiPlayEngine();
    explicit XMultiPlayEngine(const PlayEngineConfig& config);
    ~XMultiPlayEngine();

    // ========== 初始化 ==========
    bool Initialize();
    void SetConfig(const PlayEngineConfig& config);
    void SetCallbacks(const PlayEngineCallbacks& callbacks);
    void SetD3DContext(std::shared_ptr<D3D11HardwareContext> d3d_ctx);

    // ========== 流管理 ==========
    bool AddStream(size_t stream_id, const std::string& file_path);
    bool RemoveStream(size_t stream_id);
    bool StartStream(size_t stream_id);
    bool StopStream(size_t stream_id);
    bool PauseStream(size_t stream_id);
    bool ResumeStream(size_t stream_id);
    void StartAllStreams();
    void StopAllStreams();
    void PauseAllStreams();
    void ResumeAllStreams();

    // ========== 查询接口 ==========
    bool HasStream(size_t stream_id) const;
    StreamInfo GetStreamInfo(size_t stream_id) const;
    std::vector<StreamInfo> GetAllStreamInfo() const;
    size_t GetActiveStreamCount() const;
    bool IsStreamPlaying(size_t stream_id) const;
    bool IsAllStreamsPlaying() const;

    // ========== 控制 ==========
    void SetGlobalPause(bool paused);
    bool IsGlobalPaused() const { return global_paused_; }

    // ========== 渲染管理 ==========
    void StartRendering();
    void StopRendering();
    void PauseRendering();
    void ResumeRendering();
    void SetRenderConfig(const XRenderManagerConfig& config);
    bool IsRendering() const { return render_manager_ && render_manager_->IsRunning(); }
    XRenderManager* GetRenderManager() { return render_manager_.get(); }

    // ========== Frame 队列操作（供外部使用）==========
    bool TryPopFrame(size_t stream_id, AVFramePtr& frame) {
        return queue_manager_ ? queue_manager_->TryPopFrame(stream_id, frame) : false;
    }
    size_t GetFrameQueueSize(size_t stream_id) const {
        return queue_manager_ ? queue_manager_->GetFrameQueueSize(stream_id) : 0;
    }

    // ========== 统计 ==========
    void PrintStatistics() const;

private:
    // ========== 内部初始化 ==========
    bool InitializeQueueManager();
    bool InitializeDemuxerManager();
    bool InitializeDecoderManager();
    bool InitializeRenderManager();
    void SetupCallbacks();

    // ========== 回调处理 ==========
    void OnDecoderError(size_t stream_id, const std::string& error);
    void OnDemuxerError(size_t stream_id, const std::string& error);

    // ========== 流状态更新 ==========
    void UpdateStreamInfo(size_t stream_id);
    void NotifyStreamAdded(size_t stream_id);
    void NotifyStreamRemoved(size_t stream_id);
    void NotifyStreamStarted(size_t stream_id);
    void NotifyStreamStopped(size_t stream_id);
    void NotifyStreamPaused(size_t stream_id);
    void NotifyStreamResumed(size_t stream_id);
    void NotifyError(size_t stream_id, const std::string& error);

    // ========== 内部数据 ==========
    PlayEngineConfig config_;
    PlayEngineCallbacks callbacks_;
    std::shared_ptr<D3D11HardwareContext> d3d_ctx_;

    // 管理器组件
    std::unique_ptr<XMultiQueueManager> queue_manager_;
    std::unique_ptr<XMultiDemuxerManager> demuxer_manager_;
    std::unique_ptr<XMultiDecoderManager> decoder_manager_;
    std::unique_ptr<XRenderManager> render_manager_;

    // 流信息缓存
    mutable std::mutex stream_info_mutex_;
    std::unordered_map<size_t, StreamInfo> stream_info_map_;

    // 全局控制
    std::atomic<bool> global_paused_{ false };
    std::atomic<bool> initialized_{ false };
    std::atomic<bool> running_{ false };

    // 统计
    std::atomic<size_t> total_frames_processed_{ 0 };
    std::atomic<size_t> total_packets_processed_{ 0 };
};
