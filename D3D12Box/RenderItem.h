#pragma once
#include "stdafx.h"
#include "D3DAppUtil.h"
using namespace DirectX;

// A lightweight structure that stores data needed to draw an object.
// This will vary from app to app.
class RenderItem
{
public:
    RenderItem() = default;
    XMMATRIX World = XMMatrixIdentity();

    // Dirty flag indicating the object data has changed.
    // And we need to update the constant buffer.
    // We have an object cbuffer for each FrameResource, we have to apply the update
    // to each FrameResource. Thus, when we modify object data, we should set
    // NumFramesDirty = NumFrameResources so that each frame resource gets the update.
    UINT NumFramesDirty = 3;

    UINT ObjectConstantBufferIndex = -1;
    MeshGeometry*   Geo = nullptr;

    // Topology;
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    // Parameters for DrawIndexedInstaced function.
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    UINT BaseVertexLocation = 0;
};