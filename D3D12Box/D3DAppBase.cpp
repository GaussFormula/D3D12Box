#include "stdafx.h"
#include "D3DAppBase.h"
#include "Win32Application.h"
#include "UploadBuffer.h"
#include "GeometryGenerator.h"
using namespace Microsoft::WRL;
using namespace DirectX;
D3DAppBase::D3DAppBase(UINT width, UINT height, std::wstring name, UINT frameCount /* = 2 */):
    m_width(width),
    m_height(height),
    m_title(name),
    m_useWarpDevice(false),
    m_frameCount(frameCount),
    m_currentBackBuffer(0),
    m_viewport(0.0f,0.0f,static_cast<float>(width),static_cast<float>(height)),
    m_scissorRect(0,0,static_cast<LONG>(width),static_cast<LONG>(height))
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

std::wstring D3DAppBase::GetAssetsFullPath(LPCWSTR assetName)
{
    return m_assetsPath + assetName;
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
            m_gameTimer->Tick();
            CalculateFrameStats();
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
    CreateRtvAndDsvDescriptorHeaps();
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

void D3DAppBase::CreateCommandAllocators()
{
    m_commandAllocators.resize(m_frameCount);
    for (UINT i=0;i<m_frameCount;i++)
    {
        ThrowIfFailed(m_device->CreateCommandAllocator(m_commandListType, IID_PPV_ARGS(&m_commandAllocators[i])));
    }
}

void D3DAppBase::CreateCommandList()
{
    ThrowIfFailed(m_device->CreateCommandList(
        0, m_commandListType,
        m_commandAllocators[m_currentBackBuffer].Get(),
        nullptr, IID_PPV_ARGS(&m_commandList)
    ));
    //m_commandList->Close();
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
    m_fenceValues.resize(m_frameCount);
    for (UINT i=0;i<m_frameCount;i++)
    {
        m_fenceValues[i] = 0;
    }

    ThrowIfFailed(m_device->CreateFence(m_fenceValues[m_currentBackBuffer], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
    m_fenceValues[m_currentBackBuffer]++;
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

    D3D12_RESOURCE_DESC depthStencilDesc;
    depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthStencilDesc.Alignment = 0;
    depthStencilDesc.Width = m_width;
    depthStencilDesc.Height = m_height;
    depthStencilDesc.DepthOrArraySize = 1;
    depthStencilDesc.MipLevels = 1;

    // Correction 11/12/2016: SSAO chapter requires an SRV to the depth buffer to read from 
    // the depth buffer.  Therefore, because we need to create two views to the same resource:
    //   1. SRV format: DXGI_FORMAT_R24_UNORM_X8_TYPELESS
    //   2. DSV Format: DXGI_FORMAT_D24_UNORM_S8_UINT
    // we need to create the depth buffer resource with a typeless format. 
    depthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
    depthStencilDesc.SampleDesc.Count = m_4xMsaaState ? 4 : 1;
    depthStencilDesc.SampleDesc.Quality = m_4xMsaaState ? (m_4xMsaaQuality - 1) : 0;
    depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE   optClear;
    optClear.Format = m_depthBufferFormat;
    optClear.DepthStencil.Depth = 1.0f;
    optClear.DepthStencil.Stencil = 0;
    ThrowIfFailed(m_device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &depthStencilDesc,
        D3D12_RESOURCE_STATE_COMMON,
        &optClear,
        IID_PPV_ARGS(&m_depthStencilBuffer)
    ));

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvHeapDesc.NodeMask = 0;
    ThrowIfFailed(m_device->CreateDescriptorHeap(
        &dsvHeapDesc,
        IID_PPV_ARGS(&m_dsvHeap)
    ));

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Format = m_depthBufferFormat;
    dsvDesc.Texture2D.MipSlice = 0;
    m_device->CreateDepthStencilView(m_depthStencilBuffer.Get(), &dsvDesc, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());

    // Transition the resource from its initial state to be used as a depth buffer.
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_depthStencilBuffer.Get(),
        D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));
    ThrowIfFailed(m_commandList->Close());

    ID3D12CommandList* cmdLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

    WaitForPreviousFrame();
}

