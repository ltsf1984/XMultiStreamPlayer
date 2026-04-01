#pragma once
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <unordered_map>

class D3D11HardwareContext
{
public:
    struct Vertex {
        float x, y, z, w;
        float u, v;
    };

    explicit D3D11HardwareContext(HWND hwnd);
    ~D3D11HardwareContext() = default;

    // 初始化 D3D11 设备
    bool InitD3D11(HWND hwnd);
    bool InitD3D11Default(HWND hwnd);
    bool CreateDXGIFactoryWithFallback();
    bool InitShaders();
    void CreateSampler();
    void CreateFullScreenQuad();

    // 渲染
    bool RenderFrame(ID3D11Texture2D* frame_tex, int slice_index = 0);
    bool CreateOrReuseSRV(ID3D11Texture2D* texture, int slice_index = 0);
    void ClearSRVCache();

    // 属性
    ID3D11Device* GetDevice() const { return d3d11_device_.Get(); }
    ID3D11DeviceContext* GetContext() const { return d3d11_context_.Get(); }

private:
    // 用于缓存 SRV 的键
    struct SRVKey {
        ID3D11Texture2D* texture;
        int slice_index;

        bool operator==(const SRVKey& other) const {
            return texture == other.texture && slice_index == other.slice_index;
        }
    };

    // 哈希函数
    struct SRVKeyHash {
        size_t operator()(const SRVKey& key) const {
            return reinterpret_cast<size_t>(key.texture) ^ (key.slice_index << 4);
        }
    };

    // SRV 缓存：纹理+切片索引 -> SRV 对
    struct SRVPair {
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> y;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> uv;
    };
    std::unordered_map<SRVKey, SRVPair, SRVKeyHash> srv_cache_;

    // 初始化固定管线状态
    void SetupFixedPipelineState();

    // 检查并更新渲染资源（自动处理窗口大小变化）
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

    // 渲染资源
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertex_buffer_;

    // 当前使用的 SRV（从缓存中取出）
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> current_srv_y_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> current_srv_uv_;

    // 窗口信息
    HWND hwnd_ = nullptr;
    UINT cached_width_ = 0;
    UINT cached_height_ = 0;
};
