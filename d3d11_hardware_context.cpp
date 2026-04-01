#include "d3d11_hardware_context.h"
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <iostream>
#include <cstdio>
#include <d3d11_1.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

static const char g_vs_code[] =
"struct VS_INPUT\n"
"{\n"
"    float4 pos : POSITION;\n"
"    float2 tex : TEXCOORD;\n"
"};\n"
"\n"
"struct VS_OUTPUT\n"
"{\n"
"    float4 pos : SV_POSITION;\n"
"    float2 uv : TEXCOORD;\n"
"};\n"
"\n"
"VS_OUTPUT main(VS_INPUT input)\n"
"{\n"
"    VS_OUTPUT output;\n"
"    output.pos = input.pos;\n"
"    output.uv = input.tex;\n"
"    return output;\n"
"}\n";

static const char g_ps_code[] =
"Texture2D texY : register(t0);\n"
"Texture2D texUV : register(t1);\n"
"SamplerState smp : register(s0);\n"
"float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD) : SV_Target\n"
"{\n"
"    float y_val = texY.Sample(smp, uv).r;\n"
"    float2 uv_val = texUV.Sample(smp, uv).rg;\n"
"\n"
"    float Y = (y_val * 255.0 - 16.0) / 219.0;\n"
"    float U = uv_val.r - 0.5019607843;\n"
"    float V = uv_val.g - 0.5019607843;\n"
"\n"
"    float R = Y + 1.5748 * V;\n"
"    float G = Y - 0.1873 * U - 0.4681 * V;\n"
"    float B = Y + 1.8556 * U;\n"
"\n"
"    return float4(R, G, B, 1.0);\n"
"}\n";

// ========== 构造和析构 ==========
D3D11HardwareContext::D3D11HardwareContext(HWND hwnd)
{
    if (!hwnd) return;
    hwnd_ = hwnd;

    if (!InitD3D11(hwnd)) {
        std::cerr << "Failed to initialize D3D11" << std::endl;
        return;
    }

    if (!InitShaders()) {
        std::cerr << "Failed to initialize shaders" << std::endl;
        return;
    }

    CreateSampler();
    CreateFullScreenQuad();
}

// ========== D3D11 初始化 ==========
bool D3D11HardwareContext::InitD3D11(HWND hwnd)
{
    // 枚举所有显卡
    Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
    HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&factory);
    if (FAILED(hr)) {
        std::cerr << "CreateDXGIFactory1 failed" << std::endl;
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGIAdapter1> selected_adapter;
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;

    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; i++) {
        DXGI_ADAPTER_DESC desc;
        adapter->GetDesc(&desc);

        char adapter_name[256];
        size_t converted;
        wcstombs_s(&converted, adapter_name, sizeof(adapter_name), desc.Description, _TRUNCATE);

        std::cout << "Found GPU " << i << ": " << adapter_name << std::endl;

        // 只选择 NVIDIA
        if (wcsstr(desc.Description, L"NVIDIA") != nullptr) {
            std::cout << "Selecting NVIDIA GPU" << std::endl;
            selected_adapter = adapter;
            break;
        }
    }

    if (!selected_adapter) {
        std::cout << "No NVIDIA GPU found, using default" << std::endl;
        return InitD3D11Default(hwnd);
    }

    D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };

    hr = D3D11CreateDevice(
        selected_adapter.Get(),
        D3D_DRIVER_TYPE_UNKNOWN,
        nullptr,
        D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_BGRA_SUPPORT |
        D3D11_CREATE_DEVICE_DEBUG,
        feature_levels,
        3,
        D3D11_SDK_VERSION,
        &d3d11_device_,
        nullptr,
        &d3d11_context_
    );

    if (FAILED(hr)) {
        std::cerr << "D3D11CreateDevice with selected GPU failed:" << hr << std::endl;
        return false;
    }

    // 启用多线程保护
    Microsoft::WRL::ComPtr<ID3D10Multithread> pMultithread;
    hr = d3d11_device_.As(&pMultithread);
    if (SUCCEEDED(hr) && pMultithread) {
        pMultithread->SetMultithreadProtected(TRUE);
        std::cout << "D3D11 Multithread protection enabled" << std::endl;
    }
    else {
        std::cerr << "Failed to enable multithread protection" << std::endl;
    }

    if (!CreateDXGIFactoryWithFallback()) {
        std::cerr << "CreateDXGIFactoryWithFallback failed:" << std::endl;
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGIFactory2> factory2;
    hr = dxgiFactory_.As(&factory2);
    if (FAILED(hr)) {
        std::cerr << "IDXGIFactory2 not supported" << std::endl;
        return false;
    }

    DXGI_SWAP_CHAIN_DESC1 sd = {};
    sd.Width = 0;
    sd.Height = 0;
    sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 2;
    sd.Scaling = DXGI_SCALING_STRETCH;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    sd.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

    hr = factory2->CreateSwapChainForHwnd(
        d3d11_device_.Get(),
        hwnd,
        &sd,
        nullptr,
        nullptr,
        &swap_chain_
    );

    if (FAILED(hr)) {
        std::cerr << "CreateSwapChainForHwnd failed:" << hr << std::endl;
        return false;
    }

    return RecreateRenderTargetView();
}

