#pragma once
#include "D3DAppBase.h"

class D3DAppBox:public D3DAppBase
{
public:
    D3DAppBox(UINT width, UINT height, std::wstring name, UINT frameCount = 2);
    virtual ~D3DAppBox();
protected:
private:
};