void D3DAppBase::CreateRenderTargetViews()
{
    m_renderTargets.resize(m_frameCount);
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());


    // Create a RTV for each frame.
    for (UINT n = 0; n < m_frameCount; n++)
    {
        ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
        m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
        rtvHandle.Offset(1, m_rtvDescriptorSize);
    }
}

void D3DAppBase::BuildRootSignature()
{
    // Root parameter can be a table, root descriptor or root constants.
    CD3DX12_ROOT_PARAMETER slotRootParameter[2] = {};

    // Create a single descriptor table of CBVs.
    CD3DX12_DESCRIPTOR_RANGE cbvTable1;
    cbvTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
    slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable1);

    CD3DX12_DESCRIPTOR_RANGE cbvTable2;
    cbvTable2.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
    slotRootParameter[1].InitAsDescriptorTable(1, &cbvTable2);

    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc(_countof(slotRootParameter),slotRootParameter,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    
    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    ThrowIfFailed(D3D12SerializeRootSignature(
        &rootSignatureDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &signature,
        &error
    ));
    ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));

}

void D3DAppBase::BuildShader()
{
#if defined(_DEBUG)
    // Enable better shader debugging with the graphics debugging tools.
    UINT compileTags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT compileTags = 0;
#endif
    ThrowIfFailed(D3DCompileFromFile(GetAssetsFullPath(L"shader.hlsl").c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileTags, 0, &m_vertexShader, nullptr));
    ThrowIfFailed(D3DCompileFromFile(GetAssetsFullPath(L"shader.hlsl").c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileTags, 0, &m_pixelShader, nullptr));

    m_shaders["standardVS"] = m_vertexShader;
    m_shaders["opaquePS"] = m_pixelShader;
}

void D3DAppBase::BuildPSO()
{
    // Define the vertex input layout.
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
    {
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,12,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}
    };

    // Describe and create the graphics pipeline state object (PSO).
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputElementDescs,_countof(inputElementDescs) };
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(m_vertexShader.Get());
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(m_pixelShader.Get());
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;// For easily debug.
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.DSVFormat = m_depthBufferFormat;
    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
}

