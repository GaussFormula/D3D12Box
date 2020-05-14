#include "stdafx.h"
#include "D3DAppBase.h"
#include "Win32Application.h"
using namespace Microsoft::WRL;
D3DAppBase::D3DAppBase(UINT width, UINT height, std::wstring name, UINT frameCount /* = 2 */):
    m_width(width),
    m_height(height),
    m_title(name),
    m_useWarpDevice(false),
    m_frameCount(frameCount)
{
    m_gameTimer = std::make_unique<GameTimer>();
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
