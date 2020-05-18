#include "stdafx.h"
#include "D3DAppBox.h"

D3DAppBox::D3DAppBox(UINT width, UINT height, std::wstring name, UINT frameCount /* = 2 */):
    D3DAppBase(width,height,name,frameCount)
{

}

D3DAppBox::~D3DAppBox()
{
    m_gameTimer.release();
}

void D3DAppBox::OnInit()
{

}

void D3DAppBox::OnUpdate()
{

}

void D3DAppBox::OnRender()
{

}

void D3DAppBox::OnDestroy()
{

}