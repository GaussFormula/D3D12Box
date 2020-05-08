#include "Win32Application.h"

int Win32Application::Run(D3DAppBase* pSample, HINSTANCE hInstance, int nCmdShow)
{
    // Parse the command line parameters.
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
}