#pragma once
#include "stdafx.h"
#include "D3DAppUtil.h"
using namespace DirectX;
class RenderItem
{
public:
    RenderItem() = default;
    XMMATRIX World;
    UINT NumFramesDirty = 0;

    UINT ObjectConstantBufferIndex = -1;
    std::unique_ptr<MeshGeometry>   Geo = nullptr;

    // Topology;
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    // Parameters for DrawIndexedInstaced function.
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    UINT BaseVertexLocation = 0;
};