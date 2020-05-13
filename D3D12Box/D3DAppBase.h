#pragma once
#if defined(DEBUG)|| defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif // defined(DEBUG)|| defined(_DEBUG)

#include "stdafx.h"
#include "GameTimer.h"

using Microsoft::WRL::ComPtr;


class D3DAppBase
{
public:
    D3DAppBase(UINT width, UINT height, std::wstring name, UINT frameCount = 2);
    ~D3DAppBase();

    void ParseCommandLineArgs(_In_reads_(argc) WCHAR* argv[], int argc);

    void CalculateFrameStats();

    // Accessors
    UINT GetWidth()const { return m_width; }
    UINT GetHeight()const { return m_height; }
    const WCHAR* GetTitle()const { return m_title.c_str(); }

    auto Run()->int;
protected:
private:
    bool m_useWarpDevice = false;
    std::wstring m_title;
    UINT m_width;
    UINT m_height;

    std::unique_ptr<GameTimer> m_gameTimer;

    UINT m_frameCount = 2;
};