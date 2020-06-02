#pragma once
#include "stdafx.h"
#include "UploadBuffer.h"
class FrameResource
{
public:
    FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount);
    FrameResource(const FrameResource& rhs) = delete;
    FrameResource& operator=(const FrameResource& rhs) = delete;
    ~FrameResource();

    // Before GPU handled all commands tied with the command allocator,
    // Cannot be reset.
    // Every frame should has its own command allocator.
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator>  m_commandAllocator;
    
    // Before GPU had executed all commands refer to the constant buffer, cannot update this.
    // Every frame should has its own constant buffer.
    std::unique_ptr<UploadBuffer<PassConstants>>    m_passConstantBuffer = nullptr;
    std::unique_ptr<UploadBuffer<ObjectConstants>>  m_objectConstantBuffer = nullptr;

    UINT m_fenceValue = 0;
};