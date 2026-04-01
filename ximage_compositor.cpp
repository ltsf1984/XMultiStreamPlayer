// ximage_compositor.cpp
#include "ximage_compositor.h"
#include <iostream>
#include <cstring>
#include <algorithm>

extern "C" {
#include <libavcodec/codec.h>
#include <libavutil/pixfmt.h>
}

XImageCompositor::XImageCompositor()
{
}

XImageCompositor::XImageCompositor(const XCompositorConfig& config)
{
    Initialize(config);
}

XImageCompositor::~XImageCompositor()
{
    composite_texture_.Reset();
}

bool XImageCompositor::Initialize(const XCompositorConfig& config)
{
    config_ = config;

    // 计算单元格尺寸
    if (config_.cell_width > 0 && config_.cell_height > 0) {
        cell_width_ = config_.cell_width;
        cell_height_ = config_.cell_height;
    }
    else {
        cell_width_ = config_.target_width / config_.cols;
        cell_height_ = config_.target_height / config_.rows;
    }

    std::cout << "XImageCompositor initialized: "
        << config_.rows << "x" << config_.cols
        << ", cell size: " << cell_width_ << "x" << cell_height_
        << ", target: " << config_.target_width << "x" << config_.target_height
        << std::endl;

    return true;
}

void XImageCompositor::SetD3DDevice(ID3D11Device* device)
{
    d3d_device_ = device;
    if (d3d_device_) {
        d3d_device_->GetImmediateContext(&d3d_context_);
    }
}

bool XImageCompositor::ExtractTextureFromFrame(AVFrame* frame, ID3D11Texture2D*& texture, int& slice_index)
{
    if (!frame) {
        return false;
    }

    // 检查是否是硬件帧
    if (frame->format == AV_PIX_FMT_D3D11 && frame->data[0]) {
        // data[0] 是纹理指针
        texture = (ID3D11Texture2D*)frame->data[0];

        // data[1] 是切片索引（存储在指针中）
        slice_index = static_cast<int>(reinterpret_cast<uintptr_t>(frame->data[1]));

        return true;
    }

    // 非硬件帧，暂不支持
    static bool warned = false;
    if (!warned) {
        std::cerr << "Warning: Non-hardware frame not supported in XImageCompositor" << std::endl;
        warned = true;
    }

    return false;
}

bool XImageCompositor::CreateCompositeTexture()
{
    if (!d3d_device_) {
        std::cerr << "D3D11 device not set" << std::endl;
        return false;
    }

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = config_.target_width;
    desc.Height = config_.target_height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

    HRESULT hr = d3d_device_->CreateTexture2D(&desc, nullptr, &composite_texture_);
    if (FAILED(hr)) {
        std::cerr << "Failed to create composite texture, HRESULT: 0x" << std::hex << hr << std::dec << std::endl;
        return false;
    }

    std::cout << "Composite texture created: " << config_.target_width << "x" << config_.target_height << std::endl;
    return true;
}

bool XImageCompositor::CopyTextureWithScaling(ID3D11Texture2D* src_texture, int slice_index,
    int src_width, int src_height,
    int dst_x, int dst_y, int dst_width, int dst_height)
{
    if (!src_texture || !d3d_context_ || !composite_texture_) {
        return false;
    }

    // 创建 staging 纹理用于读取源数据
    D3D11_TEXTURE2D_DESC staging_desc = {};
    src_texture->GetDesc(&staging_desc);
    staging_desc.Usage = D3D11_USAGE_STAGING;
    staging_desc.BindFlags = 0;
    staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> staging_texture;
    HRESULT hr = d3d_device_->CreateTexture2D(&staging_desc, nullptr, &staging_texture);
    if (FAILED(hr)) {
        std::cerr << "Failed to create staging texture, HRESULT: 0x" << std::hex << hr << std::dec << std::endl;
        return false;
    }

    // 拷贝源纹理到 staging
    d3d_context_->CopySubresourceRegion(staging_texture.Get(), 0, 0, 0, 0,
        src_texture, slice_index, nullptr);

    // 映射 staging 纹理读取数据
    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = d3d_context_->Map(staging_texture.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        std::cerr << "Failed to map staging texture, HRESULT: 0x" << std::hex << hr << std::dec << std::endl;
        return false;
    }

    // 计算缩放比例
    float scale_x = static_cast<float>(dst_width) / src_width;
    float scale_y = static_cast<float>(dst_height) / src_height;

    // 准备目标缓冲区（RGBA）
    std::vector<uint32_t> dst_pixels(dst_width * dst_height);

    // 最近邻缩放
    for (int y = 0; y < dst_height; ++y) {
        int src_y = static_cast<int>(y / scale_y);
        if (src_y >= src_height) src_y = src_height - 1;

        for (int x = 0; x < dst_width; ++x) {
            int src_x = static_cast<int>(x / scale_x);
            if (src_x >= src_width) src_x = src_width - 1;

            // 读取源像素（假设源格式为 BGRA）
            uint32_t* src_pixels = reinterpret_cast<uint32_t*>(static_cast<char*>(mapped.pData) + src_y * mapped.RowPitch);
            dst_pixels[y * dst_width + x] = src_pixels[src_x];
        }
    }

    d3d_context_->Unmap(staging_texture.Get(), 0);

    // 更新目标纹理
    D3D11_BOX dst_box;
    dst_box.left = dst_x;
    dst_box.top = dst_y;
    dst_box.right = dst_x + dst_width;
    dst_box.bottom = dst_y + dst_height;
    dst_box.front = 0;
    dst_box.back = 1;

    d3d_context_->UpdateSubresource(composite_texture_.Get(), 0, &dst_box,
        dst_pixels.data(), dst_width * 4, 0);

    return true;
}