void D3DAppBase::BuildGeometry()
{
    /*Vertex triangleVertices[] =
    {
        { XMFLOAT3(-2.0f, +1.0f, +1.0f), XMFLOAT4(Colors::White) },
        { XMFLOAT3(-3.0f, 0.0f, +0.8f), XMFLOAT4(Colors::Black) },
        { XMFLOAT3(-1.5f, -1.0f, +1.2f), XMFLOAT4(Colors::Red) },
        { XMFLOAT3(-2.0f, -1.3f, +0.7f), XMFLOAT4(Colors::Yellow) },
        { XMFLOAT3(+1.5f, +1.0f, +1.0f), XMFLOAT4(Colors::Blue) },
        { XMFLOAT3(+1.0f, -1.0f, +0.7f), XMFLOAT4(Colors::Yellow) },
        { XMFLOAT3(+2.0f, -1.0f, +0.7f), XMFLOAT4(Colors::Cyan) },
        { XMFLOAT3(+1.7f, -0.7f, +1.2f), XMFLOAT4(Colors::Magenta) }
    };

    const UINT vertexBufferSize = sizeof(triangleVertices);

    uint16_t indices[] = { 
        0,1,3,
        0,3,2,
        3,1,2,
        2,1,0,

        0,1,2,
        0,3,1,
        0,2,3,
        3,2,1
    };

    const UINT indexBufferSize = sizeof(indices);

    m_geometry = std::make_unique<MeshGeometry>();
    m_geometry->name = "triangle";

    ThrowIfFailed(D3DCreateBlob(vertexBufferSize, &m_geometry->VertexBufferCPU));
    CopyMemory(m_geometry->VertexBufferCPU->GetBufferPointer(), triangleVertices, vertexBufferSize);

    ThrowIfFailed(D3DCreateBlob(indexBufferSize, &m_geometry->IndexBufferCPU));
    CopyMemory(m_geometry->IndexBufferCPU->GetBufferPointer(), indices, indexBufferSize);

    m_geometry->VertexBufferGPU = CreateDefaultBuffer(m_device.Get(), m_commandList.Get(), triangleVertices,
        vertexBufferSize, m_geometry->VertexBufferUploader);

    m_geometry->IndexBufferGPU = CreateDefaultBuffer(m_device.Get(), m_commandList.Get(), indices, 
        indexBufferSize, m_geometry->IndexBufferUploader);

    m_geometry->VertexByteStride = sizeof(Vertex);
    m_geometry->VertexBufferByteSize = vertexBufferSize;
    m_geometry->IndexFormat = DXGI_FORMAT_R16_UINT;
    m_geometry->IndexBufferByteSize = indexBufferSize;

    SubmeshGeometry submesh;
    submesh.IndexCount = _countof(indices) / 2;
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;

    SubmeshGeometry submesh2;
    submesh2.IndexCount = _countof(indices) / 2;
    submesh2.StartIndexLocation = _countof(indices) / 2;
    submesh2.BaseVertexLocation = _countof(triangleVertices) / 2;

    m_geometry->DrawArgs["triangle"] = submesh;
    m_geometry->DrawArgs["triangle2"] = submesh2;*/


    GeometryGenerator geoGen;
    GeometryGenerator::MeshData box = geoGen.CreateBox(1.5f, 0.5f, 1.5f, 3);
    GeometryGenerator::MeshData grid = geoGen.CreateGrid(20.0f, 30.0f, 60, 40);
    GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);
    GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);

    // We are concatenating all the geometry into a big vertex/index buffer.
    // So define the regions in the buffer each submesh covers.

    // Cache the vertex offsets to each object in the concatenated vertex buffer.
    UINT boxVertexOffset = 0;
    UINT gridVertexOffset = (UINT)box.Vertices.size();
    UINT sphereVertexOffset = (UINT)grid.Vertices.size() + gridVertexOffset;
    UINT cylinderVertexOffset = (UINT)sphere.Vertices.size() + sphereVertexOffset;

    // Cache the index offsets for each object in the concatenated index buffer.
    UINT boxIndexOffset = 0;
    UINT gridIndexOffset = (UINT)box.Indices32.size();
    UINT sphereIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size();
    UINT cylinderIndexOffset = sphereIndexOffset + (UINT)sphere.Indices32.size();

    // Define the SubmeshGeometry that cover different
    // regions of the vertex/index buffers.

    SubmeshGeometry boxSubmesh;
    boxSubmesh.IndexCount = (UINT)box.Indices32.size();
    boxSubmesh.StartIndexLocation = boxIndexOffset;
    boxSubmesh.BaseVertexLocation = boxVertexOffset;

    SubmeshGeometry gridSubmesh;
    gridSubmesh.IndexCount = (UINT)grid.Indices32.size();
    gridSubmesh.StartIndexLocation = gridIndexOffset;
    gridSubmesh.BaseVertexLocation = gridVertexOffset;

    SubmeshGeometry sphereSubmesh;
    sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
    sphereSubmesh.StartIndexLocation = sphereIndexOffset;
    sphereSubmesh.BaseVertexLocation = sphereVertexOffset;

    SubmeshGeometry cylinderSubmesh;
    cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
    cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
    cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;

    // Extract the vertex elements we are interested in and pack the 
    // vertices of all the meshes into one vertex buffer.

    unsigned int totalVertexCount =
        box.Vertices.size() +
        grid.Vertices.size() +
        sphere.Vertices.size() +
        cylinder.Vertices.size();

    std::vector<Vertex> vertices(totalVertexCount);
    UINT k = 0;
    for (size_t i = 0; i < box.Vertices.size(); i++,k++)
    {
        vertices[k].position = box.Vertices[i].Position;
        vertices[k].color = XMFLOAT4(DirectX::Colors::DarkGreen);
    }
    for (size_t i = 0; i < grid.Vertices.size(); i++,k++)
    {
        vertices[k].position = grid.Vertices[i].Position;
        vertices[k].color = XMFLOAT4(DirectX::Colors::ForestGreen);
    }
    for (size_t i = 0; i < sphere.Vertices.size(); i++,k++)
    {
        vertices[k].position = sphere.Vertices[i].Position;
        vertices[k].color = XMFLOAT4(DirectX::Colors::SteelBlue);
    }

    std::vector<std::uint16_t> indices;
    indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
    indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));
    indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));
    indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    m_geometry = std::make_unique<MeshGeometry>();
    m_geometry->name = "shapeGeo";
    ThrowIfFailed(D3DCreateBlob(vbByteSize, &m_geometry->VertexBufferCPU));
    CopyMemory(m_geometry->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &m_geometry->IndexBufferCPU));
    CopyMemory(m_geometry->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    m_geometry->VertexBufferGPU = CreateDefaultBuffer(m_device.Get(), 
        m_commandList.Get(), vertices.data(), vbByteSize
        , m_geometry->VertexBufferUploader);

    m_geometry->IndexBufferGPU = CreateDefaultBuffer(m_device.Get(),
        m_commandList.Get(), indices.data(), ibByteSize, m_geometry->IndexBufferUploader);

    m_geometry->VertexByteStride = sizeof(Vertex);
    m_geometry->VertexBufferByteSize = vbByteSize;

    m_geometry->IndexFormat = DXGI_FORMAT_R16_UINT;
    m_geometry->IndexBufferByteSize = ibByteSize;

    m_geometry->DrawArgs["box"] = boxSubmesh;
    m_geometry->DrawArgs["grid"] = gridSubmesh;
    m_geometry->DrawArgs["sphere"] = sphereSubmesh;
    m_geometry->DrawArgs["cylinder"] = cylinderSubmesh;

    m_geometries[m_geometry->name] = std::move(m_geometry);
}

