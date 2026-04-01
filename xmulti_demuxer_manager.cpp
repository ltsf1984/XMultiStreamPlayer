// xmulti_demuxer_manager.cpp
#include "xmulti_demuxer_manager.h"
#include <chrono>

XMultiDemuxerManager::XMultiDemuxerManager()
    : config_()
{
}

XMultiDemuxerManager::XMultiDemuxerManager(const DemuxConfig& config)
    : config_(config)
{
    if (config_.max_sources > 0) {
        demuxers_.resize(config_.max_sources);
        source_ids_.resize(config_.max_sources, -1);
        source_names_.resize(config_.max_sources);
        eof_flags_.resize(config_.max_sources, false);
        packet_counts_.resize(config_.max_sources, 0);
    }
}

XMultiDemuxerManager::~XMultiDemuxerManager()
{
    Stop();
}

bool XMultiDemuxerManager::AddSource(size_t source_id, const std::string& filename)
{
    // 自动扩展
    if (source_id >= demuxers_.size()) {
        if (!config_.enable_auto_expand) {
            std::cerr << "Source ID " << source_id << " out of range" << std::endl;
            return false;
        }
        ExpandTo(source_id + 1);
    }

    // 检查是否已存在
    if (demuxers_[source_id] != nullptr) {
        std::cerr << "Source " << source_id << " already exists" << std::endl;
        return false;
    }

    // 创建解封装器
    auto demuxer = std::make_unique<XDemuxer>();
    if (!demuxer->Open(filename)) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return false;
    }

    // 检查是否有视频流
    if (demuxer->video_index() < 0) {
        std::cerr << "No video stream in file: " << filename << std::endl;
        return false;
    }

    // 存储
    demuxers_[source_id] = std::move(demuxer);
    source_ids_[source_id] = static_cast<int>(source_id);
    source_names_[source_id] = filename;
    eof_flags_[source_id] = false;
    packet_counts_[source_id] = 0;

    std::cout << "Added source " << source_id << ": " << filename << std::endl;
    return true;
}

void XMultiDemuxerManager::AddSources(const std::vector<std::string>& filenames, size_t start_id)
{
    for (size_t i = 0; i < filenames.size(); ++i) {
        AddSource(start_id + i, filenames[i]);
    }
}

bool XMultiDemuxerManager::RemoveSource(size_t source_id)
{
    if (source_id >= demuxers_.size()) {
        return false;
    }

    if (demuxers_[source_id] == nullptr) {
        return false;
    }

    demuxers_[source_id].reset();
    source_ids_[source_id] = -1;
    source_names_[source_id].clear();
    eof_flags_[source_id] = false;
    packet_counts_[source_id] = 0;

    std::cout << "Removed source " << source_id << std::endl;
    return true;
}

bool XMultiDemuxerManager::HasSource(size_t source_id) const
{
    if (source_id >= demuxers_.size()) {
        return false;
    }
    return demuxers_[source_id] != nullptr;
}

XDemuxer* XMultiDemuxerManager::GetDemuxer(size_t source_id)
{
    if (source_id >= demuxers_.size()) {
        return nullptr;
    }
    return demuxers_[source_id].get();
}

size_t XMultiDemuxerManager::GetActiveCount() const
{
    size_t count = 0;
    for (const auto& demuxer : demuxers_) {
        if (demuxer != nullptr) {
            count++;
        }
    }
    return count;
}

std::vector<size_t> XMultiDemuxerManager::GetValidSourceIds() const
{
    std::vector<size_t> ids;
    for (size_t i = 0; i < demuxers_.size(); ++i) {
        if (demuxers_[i] != nullptr) {
            ids.push_back(i);
        }
    }
    return ids;
}

AVCodecParameters* XMultiDemuxerManager::GetVideoCodecpar(size_t source_id)
{
    auto* demuxer = GetDemuxer(source_id);
    if (!demuxer) return nullptr;
    return demuxer->GetVideoCodecpar();
}

AVRational XMultiDemuxerManager::GetVideoTimeBase(size_t source_id)
{
    auto* demuxer = GetDemuxer(source_id);
    if (!demuxer) return { 0, 1 };
    return demuxer->GetVideoStreamTimebase();
}

bool XMultiDemuxerManager::ReadPacket(size_t source_id, AVPacketPtr& pkt)
{
    if (source_id >= demuxers_.size()) {
        return false;
    }

    auto& demuxer = demuxers_[source_id];
    if (demuxer == nullptr) {
        return false;
    }

    if (eof_flags_[source_id]) {
        return false;
    }

    pkt = make_packet();
    if (!demuxer->Read(pkt.get())) {
        eof_flags_[source_id] = true;
        std::cout << "Source " << source_id << " reached EOF, "
            << "total packets: " << packet_counts_[source_id] << std::endl;
        pkt = nullptr;
        return false;
    }

    // 只处理视频流
    if (pkt->stream_index != demuxer->video_index()) {
        return ReadPacket(source_id, pkt);
    }

    packet_counts_[source_id]++;
    return true;
}