bool D3D11HardwareContext::InitD3D11Default(HWND hwnd)
{
    D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0
    };

    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        feature_levels,
        3,
        D3D11_SDK_VERSION,
        &d3d11_device_,
        nullptr,
        &d3d11_context_
    );

    if (FAILED(hr)) {
        std::cerr << "D3D11CreateDevice default failed:" << hr << std::endl;
        return false;
    }

    if (!CreateDXGIFactoryWithFallback()) {
        std::cerr << "CreateDXGIFactoryWithFallback failed:" << std::endl;
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGIFactory2> factory2;
    hr = dxgiFactory_.As(&factory2);
    if (FAILED(hr)) {
        std::cerr << "IDXGIFactory2 not supported" << std::endl;
        return false;
    }

    DXGI_SWAP_CHAIN_DESC1 sd = {};
    sd.Width = 0;
    sd.Height = 0;
    sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 2;
    sd.Scaling = DXGI_SCALING_STRETCH;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    sd.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

    hr = factory2->CreateSwapChainForHwnd(
        d3d11_device_.Get(),
        hwnd,
        &sd,
        nullptr,
        nullptr,
        &swap_chain_
    );

    if (FAILED(hr)) {
        std::cerr << "CreateSwapChainForHwnd failed:" << hr << std::endl;
        return false;
    }

    return RecreateRenderTargetView();
}

bool D3D11HardwareContext::CreateDXGIFactoryWithFallback()
{
    HRESULT hr = S_OK;

    Microsoft::WRL::ComPtr<IDXGIFactory7> factory7;
    hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory7));
    if (SUCCEEDED(hr)) {
        dxgiFactory_ = factory7;
        return true;
    }

    Microsoft::WRL::ComPtr<IDXGIFactory6> factory6;
    hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory6));
    if (SUCCEEDED(hr)) {
        dxgiFactory_ = factory6;
        return true;
    }

    Microsoft::WRL::ComPtr<IDXGIFactory5> factory5;
    hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory5));
    if (SUCCEEDED(hr)) {
        dxgiFactory_ = factory5;
        return true;
    }

    Microsoft::WRL::ComPtr<IDXGIFactory4> factory4;
    hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory4));
    if (SUCCEEDED(hr)) {
        dxgiFactory_ = factory4;
        return true;
    }

    Microsoft::WRL::ComPtr<IDXGIFactory2> factory2;
    hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory2));
    if (SUCCEEDED(hr)) {
        dxgiFactory_ = factory2;
        return true;
    }

    Microsoft::WRL::ComPtr<IDXGIFactory1> factory1;
    hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&factory1);
    if (SUCCEEDED(hr)) {
        dxgiFactory_ = factory1;
        return true;
    }

    return false;
}