void D3DAppBase::BuildConstantDescriptorHeaps()
{
    UINT objCount = (UINT)m_opaqueItems.size();
    // Need a CBV descriptor for each object for each frame resource.
    // +1 for the perPass CBV for each frame resource.
    UINT numDescriptors = (objCount + 1) * m_numberFrameResources;

    // Save an offset to the start of the pass CBVs. There are the last 3 descriptors.
    m_passCbvOffset = objCount * m_numberFrameResources;

    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
    cbvHeapDesc.NumDescriptors = numDescriptors;
    cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    cbvHeapDesc.NodeMask = 0;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&m_cbvHeap)));
}

void D3DAppBase::BuildConstantBuffer()
{
    m_objectCB = std::make_unique<UploadBuffer<ObjectConstants>>(m_device.Get(), 1, true);
    UINT objectConstantBufferSize = CalculateConstantBufferByteSize(sizeof(ObjectConstants));

    D3D12_GPU_VIRTUAL_ADDRESS cbAddress = m_objectCB->Resource()->GetGPUVirtualAddress();

    // Offset to the ith object constant buffer in the buffer.
    UINT64 constantBufferIndex = 0;
    cbAddress += constantBufferIndex * objectConstantBufferSize;

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
    cbvDesc.BufferLocation = cbAddress;
    cbvDesc.SizeInBytes = objectConstantBufferSize;

    m_device->CreateConstantBufferView(&cbvDesc, m_cbvHeap->GetCPUDescriptorHandleForHeapStart());
}

void D3DAppBase::BuildPSOs()
{
    m_inputLayout =
    {
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,12,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}
    };
    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueDesc;

    // PSO for opaque objects.
    ZeroMemory(&opaqueDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    opaqueDesc.InputLayout = { m_inputLayout.data(),(UINT)m_inputLayout.size() };
    opaqueDesc.pRootSignature = m_rootSignature.Get();
    opaqueDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
}

