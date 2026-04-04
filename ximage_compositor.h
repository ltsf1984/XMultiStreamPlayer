// ximage_compositor.h
#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <d3d11.h>
#include <wrl/client.h>

struct AVFrame;

// 单元格信息
struct CellInfo {
    ID3D11Texture2D* texture = nullptr;
    int slice_index = 0;
    int src_width = 0;
    int src_height = 0;
    bool valid = false;
};

// 布局配置
struct LayoutConfig {
    int rows = 4;
    int cols = 8;
    int target_width = 800;
    int target_height = 600;
};

// 图片合成器（零拷贝版本）：只管理映射关系
class XImageCompositor
{
public:
    XImageCompositor();
    explicit XImageCompositor(const LayoutConfig& config);
    ~XImageCompositor();

    // 初始化
    bool Initialize(const LayoutConfig& config);

    // 设置 D3D11 设备
    void SetD3DDevice(ID3D11Device* device);

    // 更新单元格的纹理映射
    void UpdateCell(int row, int col, AVFrame* frame);
    void UpdateCell(int index, AVFrame* frame);

    // 获取单元格信息（供渲染器使用）
    const CellInfo& GetCellInfo(int row, int col) const;
    const CellInfo& GetCellInfo(int index) const;

    // 获取所有单元格信息
    const std::vector<CellInfo>& GetAllCells() const { return cells_; }

    // 获取布局配置
    const LayoutConfig& GetLayout() const { return layout_; }

    // 动态修改布局
    void SetLayout(int rows, int cols);

    // 清空所有映射
    void Clear();

private:
    void UpdateCellInternal(int index, AVFrame* frame);
    bool ExtractTextureFromFrame(AVFrame* frame, ID3D11Texture2D*& texture, int& slice_index);

private:
    LayoutConfig layout_;
    Microsoft::WRL::ComPtr<ID3D11Device> d3d_device_;

    // 单元格数组（32个）
    std::vector<CellInfo> cells_;

    // 统计
    size_t total_updates_ = 0;
};
