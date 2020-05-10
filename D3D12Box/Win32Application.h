#pragma once
#include "D3DAppBase.h"
class D3DAppBase;

class Win32Application
{
public:
    static int Run(D3DAppBase* pSample, HINSTANCE hInstance, int nCmdShow);
    static HWND GetHwnd() { return m_hwnd; }
protected:
    static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
private:
    static HWND m_hwnd;
};