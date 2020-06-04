#pragma once
#include "stdafx.h"
class GeometryGenerator
{
public:
    typedef std::uint16_t uint16;
    typedef std::uint32_t uint32;

    class Vertex
    {
    public:
        Vertex(){}

        Vertex(
            const DirectX::XMFLOAT3& p,
            const DirectX::XMFLOAT3& n,
            const DirectX::XMFLOAT3& t,
            const DirectX::XMFLOAT2& uv
        ):
            Position(p),
            Normal(n),
            TangentU(t),
            TexC(uv)
        {}
        Vertex(
            float px,float py,float pz,
            float nx,float ny,float nz,
            float tx,float ty,float tz,
            float u,float v
        ):
            Position(px,py,pz),
            Normal(nx,ny,nz),
            TangentU(tx,ty,tz),
            TexC(u,v)
        {}

        DirectX::XMFLOAT3   Position;
        DirectX::XMFLOAT3   Normal;
        DirectX::XMFLOAT3   TangentU;
        DirectX::XMFLOAT2   TexC;
    };
    class MeshData
    {
    public:
        std::vector<Vertex> Vertices;
        std::vector<uint32> Indices32;

        std::vector<uint16>& GetIndices16()
        {
            if (m_indices16.empty())
            {
                m_indices16.resize(Indices32.size());
                for (UINT i = 0; i < Indices32.size(); i++)
                {
                    m_indices16[i] = static_cast<uint16>(Indices32[i]);
                }
            }
            return m_indices16;
        }
    private:
        std::vector<uint16> m_indices16;
    };
};