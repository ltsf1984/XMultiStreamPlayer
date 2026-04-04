// d3d11_hardware_context.cpp
#include "d3d11_hardware_context.h"
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <iostream>
#include <cstdio>
#include <bitset>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// 顶点着色器
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

// d3d11_hardware_context.cpp - 完整的像素着色器

static const char g_ps_code[] =
"cbuffer TilingParams : register(b0)\n"
"{\n"
"    float cols;\n"
"    float rows;\n"
"    float targetWidth;\n"
"    float targetHeight;\n"
"    int validMask[8];\n"
"}\n"
"\n"
"Texture2D tex0 : register(t0);\n"
"Texture2D tex1 : register(t1);\n"
"Texture2D tex2 : register(t2);\n"
"Texture2D tex3 : register(t3);\n"
"Texture2D tex4 : register(t4);\n"
"Texture2D tex5 : register(t5);\n"
"Texture2D tex6 : register(t6);\n"
"Texture2D tex7 : register(t7);\n"
"Texture2D tex8 : register(t8);\n"
"Texture2D tex9 : register(t9);\n"
"Texture2D tex10 : register(t10);\n"
"Texture2D tex11 : register(t11);\n"
"Texture2D tex12 : register(t12);\n"
"Texture2D tex13 : register(t13);\n"
"Texture2D tex14 : register(t14);\n"
"Texture2D tex15 : register(t15);\n"
"Texture2D tex16 : register(t16);\n"
"Texture2D tex17 : register(t17);\n"
"Texture2D tex18 : register(t18);\n"
"Texture2D tex19 : register(t19);\n"
"Texture2D tex20 : register(t20);\n"
"Texture2D tex21 : register(t21);\n"
"Texture2D tex22 : register(t22);\n"
"Texture2D tex23 : register(t23);\n"
"Texture2D tex24 : register(t24);\n"
"Texture2D tex25 : register(t25);\n"
"Texture2D tex26 : register(t26);\n"
"Texture2D tex27 : register(t27);\n"
"Texture2D tex28 : register(t28);\n"
"Texture2D tex29 : register(t29);\n"
"Texture2D tex30 : register(t30);\n"
"Texture2D tex31 : register(t31);\n"
"\n"
"Texture2D texUV0 : register(t32);\n"
"Texture2D texUV1 : register(t33);\n"
"Texture2D texUV2 : register(t34);\n"
"Texture2D texUV3 : register(t35);\n"
"Texture2D texUV4 : register(t36);\n"
"Texture2D texUV5 : register(t37);\n"
"Texture2D texUV6 : register(t38);\n"
"Texture2D texUV7 : register(t39);\n"
"Texture2D texUV8 : register(t40);\n"
"Texture2D texUV9 : register(t41);\n"
"Texture2D texUV10 : register(t42);\n"
"Texture2D texUV11 : register(t43);\n"
"Texture2D texUV12 : register(t44);\n"
"Texture2D texUV13 : register(t45);\n"
"Texture2D texUV14 : register(t46);\n"
"Texture2D texUV15 : register(t47);\n"
"Texture2D texUV16 : register(t48);\n"
"Texture2D texUV17 : register(t49);\n"
"Texture2D texUV18 : register(t50);\n"
"Texture2D texUV19 : register(t51);\n"
"Texture2D texUV20 : register(t52);\n"
"Texture2D texUV21 : register(t53);\n"
"Texture2D texUV22 : register(t54);\n"
"Texture2D texUV23 : register(t55);\n"
"Texture2D texUV24 : register(t56);\n"
"Texture2D texUV25 : register(t57);\n"
"Texture2D texUV26 : register(t58);\n"
"Texture2D texUV27 : register(t59);\n"
"Texture2D texUV28 : register(t60);\n"
"Texture2D texUV29 : register(t61);\n"
"Texture2D texUV30 : register(t62);\n"
"Texture2D texUV31 : register(t63);\n"
"\n"
"SamplerState smp : register(s0);\n"
"\n"
"void GetYUV(int idx, float2 uv, out float y, out float u, out float v)\n"
"{\n"
"    switch(idx)\n"
"    {\n"
"        case 0:  y = tex0.Sample(smp, uv).r;  float2 uv0 = texUV0.Sample(smp, uv).rg;  u = uv0.r; v = uv0.g; return;\n"
"        case 1:  y = tex1.Sample(smp, uv).r;  float2 uv1 = texUV1.Sample(smp, uv).rg;  u = uv1.r; v = uv1.g; return;\n"
"        case 2:  y = tex2.Sample(smp, uv).r;  float2 uv2 = texUV2.Sample(smp, uv).rg;  u = uv2.r; v = uv2.g; return;\n"
"        case 3:  y = tex3.Sample(smp, uv).r;  float2 uv3 = texUV3.Sample(smp, uv).rg;  u = uv3.r; v = uv3.g; return;\n"
"        case 4:  y = tex4.Sample(smp, uv).r;  float2 uv4 = texUV4.Sample(smp, uv).rg;  u = uv4.r; v = uv4.g; return;\n"
"        case 5:  y = tex5.Sample(smp, uv).r;  float2 uv5 = texUV5.Sample(smp, uv).rg;  u = uv5.r; v = uv5.g; return;\n"
"        case 6:  y = tex6.Sample(smp, uv).r;  float2 uv6 = texUV6.Sample(smp, uv).rg;  u = uv6.r; v = uv6.g; return;\n"
"        case 7:  y = tex7.Sample(smp, uv).r;  float2 uv7 = texUV7.Sample(smp, uv).rg;  u = uv7.r; v = uv7.g; return;\n"
"        case 8:  y = tex8.Sample(smp, uv).r;  float2 uv8 = texUV8.Sample(smp, uv).rg;  u = uv8.r; v = uv8.g; return;\n"
"        case 9:  y = tex9.Sample(smp, uv).r;  float2 uv9 = texUV9.Sample(smp, uv).rg;  u = uv9.r; v = uv9.g; return;\n"
"        case 10: y = tex10.Sample(smp, uv).r; float2 uv10 = texUV10.Sample(smp, uv).rg; u = uv10.r; v = uv10.g; return;\n"
"        case 11: y = tex11.Sample(smp, uv).r; float2 uv11 = texUV11.Sample(smp, uv).rg; u = uv11.r; v = uv11.g; return;\n"
"        case 12: y = tex12.Sample(smp, uv).r; float2 uv12 = texUV12.Sample(smp, uv).rg; u = uv12.r; v = uv12.g; return;\n"
"        case 13: y = tex13.Sample(smp, uv).r; float2 uv13 = texUV13.Sample(smp, uv).rg; u = uv13.r; v = uv13.g; return;\n"
"        case 14: y = tex14.Sample(smp, uv).r; float2 uv14 = texUV14.Sample(smp, uv).rg; u = uv14.r; v = uv14.g; return;\n"
"        case 15: y = tex15.Sample(smp, uv).r; float2 uv15 = texUV15.Sample(smp, uv).rg; u = uv15.r; v = uv15.g; return;\n"
"        case 16: y = tex16.Sample(smp, uv).r; float2 uv16 = texUV16.Sample(smp, uv).rg; u = uv16.r; v = uv16.g; return;\n"
"        case 17: y = tex17.Sample(smp, uv).r; float2 uv17 = texUV17.Sample(smp, uv).rg; u = uv17.r; v = uv17.g; return;\n"
"        case 18: y = tex18.Sample(smp, uv).r; float2 uv18 = texUV18.Sample(smp, uv).rg; u = uv18.r; v = uv18.g; return;\n"
"        case 19: y = tex19.Sample(smp, uv).r; float2 uv19 = texUV19.Sample(smp, uv).rg; u = uv19.r; v = uv19.g; return;\n"
"        case 20: y = tex20.Sample(smp, uv).r; float2 uv20 = texUV20.Sample(smp, uv).rg; u = uv20.r; v = uv20.g; return;\n"
"        case 21: y = tex21.Sample(smp, uv).r; float2 uv21 = texUV21.Sample(smp, uv).rg; u = uv21.r; v = uv21.g; return;\n"
"        case 22: y = tex22.Sample(smp, uv).r; float2 uv22 = texUV22.Sample(smp, uv).rg; u = uv22.r; v = uv22.g; return;\n"
"        case 23: y = tex23.Sample(smp, uv).r; float2 uv23 = texUV23.Sample(smp, uv).rg; u = uv23.r; v = uv23.g; return;\n"
"        case 24: y = tex24.Sample(smp, uv).r; float2 uv24 = texUV24.Sample(smp, uv).rg; u = uv24.r; v = uv24.g; return;\n"
"        case 25: y = tex25.Sample(smp, uv).r; float2 uv25 = texUV25.Sample(smp, uv).rg; u = uv25.r; v = uv25.g; return;\n"
"        case 26: y = tex26.Sample(smp, uv).r; float2 uv26 = texUV26.Sample(smp, uv).rg; u = uv26.r; v = uv26.g; return;\n"
"        case 27: y = tex27.Sample(smp, uv).r; float2 uv27 = texUV27.Sample(smp, uv).rg; u = uv27.r; v = uv27.g; return;\n"
"        case 28: y = tex28.Sample(smp, uv).r; float2 uv28 = texUV28.Sample(smp, uv).rg; u = uv28.r; v = uv28.g; return;\n"
"        case 29: y = tex29.Sample(smp, uv).r; float2 uv29 = texUV29.Sample(smp, uv).rg; u = uv29.r; v = uv29.g; return;\n"
"        case 30: y = tex30.Sample(smp, uv).r; float2 uv30 = texUV30.Sample(smp, uv).rg; u = uv30.r; v = uv30.g; return;\n"
"        default: y = tex31.Sample(smp, uv).r; float2 uv31 = texUV31.Sample(smp, uv).rg; u = uv31.r; v = uv31.g; return;\n"
"    }\n"
"}\n"
"\n"
"float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD) : SV_Target\n"
"{\n"
"    // 计算当前像素属于哪个单元格\n"
"    float2 cellSize = float2(1.0 / cols, 1.0 / rows);\n"
"    int2 cellIndex = int2(floor(uv / cellSize));\n"
"    \n"
"    // 计算单元格索引 (0-31)\n"
"    int idx = cellIndex.y * int(cols) + cellIndex.x;\n"
"    \n"
"    // 边界检查\n"
"    if (idx < 0 || idx >= 32) {\n"
"        return float4(0, 0, 0, 1);\n"
"    }\n"
"    \n"
"    // 检查纹理是否有效\n"
"    int wordIdx = idx / 32;\n"
"    int bitIdx = idx - wordIdx * 32;\n"
"    bool isValid = (validMask[wordIdx] >> bitIdx) & 1;\n"
"    \n"
"    if (!isValid) {\n"
"        return float4(0, 0, 0, 1);\n"
"    }\n"
"    \n"
"    // 计算单元格内的局部 UV\n"
"    float2 cellUV = frac(uv / cellSize);\n"
"    \n"
"    // 获取 YUV 值\n"
"    float y, u, v;\n"
"    GetYUV(idx, cellUV, y, u, v);\n"
"\n"
"    // YUV 转 RGB (BT.601)\n"
"    float Y = (y * 255.0 - 16.0) / 219.0;\n"
"    float U = u - 0.5019607843;\n"
"    float V = v - 0.5019607843;\n"
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
    CreateConstantBuffer();
}

