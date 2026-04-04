// d3d11_hardware_context.h
#pragma once
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <array>
#include <memory>
#include <unordered_map>

// 布局信息常量缓冲区（16字节对齐）
struct TilingConstants {
    float cols;          // 列数 (8)
    float rows;          // 行数 (4)
    float targetWidth;   // 目标宽度
    float targetHeight;  // 目标高度
    int validMask[8];    // 32位的有效性掩码，用8个int存储（每个int存4位，共32位）
    float padding[4];    // 对齐到16字节
};

class D3D11HardwareContext
{
public:
    struct Vertex {
        float x, y, z, w;
        float u, v;
    };

    static constexpr int MAX_TEXTURES = 32;

    explicit D3D11HardwareContext(HWND hwnd);
    ~D3D11HardwareContext() = default;

    // 初始化
    bool InitD3D11(HWND hwnd);
    bool InitD3D11Default(HWND hwnd);
    bool CreateDXGIFactoryWithFallback();
    bool InitShaders();
    void CreateSampler();
    void CreateFullScreenQuad();
    void CreateConstantBuffer();

    // 多纹理渲染（方案3核心 - 零拷贝）
    bool RenderTiled(
        const std::array<ID3D11Texture2D*, MAX_TEXTURES>& textures_y,
        const std::array<ID3D11Texture2D*, MAX_TEXTURES>& textures_uv,
        const std::array<int, MAX_TEXTURES>& slice_indices,
        int rows, int cols);

    // 兼容旧接口
    bool RenderFrame(ID3D11Texture2D* frame_tex, int slice_index = 0);

    // 属性
    ID3D11Device* GetDevice() const { return d3d11_device_.Get(); }
    ID3D11DeviceContext* GetContext() const { return d3d11_context_.Get(); }

private:
    // 获取或创建 SRV
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetOrCreateSRV(
        ID3D11Texture2D* texture, int slice_index, DXGI_FORMAT format);

    // 初始化固定管线状态
    void SetupFixedPipelineState();

    // 检查并更新渲染资源
    bool CheckAndUpdateResources();

    // 重新创建渲染目标视图
    bool RecreateRenderTargetView();

    // D3D11 核心对象
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_context_;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain_;
    Microsoft::WRL::ComPtr<IDXGIFactory> dxgiFactory_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> render_target_view_;

    // 着色器
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertex_shader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixel_shader_;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> input_layout_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler_;

    // 常量缓冲区
    Microsoft::WRL::ComPtr<ID3D11Buffer> constant_buffer_;

    // 渲染资源
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertex_buffer_;

    // 窗口信息
    HWND hwnd_ = nullptr;
    UINT cached_width_ = 0;
    UINT cached_height_ = 0;

    // SRV 缓存
    struct SRVCacheKey {
        ID3D11Texture2D* texture;
        int slice_index;
        DXGI_FORMAT format;

        bool operator==(const SRVCacheKey& other) const {
            return texture == other.texture &&
                slice_index == other.slice_index &&
                format == other.format;
        }
    };

    struct SRVCacheHash {
        size_t operator()(const SRVCacheKey& key) const {
            size_t h1 = reinterpret_cast<size_t>(key.texture);
            size_t h2 = static_cast<size_t>(key.slice_index) << 4;
            size_t h3 = static_cast<size_t>(key.format) << 8;
            return h1 ^ h2 ^ h3;
        }
    };

    std::unordered_map<SRVCacheKey, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>, SRVCacheHash> srv_cache_;
};
