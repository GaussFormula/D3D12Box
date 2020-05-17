#include "D3DAppBox.h"

D3DAppBox::D3DAppBox(UINT width, UINT height, std::wstring name, UINT frameCount /* = 2 */):
    D3DAppBase(width,height,name,frameCount)
{

}

D3DAppBox::~D3DAppBox()
{
    m_gameTimer.release();
}