// xmulti_decoder_manager.cpp
#include "xmulti_decoder_manager.h"

XMultiDecoderManager::XMultiDecoderManager()
    : config_()
{
}

XMultiDecoderManager::XMultiDecoderManager(const DecoderConfig& config)
    : config_(config)
{
    Initialize();
}

XMultiDecoderManager::~XMultiDecoderManager()
{
    StopAll();
}

void XMultiDecoderManager::SetConfig(const DecoderConfig& config)
{
    config_ = config;
    Initialize();
}

bool XMultiDecoderManager::Initialize()
{
    if (config_.decoder_count == 0) {
        std::cerr << "Invalid decoder count: 0" << std::endl;
        return false;
    }

    // 调整所有数组大小
    decoders_.resize(config_.decoder_count);
    decoder_threads_.resize(config_.decoder_count);

    // 使用 unique_ptr 包装 atomic，避免拷贝问题
    states_.clear();
    packets_received_.clear();
    packets_decoded_.clear();
    frames_output_.clear();
    last_pts_.clear();

    for (size_t i = 0; i < config_.decoder_count; ++i) {
        states_.push_back(std::make_unique<std::atomic<DecoderState>>(DecoderState::Idle));
        packets_received_.push_back(std::make_unique<std::atomic<size_t>>(0));
        packets_decoded_.push_back(std::make_unique<std::atomic<size_t>>(0));
        frames_output_.push_back(std::make_unique<std::atomic<size_t>>(0));
        last_pts_.push_back(std::make_unique<std::atomic<int64_t>>(0));
    }

    error_msgs_.resize(config_.decoder_count);

    std::cout << "MultiDecoderSystem initialized with "
        << config_.decoder_count << " decoders" << std::endl;
    return true;
}

// ========== 回调设置 ==========
void XMultiDecoderManager::SetCallbacks(const DecoderCallbacks& callbacks)
{
    callbacks_ = callbacks;
}

bool XMultiDecoderManager::InitDecoder(size_t source_id, AVCodecParameters* codecpar)
{
    if (source_id >= config_.decoder_count) {
        std::cerr << "Source ID " << source_id << " out of range" << std::endl;
        return false;
    }

    if (decoders_[source_id] != nullptr) {
        decoders_[source_id]->Close();
    }

    auto decoder = std::make_unique<XDecoder>();

    if (!decoder->Create(codecpar->codec_id, false)) {
        std::string err = "Failed to create decoder for source " + std::to_string(source_id);
        std::cerr << err << std::endl;
        SetError(source_id, err);
        return false;
    }

    if (config_.use_hardware && config_.d3d_ctx) {
        if (!decoder->SetCodecHw(config_.d3d_ctx)) {
            std::string err = "Failed to set hardware decoding for source " +
                std::to_string(source_id);
            std::cerr << err << std::endl;
            SetError(source_id, err);
        }
    }

    if (avcodec_parameters_to_context(decoder->GetContext(), codecpar) < 0) {
        std::string err = "Failed to copy codec parameters for source " +
            std::to_string(source_id);
        std::cerr << err << std::endl;
        SetError(source_id, err);
        return false;
    }

    if (!decoder->Open()) {
        std::string err = "Failed to open decoder for source " + std::to_string(source_id);
        std::cerr << err << std::endl;
        SetError(source_id, err);
        return false;
    }

    decoders_[source_id] = std::move(decoder);
    states_[source_id]->store(DecoderState::Idle);

    std::cout << "Initialized decoder for source " << source_id << std::endl;
    return true;
}

void XMultiDecoderManager::InitAllDecoders(AVCodecParameters* codecpar)
{
    for (size_t i = 0; i < config_.decoder_count; ++i) {
        InitDecoder(i, codecpar);
    }
}

bool XMultiDecoderManager::StartDecoder(size_t source_id)
{
    if (source_id >= config_.decoder_count) {
        return false;
    }

    if (decoders_[source_id] == nullptr) {
        std::cerr << "Decoder for source " << source_id << " not initialized" << std::endl;
        return false;
    }

    DecoderState current = states_[source_id]->load();
    if (current == DecoderState::Running) {
        return true;
    }

    if (current == DecoderState::Stopped) {
        packets_received_[source_id]->store(0);
        packets_decoded_[source_id]->store(0);
        frames_output_[source_id]->store(0);
        last_pts_[source_id]->store(0);
    }

    states_[source_id]->store(DecoderState::Running);

    decoder_threads_[source_id] = std::make_unique<std::thread>(
        &XMultiDecoderManager::DecoderThreadFunc, this, source_id
        );

    std::cout << "Started decoder thread for source " << source_id << std::endl;
    return true;
}

