// av_utils.h
#pragma once
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
}

#include <memory>

struct PcmBlock {
	uint8_t* buff;
	size_t size;
	int pts;
	int duration;
};

inline std::shared_ptr<AVPacket> make_packet() {
	return std::shared_ptr<AVPacket>(av_packet_alloc(), [](AVPacket* p) {
		if (p) av_packet_free(&p);
		});
}

inline std::shared_ptr<AVFrame> make_frame() {
	return std::shared_ptr<AVFrame>(av_frame_alloc(), [](AVFrame* f) {
		if (f) av_frame_free(&f);
		});
}

inline std::shared_ptr<PcmBlock> make_pcm_block() {
	return std::shared_ptr<PcmBlock>(new PcmBlock(), [](PcmBlock* p) {
		if (p) {
			av_free(p->buff);
			p->buff = nullptr;
		}
		delete p;
	});
}


// 类型别名
using AVPacketPtr = std::shared_ptr<AVPacket>;
using AVFramePtr = std::shared_ptr<AVFrame>;
using PcmBlockPtr = std::shared_ptr<PcmBlock>;