bool XMultiDemuxerManager::TryReadPacket(size_t source_id, AVPacketPtr& pkt)
{
    return ReadPacket(source_id, pkt);
}

void XMultiDemuxerManager::SetPacketEnqueue(PacketEnqueue callback)
{
    packet_enqueue_ = callback;
}

void XMultiDemuxerManager::StartPolling()
{
    if (polling_running_) {
        return;
    }

    polling_running_ = true;
    polling_thread_ = std::make_unique<std::thread>(&XMultiDemuxerManager::PollingLoop, this);
    std::cout << "Polling thread started" << std::endl;
}

void XMultiDemuxerManager::StartPolling(PacketEnqueue callback)
{
    packet_enqueue_ = callback;
    StartPolling();
}

void XMultiDemuxerManager::Stop()
{
    if (!polling_running_) {
        return;
    }

    polling_running_ = false;
    pause_cv_.notify_all();

    if (polling_thread_ && polling_thread_->joinable()) {
        polling_thread_->join();
    }
    std::cout << "Polling thread stopped" << std::endl;
}

void XMultiDemuxerManager::Pause()
{
    paused_ = true;
    std::cout << "Polling paused" << std::endl;
}

void XMultiDemuxerManager::Resume()
{
    paused_ = false;
    pause_cv_.notify_one();
    std::cout << "Polling resumed" << std::endl;
}

std::vector<SourceStats> XMultiDemuxerManager::GetAllStats() const
{
    std::vector<SourceStats> stats;
    for (size_t i = 0; i < demuxers_.size(); ++i) {
        SourceStats s;
        s.source_id = i;
        s.valid = (demuxers_[i] != nullptr);
        s.eof = eof_flags_[i];
        s.packet_count = packet_counts_[i];
        s.filename = source_names_[i];
        stats.push_back(s);
    }
    return stats;
}

void XMultiDemuxerManager::PrintStats() const
{
    std::cout << "\n=== XMultiDemuxer Statistics ===" << std::endl;
    std::cout << "Total sources: " << demuxers_.size() << std::endl;
    std::cout << "Active sources: " << GetActiveCount() << std::endl;
    std::cout << "Polling: " << (polling_running_ ? "running" : "stopped") << std::endl;
    std::cout << "Paused: " << (paused_ ? "yes" : "no") << std::endl;
    std::cout << "Sources:" << std::endl;

    for (const auto& s : GetAllStats()) {
        if (s.valid) {
            std::cout << "  [" << s.source_id << "] " << s.filename
                << " | packets: " << s.packet_count
                << (s.eof ? " (EOF)" : "") << std::endl;
        }
    }
    std::cout << "=================================" << std::endl;
}

void XMultiDemuxerManager::SetPollInterval(int microseconds)
{
    config_.poll_interval_us = microseconds;
}

const DemuxConfig& XMultiDemuxerManager::GetConfig() const
{
    return config_;
}

void XMultiDemuxerManager::ExpandTo(size_t new_size)
{
    size_t old_size = demuxers_.size();
    demuxers_.resize(new_size);
    source_ids_.resize(new_size, -1);
    source_names_.resize(new_size);
    eof_flags_.resize(new_size, false);
    packet_counts_.resize(new_size, 0);

    std::cout << "Expanded from " << old_size << " to " << new_size << std::endl;
}

void XMultiDemuxerManager::PollingLoop()
{
    std::cout << "Polling loop started" << std::endl;

    while (polling_running_) {
        // 检查暂停状态
        {
            std::unique_lock<std::mutex> lock(pause_mtx_);
            pause_cv_.wait(lock, [this]() {
                return !paused_ || !polling_running_;
                });
        }

        if (!polling_running_) break;

        bool any_read = false;

        // 轮询所有有效源
        for (size_t i = 0; i < demuxers_.size(); ++i) {
            if (demuxers_[i] == nullptr || eof_flags_[i]) {
                continue;
            }

            AVPacketPtr pkt;
            if (ReadPacket(i, pkt) && pkt) {
                any_read = true;

                // 调用回调分发 packet
                if (packet_enqueue_) {
                    packet_enqueue_(i, std::move(pkt));
                }
                std::cout << "Demux Stream " << i << std::endl;
                // 每轮只读一个 packet，保证公平性
                continue;
            }
        }

        // 如果没有读到任何 packet，短暂休眠避免忙等
        if (!any_read) {
            std::this_thread::sleep_for(std::chrono::microseconds(config_.poll_interval_us));
        }
    }

    std::cout << "Polling loop ended" << std::endl;
}