// ========== 着色器初始化 ==========
bool D3D11HardwareContext::InitShaders()
{
    Microsoft::WRL::ComPtr<ID3DBlob> vs_blob;
    Microsoft::WRL::ComPtr<ID3DBlob> ps_blob;
    Microsoft::WRL::ComPtr<ID3DBlob> error_blob;

    HRESULT hr = D3DCompile(
        g_vs_code,
        strlen(g_vs_code),
        nullptr, nullptr, nullptr,
        "main",
        "vs_5_0",
        0, 0,
        &vs_blob,
        &error_blob
    );

    if (FAILED(hr)) {
        if (error_blob) {
            std::cerr << "VS Compile Error:" << (char*)error_blob->GetBufferPointer() << std::endl;
        }
        return false;
    }

    hr = d3d11_device_->CreateVertexShader(
        vs_blob->GetBufferPointer(),
        vs_blob->GetBufferSize(),
        nullptr,
        &vertex_shader_
    );
    if (FAILED(hr))
    {
        std::cerr << "VS CreateVertexShader Error:" << hr << std::endl;
        return false;
    }

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };

    hr = d3d11_device_->CreateInputLayout(
        layout, 2,
        vs_blob->GetBufferPointer(),
        vs_blob->GetBufferSize(),
        &input_layout_
    );
    if (FAILED(hr))
    {
        std::cerr << "CreateInputLayout Error:" << hr << std::endl;
        return false;
    }

    hr = D3DCompile(
        g_ps_code,
        strlen(g_ps_code),
        nullptr, nullptr, nullptr,
        "main",
        "ps_5_0",
        0, 0,
        &ps_blob,
        &error_blob
    );
    if (FAILED(hr))
    {
        if (error_blob) {
            std::cerr << "PS Compile Error:" << (char*)error_blob->GetBufferPointer() << std::endl;
        }
        return false;
    }

    hr = d3d11_device_->CreatePixelShader(
        ps_blob->GetBufferPointer(),
        ps_blob->GetBufferSize(),
        nullptr,
        &pixel_shader_
    );
    if (FAILED(hr))
    {
        std::cerr << "PS CreatePixelShader Error:" << hr << std::endl;
        return false;
    }

    std::cout << "Shaders compiled successfully" << std::endl;
    return true;
}

// ========== 渲染资源创建 ==========
void D3D11HardwareContext::CreateSampler()
{
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    HRESULT hr = d3d11_device_->CreateSamplerState(&sampDesc, &sampler_);
    if (FAILED(hr)) {
        std::cerr << "Failed to create sampler, HRESULT:" << hr << std::endl;
    }
    else {
        std::cout << "Sampler created successfully" << std::endl;
    }
}

void D3D11HardwareContext::CreateFullScreenQuad()
{
    if (vertex_buffer_) return;

    Vertex vertices[] = {
        { -1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f },
        { -1.0f,  1.0f, 0.0f, 1.0f, 0.0f, 0.0f },
        {  1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 1.0f },
        { -1.0f,  1.0f, 0.0f, 1.0f, 0.0f, 0.0f },
        {  1.0f,  1.0f, 0.0f, 1.0f, 1.0f, 0.0f },
        {  1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 1.0f }
    };

    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(vertices);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA init_data = {};
    init_data.pSysMem = vertices;

    HRESULT hr = d3d11_device_->CreateBuffer(&bd, &init_data, &vertex_buffer_);
    if (FAILED(hr)) {
        std::cerr << "Failed to create vertex buffer, HRESULT:" << hr << std::endl;
    }
    else {
        std::cout << "Full screen quad created successfully" << std::endl;
    }
}

// ========== 固定管线状态设置 ==========
void D3D11HardwareContext::SetupFixedPipelineState()
{
    // 设置着色器
    d3d11_context_->VSSetShader(vertex_shader_.Get(), nullptr, 0);
    d3d11_context_->PSSetShader(pixel_shader_.Get(), nullptr, 0);
    d3d11_context_->IASetInputLayout(input_layout_.Get());

    // 设置采样器
    if (sampler_) {
        d3d11_context_->PSSetSamplers(0, 1, sampler_.GetAddressOf());
    }

    // 设置图元拓扑
    d3d11_context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 设置顶点缓冲区
    if (vertex_buffer_) {
        UINT stride = sizeof(Vertex);
        UINT offset = 0;
        ID3D11Buffer* buffer = vertex_buffer_.Get();
        d3d11_context_->IASetVertexBuffers(0, 1, &buffer, &stride, &offset);
    }
}

