// xmulti_queue_manager.h
#pragma once

#include <vector>
#include <memory>
#include <atomic>
#include <thread>
#include "thread_safe_queue.h"
#include "av_utils.h"

// 队列配置
struct QueueConfig {
    size_t packet_queue_size = 50;   // packet队列大小
    size_t frame_queue_size = 5;     // frame队列大小
    size_t queue_count = 32;         // 队列数量
};

// 多队列系统（管理 packet 队列和 frame 队列）
class XMultiQueueManager
{
public:
    explicit XMultiQueueManager(const QueueConfig& config = QueueConfig());
    ~XMultiQueueManager() = default;

    // ========== Packet 队列操作 ==========
    bool PushPacket(size_t source_id, AVPacketPtr packet);
    bool PopPacket(size_t source_id, AVPacketPtr& packet);
    bool TryPopPacket(size_t source_id, AVPacketPtr& packet);
    size_t GetPacketQueueSize(size_t source_id) const;

    // ========== Frame 队列操作 ==========
    bool PushFrame(size_t source_id, AVFramePtr frame);
    bool PopFrame(size_t source_id, AVFramePtr& frame);
    bool TryPopFrame(size_t source_id, AVFramePtr& frame);
    size_t GetFrameQueueSize(size_t source_id) const;

    // ========== 批量操作 ==========
    void UpdateQueueSizes();
    std::vector<size_t> GetAllPacketQueueSizes() const;
    std::vector<size_t> GetAllFrameQueueSizes() const;

    // ========== 控制操作 ==========
    void ClearAll();
    void StopAll();

    // ========== 查询接口 ==========
    const QueueConfig& GetConfig() const { return config_; }
    size_t GetQueueCount() const { return config_.queue_count; }
    void PrintStats() const;

private:
    QueueConfig config_;

    // packet 队列数组
    std::vector<std::unique_ptr<ThreadSafeQueue<AVPacketPtr>>> packet_queues_;

    // frame 队列数组
    std::vector<std::unique_ptr<ThreadSafeQueue<AVFramePtr>>> frame_queues_;

    // 统计缓存
    mutable std::vector<size_t> packet_queue_sizes_;
    mutable std::vector<size_t> frame_queue_sizes_;
};
