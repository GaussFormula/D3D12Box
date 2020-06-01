#pragma once
#include "stdafx.h"

class FrameResource
{
public:
    FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount);
    FrameResource(const FrameResource& rhs) = delete;
    FrameResource& operator=(const FrameResource& rhs) = delete;
    ~FrameResource();

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator>  m_commandAllocator;
};