void D3DAppBase::BuildRenderItems()
{
    unsigned int constantBufferIndex = 0;
    const D3D12_PRIMITIVE_TOPOLOGY primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    std::unique_ptr<RenderItem> boxRenderItem = std::make_unique<RenderItem>();
    boxRenderItem->World = XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(0.0f, 0.5f, 0.0f);
    boxRenderItem->ObjectConstantBufferIndex = constantBufferIndex;// First item in constant buffer.
    constantBufferIndex++;
    boxRenderItem->Geo = m_geometries["shapeGeo"].get();
    boxRenderItem->PrimitiveType = primitiveType;
    boxRenderItem->IndexCount = m_geometries["shapeGeo"]->DrawArgs["box"].IndexCount;
    boxRenderItem->StartIndexLocation = m_geometries["shapeGeo"]->DrawArgs["box"].StartIndexLocation;
    boxRenderItem->BaseVertexLocation = m_geometries["shapeGeo"]->DrawArgs["box"].BaseVertexLocation;
    m_allItems.push_back(std::move(boxRenderItem));

    std::unique_ptr<RenderItem> gridRenderItem = std::make_unique<RenderItem>();
    gridRenderItem->World = XMMatrixIdentity();
    gridRenderItem->ObjectConstantBufferIndex = constantBufferIndex++;
    gridRenderItem->Geo = m_geometries["shapeGeo"].get();
    gridRenderItem->PrimitiveType = primitiveType;
    gridRenderItem->IndexCount = gridRenderItem->Geo->DrawArgs["grid"].IndexCount;
    gridRenderItem->StartIndexLocation = gridRenderItem->Geo->DrawArgs["grid"].StartIndexLocation;
    gridRenderItem->BaseVertexLocation = gridRenderItem->Geo->DrawArgs["grid"].BaseVertexLocation;
    m_allItems.push_back(std::move(gridRenderItem));

    std::unique_ptr<RenderItem> cylinderItem = std::make_unique<RenderItem>();
    cylinderItem->World = XMMatrixTranslation(-5.0f, 1.5f, -10.0f);
    cylinderItem->ObjectConstantBufferIndex = constantBufferIndex++;
    cylinderItem->Geo = m_geometries["shapeGeo"].get();
    cylinderItem->PrimitiveType = primitiveType;
    cylinderItem->IndexCount = cylinderItem->Geo->DrawArgs["cylinder"].IndexCount;
    cylinderItem->StartIndexLocation = cylinderItem->Geo->DrawArgs["cylinder"].StartIndexLocation;
    cylinderItem->BaseVertexLocation = cylinderItem->Geo->DrawArgs["cylinder"].BaseVertexLocation;
    m_allItems.push_back(std::move(cylinderItem));

    std::unique_ptr<RenderItem> sphereItem = std::make_unique<RenderItem>();
    sphereItem->World = XMMatrixTranslation(-5.0f, 3.5f, -10.0f);
    sphereItem->ObjectConstantBufferIndex = constantBufferIndex++;
    sphereItem->PrimitiveType = primitiveType;
    sphereItem->Geo = m_geometries["shapeGeo"].get();
    sphereItem->IndexCount = sphereItem->Geo->DrawArgs["sphere"].IndexCount;
    sphereItem->StartIndexLocation = sphereItem->Geo->DrawArgs["sphere"].StartIndexLocation;
    sphereItem->BaseVertexLocation = sphereItem->Geo->DrawArgs["sphere"].BaseVertexLocation;
    m_allItems.push_back(std::move(sphereItem));

    for (auto& e:m_allItems)
    {
        m_opaqueItems.push_back(e.get());
    }
}

void D3DAppBase::OnInit()
{
    InitializePipeline();
    CreateRenderTargetViews();
    BuildRootSignature();
    BuildShader();
    BuildPSO();
    BuildConstantDescriptorHeaps();
    BuildConstantBuffer();

    ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_currentBackBuffer].Get(), m_pipelineState.Get()));
    BuildGeometry();
    m_commandList->Close();
    ID3D12CommandList* cmdLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

    WaitForGPU();
}