bool XImageCompositor::CopyTextureToCell(ID3D11Texture2D* src_texture, int slice_index, int row, int col)
{
    if (!src_texture || !d3d_context_ || !composite_texture_) {
        return false;
    }

    // 获取源纹理描述
    D3D11_TEXTURE2D_DESC src_desc;
    src_texture->GetDesc(&src_desc);

    // 计算目标区域
    D3D11_BOX dst_box;
    GetCellRect(row, col, dst_box);

    int dst_width = dst_box.right - dst_box.left;
    int dst_height = dst_box.bottom - dst_box.top;

    // 如果源纹理尺寸与目标单元格尺寸完全匹配，直接拷贝
    if (src_desc.Width == dst_width && src_desc.Height == dst_height) {
        d3d_context_->CopySubresourceRegion(
            composite_texture_.Get(), 0,
            dst_box.left, dst_box.top, 0,
            src_texture, slice_index,
            nullptr
        );
        return true;
    }

    // 需要缩放
    return CopyTextureWithScaling(src_texture, slice_index,
        src_desc.Width, src_desc.Height,
        dst_box.left, dst_box.top, dst_width, dst_height);
}

void XImageCompositor::FillPlaceholderToCell(int row, int col)
{
    if (!d3d_context_ || !composite_texture_) {
        return;
    }

    D3D11_BOX box;
    GetCellRect(row, col, box);

    int width = box.right - box.left;
    int height = box.bottom - box.top;
    int pixel_count = width * height;

    // 创建临时缓冲区填充占位符颜色（BGRA格式）
    std::vector<uint32_t> pixels(pixel_count);
    uint32_t placeholder_color = (config_.placeholder_b << 0) |      // B
        (config_.placeholder_g << 8) |      // G
        (config_.placeholder_r << 16) |     // R
        (config_.placeholder_a << 24);      // A

    for (int i = 0; i < pixel_count; ++i) {
        pixels[i] = placeholder_color;
    }

    // 更新纹理
    d3d_context_->UpdateSubresource(composite_texture_.Get(), 0, &box,
        pixels.data(), width * 4, 0);
}

void XImageCompositor::GetCellRect(int row, int col, D3D11_BOX& box) const
{
    box.left = col * cell_width_;
    box.top = row * cell_height_;
    box.right = (col + 1) * cell_width_;
    box.bottom = (row + 1) * cell_height_;
    box.front = 0;
    box.back = 1;

    // 边界检查
    if (box.right > config_.target_width) box.right = config_.target_width;
    if (box.bottom > config_.target_height) box.bottom = config_.target_height;
}

ID3D11Texture2D* XImageCompositor::Composite(const std::vector<AVFrame*>& frames)
{
    if (!d3d_device_) {
        static bool warned = false;
        if (!warned) {
            std::cerr << "D3D11 device not set in XImageCompositor" << std::endl;
            warned = true;
        }
        return nullptr;
    }

    // 创建合成纹理（如果不存在）
    if (!composite_texture_) {
        if (!CreateCompositeTexture()) {
            return nullptr;
        }
    }

    // 合成所有帧(std::min使用括号防止宏展开)
    size_t max_frames = (std::min)(frames.size(), static_cast<size_t>(config_.rows * config_.cols));

    for (size_t i = 0; i < max_frames; ++i) {
        int row = static_cast<int>(i) / config_.cols;
        int col = static_cast<int>(i) % config_.cols;

        if (frames[i]) {
            ID3D11Texture2D* texture = nullptr;
            int slice_index = 0;

            if (ExtractTextureFromFrame(frames[i], texture, slice_index)) {
                CopyTextureToCell(texture, slice_index, row, col);
            }
            else {
                FillPlaceholderToCell(row, col);
            }
        }
        else {
            FillPlaceholderToCell(row, col);
        }
    }

    total_frames_composited_++;
    return composite_texture_.Get();
}

void XImageCompositor::UpdateConfig(const XCompositorConfig& config)
{
    config_ = config;
    composite_texture_.Reset();
    Initialize(config);
}
