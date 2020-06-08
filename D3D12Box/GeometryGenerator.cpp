#include "stdafx.h"
#include "GeometryGenerator.h"

GeometryGenerator::MeshData GeometryGenerator::CreateCylinder(float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount)
{
    MeshData meshData;

    // Build stacks.

    float stackHeight = height / stackCount;

    // Amount to increment radius as move up each stack level from bottom to top.
    float radiusStep = (topRadius - bottomRadius) / stackCount;

    uint32 ringCount = stackCount + 1;

    // Compute vertices for each stack ring starting at the bottom and moving up.
    for (uint32 i=0;i<ringCount;i++)
    {
        float y = -0.5 * height + i * stackHeight;
        float r = bottomRadius + i * radiusStep;

        // vertices of ring
        float dTheta = 2.0f * DirectX::XM_PI / sliceCount;
        for (uint32 j=0;j<=sliceCount;j++)
        {
            Vertex vertex;
            float c = cosf(j * dTheta);
            float s = sinf(j * dTheta);

            vertex.Position = DirectX::XMFLOAT3(r * c, y, r * s);
            vertex.TexC.x = (float)j / sliceCount;
            vertex.TexC.y = 1.0f - (float)i / stackCount;

            // Cylinder can be parameterized as follows, where we introduce v
            // parameter that goes in the same direction as the v tex-coord
            // so that the bitangent goes in the same direction as the v tex-coord
            // Let r0 be the bottom radius and let r1 be the top radius.
            // y(v)=h-hv for v in [0,1].
            // r(v)=r1+(r0-r1)v
            // x(t,v)=r(v)*cos(t)
            // y(t,v)=h-hv
            // z(t,v)=r(v)*sin(t)
            //
            // dx/dt=-r(v)*sin(t)
            // dy/dt=0
            // dz/dt=+r(v)*cos(t)
            //
            // dx/dv=(r0-r1)*cos(t)
            // dy/dv=-h
            // dz/dv=(r0-r1)*sin(t)

            // This is unit length
            float deltaV1 = 0.0f;
            float deltaV2 = 1.0f / stackCount;
            float deltaU1 = 1.0f / sliceCount;
            float deltaU2 = deltaU1;
            DirectX::XMFLOAT3 Q1 = DirectX::XMFLOAT3(
                r * cosf((j + 1) * dTheta) - vertex.Position.x,
                y - vertex.Position.y,
                r * sinf((j + 1) * dTheta) - vertex.Position.z
            );
            DirectX::XMFLOAT3 Q2 = DirectX::XMFLOAT3(
                r * cosf((j + 1) * dTheta) - vertex.Position.x,
                y + stackHeight - vertex.Position.y,
                r * sinf((j + 1) * dTheta) - vertex.Position.z
            );
            
            vertex.TangentU = DirectX::XMFLOAT3(
                (deltaV1 * Q2.x - deltaV2 * Q1.x) / (deltaV1 * deltaU2 - deltaV2 * deltaU1),
                (deltaV1 * Q2.y - deltaV2 * Q1.y) / (deltaV1 * deltaU2 - deltaV2 * deltaU1),
                (deltaV1 * Q2.z - deltaV2 * Q1.z) / (deltaV1 * deltaU2 - deltaV2 * deltaU1)
            );
            DirectX::XMVECTOR temp = DirectX::XMLoadFloat3(&vertex.TangentU);
            temp = DirectX::XMVector3Normalize(temp);
            DirectX::XMStoreFloat3(&vertex.TangentU, temp);

            DirectX::XMFLOAT3 bitangent = DirectX::XMFLOAT3(
                (-deltaU1 * Q2.x + deltaU2 * Q1.x) / (deltaV1 * deltaU2 - deltaV2 * deltaU1),
                (-deltaU1 * Q2.y + deltaU2 * Q1.y) / (deltaV1 * deltaU2 - deltaV2 * deltaU1),
                (-deltaU1 * Q2.z + deltaU2 * Q1.z) / (deltaV1 * deltaU2 - deltaV2 * deltaU1)
            );
            DirectX::XMVECTOR B = DirectX::XMLoadFloat3(&bitangent);
            DirectX::XMVECTOR T = DirectX::XMLoadFloat3(&vertex.TangentU);
            DirectX::XMVECTOR N = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(T, B));
            DirectX::XMStoreFloat3(&vertex.Normal, N);

            meshData.Vertices.push_back(vertex);
        }
    }

    // Add one because we duplicate the first and last vertex per ring
    // since texture coordinates are different.
    uint32 ringVertexCount = sliceCount + 1;

    // Compute indices for each stack.
    for (uint32 i = 0; i < stackCount; i++)
    {
        for (uint32 j=0;j<sliceCount;j++)
        {
            meshData.Indices32.push_back(i * ringVertexCount + j);
            meshData.Indices32.push_back((i + 1) * ringVertexCount + j + 1);
            meshData.Indices32.push_back((i + 1) * ringVertexCount + j);
            
            meshData.Indices32.push_back(i * ringVertexCount + j);
            meshData.Indices32.push_back(i * ringVertexCount + j + 1);
            meshData.Indices32.push_back((i + 1) * ringVertexCount + j + 1);
        }
    }
    BuildCylinderTopCap(bottomRadius, topRadius, height, sliceCount, stackCount, meshData);
    BuildCylinderBottomCap(bottomRadius, topRadius, height, sliceCount, stackCount, meshData);
}