void D3DAppBase::CreateCommandObjects()
{
    CreateCommandQueue();
    CreateCommandAllocators();
    CreateCommandList();
}

void D3DAppBase::PopulateCommandList()
{
    // Command list allocators can only be reset when the associated 
    // command lists have finished execution on the GPU; apps should use 
    // fences to determine GPU execution progress.
    ThrowIfFailed(m_commandAllocators[m_currentBackBuffer]->Reset());

    // However, when ExecuteCommandList() is called on a particular command 
    // list, that command list can then be reset at any time and must be before 
    // re-recording.
    ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_currentBackBuffer].Get(), m_pipelineState.Get()));

    // Set necessary state.
    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    m_commandList->RSSetViewports(1, &m_viewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    // Indicate that the back buffer will be used as a render target.
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        m_renderTargets[m_currentBackBuffer].Get(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET));

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_currentBackBuffer, m_rtvDescriptorSize);
    

    // Record commands.
    m_commandList->ClearRenderTargetView(rtvHandle, Colors::SteelBlue, 0, nullptr);
    m_commandList->ClearDepthStencilView(m_dsvHeap->GetCPUDescriptorHandleForHeapStart(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
    m_commandList->OMSetRenderTargets(1, &rtvHandle, true, &m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_commandList->IASetVertexBuffers(0, 1, &m_geometry->VertexBufferView());
    m_commandList->IASetIndexBuffer(&m_geometry->IndexBufferView());
    
    ID3D12DescriptorHeap* descriptorHeaps[] = { m_cbvHeap.Get() };
    m_commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    m_commandList->SetGraphicsRootDescriptorTable(0, m_cbvHeap->GetGPUDescriptorHandleForHeapStart());
    
    m_commandList->DrawIndexedInstanced(m_geometry->DrawArgs["triangle"].IndexCount, 1, m_geometry->DrawArgs["triangle"].StartIndexLocation, m_geometry->DrawArgs["triangle"].BaseVertexLocation, 0);
    m_commandList->DrawIndexedInstanced(m_geometry->DrawArgs["triangle2"].IndexCount, 1, m_geometry->DrawArgs["triangle2"].StartIndexLocation, m_geometry->DrawArgs["triangle2"].BaseVertexLocation, 0);

    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_currentBackBuffer].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
    ThrowIfFailed(m_commandList->Close());
}

void D3DAppBase::WaitForPreviousFrame()
{
    const UINT64 fence = m_currentFence;
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fence));
    m_currentFence++;

    if (m_fence->GetCompletedValue() < fence)
    {
        ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }
    m_currentBackBuffer = m_swapChain->GetCurrentBackBufferIndex();
}

void D3DAppBase::WaitForGPU()
{
    // Schedule a Signal command in the queue.
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_fenceValues[m_currentBackBuffer]));

    // Wait until the fence has been processed.
    ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_currentBackBuffer], m_fenceEvent));
    WaitForSingleObjectEx(m_fenceEvent, INFINITE, false);

    // Increment the fence value for the current frame.
    m_fenceValues[m_currentBackBuffer]++;
}

void D3DAppBase::MoveToNextFrame()
{
    // Schedule a Signal command in the queue.
    const UINT64 currentFenceValue = m_fenceValues[m_currentBackBuffer];
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), currentFenceValue));

    // Update the frame index.
    m_currentBackBuffer = m_swapChain->GetCurrentBackBufferIndex();
    if (m_fence->GetCompletedValue() < m_fenceValues[m_currentBackBuffer])
    {
        ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_currentBackBuffer], m_fenceEvent));
        WaitForSingleObjectEx(m_fenceEvent, INFINITE, false);
    }

    // Set the fence value for the next frame.
    m_fenceValues[m_currentBackBuffer] = currentFenceValue + 1;
}

