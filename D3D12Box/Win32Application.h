#pragma once
#include "D3DAppBase.h"
class D3DAppBase;

class Win32Application
{
public:
    static int Run(D3DAppBase* pSample, HINSTANCE hInstance, int nCmdShow);

};