#pragma once
#include "stdafx.h"
#include "D3DAppBase.h"

class D3DAppBox:public D3DAppBase
{
public:
    D3DAppBox(UINT width, UINT height, std::wstring name, UINT frameCount = 2);
    virtual ~D3DAppBox();
protected:
private:
    virtual void OnInit()override;
    virtual void OnUpdate()override;
    virtual void OnRender()override;
    virtual void OnDestroy()override;
};