void D3DAppBase::BuildFrameResources()
{
    for (UINT i=0;i<m_numberFrameResources;i++)
    {
        m_frameResources.push_back(std::make_unique<FrameResource>(m_device.Get(), 1, (UINT)m_renderItems.size()));
    }
}

void D3DAppBase::UpdateObjectConstantBuffers()
{
    UploadBuffer<ObjectConstants>* currentObjectConstantBuffer = m_currentFrameResource->m_objectConstantBuffer.get();
    for (UINT i = 0; i < m_renderItems.size(); i++)
    {
        if (m_renderItems[i]->NumFramesDirty > 0)
        {
            DirectX::XMMATRIX& matrix = m_renderItems[i]->World;
            ObjectConstants objConstants;
            objConstants.World = DirectX::XMMatrixTranspose(matrix);
            currentObjectConstantBuffer->CopyData(
                m_renderItems[i]->ObjectConstantBufferIndex,
                objConstants);
            m_renderItems[i]->NumFramesDirty--;
        }
    }
}

void D3DAppBase::UpdateMainPassConstantBuffer(const GameTimer&gt)
{
    XMMATRIX viewProj = DirectX::XMMatrixMultiply(m_view, m_proj);
    XMMATRIX invView = DirectX::XMMatrixInverse(&DirectX::XMMatrixDeterminant(m_view), m_view);
    XMMATRIX invProj = DirectX::XMMatrixInverse(&DirectX::XMMatrixDeterminant(m_proj), m_proj);
    XMMATRIX invViewProj = DirectX::XMMatrixInverse(&DirectX::XMMatrixDeterminant(viewProj), viewProj);

    m_mainPassCB.View = m_view;
    m_mainPassCB.InvView = invView;
    m_mainPassCB.Proj = m_proj;
    m_mainPassCB.InvProj = invProj;
    m_mainPassCB.ViewProj = viewProj;
    m_mainPassCB.InvViewProj = invViewProj;
    m_mainPassCB.EyePosW = m_eyePos;
    m_mainPassCB.RenderTargetSize = XMFLOAT2(static_cast<float>(m_width), static_cast<float>(m_height));
    m_mainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / m_width, 1.0f / m_height);
    m_mainPassCB.nearZ = 1.0f;
    m_mainPassCB.farZ = 1000.0f;
    m_mainPassCB.TotalTime = gt.TotalTime;
    m_mainPassCB.DeltaTime = gt.DeltaTime;

    m_currentFrameResource->m_passConstantBuffer->CopyData(0, m_mainPassCB);
}

void D3DAppBase::OnUpdate()
{
    // Convert Spherical to Cartesian coordinates.
    float x = m_radius * sinf(m_phi) * cosf(m_theta);
    float z = m_radius * sinf(m_phi) * sinf(m_theta);
    float y = m_radius * cosf(m_phi);

    // Build the view matrix.
    XMVECTOR pos = XMVectorSet(x, y, z, 1.0f);
    XMVECTOR target = XMVectorZero();
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMMATRIX view = DirectX::XMMatrixLookAtLH(pos, target, up);
    XMMATRIX world = DirectX::XMMatrixIdentity();
    XMMATRIX proj = DirectX::XMMatrixPerspectiveFovLH(0.25f * XM_PI, m_aspectRatio, 1.0f, 1000.0f);

    XMMATRIX worldViewProj = world * view * proj;
    XMMATRIX& tempWorldViewProj = worldViewProj;

    ObjectConstants objectConstants;
    objectConstants.World = DirectX::XMMatrixTranspose(tempWorldViewProj);
    m_objectCB->CopyData(0, objectConstants);
}

void D3DAppBase::OnRender()
{
    // Record all the commands we need to render the scene into the command list.
    PopulateCommandList();

    // Execute the command list.
    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    ThrowIfFailed(m_swapChain->Present(1, 0));

    MoveToNextFrame();
}

void D3DAppBase::OnDestroy()
{
    WaitForGPU();
    CloseHandle(m_fenceEvent);
}