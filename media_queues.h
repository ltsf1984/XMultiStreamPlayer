// media_queues.h
#pragma once
#include "observable_queue.h"
#include "av_utils.h"


class MediaQueues {
public:
    static MediaQueues& Instance() {
        static MediaQueues instance;
        return instance;
    }

    ObservableQueue<AVPacketPtr>& VideoPacketQueue() { return video_packet_queue_; }
    ObservableQueue<AVPacketPtr>& AudioPacketQueue() { return audio_packet_queue_; }
    ObservableQueue<AVFramePtr>& VideoFrameQueue() { return video_frame_queue_; }
    ObservableQueue<AVFramePtr>& AudioFrameQueue() { return audio_frame_queue_; }
    ObservableQueue<PcmBlockPtr>& AudioPcmQueue() { return audio_pcm_queue_; }

private:
    MediaQueues() = default;
    ~MediaQueues() = default;

    // 禁用拷贝
    MediaQueues(const MediaQueues&) = delete;
    MediaQueues& operator=(const MediaQueues&) = delete;

    ObservableQueue<AVPacketPtr> video_packet_queue_{ 5 };
    ObservableQueue<AVPacketPtr> audio_packet_queue_{ 50 };
    ObservableQueue<AVFramePtr> video_frame_queue_{ 5 };
    ObservableQueue<AVFramePtr> audio_frame_queue_{ 10 };
    ObservableQueue<PcmBlockPtr> audio_pcm_queue_{10};
};
