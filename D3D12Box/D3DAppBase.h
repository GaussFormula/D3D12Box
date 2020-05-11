#pragma once
#if defined(DEBUG)|| defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif // defined(DEBUG)|| defined(_DEBUG)

#include "stdafx.h"


class D3DAppBase
{
public:
    void ParseCommandLineArgs(_In_reads_(argc) WCHAR* argv[], int argc);
    UINT GetWidth()const { return m_width; }
    UINT GetHeight()const { return m_height; }
protected:
private:
    bool m_useWarpDevice = false;
    std::wstring m_title;
    UINT m_width;
    UINT m_height;
};