// ========== 资源检查和更新 ==========
bool D3D11HardwareContext::CheckAndUpdateResources()
{
    if (!swap_chain_ || !d3d11_context_ || !d3d11_device_) return false;

    // 获取当前后台缓冲区
    Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer;
    HRESULT hr = swap_chain_->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back_buffer);
    if (FAILED(hr)) return false;

    D3D11_TEXTURE2D_DESC desc;
    back_buffer->GetDesc(&desc);

    // 首次初始化
    if (cached_width_ == 0 && cached_height_ == 0) {
        cached_width_ = desc.Width;
        cached_height_ = desc.Height;

        // 设置渲染目标
        d3d11_context_->OMSetRenderTargets(1, render_target_view_.GetAddressOf(), nullptr);

        // 设置视口
        D3D11_VIEWPORT viewport = {};
        viewport.Width = static_cast<FLOAT>(cached_width_);
        viewport.Height = static_cast<FLOAT>(cached_height_);
        viewport.MaxDepth = 1.0f;
        d3d11_context_->RSSetViewports(1, &viewport);

        std::cout << "Resources initialized: " << cached_width_ << "x" << cached_height_ << std::endl;
        return true;
    }

    // 尺寸未变，直接返回
    if (cached_width_ == desc.Width && cached_height_ == desc.Height) {
        return true;
    }

    // ========== 尺寸变化，需要重建资源 ==========
    std::cout << "Window size changed: " << cached_width_ << "x" << cached_height_
        << " -> " << desc.Width << "x" << desc.Height << std::endl;

    // 1. 重建交换链缓冲区
    hr = swap_chain_->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) {
        std::cerr << "ResizeBuffers failed: 0x" << std::hex << hr << std::dec << std::endl;
        return false;
    }

    // 2. 重新获取后台缓冲区
    hr = swap_chain_->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back_buffer);
    if (FAILED(hr)) {
        std::cerr << "GetBuffer after resize failed: 0x" << std::hex << hr << std::dec << std::endl;
        return false;
    }
    back_buffer->GetDesc(&desc);

    // 3. 重建渲染目标视图
    render_target_view_.Reset();
    hr = d3d11_device_->CreateRenderTargetView(back_buffer.Get(), nullptr, &render_target_view_);
    if (FAILED(hr)) {
        std::cerr << "CreateRenderTargetView failed: 0x" << std::hex << hr << std::dec << std::endl;
        return false;
    }

    // 4. 更新缓存
    cached_width_ = desc.Width;
    cached_height_ = desc.Height;

    // 5. 设置渲染目标
    d3d11_context_->OMSetRenderTargets(1, render_target_view_.GetAddressOf(), nullptr);

    // 6. 设置视口
    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<FLOAT>(cached_width_);
    viewport.Height = static_cast<FLOAT>(cached_height_);
    viewport.MaxDepth = 1.0f;
    d3d11_context_->RSSetViewports(1, &viewport);

    // 7. 清空 SRV 缓存（纹理尺寸可能变化）
    ClearSRVCache();

    std::cout << "Resources recreated: " << cached_width_ << "x" << cached_height_ << std::endl;
    return true;
}

bool D3D11HardwareContext::RecreateRenderTargetView()
{
    if (!swap_chain_ || !d3d11_device_) return false;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer;
    HRESULT hr = swap_chain_->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back_buffer);
    if (FAILED(hr)) {
        std::cerr << "GetBuffer failed:" << hr << std::endl;
        return false;
    }

    render_target_view_.Reset();
    hr = d3d11_device_->CreateRenderTargetView(back_buffer.Get(), nullptr, &render_target_view_);
    if (FAILED(hr)) {
        std::cerr << "CreateRenderTargetView failed:" << hr << std::endl;
        return false;
    }

    return true;
}

