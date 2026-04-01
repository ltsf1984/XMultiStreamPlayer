//xdemuxer.cpp
#include "xdemuxer.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

#pragma comment(lib, "avformat.lib")   // 格式处理
#pragma comment(lib, "avcodec.lib")    // 编解码

bool XDemuxer::Open(std::string file)
{
	// 防止重复打开
	if (fmt_ctx_) {
		avformat_close_input(&fmt_ctx_);
		video_index_ = -1;
		audio_index_ = -1;
	}

	// 1、打开文件
	// 指向AVFormatContext指针的指针，函数会分配并填充上下文
	if (avformat_open_input(&fmt_ctx_, file.c_str(), nullptr, nullptr) < 0)
	{
		std::cerr << "Error: Cannot open input file '" << file << "'" << std::endl;
		return false;
	}

	// 2、获取媒体文件的完整流信息
	// 从媒体文件中读取并解析实际的数据，然后将获取到的详细信息设置到 input_fmt_ctx_ 的结构体成员中。
	if (avformat_find_stream_info(fmt_ctx_, nullptr) < 0)
	{
		std::cerr << "Error: Cannot find stream info in '" << fmt_ctx_ << "'" << std::endl;
		return false;
	}

	//3. 查找视频流（安全！）
	video_index_ = av_find_best_stream(fmt_ctx_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
	if (video_index_ < 0) {
		std::cerr << "Error: No video stream found in raw file!" << std::endl;
		return false;
	}

	//4. 查找音频流（可能不存在，但不影响转码）
	audio_index_ = av_find_best_stream(fmt_ctx_, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
	if (audio_index_ < 0) {
		std::cerr << "Error: No audio stream found in raw file!" << std::endl;
		//return false;
	}

    return true;
}

bool XDemuxer::CopyPara(int stream_index, AVCodecContext* dec_ctx)
{
	if (!dec_ctx) return false;

	// 复制参数到编解码器上下文
	std::lock_guard<std::mutex> lock(mtx_);
	if (!fmt_ctx_ || stream_index < 0 || stream_index >= (int)fmt_ctx_->nb_streams) {
		return false;
	}

	if (avcodec_parameters_to_context(dec_ctx, fmt_ctx_->streams[stream_index]->codecpar) < 0) {
		std::cerr << "Error: Failed to copy decoder parameters" << std::endl;
		return false;
	}
	return true;
}

AVCodecParameters* XDemuxer::GetVideoCodecpar()
{
	std::lock_guard<std::mutex> lock(mtx_);
	if (!fmt_ctx_ || video_index_ < 0) return nullptr;
	return fmt_ctx_->streams[video_index_]->codecpar;
}

AVCodecParameters* XDemuxer::GetAudioCodecpar()
{
	std::lock_guard<std::mutex> lock(mtx_);
	if (!fmt_ctx_ || audio_index_ < 0) return nullptr;
	return fmt_ctx_->streams[audio_index_]->codecpar;
}

AVRational XDemuxer::GetVideoStreamTimebase()
{
	std::lock_guard<std::mutex> lock(mtx_);
	if (!fmt_ctx_ || video_index_ < 0) return { 0, 1 };
	return fmt_ctx_->streams[video_index_]->time_base;
}

AVRational XDemuxer::GetAudioStreamTimebase()
{
	std::lock_guard<std::mutex> lock(mtx_);
	if (!fmt_ctx_ || audio_index_ < 0) return { 0, 1 };
	return fmt_ctx_->streams[audio_index_]->time_base;
}

bool XDemuxer::Read(AVPacket* pkt)
{
	if (!pkt) return false;

	std::lock_guard<std::mutex> lock(mtx_);
	if (!fmt_ctx_) return false; 
	int ret = av_read_frame(fmt_ctx_, pkt);
	if (ret < 0) {
		if (ret == AVERROR_EOF) {
			std::cout << "Demuxer: EOF reached." << std::endl;
		}
		else {
			char errbuf[256] = { 0 };
			av_strerror(ret, errbuf, sizeof(errbuf));
			std::cerr << "av_read_frame error: " << errbuf << std::endl;
		}
		return false;
	}
	return true;
}

bool XDemuxer::Close()
{
	if (!fmt_ctx_) return false;
	avformat_close_input(&fmt_ctx_);
	fmt_ctx_ = nullptr;
	return true;
}
