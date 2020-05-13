#include "stdafx.h"
#include "D3DAppBase.h"
#include "Win32Application.h"

_Use_decl_annotations_
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
    D3DAppBase sample(1280, 720, L"Hello Box!");
    return Win32Application::Run(&sample, hInstance, nShowCmd);
}