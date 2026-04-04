// ximage_compositor.cpp
#include "ximage_compositor.h"
#include <iostream>
#include <algorithm>

extern "C" {
#include <libavcodec/codec.h>
#include <libavutil/pixfmt.h>
}

XImageCompositor::XImageCompositor()
{
    cells_.resize(32);
}

XImageCompositor::XImageCompositor(const LayoutConfig& config)
{
    cells_.resize(32);
    Initialize(config);
}

XImageCompositor::~XImageCompositor()
{
    Clear();
}

bool XImageCompositor::Initialize(const LayoutConfig& config)
{
    layout_ = config;

    // 计算单元格尺寸
    int cell_width = layout_.target_width / layout_.cols;
    int cell_height = layout_.target_height / layout_.rows;

    std::cout << "XImageCompositor (zero-copy) initialized: "
        << layout_.rows << "x" << layout_.cols
        << ", cell size: " << cell_width << "x" << cell_height
        << ", target: " << layout_.target_width << "x" << layout_.target_height
        << std::endl;

    Clear();
    return true;
}

void XImageCompositor::SetD3DDevice(ID3D11Device* device)
{
    d3d_device_ = device;
}

bool XImageCompositor::ExtractTextureFromFrame(AVFrame* frame, ID3D11Texture2D*& texture, int& slice_index)
{
    if (!frame) {
        return false;
    }

    if (frame->format == AV_PIX_FMT_D3D11 && frame->data[0]) {
        texture = (ID3D11Texture2D*)frame->data[0];
        slice_index = static_cast<int>(reinterpret_cast<uintptr_t>(frame->data[1]));
        return true;
    }

    static bool warned = false;
    if (!warned) {
        std::cerr << "Warning: Non-hardware frame not supported" << std::endl;
        warned = true;
    }
    return false;
}

void XImageCompositor::UpdateCellInternal(int index, AVFrame* frame)
{
    if (index < 0 || index >= (int)cells_.size()) {
        return;
    }

    if (!frame) {
        cells_[index].valid = false;
        cells_[index].texture = nullptr;
        return;
    }

    ID3D11Texture2D* texture = nullptr;
    int slice_index = 0;

    if (ExtractTextureFromFrame(frame, texture, slice_index)) {
        cells_[index].texture = texture;
        cells_[index].slice_index = slice_index;

        // 获取纹理尺寸
        D3D11_TEXTURE2D_DESC desc;
        texture->GetDesc(&desc);
        cells_[index].src_width = desc.Width;
        cells_[index].src_height = desc.Height;
        cells_[index].valid = true;

        total_updates_++;
    }
    else {
        cells_[index].valid = false;
        cells_[index].texture = nullptr;
    }
}

void XImageCompositor::UpdateCell(int row, int col, AVFrame* frame)
{
    int index = row * layout_.cols + col;
    UpdateCellInternal(index, frame);
}

void XImageCompositor::UpdateCell(int index, AVFrame* frame)
{
    UpdateCellInternal(index, frame);
}

const CellInfo& XImageCompositor::GetCellInfo(int row, int col) const
{
    int index = row * layout_.cols + col;
    return cells_[index];
}

const CellInfo& XImageCompositor::GetCellInfo(int index) const
{
    return cells_[index];
}

void XImageCompositor::SetLayout(int rows, int cols)
{
    layout_.rows = rows;
    layout_.cols = cols;

    // 重新计算单元格尺寸
    int cell_width = layout_.target_width / layout_.cols;
    int cell_height = layout_.target_height / layout_.rows;

    std::cout << "Layout changed to: " << rows << "x" << cols
        << ", cell size: " << cell_width << "x" << cell_height << std::endl;
}

void XImageCompositor::Clear()
{
    for (auto& cell : cells_) {
        cell.texture = nullptr;
        cell.valid = false;
    }
}