void GeometryGenerator::BuildCylinderTopCap(float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount, MeshData& meshData)
{
    uint32 baseIndex = (uint32)meshData.Vertices.size();
    float y = 0.5f * height;
    float dTheta = 2.0 * DirectX::XM_PI / sliceCount;

    // Duplicate cap ring vertices because the texture coordinates and normals differ.
    for (uint32 i=0;i<=sliceCount;i++)
    {
        float x = topRadius * cosf(i * dTheta);
        float z = topRadius * sinf(i * dTheta);

        // Scale down by the height to try and make top cap texture coordinate
        // area proportional to base.
        float u = x / height + 0.5f;
        float v = z / height + 0.5f;

        meshData.Vertices.push_back(Vertex(x, y, z, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, u, v));

        // Cap center vertex
        meshData.Vertices.push_back(
            Vertex(0.0f, y, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.5f, 0.5f)
        );

        // Index of center vertex.
        uint32 centerIndex = (uint32)meshData.Vertices.size() - 1;

        for (uint32 i=0;i<sliceCount;i++)
        {
            meshData.Indices32.push_back(centerIndex);
            meshData.Indices32.push_back(baseIndex + i);
            meshData.Indices32.push_back(baseIndex + i + 1);
        }
    }
}

void GeometryGenerator::BuildCylinderBottomCap(float bottomRadius, float topRadius, float height, uint32 sliceCount, uint32 stackCount, MeshData& meshData)
{
    uint32 baseIndex = (uint32)meshData.Vertices.size();
    float y = -0.5f * height;
    float dTheta = 2.0f * DirectX::XM_PI / sliceCount;

    for (uint32 i = 0; i < sliceCount; i++)
    {
        float x = bottomRadius * cosf(i * dTheta);
        float z = bottomRadius * sinf(i * dTheta);

        float u = x / height + 0.5f;
        float v = z / height + 0.5f;

        meshData.Vertices.push_back(
            Vertex(x, y, z, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, u, v)
        );
    }

    // Bottom center vertex.
    meshData.Vertices.push_back(
        Vertex(0.0f, y, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.5f, 0.5f)
    );

    // Cache the index of center vertex.
    uint32 centerIndex = (uint32)meshData.Vertices.size() - 1;

    for (uint32 i = 0; i < sliceCount; i++)
    {
        meshData.Indices32.push_back(centerIndex);
        meshData.Indices32.push_back(baseIndex + i + 1);
        meshData.Indices32.push_back(baseIndex + i);
    }
}

GeometryGenerator::MeshData GeometryGenerator::CreateSphere(float radius, uint32 sliceCount, uint32 stackCount)
{
    MeshData meshData;

    // Compute the vertices starting at the top pole and moving down the stacks.

    // Poles: note that there will be texture coordinate distortion as there is
    // not a unique point on the texture map to assign to the pole when mapping
    // a rectangular texture onto a sphere.
    Vertex topVertex(0.0f, +radius, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    Vertex bottomVertex(0.0f, -radius, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);

    meshData.Vertices.push_back(topVertex);

    float phiStep = DirectX::XM_PI / stackCount;
    float thetaStep = DirectX::XM_PI * 2.0f / sliceCount;

    // Compute vertices for each stack ring (do not count the poles as rings).
    for (uint32 i = 1; i <= stackCount - 1; i++)
    {
        float phi = i * phiStep;

        // Vertices of rings.
        for (uint32 j = 0; j <= sliceCount; j++)
        {
            float theta = j * thetaStep;
            
            Vertex v;

            // Spherical to cartesian
            v.Position.x = radius * sinf(phi) * cosf(theta);
            v.Position.y = radius * cosf(phi);
            v.Position.z = radius * sinf(phi) * sinf(theta);
        }
    }
}