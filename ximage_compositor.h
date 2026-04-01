// ximage_compositor.h
#pragma once

#include <memory>
#include <vector>
#include <cstdint>
#include <d3d11.h>
#include <wrl/client.h>

struct AVFrame;

// 图片合成器配置
struct XCompositorConfig {
    int rows = 4;               // 行数
    int cols = 8;               // 列数
    int cell_width = 0;         // 单个单元格宽度（0表示自动计算）
    int cell_height = 0;        // 单个单元格高度（0表示自动计算）
    int target_width = 800;    // 目标合成图宽度
    int target_height = 600;   // 目标合成图高度

    // 红色占位符颜色（RGBA）
    uint8_t placeholder_r = 255;
    uint8_t placeholder_g = 0;
    uint8_t placeholder_b = 0;
    uint8_t placeholder_a = 255;
};

// 图片合成器：将多个 AVFrame 合成为一张大图
class XImageCompositor
{
public:
    XImageCompositor();
    explicit XImageCompositor(const XCompositorConfig& config);
    ~XImageCompositor();

    // 初始化合成器
    bool Initialize(const XCompositorConfig& config);

    // 设置 D3D11 设备（用于硬件帧处理）
    void SetD3DDevice(ID3D11Device* device);

    // 合成图片（核心接口）
    // frames: 帧数组（nullptr表示无数据），内部自动提取纹理和切片索引
    // 返回合成后的纹理（生命周期由 XImageCompositor 管理）
    ID3D11Texture2D* Composite(const std::vector<AVFrame*>& frames);

    // 获取合成后的纹理
    ID3D11Texture2D* GetCompositeTexture() const { return composite_texture_.Get(); }

    // 更新配置
    void UpdateConfig(const XCompositorConfig& config);

    // 获取单元格尺寸
    int GetCellWidth() const { return cell_width_; }
    int GetCellHeight() const { return cell_height_; }

    // 获取统计信息
    size_t GetTotalFramesComposited() const { return total_frames_composited_; }

private:
    // 从 AVFrame 提取纹理和切片索引
    bool ExtractTextureFromFrame(AVFrame* frame, ID3D11Texture2D*& texture, int& slice_index);

    // 创建合成纹理
    bool CreateCompositeTexture();

    // 拷贝纹理到指定单元格（带缩放）
    bool CopyTextureToCell(ID3D11Texture2D* src_texture, int slice_index, int row, int col);

    // 填充红色占位符到单元格
    void FillPlaceholderToCell(int row, int col);

    // 计算目标区域
    void GetCellRect(int row, int col, D3D11_BOX& box) const;

    // 使用 Staging 纹理进行缩放拷贝
    bool CopyTextureWithScaling(ID3D11Texture2D* src_texture, int slice_index,
        int src_width, int src_height,
        int dst_x, int dst_y, int dst_width, int dst_height);

private:
    XCompositorConfig config_;
    Microsoft::WRL::ComPtr<ID3D11Device> d3d_device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d_context_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> composite_texture_;

    int cell_width_ = 0;
    int cell_height_ = 0;

    // 统计
    size_t total_frames_composited_ = 0;
};
