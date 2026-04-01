// xmulti_demuxer_manager.h
#pragma once

#include <vector>
#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <iostream>
#include <condition_variable>
#include "xdemuxer.h"
#include "av_utils.h"

// 解封装器配置
struct DemuxConfig {
    size_t max_sources = 32;           // 最大源数量
    int poll_interval_us = 100;        // 轮询间隔（微秒）
    bool enable_auto_expand = true;    // 是否自动扩展
};

// 源统计信息
struct SourceStats {
    size_t source_id;
    bool valid;
    bool eof;
    size_t packet_count;
    std::string filename;
};

// 多源解封装管理器
class XMultiDemuxerManager
{
public:
    XMultiDemuxerManager();
    explicit XMultiDemuxerManager(const DemuxConfig& config);
    ~XMultiDemuxerManager();

    // ========== 源管理 ==========
    bool AddSource(size_t source_id, const std::string& filename);
    void AddSources(const std::vector<std::string>& filenames, size_t start_id = 0);
    bool RemoveSource(size_t source_id);

    // ========== 获取信息 ==========
    bool HasSource(size_t source_id) const;
    XDemuxer* GetDemuxer(size_t source_id);
    size_t GetActiveCount() const;
    std::vector<size_t> GetValidSourceIds() const;
    AVCodecParameters* GetVideoCodecpar(size_t source_id);
    AVRational GetVideoTimeBase(size_t source_id);

    // ========== 读取操作 ==========
    bool ReadPacket(size_t source_id, AVPacketPtr& pkt);
    bool TryReadPacket(size_t source_id, AVPacketPtr& pkt);

    // ========== 轮询模式 ==========
    using PacketEnqueue = std::function<void(size_t, AVPacketPtr)>;
    void SetPacketEnqueue(PacketEnqueue callback);
    void StartPolling();
    void StartPolling(PacketEnqueue callback);
    void Stop();
    void Pause();
    void Resume();

    // ========== 统计信息 ==========
    std::vector<SourceStats> GetAllStats() const;
    void PrintStats() const;

    // ========== 配置 ==========
    void SetPollInterval(int microseconds);
    const DemuxConfig& GetConfig() const;

private:
    void ExpandTo(size_t new_size);
    void PollingLoop();

private:
    DemuxConfig config_;

    // 解封装器数组（nullptr 表示该位置未使用）
    std::vector<std::unique_ptr<XDemuxer>> demuxers_;

    // 元数据
    std::vector<int> source_ids_;
    std::vector<std::string> source_names_;
    std::vector<bool> eof_flags_;
    std::vector<size_t> packet_counts_;

    // 轮询相关
    std::unique_ptr<std::thread> polling_thread_;
    std::atomic<bool> polling_running_{ false };
    std::atomic<bool> paused_{ false };
    std::condition_variable pause_cv_;
    std::mutex pause_mtx_;

    // 回调函数（用于分发 packet）
    PacketEnqueue packet_enqueue_;
};