// ========== D3D11 初始化 ==========
bool D3D11HardwareContext::InitD3D11(HWND hwnd)
{
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

    Microsoft::WRL::ComPtr<ID3D10Multithread> pMultithread;
    hr = d3d11_device_.As(&pMultithread);
    if (SUCCEEDED(hr) && pMultithread) {
        pMultithread->SetMultithreadProtected(TRUE);
        std::cout << "D3D11 Multithread protection enabled" << std::endl;
    }

    if (!CreateDXGIFactoryWithFallback()) {
        std::cerr << "CreateDXGIFactoryWithFallback failed" << std::endl;
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
        std::cerr << "CreateDXGIFactoryWithFallback failed" << std::endl;
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
    if (FAILED(hr)) {
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
    if (FAILED(hr)) {
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
    if (FAILED(hr)) {
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
    if (FAILED(hr)) {
        std::cerr << "PS CreatePixelShader Error:" << hr << std::endl;
        return false;
    }

    std::cout << "Shaders compiled successfully (multi-texture with validity check)" << std::endl;
    return true;
}

void D3D11HardwareContext::CreateConstantBuffer()
{
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.ByteWidth = sizeof(TilingConstants);
    cbDesc.Usage = D3D11_USAGE_DEFAULT;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    HRESULT hr = d3d11_device_->CreateBuffer(&cbDesc, nullptr, &constant_buffer_);
    if (FAILED(hr)) {
        std::cerr << "Failed to create constant buffer, HRESULT:" << hr << std::endl;
    }
    else {
        std::cout << "Constant buffer created successfully" << std::endl;
    }
}

void D3D11HardwareContext::CreateSampler()
{
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
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
}

void D3D11HardwareContext::SetupFixedPipelineState()
{
    d3d11_context_->VSSetShader(vertex_shader_.Get(), nullptr, 0);
    d3d11_context_->PSSetShader(pixel_shader_.Get(), nullptr, 0);
    d3d11_context_->IASetInputLayout(input_layout_.Get());

    if (sampler_) {
        d3d11_context_->PSSetSamplers(0, 1, sampler_.GetAddressOf());
    }

    if (constant_buffer_) {
        d3d11_context_->PSSetConstantBuffers(0, 1, constant_buffer_.GetAddressOf());
    }

    d3d11_context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    if (vertex_buffer_) {
        UINT stride = sizeof(Vertex);
        UINT offset = 0;
        ID3D11Buffer* buffer = vertex_buffer_.Get();
        d3d11_context_->IASetVertexBuffers(0, 1, &buffer, &stride, &offset);
    }
}

bool D3D11HardwareContext::CheckAndUpdateResources()
{
    if (!swap_chain_ || !d3d11_context_ || !d3d11_device_) return false;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer;
    HRESULT hr = swap_chain_->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back_buffer);
    if (FAILED(hr)) return false;

    D3D11_TEXTURE2D_DESC desc;
    back_buffer->GetDesc(&desc);

    if (cached_width_ == 0 && cached_height_ == 0) {
        cached_width_ = desc.Width;
        cached_height_ = desc.Height;

        d3d11_context_->OMSetRenderTargets(1, render_target_view_.GetAddressOf(), nullptr);

        D3D11_VIEWPORT viewport = {};
        viewport.Width = static_cast<FLOAT>(cached_width_);
        viewport.Height = static_cast<FLOAT>(cached_height_);
        viewport.MaxDepth = 1.0f;
        d3d11_context_->RSSetViewports(1, &viewport);

        std::cout << "Resources initialized: " << cached_width_ << "x" << cached_height_ << std::endl;
        return true;
    }

    if (cached_width_ == desc.Width && cached_height_ == desc.Height) {
        return true;
    }

    hr = swap_chain_->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) return false;

    hr = swap_chain_->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back_buffer);
    if (FAILED(hr)) return false;
    back_buffer->GetDesc(&desc);

    render_target_view_.Reset();
    hr = d3d11_device_->CreateRenderTargetView(back_buffer.Get(), nullptr, &render_target_view_);
    if (FAILED(hr)) return false;

    cached_width_ = desc.Width;
    cached_height_ = desc.Height;

    d3d11_context_->OMSetRenderTargets(1, render_target_view_.GetAddressOf(), nullptr);

    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<FLOAT>(cached_width_);
    viewport.Height = static_cast<FLOAT>(cached_height_);
    viewport.MaxDepth = 1.0f;
    d3d11_context_->RSSetViewports(1, &viewport);

    return true;
}

bool D3D11HardwareContext::RecreateRenderTargetView()
{
    if (!swap_chain_ || !d3d11_device_) return false;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer;
    HRESULT hr = swap_chain_->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back_buffer);
    if (FAILED(hr)) return false;

    render_target_view_.Reset();
    hr = d3d11_device_->CreateRenderTargetView(back_buffer.Get(), nullptr, &render_target_view_);
    if (FAILED(hr)) return false;

    return true;
}

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> D3D11HardwareContext::GetOrCreateSRV(
    ID3D11Texture2D* texture, int slice_index, DXGI_FORMAT format)
{
    if (!texture) return nullptr;

    SRVCacheKey key = { texture, slice_index, format };
    auto it = srv_cache_.find(key);
    if (it != srv_cache_.end()) {
        return it->second;
    }

    D3D11_TEXTURE2D_DESC desc;
    texture->GetDesc(&desc);

    bool isArray = (desc.ArraySize > 1);

    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Format = format;

    if (isArray) {
        srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
        srv_desc.Texture2DArray.MostDetailedMip = 0;
        srv_desc.Texture2DArray.MipLevels = 1;
        srv_desc.Texture2DArray.FirstArraySlice = slice_index;
        srv_desc.Texture2DArray.ArraySize = 1;
    }
    else {
        srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srv_desc.Texture2D.MostDetailedMip = 0;
        srv_desc.Texture2D.MipLevels = 1;
    }

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    HRESULT hr = d3d11_device_->CreateShaderResourceView(texture, &srv_desc, &srv);
    if (FAILED(hr)) {
        std::cerr << "Failed to create SRV for format " << format << ", HRESULT:" << hr << std::endl;
        return nullptr;
    }

    srv_cache_[key] = srv;
    return srv;
}