// ========== SRV 管理 ==========
bool D3D11HardwareContext::CreateOrReuseSRV(ID3D11Texture2D* texture, int slice_index)
{
    if (!texture) return false;

    // 构建缓存键
    SRVKey key = { texture, slice_index };

    // 查找缓存
    auto it = srv_cache_.find(key);
    if (it != srv_cache_.end()) {
        // 找到缓存，直接使用
        current_srv_y_ = it->second.y;
        current_srv_uv_ = it->second.uv;
        return true;
    }

    // 缓存未命中，创建新的 SRV
    D3D11_TEXTURE2D_DESC desc;
    texture->GetDesc(&desc);

    // 验证切片索引
    if (slice_index >= (int)desc.ArraySize) {
        std::cerr << "Slice index " << slice_index << " out of range (0-" << desc.ArraySize - 1 << ")" << std::endl;
        slice_index = 0;
    }

    if (desc.Format != DXGI_FORMAT_NV12) {
        std::cerr << "Unsupported texture format:" << desc.Format << std::endl;
        return false;
    }

    bool isArray = (desc.ArraySize > 1);

    SRVPair srvs;

    // 创建 Y 平面 SRV
    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc_y = {};
    srv_desc_y.Format = DXGI_FORMAT_R8_UNORM;

    if (isArray) {
        srv_desc_y.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
        srv_desc_y.Texture2DArray.MostDetailedMip = 0;
        srv_desc_y.Texture2DArray.MipLevels = 1;
        srv_desc_y.Texture2DArray.FirstArraySlice = slice_index;
        srv_desc_y.Texture2DArray.ArraySize = 1;
    }
    else {
        srv_desc_y.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srv_desc_y.Texture2D.MostDetailedMip = 0;
        srv_desc_y.Texture2D.MipLevels = 1;
    }

    HRESULT hr = d3d11_device_->CreateShaderResourceView(
        texture, &srv_desc_y, &srvs.y);

    if (FAILED(hr)) {
        std::cerr << "Failed to create Y plane SRV, HRESULT:" << hr << std::endl;
        return false;
    }

    // 创建 UV 平面 SRV
    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc_uv = {};
    srv_desc_uv.Format = DXGI_FORMAT_R8G8_UNORM;

    if (isArray) {
        srv_desc_uv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
        srv_desc_uv.Texture2DArray.MostDetailedMip = 0;
        srv_desc_uv.Texture2DArray.MipLevels = 1;
        srv_desc_uv.Texture2DArray.FirstArraySlice = slice_index;
        srv_desc_uv.Texture2DArray.ArraySize = 1;
    }
    else {
        srv_desc_uv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srv_desc_uv.Texture2D.MostDetailedMip = 0;
        srv_desc_uv.Texture2D.MipLevels = 1;
    }

    hr = d3d11_device_->CreateShaderResourceView(
        texture, &srv_desc_uv, &srvs.uv);

    if (FAILED(hr)) {
        std::cerr << "Failed to create UV plane SRV, HRESULT:" << hr << std::endl;
        srvs.y.Reset();
        return false;
    }

    // 存入缓存
    srv_cache_[key] = std::move(srvs);

    // 设置当前使用的 SRV
    current_srv_y_ = srv_cache_[key].y;
    current_srv_uv_ = srv_cache_[key].uv;

    std::cout << "Created new SRV for texture=" << texture << ", slice=" << slice_index << std::endl;
    return true;
}

void D3D11HardwareContext::ClearSRVCache()
{
    srv_cache_.clear();
    current_srv_y_.Reset();
    current_srv_uv_.Reset();
    std::cout << "SRV cache cleared" << std::endl;
}

// ========== 渲染 ==========
bool D3D11HardwareContext::RenderFrame(ID3D11Texture2D* frame_tex, int slice_index)
{
    if (!swap_chain_ || !d3d11_context_ || !d3d11_device_) return false;

    HRESULT hr = d3d11_device_->GetDeviceRemovedReason();
    if (FAILED(hr)) {
        std::cerr << "Device removed! Reason:" << hr << std::endl;
        return false;
    }

    if (!frame_tex) {
        std::cerr << "RenderFrame: frame_tex is null" << std::endl;
        return false;
    }

    // 设置固定管线状态
    SetupFixedPipelineState();

    // 自动检查并更新资源（处理窗口大小变化）
    if (!CheckAndUpdateResources()) {
        return false;
    }

    // 关键修复：每次绘制前重新绑定渲染目标
    // 因为 DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL 在 Present 后会解绑
    d3d11_context_->OMSetRenderTargets(1, render_target_view_.GetAddressOf(), nullptr);

    // 清屏
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    d3d11_context_->ClearRenderTargetView(render_target_view_.Get(), clearColor);

    // 创建或复用 SRV
    if (!CreateOrReuseSRV(frame_tex, slice_index)) {
        return false;
    }

    if (!current_srv_y_ || !current_srv_uv_) {
        std::cerr << "SRVs are null!" << std::endl;
        return false;
    }

    // 绑定纹理
    ID3D11ShaderResourceView* srvs[2] = { current_srv_y_.Get(), current_srv_uv_.Get() };
    d3d11_context_->PSSetShaderResources(0, 2, srvs);

    // 绘制
    d3d11_context_->Draw(6, 0);

    // 解绑 SRV
    ID3D11ShaderResourceView* null_srvs[2] = { nullptr, nullptr };
    d3d11_context_->PSSetShaderResources(0, 2, null_srvs);

    // 呈现
    //d3d11_context_->Flush();
    hr = swap_chain_->Present(0, 0);
    if (FAILED(hr)) {
        std::cerr << "Present failed:" << hr << std::endl;
        return false;
    }

    return true;
}
