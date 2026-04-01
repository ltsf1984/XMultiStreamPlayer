// xmulti_queue_manager.cpp
#include "xmulti_queue_manager.h"
#include <iostream>

XMultiQueueManager::XMultiQueueManager(const QueueConfig& config)
    : config_(config)
{
    // 创建 packet 队列
    for (size_t i = 0; i < config_.queue_count; ++i) {
        packet_queues_.push_back(
            std::make_unique<ThreadSafeQueue<AVPacketPtr>>(config_.packet_queue_size)
        );
    }

    // 创建 frame 队列
    for (size_t i = 0; i < config_.queue_count; ++i) {
        frame_queues_.push_back(
            std::make_unique<ThreadSafeQueue<AVFramePtr>>(config_.frame_queue_size)
        );
    }

    // 初始化统计
    packet_queue_sizes_.resize(config_.queue_count, 0);
    frame_queue_sizes_.resize(config_.queue_count, 0);
}

bool XMultiQueueManager::PushPacket(size_t source_id, AVPacketPtr packet)
{
    if (source_id >= packet_queues_.size()) {
        return false;
    }
    return packet_queues_[source_id]->Push(std::move(packet));
}

bool XMultiQueueManager::PopPacket(size_t source_id, AVPacketPtr& packet)
{
    if (source_id >= packet_queues_.size()) {
        return false;
    }
    packet_queues_[source_id]->Pop(packet);
    return true;
}

bool XMultiQueueManager::TryPopPacket(size_t source_id, AVPacketPtr& packet)
{
    if (source_id >= packet_queues_.size()) {
        return false;
    }
    return packet_queues_[source_id]->TryPop(packet);
}

size_t XMultiQueueManager::GetPacketQueueSize(size_t source_id) const
{
    if (source_id >= packet_queues_.size()) {
        return 0;
    }
    return packet_queues_[source_id]->Size();
}

bool XMultiQueueManager::PushFrame(size_t source_id, AVFramePtr frame)
{
    if (source_id >= frame_queues_.size()) {
        return false;
    }
    return frame_queues_[source_id]->Push(std::move(frame));
}

bool XMultiQueueManager::PopFrame(size_t source_id, AVFramePtr& frame)
{
    if (source_id >= frame_queues_.size()) {
        return false;
    }
    frame_queues_[source_id]->Pop(frame);
    return true;
}

bool XMultiQueueManager::TryPopFrame(size_t source_id, AVFramePtr& frame)
{
    if (source_id >= frame_queues_.size()) {
        return false;
    }
    return frame_queues_[source_id]->TryPop(frame);
}

size_t XMultiQueueManager::GetFrameQueueSize(size_t source_id) const
{
    if (source_id >= frame_queues_.size()) {
        return 0;
    }
    return frame_queues_[source_id]->Size();
}

void XMultiQueueManager::UpdateQueueSizes()
{
    for (size_t i = 0; i < config_.queue_count; ++i) {
        packet_queue_sizes_[i] = GetPacketQueueSize(i);
        frame_queue_sizes_[i] = GetFrameQueueSize(i);
    }
}

std::vector<size_t> XMultiQueueManager::GetAllPacketQueueSizes() const
{
    std::vector<size_t> sizes;
    for (const auto& queue : packet_queues_) {
        sizes.push_back(queue->Size());
    }
    return sizes;
}

std::vector<size_t> XMultiQueueManager::GetAllFrameQueueSizes() const
{
    std::vector<size_t> sizes;
    for (const auto& queue : frame_queues_) {
        sizes.push_back(queue->Size());
    }
    return sizes;
}

void XMultiQueueManager::ClearAll()
{
    for (auto& queue : packet_queues_) {
        queue->Clear();
    }
    for (auto& queue : frame_queues_) {
        queue->Clear();
    }
}

void XMultiQueueManager::StopAll()
{
    for (auto& queue : packet_queues_) {
        queue->Push(nullptr);
    }
    for (auto& queue : frame_queues_) {
        queue->Push(nullptr);
    }
}

void XMultiQueueManager::PrintStats() const
{
    std::cout << "=== Multi-Queue Statistics ===" << std::endl;
    std::cout << "Packet Queues:" << std::endl;
    for (size_t i = 0; i < config_.queue_count; ++i) {
        if (packet_queue_sizes_[i] > 0 || frame_queue_sizes_[i] > 0) {
            std::cout << "  Source[" << i << "]: packets=" << packet_queue_sizes_[i]
                << ", frames=" << frame_queue_sizes_[i] << std::endl;
        }
    }
    std::cout << "================================" << std::endl;
}