bool D3D11HardwareContext::RenderTiled(
    const std::array<ID3D11Texture2D*, MAX_TEXTURES>& textures_y,
    const std::array<ID3D11Texture2D*, MAX_TEXTURES>& textures_uv,
    const std::array<int, MAX_TEXTURES>& slice_indices,
    int rows, int cols)
{
    if (!swap_chain_ || !d3d11_context_ || !d3d11_device_) return false;

    HRESULT hr = d3d11_device_->GetDeviceRemovedReason();
    if (FAILED(hr)) {
        std::cerr << "Device removed! Reason:" << hr << std::endl;
        return false;
    }

    // 设置常量缓冲区
    TilingConstants constants = {};
    constants.cols = static_cast<float>(cols);
    constants.rows = static_cast<float>(rows);
    constants.targetWidth = static_cast<float>(cached_width_);
    constants.targetHeight = static_cast<float>(cached_height_);

    // 构建有效性掩码
    for (int i = 0; i < MAX_TEXTURES; ++i) {
        int wordIdx = i / 32;
        int bitIdx = i % 32;
        if (textures_y[i] != nullptr) {
            constants.validMask[wordIdx] |= (1 << bitIdx);
        }
    }

    if (constant_buffer_) {
        d3d11_context_->UpdateSubresource(constant_buffer_.Get(), 0, nullptr, &constants, 0, 0);
    }

    // 创建所有 SRV
    std::array<ID3D11ShaderResourceView*, MAX_TEXTURES> srvs_y = {};
    std::array<ID3D11ShaderResourceView*, MAX_TEXTURES> srvs_uv = {};

    for (int i = 0; i < MAX_TEXTURES; ++i) {
        if (textures_y[i]) {
            auto srv = GetOrCreateSRV(textures_y[i], slice_indices[i], DXGI_FORMAT_R8_UNORM);
            srvs_y[i] = srv.Get();
        }
        if (textures_uv[i]) {
            auto srv = GetOrCreateSRV(textures_uv[i], slice_indices[i], DXGI_FORMAT_R8G8_UNORM);
            srvs_uv[i] = srv.Get();
        }
    }

    SetupFixedPipelineState();

    if (!CheckAndUpdateResources()) {
        return false;
    }

    d3d11_context_->OMSetRenderTargets(1, render_target_view_.GetAddressOf(), nullptr);

    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    d3d11_context_->ClearRenderTargetView(render_target_view_.Get(), clearColor);

    // 绑定 Y 纹理数组（t0 - t31）
    d3d11_context_->PSSetShaderResources(0, MAX_TEXTURES, srvs_y.data());
    // 绑定 UV 纹理数组（t32 - t63）
    d3d11_context_->PSSetShaderResources(32, MAX_TEXTURES, srvs_uv.data());

    d3d11_context_->Draw(6, 0);

    // 解绑
    ID3D11ShaderResourceView* null_srvs[64] = { nullptr };
    d3d11_context_->PSSetShaderResources(0, 64, null_srvs);

    hr = swap_chain_->Present(0, 0);
    if (FAILED(hr)) {
        std::cerr << "Present failed:" << hr << std::endl;
        return false;
    }

    return true;
}

bool D3D11HardwareContext::RenderFrame(ID3D11Texture2D* frame_tex, int slice_index)
{
    // 兼容旧接口：单纹理模式
    if (!frame_tex) return false;

    std::array<ID3D11Texture2D*, MAX_TEXTURES> textures_y = {};
    std::array<ID3D11Texture2D*, MAX_TEXTURES> textures_uv = {};
    std::array<int, MAX_TEXTURES> slice_indices = {};

    textures_y[0] = frame_tex;
    textures_uv[0] = frame_tex;
    slice_indices[0] = slice_index;

    return RenderTiled(textures_y, textures_uv, slice_indices, 1, 1);
}
