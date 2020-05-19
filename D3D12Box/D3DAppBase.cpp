#include "stdafx.h"
#include "D3DAppBase.h"
#include "Win32Application.h"
#include "D3DAppUtil.h"
using namespace Microsoft::WRL;
D3DAppBase::D3DAppBase(UINT width, UINT height, std::wstring name, UINT frameCount /* = 2 */):
    m_width(width),
    m_height(height),
    m_title(name),
    m_useWarpDevice(false),
    m_frameCount(frameCount)
{
    WCHAR assetsPath[512];
    GetAssetsPath(assetsPath, _countof(assetsPath));
    m_assetsPath = assetsPath;
    m_gameTimer = std::make_unique<GameTimer>();
    m_aspectRatio = static_cast<float>(width) / static_cast<float>(height);
}

D3DAppBase::~D3DAppBase()
{
    m_gameTimer.release();
}

void D3DAppBase::ParseCommandLineArgs(_In_reads_(argc) WCHAR* argv[], int argc)
{
    for (int i = 1; i < argc; ++i)
    {
        if (_wcsnicmp(argv[i], L"-warp", wcslen(argv[i])) == 0 ||
            _wcsnicmp(argv[i], L"/warp", wcslen(argv[i])) == 0)
        {
            m_useWarpDevice = true;
            m_title = m_title + L"(WAPR)";
        }
    }
}

auto D3DAppBase::Run()->int
{
    MSG msg = { 0 };
    m_gameTimer->Reset();
    while (msg.message!=WM_QUIT)
    {
        // Process any messages in the queue.
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        // Otherwise, do animation/game stuff.
        else
        {
            m_gameTimer->Tick();
            CalculateFrameStats();
        }
    }
    return (int)msg.wParam;
}

void D3DAppBase::CalculateFrameStats()
{
    // Code computes the average frames per second, and also the 
    // average time it takes to render one frame.  These stats 
    // are appended to the window caption bar.

    static int frameCnt = 0;
    static float timeElapsed = 0.0f;

    frameCnt++;

    // Compute averages over one second period.
    if (D3DAppBase::m_gameTimer->TotalTime() - timeElapsed >= 1.0f)
    {
        float fps = (float)frameCnt;
        float mspf = 1000.0f / fps;

        std::wstring fpsStr = std::to_wstring(fps);
        std::wstring mspfStr = std::to_wstring(mspf);

        std::wstring windowText = D3DAppBase::m_title +
            L"  fps: " + fpsStr +
            L"  mspf: " + mspfStr;

        SetWindowText(Win32Application::GetHwnd(), windowText.c_str());

        // Reset for next average.
        frameCnt = 0;
        timeElapsed += 1.0f;
    }
}


void D3DAppBase::InitializePipeline()
{
    CreateFactoryDeviceAdapter();
    InitializeDescriptorSize();
    CreateCommandObjects();
    CheckFeatureSupport();
    CreateSwapChain();
    CreateFenceObjects();
}

