#pragma once
#if defined(DEBUG)|| defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif // defined(DEBUG)|| defined(_DEBUG)

#include "stdafx.h"
#include "GameTimer.h"
#include "D3DAppUtil.h"
#include "UploadBuffer.h"

struct Vertex
{
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT4 color;
};

struct ObjectConstants
{
    DirectX::XMMATRIX WorldViewProj = DirectX::XMMatrixIdentity();
};

using Microsoft::WRL::ComPtr;


class D3DAppBase
{
public:
    D3DAppBase(UINT width, UINT height, std::wstring name, UINT frameCount = 2);
    virtual ~D3DAppBase();

    virtual void OnInit();
    virtual void OnUpdate();
    virtual void OnRender();
    virtual void OnDestroy();

    D3DAppBase(const D3DAppBase& rhs) = delete;
    D3DAppBase& operator=(const D3DAppBase& rhs) = delete;

    void ParseCommandLineArgs(_In_reads_(argc) WCHAR* argv[], int argc);

    void CalculateFrameStats();

    // Accessors
    UINT GetWidth()const { return m_width; }
    UINT GetHeight()const { return m_height; }
    const WCHAR* GetTitle()const { return m_title.c_str(); }

    auto Run()->int;
protected:

    // Functions.
    void CreateFactoryDeviceAdapter();
    void InitializeDescriptorSize();
    void CheckFeatureSupport();
    void InitializePipeline();
    void GetHardwareAdapter(_In_ IDXGIFactory2* pFactory, _Outptr_result_maybenull_ IDXGIAdapter1** ppAdapter);
    void CreateCommandObjects();
    void CreateCommandQueue();
    void CreateCommandAllocator();
    void CreateCommandList();
    void CreateSwapChain();
    void CreateFenceObjects();
    void CreateRtvAndDsvDescriptorHeaps();
    void CreateFrameResources();
    void PopulateCommandList();
    void WaitForPreviousFrame();
    void BuildRootSignature();
    void BuildShader();
    void BuildPSO();
    void BuildGeometry();
    void BuildConstantDescriptorHeaps();
    void BuildConstantBuffer();

    // Helper function.
    std::wstring GetAssetsFullPath(LPCWSTR assetName);

    ComPtr<IDXGIFactory4>   m_factory;
    ComPtr<ID3D12Device>    m_device;
    ComPtr<IDXGIAdapter1>   m_adapter;

    ComPtr<ID3DBlob>    m_vertexShader;
    ComPtr<ID3DBlob>    m_pixelShader;

    ComPtr<ID3D12CommandAllocator>  m_commandAllocator;
    ComPtr<ID3D12CommandQueue>  m_commandQueue;
    ComPtr<ID3D12GraphicsCommandList>   m_commandList;
    ComPtr<ID3D12PipelineState> m_pipelineState;

    ComPtr<ID3D12RootSignature> m_rootSignature;

    ComPtr<IDXGISwapChain3> m_swapChain;
    std::vector<ComPtr<ID3D12Resource>> m_renderTargets;
    UINT m_frameCount = 2;
    UINT m_currentBackBuffer = 0;

    ComPtr<ID3D12Resource>  m_depthStencilBuffer;

    // App resources.
    ComPtr<ID3D12Resource>  m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW    m_vertexBufferView;

    std::unique_ptr<MeshGeometry>   m_geometry = nullptr;

    ComPtr<ID3D12DescriptorHeap>    m_rtvHeap;
    ComPtr<ID3D12DescriptorHeap>    m_dsvHeap;
    ComPtr<ID3D12DescriptorHeap>    m_cbvHeap;

    std::unique_ptr<UploadBuffer<ObjectConstants>>  m_objectCB = nullptr;

    CD3DX12_VIEWPORT  m_viewport;
    CD3DX12_RECT  m_scissorRect;

    UINT m_rtvDescriptorSize = 0;
    UINT m_dsvDescriptorSize = 0;
    UINT m_cbvSrvUavDescriptorSize = 0;

    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_currentFence = 0;
    HANDLE m_fenceEvent;

    bool m_useWarpDevice = false;
    UINT m_width;
    UINT m_height;
    float m_aspectRatio;

    std::unique_ptr<GameTimer> m_gameTimer;

    bool    m_4xMsaaState = false;
    UINT    m_4xMsaaQuality = 0;

    DXGI_FORMAT m_backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT m_depthBufferFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    D3D_DRIVER_TYPE m_d3dDriverType = D3D_DRIVER_TYPE_HARDWARE;
    D3D12_COMMAND_LIST_TYPE m_commandListType = D3D12_COMMAND_LIST_TYPE_DIRECT;

private:
    std::wstring m_assetsPath;
    std::wstring m_title;

    float m_theta = 1.5f * DirectX::XM_PI;
    float m_phi = DirectX::XM_PIDIV4;
    float m_radius = 5.0f;
};