void XMultiDecoderManager::StartAll()
{
    for (size_t i = 0; i < config_.decoder_count; ++i) {
        if (decoders_[i] != nullptr) {
            StartDecoder(i);
        }
    }
    std::cout << "All decoders started" << std::endl;
}

void XMultiDecoderManager::StopDecoder(size_t source_id, bool wait)
{
    if (source_id >= config_.decoder_count) {
        return;
    }

    DecoderState current = states_[source_id]->load();
    if (current != DecoderState::Running && current != DecoderState::Paused) {
        return;
    }

    states_[source_id]->store(DecoderState::Stopped);

    if (wait && decoder_threads_[source_id] && decoder_threads_[source_id]->joinable()) {
        decoder_threads_[source_id]->join();
    }

    std::cout << "Stopped decoder thread for source " << source_id
        << ", frames output: " << frames_output_[source_id]->load() << std::endl;
}

void XMultiDecoderManager::StopAll()
{
    for (size_t i = 0; i < config_.decoder_count; ++i) {
        DecoderState current = states_[i]->load();
        if (current == DecoderState::Running || current == DecoderState::Paused) {
            StopDecoder(i, false);
        }
    }

    for (auto& thread : decoder_threads_) {
        if (thread && thread->joinable()) {
            thread->join();
        }
    }

    std::cout << "All decoders stopped" << std::endl;
}

void XMultiDecoderManager::PauseDecoder(size_t source_id)
{
    if (source_id >= config_.decoder_count) {
        return;
    }

    DecoderState expected = DecoderState::Running;
    if (states_[source_id]->compare_exchange_strong(expected, DecoderState::Paused)) {
        std::cout << "Paused decoder for source " << source_id << std::endl;
    }
}

void XMultiDecoderManager::PauseAll()
{
    for (size_t i = 0; i < config_.decoder_count; ++i) {
        PauseDecoder(i);
    }
}

void XMultiDecoderManager::ResumeDecoder(size_t source_id)
{
    if (source_id >= config_.decoder_count) {
        return;
    }

    DecoderState expected = DecoderState::Paused;
    if (states_[source_id]->compare_exchange_strong(expected, DecoderState::Running)) {
        pause_cv_.notify_all();
        std::cout << "Resumed decoder for source " << source_id << std::endl;
    }
}

void XMultiDecoderManager::ResumeAll()
{
    for (size_t i = 0; i < config_.decoder_count; ++i) {
        ResumeDecoder(i);
    }
}

DecoderState XMultiDecoderManager::GetState(size_t source_id) const
{
    if (source_id >= config_.decoder_count) {
        return DecoderState::Error;
    }
    return states_[source_id]->load();
}

bool XMultiDecoderManager::IsRunning(size_t source_id) const
{
    return states_[source_id]->load() == DecoderState::Running;
}

bool XMultiDecoderManager::IsAllRunning() const
{
    for (size_t i = 0; i < config_.decoder_count; ++i) {
        if (decoders_[i] != nullptr && states_[i]->load() != DecoderState::Running) {
            return false;
        }
    }
    return true;
}

DecoderStats XMultiDecoderManager::GetStats(size_t source_id) const
{
    DecoderStats stats;
    stats.source_id = source_id;
    stats.state = states_[source_id]->load();
    stats.packets_received = packets_received_[source_id]->load();
    stats.packets_decoded = packets_decoded_[source_id]->load();
    stats.frames_output = frames_output_[source_id]->load();
    stats.queue_size = 0;
    stats.last_pts = last_pts_[source_id]->load();
    stats.error_msg = error_msgs_[source_id];
    return stats;
}

std::vector<DecoderStats> XMultiDecoderManager::GetAllStats() const
{
    std::vector<DecoderStats> stats;
    for (size_t i = 0; i < config_.decoder_count; ++i) {
        if (decoders_[i] != nullptr) {
            stats.push_back(GetStats(i));
        }
    }
    return stats;
}

void XMultiDecoderManager::PrintStats() const
{
    std::cout << "\n=== MultiDecoderSystem Statistics ===" << std::endl;
    std::cout << "Total decoders: " << config_.decoder_count << std::endl;

    size_t total_packets = 0;
    size_t total_frames = 0;
    size_t active_count = 0;

    for (size_t i = 0; i < config_.decoder_count; ++i) {
        if (decoders_[i] != nullptr) {
            active_count++;
            total_packets += packets_received_[i]->load();
            total_frames += frames_output_[i]->load();

            std::cout << "  Decoder[" << i << "]: "
                << "state=" << StateToString(states_[i]->load())
                << ", packets=" << packets_received_[i]->load()
                << "/" << packets_decoded_[i]->load()
                << ", frames=" << frames_output_[i]->load()
                << ", pts=" << last_pts_[i]->load()
                << std::endl;
        }
    }

    std::cout << "Active decoders: " << active_count << std::endl;
    std::cout << "Total packets: " << total_packets << std::endl;
    std::cout << "Total frames: " << total_frames << std::endl;
    std::cout << "=======================================" << std::endl;
}