void D3DAppBase::CreateFactoryDeviceAdapter()
{
    UINT dxgiFactoryFlags = 0;
#if defined(DEBUG)||defined(_DEBUG)
    // Enable the d3d12 debug layer.
    {
        ComPtr<ID3D12Debug> debugController;
        ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
        debugController->EnableDebugLayer();
        dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }
#endif
    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags,IID_PPV_ARGS(&m_factory)));
    if (m_useWarpDevice)
    {
        ThrowIfFailed(m_factory->EnumWarpAdapter(IID_PPV_ARGS(&m_adapter)));
        ThrowIfFailed(D3D12CreateDevice(
            m_adapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)
        ));
    }
    else
    {
        GetHardwareAdapter(m_factory.Get(), &m_adapter);
        ThrowIfFailed(D3D12CreateDevice(
            m_adapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)
        ));
    }

}
void D3DAppBase::InitializeDescriptorSize()
{
    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    m_dsvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    m_cbvSrvUavDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void D3DAppBase::CheckFeatureSupport()
{
    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
    msQualityLevels.Format = m_backBufferFormat;
    msQualityLevels.SampleCount = 4;
    msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
    msQualityLevels.NumQualityLevels = 0;
    ThrowIfFailed(m_device->CheckFeatureSupport(
        D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
        &msQualityLevels, sizeof(msQualityLevels)
    ));

    m_4xMsaaQuality = msQualityLevels.NumQualityLevels;
    assert(m_4xMsaaQuality > 0 && "Unexpected MSAA quality level");
}

void D3DAppBase::GetHardwareAdapter(_In_ IDXGIFactory2* pFactory, _Outptr_result_maybenull_ IDXGIAdapter1** ppAdapter)
{
    ComPtr<IDXGIAdapter1> adapter;
    *ppAdapter = nullptr;
    for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != m_factory->EnumAdapters1(adapterIndex, &adapter); adapterIndex++)
    {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            //Don't select the Basic Render Driver adapter
            //If you want a software adapter, pass in "warp" on the command line.
            continue;
        }
        if (SUCCEEDED(D3D12CreateDevice(m_adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
        {
            break;
        }
    }
    *ppAdapter = adapter.Detach();
}

void D3DAppBase::CreateCommandQueue()
{
    // Describe and create the command queue.
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = m_commandListType;
    ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));
}

void D3DAppBase::CreateCommandAllocator()
{
    ThrowIfFailed(m_device->CreateCommandAllocator(m_commandListType, IID_PPV_ARGS(&m_commandAllocator)));
}

void D3DAppBase::CreateCommandList()
{
    ThrowIfFailed(m_device->CreateCommandList(
        0, m_commandListType,
        m_commandAllocator.Get(),
        nullptr, IID_PPV_ARGS(&m_commandList)
    ));
    m_commandList->Close();
}

void D3DAppBase::CreateSwapChain()
{
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = m_frameCount;
    swapChainDesc.Width = m_width;
    swapChainDesc.Height = m_height;
    swapChainDesc.Format = m_backBufferFormat;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = m_4xMsaaState ? 4 : 1;
    swapChainDesc.SampleDesc.Quality = m_4xMsaaState ? (m_4xMsaaQuality - 1) : 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

    ComPtr<IDXGISwapChain1> swapchain;
    ThrowIfFailed(m_factory->CreateSwapChainForHwnd(
        m_commandQueue.Get(),
        Win32Application::GetHwnd(),
        &swapChainDesc,
        nullptr, nullptr,
        &swapchain
    ));

    // This sample does not support fullscreen transitions.
    ThrowIfFailed(m_factory->MakeWindowAssociation(Win32Application::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));

    ThrowIfFailed(swapchain.As(&m_swapChain));
    m_currentBackBuffer = m_swapChain->GetCurrentBackBufferIndex();
}
void D3DAppBase::CreateFenceObjects()
{
    ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
    m_currentFence = 1;
    m_fenceEvent = CreateEvent(nullptr, false, false, nullptr);
    if (m_fenceEvent == nullptr)
    {
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    }
}

void D3DAppBase::CreateRtvAndDsvDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = m_frameCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvHeapDesc.NodeMask = 0;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));


    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvHeapDesc.NodeMask = 0;
    ThrowIfFailed(m_device->CreateDescriptorHeap(
        &dsvHeapDesc,
        IID_PPV_ARGS(&m_dsvHeap)
    ));
}


void D3DAppBase::OnInit()
{
    InitializePipeline();
}

void D3DAppBase::CreateCommandObjects()
{
    CreateCommandQueue();
    CreateCommandAllocator();
    CreateCommandList();
}

void D3DAppBase::OnUpdate()
{

}

void D3DAppBase::OnRender()
{

}

void D3DAppBase::OnDestroy()
{

}