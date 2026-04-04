// xcodec.cpp
#include "xcodec.h"
#include <iostream>
#include <fstream>
#include "d3d11_hardware_context.h"
#include <d3d11.h>
#include <dxgi.h>
#include <QDebug>
#include <QString>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_d3d11va.h>
}

#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "dxgi.lib")

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4996)
#endif

XCodec::XCodec()
{
}

XCodec::~XCodec()
{
    Close();
}

bool XCodec::Create(AVCodecID codec_id, bool is_encoder)
{
    is_encoder_ = is_encoder;
    const AVCodec* codec = nullptr;
    if (is_encoder)
    {
        codec = avcodec_find_encoder(codec_id);
    }
    else
    {
        codec = avcodec_find_decoder(codec_id);
    }
    if (!codec) {
        std::cerr << "Failed to find codec: " << codec_id << std::endl;
        return false;
    }

    context_ = avcodec_alloc_context3(codec);
    if (!context_) {
        std::cerr << "Failed to allocate codec context" << std::endl;
        return false;
    }

    //std::cout << "Codec created: " << avcodec_get_name(codec_id) << std::endl;
    return true;
}

bool XCodec::SetCodecHw(std::shared_ptr< D3D11HardwareContext> d3d_ctx)
{
    if (!d3d_ctx) return false;

    ID3D11Device* d3d11_device = d3d_ctx->GetDevice();
    if (!d3d11_device) {
        std::cerr << "Invalid D3D11 device" << std::endl;
        return false;
    }

    // 创建硬件设备上下文
    AVBufferRef* hw_device_ctx_buf = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
    if (!hw_device_ctx_buf)
    {
        std::cerr << "Failed to allocate hw device context" << std::endl;
        return false;
    }

    AVHWDeviceContext* hw_device_ctx = (AVHWDeviceContext*)hw_device_ctx_buf->data;
    AVD3D11VADeviceContext* d3d11_dev_ctx = (AVD3D11VADeviceContext*)hw_device_ctx->hwctx;
    d3d11_dev_ctx->device = d3d11_device;
    d3d11_dev_ctx->device_context = d3d_ctx->GetContext();

    // 增加引用计数
    d3d11_dev_ctx->device->AddRef();
    if (d3d11_dev_ctx->device_context) {
        d3d11_dev_ctx->device_context->AddRef();
    }

    int ret = av_hwdevice_ctx_init(hw_device_ctx_buf);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, sizeof(errbuf));
        std::cerr << "av_hwdevice_ctx_init failed: " << errbuf << std::endl;
        av_buffer_unref(&hw_device_ctx_buf);
        return false;
    }

    context_->hw_device_ctx = hw_device_ctx_buf;
    context_->get_format = get_hw_format_static;
    context_->opaque = this;

    // 设置解码器选项以提高兼容性
    av_opt_set_int(context_->priv_data, "threads", 1, 0);           // 单线程解码，避免驱动问题
    av_opt_set_int(context_->priv_data, "refcounted_frames", 1, 0); // 引用计数帧

    //std::cout << "Hardware decoder configured successfully" << std::endl;
    return true;
}

enum AVPixelFormat XCodec::get_hw_format_static(
    AVCodecContext* ctx,
    const AVPixelFormat* pix_fmts)
{
    for (const enum AVPixelFormat* p = pix_fmts; *p != AV_PIX_FMT_NONE; p++) {
        if (*p == AV_PIX_FMT_D3D11) {
            //std::cout << "Selected format: D3D11" << std::endl;

            if (!ctx->hw_frames_ctx) {
                AVBufferRef* frames_ref = av_hwframe_ctx_alloc(ctx->hw_device_ctx);
                if (!frames_ref) {
                    std::cerr << "Failed to allocate hw frames context" << std::endl;
                    return AV_PIX_FMT_NONE;
                }

                AVHWFramesContext* frames_ctx = (AVHWFramesContext*)frames_ref->data;
                int md = 16 - 1;
                int align_width = (ctx->width + md) & ~md;
                int align_height = (ctx->height + md) & ~md;

                // 使用原始尺寸，让 FFmpeg 处理对齐
                frames_ctx->format = AV_PIX_FMT_D3D11;
                frames_ctx->sw_format = AV_PIX_FMT_NV12;
                frames_ctx->width = ctx->width;
                frames_ctx->height = ctx->height;
                frames_ctx->initial_pool_size = 20;

                AVD3D11VAFramesContext* d3d11_frames_ctx = (AVD3D11VAFramesContext*)frames_ctx->hwctx;
                d3d11_frames_ctx->BindFlags = D3D11_BIND_DECODER | D3D11_BIND_SHADER_RESOURCE;
                d3d11_frames_ctx->MiscFlags = D3D11_RESOURCE_MISC_SHARED;

                int ret = av_hwframe_ctx_init(frames_ref);
                if (ret < 0) {
                    char errbuf[AV_ERROR_MAX_STRING_SIZE];
                    av_strerror(ret, errbuf, sizeof(errbuf));
                    std::cerr << "Failed to init hw frames context: " << errbuf << std::endl;
                    av_buffer_unref(&frames_ref);
                    return AV_PIX_FMT_NONE;
                }

                ctx->hw_frames_ctx = frames_ref;
                //std::cout << "HW frames context created successfully" << std::endl;

                // 获取实际分配的对齐尺寸
                AVHWFramesContext* actual_ctx = (AVHWFramesContext*)ctx->hw_frames_ctx->data;
                //std::cout << "  Actual HW frames dimensions: " << actual_ctx->width << "x" << actual_ctx->height << std::endl;
            }
            return AV_PIX_FMT_D3D11;
        }
    }

    //std::cout << "D3D11 format not supported, falling back to software: "
    //    << av_get_pix_fmt_name(pix_fmts[0]) << std::endl;
    return pix_fmts[0];
}