bool XMultiDecoderManager::AddDecoder()
{
    size_t new_count = config_.decoder_count + 1;
    config_.decoder_count = new_count;

    decoders_.resize(new_count);
    decoder_threads_.resize(new_count);

    states_.push_back(std::make_unique<std::atomic<DecoderState>>(DecoderState::Idle));
    packets_received_.push_back(std::make_unique<std::atomic<size_t>>(0));
    packets_decoded_.push_back(std::make_unique<std::atomic<size_t>>(0));
    frames_output_.push_back(std::make_unique<std::atomic<size_t>>(0));
    last_pts_.push_back(std::make_unique<std::atomic<int64_t>>(0));
    error_msgs_.resize(new_count);

    std::cout << "Added decoder, total: " << new_count << std::endl;
    return true;
}

void XMultiDecoderManager::SetD3DContext(std::shared_ptr<D3D11HardwareContext> d3d_ctx)
{
    config_.d3d_ctx = d3d_ctx;
}

void XMultiDecoderManager::DecoderThreadFunc(size_t source_id)
{
    auto& decoder = decoders_[source_id];
    auto& state = states_[source_id];

    std::cout << "Decoder thread " << source_id << " started" << std::endl;

    while (true) {
        DecoderState current = state->load();
        if (current == DecoderState::Stopped) {
            break;
        }

        if (current == DecoderState::Paused) {
            std::unique_lock<std::mutex> lock(pause_mtx_);
            pause_cv_.wait(lock, [this, source_id]() {
                return states_[source_id]->load() != DecoderState::Paused;
                });
            continue;
        }

        if (current != DecoderState::Running) {
            break;
        }

        // 使用 on_packet_dequeue 回调获取 packet
        AVPacketPtr pkt;
        if (!callbacks_.on_packet_dequeue || !callbacks_.on_packet_dequeue(source_id, pkt)) {
            break;
        }

        if (!pkt) {
            break;
        }

        packets_received_[source_id]->fetch_add(1);

        int retry_count = 0;
        XCodec::SendResult send_result;
        do {
            send_result = decoder->SendPacket(pkt.get());
            if (send_result == XCodec::SendResult::NeedDrain) {
                DrainDecoder(source_id);
            }
            else if (send_result == XCodec::SendResult::Failed) {
                retry_count++;
                if (retry_count >= config_.max_retry_count) {
                    std::string err = "Failed to send packet after " +
                        std::to_string(retry_count) + " retries";
                    SetError(source_id, err);
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        } while (send_result == XCodec::SendResult::NeedDrain);

        if (send_result == XCodec::SendResult::Failed) {
            continue;
        }

        packets_decoded_[source_id]->fetch_add(1);
        DrainDecoder(source_id);
    }

    DrainDecoder(source_id);

    std::cout << "Decoder thread " << source_id << " stopped, "
        << "packets: " << packets_received_[source_id]->load()
        << ", frames: " << frames_output_[source_id]->load() << std::endl;

    state->store(DecoderState::Stopped);
}

void XMultiDecoderManager::DrainDecoder(size_t source_id)
{
    auto& decoder = decoders_[source_id];

    while (true) {
        AVFramePtr frame = make_frame();
        auto recv_result = decoder->ReceiveFrame(frame.get());

        if (recv_result == XCodec::ReceiveResult::NeedFeed ||
            recv_result == XCodec::ReceiveResult::Ended ||
            recv_result == XCodec::ReceiveResult::Failed) {
            break;
        }

        if (recv_result == XCodec::ReceiveResult::Success) {
            frames_output_[source_id]->fetch_add(1);
            if (frame->pts != AV_NOPTS_VALUE) {
                last_pts_[source_id]->store(frame->pts);
            }

            // 使用 on_frame_enqueue 回调将 frame 入队
            if (callbacks_.on_frame_enqueue) {
                callbacks_.on_frame_enqueue(source_id, std::move(frame));
            }
        }
    }
}

void XMultiDecoderManager::SetError(size_t source_id, const std::string& error)
{
    error_msgs_[source_id] = error;
    states_[source_id]->store(DecoderState::Error);

    if (callbacks_.on_error) {
        callbacks_.on_error(source_id, error);
    }
}

std::string XMultiDecoderManager::StateToString(DecoderState state)
{
    switch (state) {
    case DecoderState::Idle:    return "Idle";
    case DecoderState::Running: return "Running";
    case DecoderState::Paused:  return "Paused";
    case DecoderState::Stopped: return "Stopped";
    case DecoderState::Error:   return "Error";
    default:                    return "Unknown";
    }
}
