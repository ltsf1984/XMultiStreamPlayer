// xcodec.h
#pragma once
#include <iostream>
#include <mutex>
#include <string>

extern "C" {
#include <libavcodec/codec_id.h>
#include <libavutil/pixfmt.h>
#include <libavutil/rational.h>
}

struct AVCodecContext;
struct AVFrame;
struct AVCodecParameters;
class D3D11HardwareContext;

class XCodec
{
public:
    enum class SendResult {
        Success,     // 成功发送一个输入单元（packet 或 frame）
        NeedDrain,   // 输出缓冲区已满，需要先接收数据，排空输出缓冲区
        Ended,       // 编解码器已处于结束状态（AVERROR_EOF）
        Failed       // 永久性错误
    };

    enum class ReceiveResult {
        Success,     // 成功接收一个输出单元（packet 或 frame）
        NeedFeed,    // 输入缓冲区已空，需要先发送数据
        Ended,       // 编解码流程已正常结束（AVERROR_EOF）
        Failed       // 永久性错误
    };

public:
    XCodec();
    ~XCodec();

    // 创建上下文(true为编码，false为解码)
    bool Create(AVCodecID codec_id, bool is_encoder = true);
    // 使用硬件（GPU）编解码
    bool SetCodecHw(std::shared_ptr< D3D11HardwareContext> d3d_ctx);
    // 列出所有可用显卡
    void ListAvailableGPUs();

    // 获取硬解码格式回调
    static enum AVPixelFormat get_hw_format_static(
        AVCodecContext* ctx,
        const enum AVPixelFormat* pix_fmts);

    // 打开上下文
    bool Open();
    bool Close();

    // 获取上下文
    AVCodecContext* GetContext() {
        std::lock_guard<std::mutex> lock(mtx_);
        return context_;
    }

    // 参数设置（必须在 Open() 前调用）
    bool SetVideoParam(int width, int height, AVPixelFormat pix_fmt);
    bool SetTimeBase(int num, int den);
    bool SetTimeBase(AVRational time_base);
    bool SetFrameRate(int num, int den);
    bool SetFrameRate(AVRational framerate);
    bool SetBitRate(int64_t bit_rate);
    bool SetGopSize(int gop_size);
    bool SetOpt(const std::string& key, const std::string& value);
    bool SetOpt(const std::string& key, int value);

    // 创建帧frame
    AVFrame* CreateFrame();

protected:
    AVCodecContext* context_{ nullptr };
    std::mutex mtx_;
    bool is_encoder_{ true };
};
