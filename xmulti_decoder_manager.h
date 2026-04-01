// xmulti_decoder_manager.h
#pragma once

#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include "xdecoder.h"
#include "d3d11_hardware_context.h"
#include "av_utils.h"

// 解码器配置
struct DecoderConfig {
    size_t decoder_count = 32;           // 解码器数量
    bool use_hardware = true;            // 是否使用硬件解码
    bool enable_auto_attach = true;      // 是否自动附加硬件帧上下文
    int max_retry_count = 3;             // 最大重试次数
    std::shared_ptr<D3D11HardwareContext> d3d_ctx;  // D3D11上下文
};

// 解码器状态
enum class DecoderState {
    Idle,           // 空闲
    Running,        // 运行中
    Paused,         // 暂停
    Stopped,        // 已停止
    Error           // 错误状态
};

// 解码器统计信息
struct DecoderStats {
    size_t source_id;
    DecoderState state;
    size_t packets_received;    // 接收的packet数
    size_t packets_decoded;     // 解码的packet数
    size_t frames_output;       // 输出的frame数
    size_t queue_size;          // 当前输入队列大小
    int64_t last_pts;           // 最后解码的PTS
    std::string error_msg;
};

// 解码器回调函数集合
struct DecoderCallbacks {
    // packet 出队回调（解码器需要 packet 时调用）
    // 返回值：true 表示成功获取 packet，false 表示无数据或结束
    std::function<bool(size_t source_id, AVPacketPtr& packet)> on_packet_dequeue;

    // frame 入队回调（解码器产出 frame 时调用）
    std::function<void(size_t source_id, AVFramePtr frame)> on_frame_enqueue;

    // 错误回调
    std::function<void(size_t source_id, const std::string& error)> on_error;
};

// 多解码器系统
class XMultiDecoderManager {
public:
    XMultiDecoderManager();
    explicit XMultiDecoderManager(const DecoderConfig& config);
    ~XMultiDecoderManager();

    // ========== 初始化 ==========
    void SetConfig(const DecoderConfig& config);
    bool Initialize();

    // ========== 回调设置 ==========
    void SetCallbacks(const DecoderCallbacks& callbacks);

    // ========== 解码器管理 ==========
    bool InitDecoder(size_t source_id, AVCodecParameters* codecpar);
    void InitAllDecoders(AVCodecParameters* codecpar);

    // ========== 启动/停止 ==========
    bool StartDecoder(size_t source_id);
    void StartAll();
    void StopDecoder(size_t source_id, bool wait = true);
    void StopAll();

    // ========== 暂停/恢复 ==========
    void PauseDecoder(size_t source_id);
    void PauseAll();
    void ResumeDecoder(size_t source_id);
    void ResumeAll();

    // ========== 查询接口 ==========
    DecoderState GetState(size_t source_id) const;
    bool IsRunning(size_t source_id) const;
    bool IsAllRunning() const;
    DecoderStats GetStats(size_t source_id) const;
    std::vector<DecoderStats> GetAllStats() const;
    void PrintStats() const;

    // ========== 动态调整 ==========
    bool AddDecoder();
    void SetD3DContext(std::shared_ptr<D3D11HardwareContext> d3d_ctx);

private:
    // ========== 线程函数 ==========
    void DecoderThreadFunc(size_t source_id);
    void DrainDecoder(size_t source_id);
    void SetError(size_t source_id, const std::string& error);
    static std::string StateToString(DecoderState state);

private:
    DecoderConfig config_;

    // 解码器数组
    std::vector<std::unique_ptr<XDecoder>> decoders_;
    std::vector<std::unique_ptr<std::thread>> decoder_threads_;

    // 使用 unique_ptr 包装 atomic（因为 atomic 不可拷贝）
    std::vector<std::unique_ptr<std::atomic<DecoderState>>> states_;
    std::vector<std::unique_ptr<std::atomic<size_t>>> packets_received_;
    std::vector<std::unique_ptr<std::atomic<size_t>>> packets_decoded_;
    std::vector<std::unique_ptr<std::atomic<size_t>>> frames_output_;
    std::vector<std::unique_ptr<std::atomic<int64_t>>> last_pts_;
    std::vector<std::string> error_msgs_;

    // 回调函数
    DecoderCallbacks callbacks_;

    // 同步
    std::mutex pause_mtx_;
    std::condition_variable pause_cv_;
};