bool XCodec::Open()
{
    std::unique_lock<std::mutex> lock(mtx_);
    if (!context_) return false;

    if (is_encoder_)
    {
        if (context_->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            if (context_->width <= 0 || context_->height <= 0) {
                std::cerr << "Invalid video width/height!" << std::endl;
                return false;
            }
            if (context_->pix_fmt == AV_PIX_FMT_NONE) {
                std::cerr << "Invalid video pixel format!" << std::endl;
                return false;
            }
        }
        else if (context_->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            if (context_->sample_rate <= 0)
            {
                std::cerr << "Invalid audio sample rate!" << std::endl;
                return false;
            }
            if (context_->ch_layout.nb_channels <= 0)
            {
                std::cerr << "Invalid audio channel layout!" << std::endl;
                return false;
            }
            if (context_->sample_fmt <= AV_SAMPLE_FMT_NONE)
            {
                std::cerr << "Invalid audio sample format!" << std::endl;
                return false;
            }
        }
        else if (context_->codec_type == AVMEDIA_TYPE_UNKNOWN)
        {
            std::cerr << "Unknown codec type! Make sure codec is properly set." << std::endl;
            return false;
        }
    }

    int ret = avcodec_open2(context_, nullptr, nullptr);
    if (ret < 0) {
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        std::cerr << "avcodec_open2 failed: " << errbuf << std::endl;
        return false;
    }

    //std::cout << "Codec opened successfully" << std::endl;
    return true;
}

bool XCodec::Close()
{
    if (!context_) return false;
    avcodec_free_context(&context_);
    context_ = nullptr;
    return true;
}

bool XCodec::SetVideoParam(int width, int height, AVPixelFormat pix_fmt)
{
    std::lock_guard<std::mutex> lock(mtx_);
    if (!context_) return false;
    if (width <= 0 || height <= 0) return false;
    if (pix_fmt == AV_PIX_FMT_NONE) return false;

    if (!context_->codec) {
        std::cerr << "Error: video params should be set before Open()!" << std::endl;
    }

    context_->width = width;
    context_->height = height;
    context_->pix_fmt = pix_fmt;
    return true;
}

bool XCodec::SetTimeBase(int num, int den)
{
    std::lock_guard<std::mutex> lock(mtx_);
    if (num <= 0 || den <= 0) {
        std::cerr << "Invalid time base: " << num << "/" << den << std::endl;
        return false;
    }

    if (!context_) {
        std::cerr << "Context not created yet!" << std::endl;
        return false;
    }

    if (!context_->codec) {
        std::cerr << "Error: time_base should be set before Open()!" << std::endl;
    }

    context_->time_base = { num, den };
    return true;
}

bool XCodec::SetTimeBase(AVRational time_base)
{
    return SetTimeBase(time_base.num, time_base.den);
}

bool XCodec::SetFrameRate(int num, int den)
{
    std::lock_guard<std::mutex> lock(mtx_);
    if (!context_) return false;
    if (num <= 0 || den <= 0) return false;
    context_->framerate = { num, den };
    return true;
}

bool XCodec::SetFrameRate(AVRational framerate)
{
    return SetFrameRate(framerate.num, framerate.den);
}

bool XCodec::SetBitRate(int64_t bit_rate) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!context_) return false;
    if (bit_rate <= 0) return false;
    context_->bit_rate = bit_rate;
    return true;
}

bool XCodec::SetGopSize(int gop_size) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!context_) return false;
    context_->gop_size = gop_size;
    return true;
}

bool XCodec::SetOpt(const std::string& key, const std::string& value)
{
    std::lock_guard<std::mutex> lock(mtx_);
    int ret = av_opt_set(context_->priv_data, key.c_str(), value.c_str(), 0);
    if (ret == 0) return true;
    return false;
}

bool XCodec::SetOpt(const std::string& key, int value)
{
    std::lock_guard<std::mutex> lock(mtx_);
    int ret = av_opt_set_int(context_->priv_data, key.c_str(), value, 0);
    if (ret == 0) return true;
    return false;
}

AVFrame* XCodec::CreateFrame()
{
    std::lock_guard<std::mutex> lock(mtx_);
    AVFrame* frame = av_frame_alloc();
    if (!frame) return nullptr;

    frame->width = context_->width;
    frame->height = context_->height;
    frame->format = context_->pix_fmt;

    int ret = av_frame_get_buffer(frame, 32);
    if (ret < 0)
    {
        av_frame_free(&frame);
        std::cout << "av_frame_get_buffer failed!" << std::endl;
        return nullptr;
    }
    return frame;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
